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

  if (path == NULL || table == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
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
