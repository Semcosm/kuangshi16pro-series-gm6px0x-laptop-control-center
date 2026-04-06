#include "daemon/manager.h"

#include <stdio.h>
#include <string.h>

#include "daemon/transaction.h"

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

static int append_target_json(char *buffer, size_t buffer_len,
                              const lcc_state_target_t *target) {
  char power_json[128];
  int power_written = 0;

  if (buffer == NULL || buffer_len == 0u || target == NULL) {
    return -1;
  }

  power_written = append_power_json(power_json, sizeof(power_json), target);
  if (power_written < 0) {
    return -1;
  }

  return snprintf(buffer, buffer_len,
                  "{\"profile\":\"%s\",\"fan_table\":\"%s\",\"power\":%s}",
                  target->profile, target->fan_table, power_json);
}

static const char *transaction_state_name(lcc_transaction_state_t state) {
  switch (state) {
    case LCC_TRANSACTION_STATE_IDLE:
      return "idle";
    case LCC_TRANSACTION_STATE_PENDING:
      return "pending";
    case LCC_TRANSACTION_STATE_FAILED:
      return "failed";
  }

  return "unknown";
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

  status = lcc_transaction_refresh_state(manager);
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
  char requested_json[256];
  char effective_json[256];
  char pending_json[256];
  char operation_json[64];
  char last_error_json[64];
  int requested_written = 0;
  int effective_written = 0;
  int pending_written = 0;
  int operation_written = 0;
  int error_written = 0;
  int written = 0;

  if (manager == NULL || buffer == NULL || buffer_len == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  requested_written = append_target_json(requested_json, sizeof(requested_json),
                                         &manager->state_cache.requested);
  effective_written = append_target_json(effective_json, sizeof(effective_json),
                                         &manager->state_cache.effective);
  if (manager->state_cache.transaction.has_pending_target) {
    pending_written = append_target_json(
        pending_json, sizeof(pending_json),
        &manager->state_cache.transaction.pending_target);
  } else {
    pending_written = snprintf(pending_json, sizeof(pending_json), "null");
  }
  if (requested_written < 0 || effective_written < 0 || pending_written < 0) {
    return LCC_ERR_IO;
  }
  if (manager->state_cache.transaction.operation[0] == '\0') {
    operation_written = snprintf(operation_json, sizeof(operation_json), "null");
  } else {
    operation_written =
        snprintf(operation_json, sizeof(operation_json), "\"%s\"",
                 manager->state_cache.transaction.operation);
  }
  if (manager->state_cache.transaction.last_error == LCC_OK) {
    error_written = snprintf(last_error_json, sizeof(last_error_json), "null");
  } else {
    error_written = snprintf(
        last_error_json, sizeof(last_error_json), "\"%s\"",
        lcc_status_string(manager->state_cache.transaction.last_error));
  }
  if (operation_written < 0 || error_written < 0) {
    return LCC_ERR_IO;
  }

  written = snprintf(
      buffer, buffer_len,
      "{"
      "\"service\":\"lccd\","
      "\"backend\":\"%s\","
      "\"hardware_write\":%s,"
      "\"requested\":%s,"
      "\"effective\":%s,"
      "\"pending\":%s,"
      "\"transaction\":{\"state\":\"%s\",\"operation\":%s,\"last_error\":%s}"
      "}",
      manager->state_cache.backend_name,
      manager->state_cache.hardware_write ? "true" : "false",
      requested_json, effective_json, pending_json,
      transaction_state_name(manager->state_cache.transaction.state),
      operation_json, last_error_json);
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
  const lcc_transaction_request_t request = {
      .kind = LCC_TRANSACTION_MODE,
      .input.mode_name = mode_name,
  };

  return lcc_transaction_execute(manager, &request);
}

lcc_status_t lcc_manager_set_profile(lcc_manager_t *manager,
                                     const char *profile_name) {
  const lcc_transaction_request_t request = {
      .kind = LCC_TRANSACTION_PROFILE,
      .input.profile_name = profile_name,
  };

  return lcc_transaction_execute(manager, &request);
}

lcc_status_t lcc_manager_apply_fan_table(lcc_manager_t *manager,
                                         const char *table_name) {
  const lcc_transaction_request_t request = {
      .kind = LCC_TRANSACTION_FAN_TABLE,
      .input.fan_table_name = table_name,
  };

  return lcc_transaction_execute(manager, &request);
}

lcc_status_t lcc_manager_set_power_limits(lcc_manager_t *manager,
                                          const lcc_power_limits_t *limits) {
  const lcc_transaction_request_t request = {
      .kind = LCC_TRANSACTION_POWER_LIMITS,
      .input.power_limits = limits,
  };

  return lcc_transaction_execute(manager, &request);
}
