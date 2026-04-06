#include "daemon/manager.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const default_capability_paths[] = {
    "data/capabilities/gm6px0x.json",
    "kuangshi16pro-lcc/data/capabilities/gm6px0x.json",
    "/usr/share/kuangshi16pro-lcc/data/capabilities/gm6px0x.json",
};

static const char *fallback_capabilities_json(void) {
  return "{"
         "\"model\":\"GM6PX0X\","
         "\"service\":\"lccd\","
         "\"backend_order\":[\"standard\",\"amw0\",\"uefi\"],"
         "\"features\":{\"fan_table_1p5\":true,\"smart_apc\":true,"
         "\"gpu_mux\":\"experimental\"}"
         "}";
}

static bool name_is_safe(const char *text) {
  size_t index = 0;

  if (text == NULL || text[0] == '\0') {
    return false;
  }

  for (index = 0; text[index] != '\0'; ++index) {
    const unsigned char c = (unsigned char)text[index];
    if (!(isalnum(c) || c == '-' || c == '_' || c == '.')) {
      return false;
    }
  }

  return true;
}

static const char *canonical_mode_name(const char *mode_name) {
  if (mode_name == NULL) {
    return NULL;
  }
  if (strcmp(mode_name, "gaming") == 0) {
    return "gaming";
  }
  if (strcmp(mode_name, "office") == 0) {
    return "office";
  }
  if (strcmp(mode_name, "turbo") == 0) {
    return "turbo";
  }
  if (strcmp(mode_name, "custom") == 0) {
    return "custom";
  }

  return NULL;
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

static void set_default_state(lcc_manager_t *manager) {
  (void)snprintf(manager->requested_profile, sizeof(manager->requested_profile),
                 "%s", "balanced");
  (void)snprintf(manager->effective_profile, sizeof(manager->effective_profile),
                 "%s", "balanced");
  (void)snprintf(manager->requested_fan_table,
                 sizeof(manager->requested_fan_table), "%s", "M4T1");
  (void)snprintf(manager->effective_fan_table,
                 sizeof(manager->effective_fan_table), "%s", "M4T1");
}

static void set_default_capabilities(lcc_manager_t *manager) {
  (void)snprintf(manager->capabilities_json, sizeof(manager->capabilities_json),
                 "%s", fallback_capabilities_json());
}

static void merge_power_limit(lcc_optional_byte_t *target,
                              lcc_optional_byte_t source) {
  if (target != NULL && source.present) {
    *target = source;
  }
}

static lcc_status_t init_capabilities(lcc_manager_t *manager,
                                      const char *capabilities_path) {
  size_t index = 0;

  if (capabilities_path != NULL) {
    return load_text_file(capabilities_path, manager->capabilities_json,
                          sizeof(manager->capabilities_json));
  }

  for (index = 0;
       index < sizeof(default_capability_paths) / sizeof(default_capability_paths[0]);
       ++index) {
    if (load_text_file(default_capability_paths[index], manager->capabilities_json,
                       sizeof(manager->capabilities_json)) == LCC_OK) {
      return LCC_OK;
    }
  }

  set_default_capabilities(manager);
  return LCC_OK;
}

static int append_power_json(char *buffer, size_t buffer_len,
                             const lcc_power_limits_t *limits, bool present) {
  if (!present) {
    return snprintf(buffer, buffer_len, "null");
  }

  return snprintf(buffer, buffer_len,
                  "{\"pl1\":%u,\"pl2\":%u,\"pl4\":%u,\"tcc_offset\":%u}",
                  (unsigned int)limits->pl1.value,
                  (unsigned int)limits->pl2.value,
                  (unsigned int)limits->pl4.value,
                  (unsigned int)limits->tcc_offset.value);
}

lcc_status_t lcc_manager_init(lcc_manager_t *manager,
                              const char *capabilities_path) {
  lcc_power_limits_t defaults;
  lcc_status_t status = LCC_OK;

  if (manager == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  memset(manager, 0, sizeof(*manager));
  set_default_state(manager);

  memset(&defaults, 0, sizeof(defaults));
  defaults.pl1.present = true;
  defaults.pl1.value = 55u;
  defaults.pl2.present = true;
  defaults.pl2.value = 95u;
  defaults.pl4.present = true;
  defaults.pl4.value = 125u;
  defaults.tcc_offset.present = true;
  defaults.tcc_offset.value = 10u;

  status = lcc_manager_set_power_limits(manager, &defaults);
  if (status != LCC_OK) {
    return status;
  }

  return init_capabilities(manager, capabilities_path);
}

const char *lcc_manager_capabilities_json(const lcc_manager_t *manager) {
  if (manager == NULL || manager->capabilities_json[0] == '\0') {
    return fallback_capabilities_json();
  }

  return manager->capabilities_json;
}

lcc_status_t lcc_manager_get_state_json(const lcc_manager_t *manager,
                                        char *buffer, size_t buffer_len) {
  char requested_power[128];
  char effective_power[128];
  int requested_written = 0;
  int effective_written = 0;
  int written = 0;

  if (manager == NULL || buffer == NULL || buffer_len == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  requested_written = append_power_json(requested_power, sizeof(requested_power),
                                        &manager->requested_power,
                                        manager->has_requested_power);
  effective_written = append_power_json(effective_power, sizeof(effective_power),
                                        &manager->effective_power,
                                        manager->has_effective_power);
  if (requested_written < 0 || effective_written < 0) {
    return LCC_ERR_IO;
  }

  written = snprintf(
      buffer, buffer_len,
      "{"
      "\"service\":\"lccd\","
      "\"backend\":\"daemon-scaffold\","
      "\"hardware_write\":false,"
      "\"requested\":{\"profile\":\"%s\",\"fan_table\":\"%s\",\"power\":%s},"
      "\"effective\":{\"profile\":\"%s\",\"fan_table\":\"%s\",\"power\":%s}"
      "}",
      manager->requested_profile, manager->requested_fan_table, requested_power,
      manager->effective_profile, manager->effective_fan_table, effective_power);
  if (written < 0 || (size_t)written >= buffer_len) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return LCC_OK;
}

lcc_status_t lcc_manager_get_thermal_json(const lcc_manager_t *manager,
                                          char *buffer, size_t buffer_len) {
  int written = 0;

  if (manager == NULL || buffer == NULL || buffer_len == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  written = snprintf(
      buffer, buffer_len,
      "{"
      "\"service\":\"lccd\","
      "\"source\":\"daemon-scaffold\","
      "\"profile\":\"%s\","
      "\"cpu_temp_c\":null,"
      "\"gpu_temp_c\":null,"
      "\"cpu_fan_rpm\":null,"
      "\"gpu_fan_rpm\":null"
      "}",
      manager->effective_profile);
  if (written < 0 || (size_t)written >= buffer_len) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return LCC_OK;
}

lcc_status_t lcc_manager_set_mode(lcc_manager_t *manager,
                                  const char *mode_name) {
  const char *canonical_name = NULL;

  if (manager == NULL || mode_name == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  canonical_name = canonical_mode_name(mode_name);
  if (canonical_name == NULL) {
    return LCC_ERR_PARSE;
  }

  return lcc_manager_set_profile(manager, canonical_name);
}

lcc_status_t lcc_manager_set_profile(lcc_manager_t *manager,
                                     const char *profile_name) {
  int written = 0;

  if (manager == NULL || !name_is_safe(profile_name)) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  written = snprintf(manager->requested_profile,
                     sizeof(manager->requested_profile), "%s", profile_name);
  if (written < 0 || (size_t)written >= sizeof(manager->requested_profile)) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  written = snprintf(manager->effective_profile,
                     sizeof(manager->effective_profile), "%s", profile_name);
  if (written < 0 || (size_t)written >= sizeof(manager->effective_profile)) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return LCC_OK;
}

lcc_status_t lcc_manager_apply_fan_table(lcc_manager_t *manager,
                                         const char *table_name) {
  int written = 0;

  if (manager == NULL || !name_is_safe(table_name)) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  written = snprintf(manager->requested_fan_table,
                     sizeof(manager->requested_fan_table), "%s", table_name);
  if (written < 0 || (size_t)written >= sizeof(manager->requested_fan_table)) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  written = snprintf(manager->effective_fan_table,
                     sizeof(manager->effective_fan_table), "%s", table_name);
  if (written < 0 || (size_t)written >= sizeof(manager->effective_fan_table)) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return LCC_OK;
}

lcc_status_t lcc_manager_set_power_limits(lcc_manager_t *manager,
                                          const lcc_power_limits_t *limits) {
  if (manager == NULL || limits == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (!limits->pl1.present && !limits->pl2.present && !limits->pl4.present &&
      !limits->tcc_offset.present) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  merge_power_limit(&manager->requested_power.pl1, limits->pl1);
  merge_power_limit(&manager->requested_power.pl2, limits->pl2);
  merge_power_limit(&manager->requested_power.pl4, limits->pl4);
  merge_power_limit(&manager->requested_power.tcc_offset, limits->tcc_offset);
  manager->has_requested_power = true;

  manager->effective_power = manager->requested_power;
  manager->has_effective_power = true;
  return LCC_OK;
}
