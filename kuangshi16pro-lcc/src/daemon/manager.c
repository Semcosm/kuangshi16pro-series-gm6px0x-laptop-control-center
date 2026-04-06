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

static lcc_status_t mode_from_name(const char *mode_name,
                                   lcc_operating_mode_t *mode) {
  if (mode_name == NULL || mode == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (strcmp(mode_name, "gaming") == 0) {
    *mode = LCC_MODE_GAMING;
    return LCC_OK;
  }
  if (strcmp(mode_name, "office") == 0) {
    *mode = LCC_MODE_OFFICE;
    return LCC_OK;
  }
  if (strcmp(mode_name, "turbo") == 0) {
    *mode = LCC_MODE_TURBO;
    return LCC_OK;
  }
  if (strcmp(mode_name, "custom") == 0) {
    *mode = LCC_MODE_CUSTOM;
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

static void set_default_capabilities(lcc_manager_t *manager) {
  (void)snprintf(manager->capabilities_json, sizeof(manager->capabilities_json),
                 "%s", fallback_capabilities_json());
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
                             const lcc_state_target_t *target) {
  if (target == NULL || !target->has_power_limits) {
    return snprintf(buffer, buffer_len, "null");
  }

  return snprintf(buffer, buffer_len,
                  "{\"pl1\":%u,\"pl2\":%u,\"pl4\":%u,\"tcc_offset\":%u}",
                  (unsigned int)target->power_limits.pl1.value,
                  (unsigned int)target->power_limits.pl2.value,
                  (unsigned int)target->power_limits.pl4.value,
                  (unsigned int)target->power_limits.tcc_offset.value);
}

static void fill_backend_name(lcc_manager_t *manager) {
  if (manager->state_cache.backend_name[0] == '\0') {
    (void)snprintf(manager->state_cache.backend_name,
                   sizeof(manager->state_cache.backend_name), "%s",
                   lcc_backend_name(manager->backend));
  }
}

static lcc_status_t refresh_state(lcc_manager_t *manager) {
  lcc_backend_result_t result;
  lcc_status_t status = LCC_OK;

  if (manager == NULL || manager->backend == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  status = lcc_backend_read_state(manager->backend, &manager->state_cache,
                                  &result);
  if (status != LCC_OK) {
    return status;
  }

  if (result.hardware_write) {
    manager->state_cache.hardware_write = true;
  }
  fill_backend_name(manager);
  return LCC_OK;
}

lcc_status_t lcc_manager_init(lcc_manager_t *manager, lcc_backend_t *backend,
                              const char *capabilities_path) {
  lcc_backend_result_t result;
  lcc_status_t status = LCC_OK;

  if (manager == NULL || backend == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  memset(manager, 0, sizeof(*manager));
  manager->backend = backend;

  status = lcc_backend_probe(manager->backend, &manager->backend_capabilities,
                             &result);
  if (status != LCC_OK) {
    return status;
  }

  status = refresh_state(manager);
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
                                        &manager->state_cache.requested);
  effective_written = append_power_json(effective_power, sizeof(effective_power),
                                        &manager->state_cache.effective);
  if (requested_written < 0 || effective_written < 0) {
    return LCC_ERR_IO;
  }

  written = snprintf(
      buffer, buffer_len,
      "{"
      "\"service\":\"lccd\","
      "\"backend\":\"%s\","
      "\"hardware_write\":%s,"
      "\"requested\":{\"profile\":\"%s\",\"fan_table\":\"%s\",\"power\":%s},"
      "\"effective\":{\"profile\":\"%s\",\"fan_table\":\"%s\",\"power\":%s}"
      "}",
      manager->state_cache.backend_name,
      manager->state_cache.hardware_write ? "true" : "false",
      manager->state_cache.requested.profile, manager->state_cache.requested.fan_table,
      requested_power, manager->state_cache.effective.profile,
      manager->state_cache.effective.fan_table, effective_power);
  if (written < 0 || (size_t)written >= buffer_len) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return LCC_OK;
}

lcc_status_t lcc_manager_get_thermal_json(const lcc_manager_t *manager,
                                          char *buffer, size_t buffer_len) {
  char cpu_temp[32];
  char gpu_temp[32];
  char cpu_fan[32];
  char gpu_fan[32];
  int written = 0;

  if (manager == NULL || buffer == NULL || buffer_len == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  if (manager->state_cache.thermal.has_cpu_temp_c) {
    (void)snprintf(cpu_temp, sizeof(cpu_temp), "%u",
                   (unsigned int)manager->state_cache.thermal.cpu_temp_c);
  } else {
    (void)snprintf(cpu_temp, sizeof(cpu_temp), "%s", "null");
  }
  if (manager->state_cache.thermal.has_gpu_temp_c) {
    (void)snprintf(gpu_temp, sizeof(gpu_temp), "%u",
                   (unsigned int)manager->state_cache.thermal.gpu_temp_c);
  } else {
    (void)snprintf(gpu_temp, sizeof(gpu_temp), "%s", "null");
  }
  if (manager->state_cache.thermal.has_cpu_fan_rpm) {
    (void)snprintf(cpu_fan, sizeof(cpu_fan), "%u",
                   (unsigned int)manager->state_cache.thermal.cpu_fan_rpm);
  } else {
    (void)snprintf(cpu_fan, sizeof(cpu_fan), "%s", "null");
  }
  if (manager->state_cache.thermal.has_gpu_fan_rpm) {
    (void)snprintf(gpu_fan, sizeof(gpu_fan), "%u",
                   (unsigned int)manager->state_cache.thermal.gpu_fan_rpm);
  } else {
    (void)snprintf(gpu_fan, sizeof(gpu_fan), "%s", "null");
  }

  written = snprintf(
      buffer, buffer_len,
      "{"
      "\"service\":\"lccd\","
      "\"source\":\"%s\","
      "\"profile\":\"%s\","
      "\"cpu_temp_c\":%s,"
      "\"gpu_temp_c\":%s,"
      "\"cpu_fan_rpm\":%s,"
      "\"gpu_fan_rpm\":%s"
      "}",
      manager->state_cache.backend_name, manager->state_cache.effective.profile,
      cpu_temp, gpu_temp, cpu_fan, gpu_fan);
  if (written < 0 || (size_t)written >= buffer_len) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return LCC_OK;
}

lcc_status_t lcc_manager_set_mode(lcc_manager_t *manager,
                                  const char *mode_name) {
  lcc_backend_result_t result;
  lcc_operating_mode_t mode = LCC_MODE_OFFICE;
  lcc_status_t status = LCC_OK;

  if (manager == NULL || mode_name == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  status = mode_from_name(mode_name, &mode);
  if (status != LCC_OK) {
    return status;
  }

  status = lcc_backend_apply_mode(manager->backend, mode, &result);
  if (status != LCC_OK) {
    return status;
  }

  return refresh_state(manager);
}

lcc_status_t lcc_manager_set_profile(lcc_manager_t *manager,
                                     const char *profile_name) {
  lcc_backend_result_t result;
  lcc_status_t status = LCC_OK;

  if (manager == NULL || !name_is_safe(profile_name)) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  status = lcc_backend_apply_profile(manager->backend, profile_name, &result);
  if (status == LCC_ERR_NOT_SUPPORTED) {
    return lcc_manager_set_mode(manager, profile_name);
  }
  if (status != LCC_OK) {
    return status;
  }

  return refresh_state(manager);
}

lcc_status_t lcc_manager_apply_fan_table(lcc_manager_t *manager,
                                         const char *table_name) {
  lcc_backend_result_t result;
  lcc_status_t status = LCC_OK;

  if (manager == NULL || !name_is_safe(table_name)) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  status = lcc_backend_apply_fan_table(manager->backend, table_name, &result);
  if (status != LCC_OK) {
    return status;
  }

  return refresh_state(manager);
}

lcc_status_t lcc_manager_set_power_limits(lcc_manager_t *manager,
                                          const lcc_power_limits_t *limits) {
  lcc_backend_result_t result;
  lcc_status_t status = LCC_OK;

  if (manager == NULL || limits == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (!limits->pl1.present && !limits->pl2.present && !limits->pl4.present &&
      !limits->tcc_offset.present) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  status = lcc_backend_apply_power_limits(manager->backend, limits, &result);
  if (status != LCC_OK) {
    return status;
  }

  return refresh_state(manager);
}
