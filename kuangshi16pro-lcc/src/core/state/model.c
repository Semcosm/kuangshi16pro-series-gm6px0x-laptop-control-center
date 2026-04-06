#include "core/state/model.h"

#include <stdio.h>
#include <string.h>

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

static int append_u16_or_null(char *buffer, size_t buffer_len, bool present,
                              uint16_t value) {
  if (!present) {
    return snprintf(buffer, buffer_len, "null");
  }

  return snprintf(buffer, buffer_len, "%u", (unsigned int)value);
}

static int append_u8_or_null(char *buffer, size_t buffer_len, bool present,
                             uint8_t value) {
  if (!present) {
    return snprintf(buffer, buffer_len, "null");
  }

  return snprintf(buffer, buffer_len, "%u", (unsigned int)value);
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

lcc_status_t lcc_state_render_json(
    const lcc_state_snapshot_t *state,
    const lcc_backend_capabilities_t *backend_capabilities, char *buffer,
    size_t buffer_len) {
  char requested_json[256];
  char effective_json[256];
  char pending_json[256];
  char operation_json[64];
  char stage_json[128];
  char last_error_json[64];
  char thermal_cpu_temp[32];
  char thermal_gpu_temp[32];
  char thermal_cpu_fan[32];
  char thermal_gpu_fan[32];
  int written = 0;

  if (state == NULL || backend_capabilities == NULL || buffer == NULL ||
      buffer_len == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  if (append_target_json(requested_json, sizeof(requested_json),
                         &state->requested) < 0 ||
      append_target_json(effective_json, sizeof(effective_json),
                         &state->effective) < 0) {
    return LCC_ERR_IO;
  }
  if (state->transaction.has_pending_target) {
    if (append_target_json(pending_json, sizeof(pending_json),
                           &state->transaction.pending_target) < 0) {
      return LCC_ERR_IO;
    }
  } else {
    (void)snprintf(pending_json, sizeof(pending_json), "%s", "null");
  }
  if (state->transaction.operation[0] == '\0') {
    (void)snprintf(operation_json, sizeof(operation_json), "%s", "null");
  } else {
    (void)snprintf(operation_json, sizeof(operation_json), "\"%s\"",
                   state->transaction.operation);
  }
  if (state->transaction.stage[0] == '\0') {
    (void)snprintf(stage_json, sizeof(stage_json), "%s", "null");
  } else {
    (void)snprintf(stage_json, sizeof(stage_json), "\"%s\"",
                   state->transaction.stage);
  }
  if (state->transaction.last_error == LCC_OK) {
    (void)snprintf(last_error_json, sizeof(last_error_json), "%s", "null");
  } else {
    (void)snprintf(last_error_json, sizeof(last_error_json), "\"%s\"",
                   lcc_status_string(state->transaction.last_error));
  }
  (void)append_u8_or_null(thermal_cpu_temp, sizeof(thermal_cpu_temp),
                          state->thermal.has_cpu_temp_c,
                          state->thermal.cpu_temp_c);
  (void)append_u8_or_null(thermal_gpu_temp, sizeof(thermal_gpu_temp),
                          state->thermal.has_gpu_temp_c,
                          state->thermal.gpu_temp_c);
  (void)append_u16_or_null(thermal_cpu_fan, sizeof(thermal_cpu_fan),
                           state->thermal.has_cpu_fan_rpm,
                           state->thermal.cpu_fan_rpm);
  (void)append_u16_or_null(thermal_gpu_fan, sizeof(thermal_gpu_fan),
                           state->thermal.has_gpu_fan_rpm,
                           state->thermal.gpu_fan_rpm);

  written = snprintf(
      buffer, buffer_len,
      "{"
      "\"service\":\"lccd\","
      "\"backend\":\"%s\","
      "\"hardware_write\":%s,"
      "\"support\":{\"read_state\":%s,\"apply_profile\":%s,"
      "\"apply_mode\":%s,\"apply_power_limits\":%s,\"apply_fan_table\":%s,"
      "\"platform_profile\":%s,\"powercap\":%s,\"mux_reboot_required\":%s},"
      "\"requested\":%s,"
      "\"effective\":%s,"
      "\"pending\":%s,"
      "\"transaction\":{\"state\":\"%s\",\"operation\":%s,\"stage\":%s,\"last_error\":%s},"
      "\"thermal\":{\"cpu_temp_c\":%s,\"gpu_temp_c\":%s,"
      "\"cpu_fan_rpm\":%s,\"gpu_fan_rpm\":%s}"
      "}",
      state->backend_name, state->hardware_write ? "true" : "false",
      backend_capabilities->can_read_state ? "true" : "false",
      backend_capabilities->can_apply_profile ? "true" : "false",
      backend_capabilities->can_apply_mode ? "true" : "false",
      backend_capabilities->can_apply_power_limits ? "true" : "false",
      backend_capabilities->can_apply_fan_table ? "true" : "false",
      backend_capabilities->has_platform_profile ? "true" : "false",
      backend_capabilities->has_powercap ? "true" : "false",
      backend_capabilities->needs_reboot_for_mux ? "true" : "false",
      requested_json, effective_json, pending_json,
      transaction_state_name(state->transaction.state), operation_json,
      stage_json,
      last_error_json, thermal_cpu_temp, thermal_gpu_temp, thermal_cpu_fan,
      thermal_gpu_fan);
  if (written < 0 || (size_t)written >= buffer_len) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return LCC_OK;
}

lcc_status_t lcc_state_render_thermal_json(const lcc_state_snapshot_t *state,
                                           char *buffer, size_t buffer_len) {
  char cpu_temp[32];
  char gpu_temp[32];
  char cpu_fan[32];
  char gpu_fan[32];
  int written = 0;

  if (state == NULL || buffer == NULL || buffer_len == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  (void)append_u8_or_null(cpu_temp, sizeof(cpu_temp), state->thermal.has_cpu_temp_c,
                          state->thermal.cpu_temp_c);
  (void)append_u8_or_null(gpu_temp, sizeof(gpu_temp), state->thermal.has_gpu_temp_c,
                          state->thermal.gpu_temp_c);
  (void)append_u16_or_null(cpu_fan, sizeof(cpu_fan),
                           state->thermal.has_cpu_fan_rpm,
                           state->thermal.cpu_fan_rpm);
  (void)append_u16_or_null(gpu_fan, sizeof(gpu_fan),
                           state->thermal.has_gpu_fan_rpm,
                           state->thermal.gpu_fan_rpm);

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
      state->backend_name, state->effective.profile, cpu_temp, gpu_temp,
      cpu_fan, gpu_fan);
  if (written < 0 || (size_t)written >= buffer_len) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return LCC_OK;
}
