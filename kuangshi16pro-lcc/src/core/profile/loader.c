#include "lcc/profile.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  LCC_SECTION_NONE = 0,
  LCC_SECTION_MODE,
  LCC_SECTION_POWER,
  LCC_SECTION_FAN
} lcc_profile_section_t;

static char *trim(char *text) {
  char *end = NULL;

  if (text == NULL) {
    return NULL;
  }

  while (*text != '\0' && isspace((unsigned char)*text)) {
    ++text;
  }

  if (*text == '\0') {
    return text;
  }

  end = text + strlen(text) - 1;
  while (end > text && isspace((unsigned char)*end)) {
    *end = '\0';
    --end;
  }

  return text;
}

static lcc_status_t parse_u8_text(const char *text, uint8_t *value) {
  char *end = NULL;
  unsigned long parsed = 0;

  if (text == NULL || value == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  parsed = strtoul(text, &end, 0);
  if (end == text || *end != '\0') {
    return LCC_ERR_PARSE;
  }
  if (parsed > 0xfful) {
    return LCC_ERR_RANGE;
  }

  *value = (uint8_t)parsed;
  return LCC_OK;
}

static lcc_status_t parse_bool_text(const char *text, bool *value) {
  if (text == NULL || value == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  if (strcmp(text, "1") == 0 || strcmp(text, "true") == 0 ||
      strcmp(text, "yes") == 0 || strcmp(text, "on") == 0) {
    *value = true;
    return LCC_OK;
  }
  if (strcmp(text, "0") == 0 || strcmp(text, "false") == 0 ||
      strcmp(text, "no") == 0 || strcmp(text, "off") == 0) {
    *value = false;
    return LCC_OK;
  }

  return LCC_ERR_PARSE;
}

static lcc_status_t load_text_file(const char *path, char *buffer,
                                   size_t buffer_len) {
  FILE *stream = NULL;
  size_t bytes = 0;

  if (path == NULL || buffer == NULL || buffer_len < 2u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  stream = fopen(path, "r");
  if (stream == NULL) {
    return LCC_ERR_IO;
  }

  bytes = fread(buffer, 1u, buffer_len - 1u, stream);
  if (ferror(stream) != 0) {
    (void)fclose(stream);
    return LCC_ERR_IO;
  }
  if (!feof(stream)) {
    (void)fclose(stream);
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  buffer[bytes] = '\0';
  (void)fclose(stream);
  return LCC_OK;
}

static void profile_document_init(lcc_profile_document_t *document) {
  if (document == NULL) {
    return;
  }

  memset(document, 0, sizeof(*document));
  document->fan_table.activated = true;
  document->fan_table.fan_control_respective = true;
}

static void default_fan_name_from_path(const char *path, char *buffer,
                                       size_t buffer_len) {
  const char *base = NULL;
  const char *dot = NULL;
  size_t name_len = 0;

  if (path == NULL || buffer == NULL || buffer_len == 0u) {
    return;
  }

  base = strrchr(path, '/');
  base = base == NULL ? path : base + 1;
  dot = strrchr(base, '.');
  name_len = dot != NULL && dot > base ? (size_t)(dot - base) : strlen(base);

  if (name_len == 0u) {
    (void)snprintf(buffer, buffer_len, "from-file");
    return;
  }

  if (name_len >= buffer_len) {
    name_len = buffer_len - 1u;
  }

  memcpy(buffer, base, name_len);
  buffer[name_len] = '\0';
}

static bool json_find_key(const char *json, const char *key, const char **cursor) {
  const char *match = NULL;

  if (json == NULL || key == NULL || cursor == NULL) {
    return false;
  }

  match = strstr(json, key);
  if (match == NULL) {
    return false;
  }

  *cursor = match;
  return true;
}

static bool json_parse_string(const char *json, const char *key, char *buffer,
                              size_t buffer_len) {
  const char *cursor = NULL;
  const char *start = NULL;
  const char *end = NULL;
  size_t len = 0;

  if (!json_find_key(json, key, &cursor) || buffer == NULL || buffer_len == 0u) {
    return false;
  }
  start = strchr(cursor, ':');
  if (start == NULL) {
    return false;
  }
  start = strchr(start, '"');
  if (start == NULL) {
    return false;
  }
  ++start;
  end = strchr(start, '"');
  if (end == NULL) {
    return false;
  }
  len = (size_t)(end - start);
  if (len + 1u > buffer_len) {
    return false;
  }

  memcpy(buffer, start, len);
  buffer[len] = '\0';
  return true;
}

static bool json_parse_bool(const char *json, const char *key, bool *value) {
  const char *cursor = NULL;

  if (!json_find_key(json, key, &cursor) || value == NULL) {
    return false;
  }
  cursor = strchr(cursor, ':');
  if (cursor == NULL) {
    return false;
  }
  ++cursor;
  while (*cursor == ' ' || *cursor == '\n' || *cursor == '\r' ||
         *cursor == '\t') {
    ++cursor;
  }
  if (strncmp(cursor, "true", 4) == 0) {
    *value = true;
    return true;
  }
  if (strncmp(cursor, "false", 5) == 0) {
    *value = false;
    return true;
  }

  return false;
}

static bool json_parse_u8_from_object(const char *object, const char *key,
                                      uint8_t *value) {
  const char *cursor = NULL;
  char *end = NULL;
  unsigned long parsed = 0;

  if (!json_find_key(object, key, &cursor) || value == NULL) {
    return false;
  }
  cursor = strchr(cursor, ':');
  if (cursor == NULL) {
    return false;
  }
  ++cursor;
  while (*cursor == ' ' || *cursor == '\n' || *cursor == '\r' ||
         *cursor == '\t') {
    ++cursor;
  }

  parsed = strtoul(cursor, &end, 10);
  if (end == cursor || parsed > 0xfful) {
    return false;
  }

  *value = (uint8_t)parsed;
  return true;
}

static lcc_status_t parse_fan_json_array(const char *json, const char *key,
                                         lcc_fan_point_t *points) {
  const char *array = NULL;
  const char *cursor = NULL;
  size_t count = 0;
  lcc_fan_point_t last_point;

  if (json == NULL || key == NULL || points == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (!json_find_key(json, key, &array)) {
    return LCC_ERR_PARSE;
  }
  array = strchr(array, '[');
  if (array == NULL) {
    return LCC_ERR_PARSE;
  }
  cursor = array + 1;
  memset(&last_point, 0, sizeof(last_point));

  while (*cursor != '\0' && *cursor != ']') {
    const char *object_end = NULL;
    char object[256];
    size_t object_len = 0;
    lcc_fan_point_t point;

    while (*cursor != '\0' && *cursor != '{' && *cursor != ']') {
      ++cursor;
    }
    if (*cursor == ']') {
      break;
    }
    object_end = strchr(cursor, '}');
    if (object_end == NULL) {
      return LCC_ERR_PARSE;
    }
    object_len = (size_t)(object_end - cursor + 1);
    if (object_len >= sizeof(object)) {
      return LCC_ERR_BUFFER_TOO_SMALL;
    }
    memcpy(object, cursor, object_len);
    object[object_len] = '\0';

    memset(&point, 0, sizeof(point));
    if (!json_parse_u8_from_object(object, "\"up\"", &point.up_temp) ||
        !json_parse_u8_from_object(object, "\"down\"", &point.down_temp) ||
        !json_parse_u8_from_object(object, "\"duty\"", &point.duty)) {
      return LCC_ERR_PARSE;
    }
    if (count >= LCC_FAN_POINTS) {
      return LCC_ERR_RANGE;
    }

    points[count] = point;
    last_point = point;
    ++count;
    cursor = object_end + 1;
  }

  if (count == 0u) {
    return LCC_ERR_PARSE;
  }
  while (count < LCC_FAN_POINTS) {
    points[count] = last_point;
    ++count;
  }

  return LCC_OK;
}

static lcc_status_t parse_fan_table_json(const char *path, lcc_fan_table_t *table) {
  char json[4096];
  lcc_status_t status = LCC_OK;

  if (path == NULL || table == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  status = load_text_file(path, json, sizeof(json));
  if (status != LCC_OK) {
    return status;
  }

  memset(table, 0, sizeof(*table));
  if (!json_parse_string(json, "\"name\"", table->name, sizeof(table->name))) {
    default_fan_name_from_path(path, table->name, sizeof(table->name));
  }
  table->activated = true;
  table->fan_control_respective = true;
  (void)json_parse_bool(json, "\"activated\"", &table->activated);
  (void)json_parse_bool(json, "\"Activated\"", &table->activated);
  (void)json_parse_bool(json, "\"fan_control_respective\"",
                        &table->fan_control_respective);
  (void)json_parse_bool(json, "\"FanControlRespective\"",
                        &table->fan_control_respective);

  status = parse_fan_json_array(json, "\"cpu\"", table->cpu);
  if (status != LCC_OK) {
    return status;
  }
  status = parse_fan_json_array(json, "\"gpu\"", table->gpu);
  if (status != LCC_OK) {
    return status;
  }

  return lcc_validate_fan_table(table);
}

static lcc_status_t parse_fan_triplet(const char *value,
                                      lcc_fan_point_t *point) {
  char local[64];
  char *parts[3];
  char *cursor = NULL;
  char *comma = NULL;
  uint8_t parsed[3];
  size_t count = 0;
  lcc_status_t status = LCC_OK;

  if (value == NULL || point == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  if (strlen(value) >= sizeof(local)) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  (void)snprintf(local, sizeof(local), "%s", value);
  cursor = local;

  while (count < 3u) {
    parts[count] = cursor;
    comma = strchr(cursor, ',');
    if (comma != NULL) {
      *comma = '\0';
      cursor = comma + 1;
    } else {
      cursor = NULL;
    }

    parts[count] = trim(parts[count]);
    if (*parts[count] == '\0') {
      return LCC_ERR_PARSE;
    }

    status = parse_u8_text(parts[count], &parsed[count]);
    if (status != LCC_OK) {
      return status;
    }

    ++count;
    if (cursor == NULL) {
      break;
    }
  }

  if (count != 3u || cursor != NULL) {
    return LCC_ERR_PARSE;
  }

  point->up_temp = parsed[0];
  point->down_temp = parsed[1];
  point->duty = parsed[2];
  return LCC_OK;
}

static lcc_status_t parse_fan_point_assignment(const char *key,
                                               const char *value,
                                               lcc_fan_table_t *table) {
  const char *index_text = NULL;
  char *end = NULL;
  unsigned long index = 0;
  lcc_fan_point_t *point = NULL;

  if (key == NULL || value == NULL || table == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  if (strncmp(key, "cpu.", 4) == 0) {
    index_text = key + 4;
    point = table->cpu;
  } else if (strncmp(key, "gpu.", 4) == 0) {
    index_text = key + 4;
    point = table->gpu;
  } else {
    return LCC_ERR_PARSE;
  }

  index = strtoul(index_text, &end, 10);
  if (end == index_text || *end != '\0' || index >= LCC_FAN_POINTS) {
    return LCC_ERR_RANGE;
  }

  return parse_fan_triplet(value, &point[index]);
}

static lcc_status_t parse_mode_entry(lcc_profile_document_t *document,
                                     const char *key, const char *value) {
  lcc_operating_mode_t mode = LCC_MODE_OFFICE;
  lcc_status_t status = LCC_OK;

  if (document == NULL || key == NULL || value == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  if (strcmp(key, "value") != 0) {
    return LCC_ERR_PARSE;
  }

  status = lcc_mode_from_string(value, &mode);
  if (status != LCC_OK) {
    return status;
  }

  document->has_mode = true;
  document->mode = mode;
  return LCC_OK;
}

static lcc_status_t parse_power_entry(lcc_profile_document_t *document,
                                      const char *key, const char *value) {
  lcc_optional_byte_t *target = NULL;
  lcc_status_t status = LCC_OK;

  if (document == NULL || key == NULL || value == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  if (strcmp(key, "pl1") == 0) {
    target = &document->power_limits.pl1;
  } else if (strcmp(key, "pl2") == 0) {
    target = &document->power_limits.pl2;
  } else if (strcmp(key, "pl4") == 0) {
    target = &document->power_limits.pl4;
  } else if (strcmp(key, "tcc_offset") == 0) {
    target = &document->power_limits.tcc_offset;
  } else {
    return LCC_ERR_PARSE;
  }

  status = parse_u8_text(value, &target->value);
  if (status != LCC_OK) {
    return status;
  }

  target->present = true;
  document->has_power_limits = true;
  return LCC_OK;
}

static lcc_status_t parse_fan_entry(lcc_profile_document_t *document,
                                    const char *key, const char *value) {
  lcc_status_t status = LCC_OK;

  if (document == NULL || key == NULL || value == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  document->has_fan_table = true;

  if (strcmp(key, "name") == 0) {
    if (strlen(value) >= sizeof(document->fan_table.name)) {
      return LCC_ERR_BUFFER_TOO_SMALL;
    }
    (void)snprintf(document->fan_table.name, sizeof(document->fan_table.name),
                   "%s", value);
    return LCC_OK;
  }

  if (strcmp(key, "activated") == 0) {
    return parse_bool_text(value, &document->fan_table.activated);
  }

  if (strcmp(key, "fan_control_respective") == 0) {
    return parse_bool_text(value, &document->fan_table.fan_control_respective);
  }

  status = parse_fan_point_assignment(key, value, &document->fan_table);
  if (status != LCC_OK) {
    return status;
  }

  return LCC_OK;
}

lcc_status_t lcc_profile_document_load(const char *path,
                                       lcc_profile_document_t *document) {
  FILE *stream = NULL;
  char line[256];
  lcc_profile_section_t section = LCC_SECTION_NONE;

  if (path == NULL || document == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  stream = fopen(path, "r");
  if (stream == NULL) {
    return LCC_ERR_IO;
  }

  profile_document_init(document);

  while (fgets(line, sizeof(line), stream) != NULL) {
    char *text = trim(line);
    char *separator = NULL;
    lcc_status_t status = LCC_OK;

    if (*text == '\0' || *text == '#' || *text == ';') {
      continue;
    }

    if (*text == '[') {
      const size_t length = strlen(text);
      if (length < 3u || text[length - 1u] != ']') {
        (void)fclose(stream);
        return LCC_ERR_PARSE;
      }
      text[length - 1u] = '\0';
      ++text;
      if (strcmp(text, "mode") == 0) {
        section = LCC_SECTION_MODE;
      } else if (strcmp(text, "power") == 0) {
        section = LCC_SECTION_POWER;
      } else if (strcmp(text, "fan") == 0) {
        section = LCC_SECTION_FAN;
      } else {
        (void)fclose(stream);
        return LCC_ERR_PARSE;
      }
      continue;
    }

    separator = strchr(text, '=');
    if (separator == NULL) {
      (void)fclose(stream);
      return LCC_ERR_PARSE;
    }

    *separator = '\0';
    ++separator;
    text = trim(text);
    separator = trim(separator);

    switch (section) {
      case LCC_SECTION_MODE:
        status = parse_mode_entry(document, text, separator);
        break;
      case LCC_SECTION_POWER:
        status = parse_power_entry(document, text, separator);
        break;
      case LCC_SECTION_FAN:
        status = parse_fan_entry(document, text, separator);
        break;
      case LCC_SECTION_NONE:
        status = LCC_ERR_PARSE;
        break;
    }

    if (status != LCC_OK) {
      (void)fclose(stream);
      return status;
    }
  }

  (void)fclose(stream);

  if (document->has_fan_table && document->fan_table.name[0] == '\0') {
    default_fan_name_from_path(path, document->fan_table.name,
                               sizeof(document->fan_table.name));
  }

  return lcc_profile_document_validate(document);
}

lcc_status_t lcc_fan_table_load_file(const char *path, lcc_fan_table_t *table) {
  lcc_profile_document_t document;
  lcc_status_t status = LCC_OK;
  const char *extension = NULL;

  if (path == NULL || table == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  extension = strrchr(path, '.');
  if (extension != NULL && strcmp(extension, ".json") == 0) {
    return parse_fan_table_json(path, table);
  }

  status = lcc_profile_document_load(path, &document);
  if (status != LCC_OK) {
    return status;
  }
  if (!document.has_fan_table) {
    return LCC_ERR_PARSE;
  }

  *table = document.fan_table;
  return LCC_OK;
}

lcc_status_t lcc_fan_table_load_named(const char *table_name,
                                      lcc_fan_table_t *table) {
  static const char *const prefixes[] = {
      "data/fan-tables/",
      "kuangshi16pro-lcc/data/fan-tables/",
      "/usr/share/kuangshi16pro-lcc/data/fan-tables/",
  };
  char path[512];
  size_t index = 0;

  if (table_name == NULL || table_name[0] == '\0' || table == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (strcmp(table_name, "demo") == 0) {
    return lcc_fan_table_build_demo(table, table_name);
  }

  for (index = 0; index < sizeof(prefixes) / sizeof(prefixes[0]); ++index) {
    if (snprintf(path, sizeof(path), "%s%s.json", prefixes[index], table_name) < 0) {
      return LCC_ERR_BUFFER_TOO_SMALL;
    }
    if (lcc_fan_table_load_file(path, table) == LCC_OK) {
      return LCC_OK;
    }

    if (snprintf(path, sizeof(path), "%s%s.ini", prefixes[index], table_name) < 0) {
      return LCC_ERR_BUFFER_TOO_SMALL;
    }
    if (lcc_fan_table_load_file(path, table) == LCC_OK) {
      return LCC_OK;
    }
  }

  return LCC_ERR_NOT_FOUND;
}
