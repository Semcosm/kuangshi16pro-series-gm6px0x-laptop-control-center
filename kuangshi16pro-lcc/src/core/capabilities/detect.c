#include "core/capabilities/detect.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "core/capabilities/model-map.h"

static lcc_status_t load_text_file(const char *path, char *buffer,
                                   size_t buffer_len) {
  FILE *stream = NULL;
  size_t bytes = 0;

  if (path == NULL || buffer == NULL || buffer_len < 2u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  stream = fopen(path, "r");
  if (stream == NULL) {
    return LCC_ERR_NOT_FOUND;
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

static bool parse_json_bool(const char *json, const char *key, bool *value) {
  const char *match = NULL;
  const char *cursor = NULL;

  if (json == NULL || key == NULL || value == NULL) {
    return false;
  }

  match = strstr(json, key);
  if (match == NULL) {
    return false;
  }
  cursor = strchr(match, ':');
  if (cursor == NULL) {
    return false;
  }
  ++cursor;
  while (*cursor == ' ' || *cursor == '\n' || *cursor == '\t') {
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

static bool parse_json_string(const char *json, const char *key, char *buffer,
                              size_t buffer_len) {
  const char *match = NULL;
  const char *start = NULL;
  const char *end = NULL;
  size_t len = 0;

  if (json == NULL || key == NULL || buffer == NULL || buffer_len == 0u) {
    return false;
  }

  match = strstr(json, key);
  if (match == NULL) {
    return false;
  }
  start = strchr(match, ':');
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

static void apply_json_overrides(const char *json, lcc_model_map_t *map) {
  if (json == NULL || map == NULL) {
    return;
  }

  (void)parse_json_string(json, "\"model\"", map->model, sizeof(map->model));
  (void)parse_json_bool(json, "\"preferred\"",
                        &map->platform_profile_preferred);
  (void)parse_json_bool(json, "\"fallback_required\"",
                        &map->platform_profile_fallback_required);
  (void)parse_json_bool(json, "\"fan_table_1p5\"",
                        &map->feature_fan_table_1p5);
  (void)parse_json_bool(json, "\"smart_apc\"", &map->feature_smart_apc);
  (void)parse_json_string(json, "\"gpu_mux\"", map->gpu_mux,
                          sizeof(map->gpu_mux));
}

lcc_status_t lcc_capabilities_detect_json(
    const char *backend_selected,
    const lcc_backend_capabilities_t *backend_capabilities,
    const char *capabilities_path, char *buffer, size_t buffer_len) {
  lcc_model_map_t model_map;
  char file_json[2048];
  int written = 0;

  if (backend_selected == NULL || backend_selected[0] == '\0' ||
      backend_capabilities == NULL || buffer == NULL ||
      buffer_len == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  lcc_model_map_seed_defaults(&model_map);
  if (capabilities_path != NULL &&
      load_text_file(capabilities_path, file_json, sizeof(file_json)) == LCC_OK) {
    apply_json_overrides(file_json, &model_map);
  }

  written = snprintf(
      buffer, buffer_len,
      "{"
      "\"model\":\"%s\","
      "\"service\":\"lccd\","
      "\"backend_selected\":\"%s\","
      "\"support\":{\"read_state\":%s,\"apply_profile\":%s,"
      "\"apply_mode\":%s,\"apply_power_limits\":%s,\"apply_fan_table\":%s,"
      "\"apply_fan_boost\":%s},"
      "\"platform_profile\":{\"preferred\":%s,\"fallback_required\":%s,"
      "\"available\":%s},"
      "\"powercap\":{\"available\":%s},"
      "\"features\":{\"fan_table_1p5\":%s,\"smart_apc\":%s,"
      "\"gpu_mux\":\"%s\",\"needs_reboot_for_mux\":%s}"
      "}",
      model_map.model, backend_selected,
      backend_capabilities->can_read_state ? "true" : "false",
      backend_capabilities->can_apply_profile ? "true" : "false",
      backend_capabilities->can_apply_mode ? "true" : "false",
      backend_capabilities->can_apply_power_limits ? "true" : "false",
      backend_capabilities->can_apply_fan_table ? "true" : "false",
      backend_capabilities->can_apply_fan_boost ? "true" : "false",
      model_map.platform_profile_preferred ? "true" : "false",
      model_map.platform_profile_fallback_required ? "true" : "false",
      backend_capabilities->has_platform_profile ? "true" : "false",
      backend_capabilities->has_powercap ? "true" : "false",
      model_map.feature_fan_table_1p5 ? "true" : "false",
      model_map.feature_smart_apc ? "true" : "false", model_map.gpu_mux,
      backend_capabilities->needs_reboot_for_mux ? "true" : "false");
  if (written < 0 || (size_t)written >= buffer_len) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return LCC_OK;
}
