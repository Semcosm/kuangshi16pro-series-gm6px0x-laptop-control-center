#include "core/state/model.h"

#include <stdio.h>
#include <string.h>

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

static int append_bool_or_null(char *buffer, size_t buffer_len, bool present,
                               bool value) {
  if (!present) {
    return snprintf(buffer, buffer_len, "null");
  }

  return snprintf(buffer, buffer_len, "%s", value ? "true" : "false");
}

static int append_power_json(char *buffer, size_t buffer_len,
                             const lcc_state_target_t *target) {
  char pl1_json[16];
  char pl2_json[16];
  char pl4_json[16];
  char tcc_offset_json[16];

  if (target == NULL || !target->has_power_limits) {
    return snprintf(buffer, buffer_len, "null");
  }
  if (append_u8_or_null(pl1_json, sizeof(pl1_json),
                        target->power_limits.pl1.present,
                        target->power_limits.pl1.value) < 0 ||
      append_u8_or_null(pl2_json, sizeof(pl2_json),
                        target->power_limits.pl2.present,
                        target->power_limits.pl2.value) < 0 ||
      append_u8_or_null(pl4_json, sizeof(pl4_json),
                        target->power_limits.pl4.present,
                        target->power_limits.pl4.value) < 0 ||
      append_u8_or_null(tcc_offset_json, sizeof(tcc_offset_json),
                        target->power_limits.tcc_offset.present,
                        target->power_limits.tcc_offset.value) < 0) {
    return -1;
  }

  return snprintf(buffer, buffer_len,
                  "{\"pl1\":%s,\"pl2\":%s,\"pl4\":%s,\"tcc_offset\":%s}",
                  pl1_json, pl2_json, pl4_json, tcc_offset_json);
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

static int append_string_or_null(char *buffer, size_t buffer_len,
                                 const char *value) {
  int written = 0;

  if (buffer == NULL || buffer_len == 0u) {
    return -1;
  }
  if (value == NULL || value[0] == '\0') {
    written = snprintf(buffer, buffer_len, "null");
    return (written < 0 || (size_t)written >= buffer_len) ? -1 : written;
  }

  written = snprintf(buffer, buffer_len, "\"%s\"", value);
  return (written < 0 || (size_t)written >= buffer_len) ? -1 : written;
}

static int append_execution_json(char *buffer, size_t buffer_len,
                                 const lcc_execution_snapshot_t *execution) {
  char read_state_json[64];
  char apply_profile_json[64];
  char apply_mode_json[64];
  char apply_power_limits_json[64];
  char apply_fan_table_json[64];

  if (buffer == NULL || buffer_len == 0u || execution == NULL) {
    return -1;
  }
  if (append_string_or_null(read_state_json, sizeof(read_state_json),
                            execution->read_state) < 0 ||
      append_string_or_null(apply_profile_json, sizeof(apply_profile_json),
                            execution->apply_profile) < 0 ||
      append_string_or_null(apply_mode_json, sizeof(apply_mode_json),
                            execution->apply_mode) < 0 ||
      append_string_or_null(apply_power_limits_json,
                            sizeof(apply_power_limits_json),
                            execution->apply_power_limits) < 0 ||
      append_string_or_null(apply_fan_table_json,
                            sizeof(apply_fan_table_json),
                            execution->apply_fan_table) < 0) {
    return -1;
  }

  return snprintf(buffer, buffer_len,
                  "{"
                  "\"read_state\":%s,"
                  "\"apply_profile\":%s,"
                  "\"apply_mode\":%s,"
                  "\"apply_power_limits\":%s,"
                  "\"apply_fan_table\":%s"
                  "}",
                  read_state_json, apply_profile_json, apply_mode_json,
                  apply_power_limits_json, apply_fan_table_json);
}

static int append_component_attribution_json(
    char *buffer, size_t buffer_len,
    const lcc_state_component_attribution_t *component) {
  char source_json[64];
  char freshness_json[64];

  if (buffer == NULL || buffer_len == 0u || component == NULL) {
    return -1;
  }
  if (append_string_or_null(source_json, sizeof(source_json),
                            component->source) < 0 ||
      append_string_or_null(freshness_json, sizeof(freshness_json),
                            component->freshness) < 0) {
    return -1;
  }

  return snprintf(buffer, buffer_len, "{\"source\":%s,\"freshness\":%s}",
                  source_json, freshness_json);
}

static int append_power_attribution_json(
    char *buffer, size_t buffer_len,
    const lcc_effective_state_metadata_t *effective_meta) {
  char source_json[64];
  char freshness_json[64];
  char pl1_json[96];
  char pl2_json[96];
  char pl4_json[96];
  char tcc_offset_json[96];

  if (buffer == NULL || buffer_len == 0u || effective_meta == NULL) {
    return -1;
  }
  if (append_string_or_null(source_json, sizeof(source_json),
                            effective_meta->power.source) < 0 ||
      append_string_or_null(freshness_json, sizeof(freshness_json),
                            effective_meta->power.freshness) < 0 ||
      append_component_attribution_json(pl1_json, sizeof(pl1_json),
                                        &effective_meta->power_fields.pl1) < 0 ||
      append_component_attribution_json(pl2_json, sizeof(pl2_json),
                                        &effective_meta->power_fields.pl2) < 0 ||
      append_component_attribution_json(pl4_json, sizeof(pl4_json),
                                        &effective_meta->power_fields.pl4) < 0 ||
      append_component_attribution_json(
          tcc_offset_json, sizeof(tcc_offset_json),
          &effective_meta->power_fields.tcc_offset) < 0) {
    return -1;
  }

  return snprintf(buffer, buffer_len,
                  "{"
                  "\"source\":%s,"
                  "\"freshness\":%s,"
                  "\"fields\":{"
                  "\"pl1\":%s,"
                  "\"pl2\":%s,"
                  "\"pl4\":%s,"
                  "\"tcc_offset\":%s"
                  "}"
                  "}",
                  source_json, freshness_json, pl1_json, pl2_json, pl4_json,
                  tcc_offset_json);
}

static int append_effective_meta_json(
    char *buffer, size_t buffer_len,
    const lcc_effective_state_metadata_t *effective_meta) {
  char source_json[64];
  char freshness_json[64];
  char profile_json[96];
  char fan_table_json[96];
  char power_json[512];
  char thermal_json[96];

  if (buffer == NULL || buffer_len == 0u || effective_meta == NULL) {
    return -1;
  }
  if (append_string_or_null(source_json, sizeof(source_json),
                            effective_meta->source) < 0 ||
      append_string_or_null(freshness_json, sizeof(freshness_json),
                            effective_meta->freshness) < 0 ||
      append_component_attribution_json(profile_json, sizeof(profile_json),
                                        &effective_meta->profile) < 0 ||
      append_component_attribution_json(fan_table_json,
                                        sizeof(fan_table_json),
                                        &effective_meta->fan_table) < 0 ||
      append_power_attribution_json(power_json, sizeof(power_json),
                                    effective_meta) < 0 ||
      append_component_attribution_json(thermal_json, sizeof(thermal_json),
                                        &effective_meta->thermal) < 0) {
    return -1;
  }

  return snprintf(buffer, buffer_len,
                  "{"
                  "\"source\":%s,"
                  "\"freshness\":%s,"
                  "\"components\":{"
                  "\"profile\":%s,"
                  "\"fan_table\":%s,"
                  "\"power\":%s,"
                  "\"thermal\":%s"
                  "}"
                  "}",
                  source_json, freshness_json, profile_json, fan_table_json,
                  power_json, thermal_json);
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
  char effective_meta_json[1024];
  char pending_json[256];
  char execution_json[320];
  char operation_json[64];
  char stage_json[128];
  char last_error_json[64];
  char backend_selected_json[64];
  char backend_fallback_reason_json[256];
  char last_apply_stage_json[128];
  char last_apply_backend_json[64];
  char last_apply_error_json[64];
  char last_apply_hardware_write_json[16];
  char last_apply_target_json[256];
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
                         &state->effective) < 0 ||
      append_effective_meta_json(effective_meta_json,
                                 sizeof(effective_meta_json),
                                 &state->effective_meta) < 0) {
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
  if (append_execution_json(execution_json, sizeof(execution_json),
                            &state->execution) < 0) {
    return LCC_ERR_IO;
  }
  if (append_string_or_null(backend_selected_json, sizeof(backend_selected_json),
                            state->backend_selected) < 0 ||
      append_string_or_null(backend_fallback_reason_json,
                            sizeof(backend_fallback_reason_json),
                            state->backend_fallback_reason) < 0 ||
      append_string_or_null(last_apply_stage_json, sizeof(last_apply_stage_json),
                            state->last_apply.stage) < 0 ||
      append_string_or_null(last_apply_backend_json,
                            sizeof(last_apply_backend_json),
                            state->last_apply.backend) < 0) {
    return LCC_ERR_IO;
  }
  if (state->last_apply.has_target) {
    if (append_target_json(last_apply_target_json, sizeof(last_apply_target_json),
                           &state->last_apply.target) < 0) {
      return LCC_ERR_IO;
    }
  } else {
    (void)snprintf(last_apply_target_json, sizeof(last_apply_target_json), "%s",
                   "null");
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
  if (state->last_apply.error == LCC_OK) {
    (void)snprintf(last_apply_error_json, sizeof(last_apply_error_json), "%s",
                   "null");
  } else {
    (void)snprintf(last_apply_error_json, sizeof(last_apply_error_json),
                   "\"%s\"", lcc_status_string(state->last_apply.error));
  }
  if (append_bool_or_null(last_apply_hardware_write_json,
                          sizeof(last_apply_hardware_write_json),
                          state->last_apply.has_hardware_write,
                          state->last_apply.hardware_write) < 0) {
    return LCC_ERR_IO;
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
      "\"backend_selected\":%s,"
      "\"backend_fallback_reason\":%s,"
      "\"execution\":%s,"
      "\"hardware_write\":%s,"
      "\"support\":{\"read_state\":%s,\"apply_profile\":%s,"
      "\"apply_mode\":%s,\"apply_power_limits\":%s,\"apply_fan_table\":%s,"
      "\"platform_profile\":%s,\"powercap\":%s,\"mux_reboot_required\":%s},"
      "\"requested\":%s,"
      "\"effective\":%s,"
      "\"effective_meta\":%s,"
      "\"pending\":%s,"
      "\"last_apply_stage\":%s,"
      "\"last_apply_backend\":%s,"
      "\"last_apply_error\":%s,"
      "\"last_apply_hardware_write\":%s,"
      "\"last_apply_target\":%s,"
      "\"transaction\":{\"state\":\"%s\",\"operation\":%s,\"stage\":%s,\"last_error\":%s},"
      "\"thermal\":{\"cpu_temp_c\":%s,\"gpu_temp_c\":%s,"
      "\"cpu_fan_rpm\":%s,\"gpu_fan_rpm\":%s}"
      "}",
      state->backend_name, backend_selected_json, backend_fallback_reason_json,
      execution_json, state->hardware_write ? "true" : "false",
      backend_capabilities->can_read_state ? "true" : "false",
      backend_capabilities->can_apply_profile ? "true" : "false",
      backend_capabilities->can_apply_mode ? "true" : "false",
      backend_capabilities->can_apply_power_limits ? "true" : "false",
      backend_capabilities->can_apply_fan_table ? "true" : "false",
      backend_capabilities->has_platform_profile ? "true" : "false",
      backend_capabilities->has_powercap ? "true" : "false",
      backend_capabilities->needs_reboot_for_mux ? "true" : "false",
      requested_json, effective_json, effective_meta_json, pending_json,
      last_apply_stage_json,
      last_apply_backend_json, last_apply_error_json,
      last_apply_hardware_write_json, last_apply_target_json,
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
