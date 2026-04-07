#include "backends/backend.h"

#include <stdio.h>
#include <string.h>

#include "common/lcc_log.h"

typedef enum {
  LCC_CONVERGED_ROUTE_READ_STATE = 0,
  LCC_CONVERGED_ROUTE_APPLY_PROFILE,
  LCC_CONVERGED_ROUTE_APPLY_MODE,
  LCC_CONVERGED_ROUTE_APPLY_POWER_LIMITS,
  LCC_CONVERGED_ROUTE_APPLY_FAN_TABLE,
} lcc_converged_route_kind_t;

static bool capability_for_route(const lcc_backend_capabilities_t *capabilities,
                                 lcc_converged_route_kind_t route) {
  if (capabilities == NULL) {
    return false;
  }

  switch (route) {
    case LCC_CONVERGED_ROUTE_READ_STATE:
      return capabilities->can_read_state;
    case LCC_CONVERGED_ROUTE_APPLY_PROFILE:
      return capabilities->can_apply_profile;
    case LCC_CONVERGED_ROUTE_APPLY_MODE:
      return capabilities->can_apply_mode;
    case LCC_CONVERGED_ROUTE_APPLY_POWER_LIMITS:
      return capabilities->can_apply_power_limits;
    case LCC_CONVERGED_ROUTE_APPLY_FAN_TABLE:
      return capabilities->can_apply_fan_table;
  }

  return false;
}

static const lcc_backend_t *route_primary_backend(
    const lcc_converged_backend_t *converged, lcc_converged_route_kind_t route) {
  if (converged == NULL) {
    return NULL;
  }

  if (converged->standard_available &&
      capability_for_route(&converged->standard_capabilities, route)) {
    return converged->standard_backend;
  }
  if (converged->amw0_available &&
      capability_for_route(&converged->amw0_capabilities, route)) {
    return converged->amw0_backend;
  }

  return NULL;
}

static const lcc_backend_t *route_secondary_backend(
    const lcc_converged_backend_t *converged, lcc_converged_route_kind_t route,
    const lcc_backend_t *primary_backend) {
  if (converged == NULL) {
    return NULL;
  }

  if (primary_backend != converged->standard_backend && converged->standard_available &&
      capability_for_route(&converged->standard_capabilities, route)) {
    return converged->standard_backend;
  }
  if (primary_backend != converged->amw0_backend && converged->amw0_available &&
      capability_for_route(&converged->amw0_capabilities, route)) {
    return converged->amw0_backend;
  }

  return NULL;
}

static const char *route_backend_name(const lcc_backend_t *backend) {
  return backend != NULL ? lcc_backend_name(backend) : "";
}

static void set_result_executor(lcc_backend_result_t *result,
                                const lcc_backend_t *backend) {
  if (result == NULL || backend == NULL) {
    return;
  }

  lcc_backend_result_set_executor(result, lcc_backend_name(backend));
}

static void set_fallback_detail(lcc_backend_result_t *result,
                                const char *operation_name,
                                const lcc_backend_t *from_backend,
                                const lcc_backend_t *to_backend,
                                lcc_status_t reason) {
  char detail[LCC_STATE_REASON_MAX];

  if (result == NULL || operation_name == NULL || from_backend == NULL) {
    return;
  }

  if (to_backend != NULL) {
    (void)snprintf(detail, sizeof(detail),
                   "%s fell back from %s to %s after %s", operation_name,
                   lcc_backend_name(from_backend), lcc_backend_name(to_backend),
                   lcc_status_string(reason));
  } else {
    (void)snprintf(detail, sizeof(detail),
                   "%s stopped at %s because no fallback route is available after %s",
                   operation_name, lcc_backend_name(from_backend),
                   lcc_status_string(reason));
  }
  lcc_backend_result_set_detail(result, detail);
}

static void merge_optional_byte(lcc_optional_byte_t *target,
                                lcc_optional_byte_t source) {
  if (target != NULL && source.present) {
    *target = source;
  }
}

static void overlay_amw0_state(lcc_state_snapshot_t *state,
                               const lcc_state_snapshot_t *amw0_state,
                               const lcc_converged_backend_t *converged) {
  if (state == NULL || amw0_state == NULL || converged == NULL) {
    return;
  }

  if (strcmp(converged->execution.apply_profile, "amw0") == 0 ||
      strcmp(converged->execution.apply_mode, "amw0") == 0 ||
      strcmp(converged->execution.apply_fan_table, "amw0") == 0) {
    (void)lcc_backend_copy_text(state->requested.profile,
                                sizeof(state->requested.profile),
                                amw0_state->requested.profile);
    (void)lcc_backend_copy_text(state->effective.profile,
                                sizeof(state->effective.profile),
                                amw0_state->effective.profile);
  }
  if (strcmp(converged->execution.apply_fan_table, "amw0") == 0) {
    (void)lcc_backend_copy_text(state->requested.fan_table,
                                sizeof(state->requested.fan_table),
                                amw0_state->requested.fan_table);
    (void)lcc_backend_copy_text(state->effective.fan_table,
                                sizeof(state->effective.fan_table),
                                amw0_state->effective.fan_table);
  }
  if (strcmp(converged->execution.apply_power_limits, "amw0") == 0 &&
      amw0_state->effective.has_power_limits) {
    state->requested.has_power_limits = amw0_state->requested.has_power_limits;
    state->effective.has_power_limits = amw0_state->effective.has_power_limits;
    merge_optional_byte(&state->requested.power_limits.pl1,
                        amw0_state->requested.power_limits.pl1);
    merge_optional_byte(&state->requested.power_limits.pl2,
                        amw0_state->requested.power_limits.pl2);
    merge_optional_byte(&state->requested.power_limits.pl4,
                        amw0_state->requested.power_limits.pl4);
    merge_optional_byte(&state->requested.power_limits.tcc_offset,
                        amw0_state->requested.power_limits.tcc_offset);
    merge_optional_byte(&state->effective.power_limits.pl1,
                        amw0_state->effective.power_limits.pl1);
    merge_optional_byte(&state->effective.power_limits.pl2,
                        amw0_state->effective.power_limits.pl2);
    merge_optional_byte(&state->effective.power_limits.pl4,
                        amw0_state->effective.power_limits.pl4);
    merge_optional_byte(&state->effective.power_limits.tcc_offset,
                        amw0_state->effective.power_limits.tcc_offset);
  }

  if (!state->thermal.has_cpu_temp_c && amw0_state->thermal.has_cpu_temp_c) {
    state->thermal.has_cpu_temp_c = true;
    state->thermal.cpu_temp_c = amw0_state->thermal.cpu_temp_c;
  }
  if (!state->thermal.has_gpu_temp_c && amw0_state->thermal.has_gpu_temp_c) {
    state->thermal.has_gpu_temp_c = true;
    state->thermal.gpu_temp_c = amw0_state->thermal.gpu_temp_c;
  }
  if (!state->thermal.has_cpu_fan_rpm && amw0_state->thermal.has_cpu_fan_rpm) {
    state->thermal.has_cpu_fan_rpm = true;
    state->thermal.cpu_fan_rpm = amw0_state->thermal.cpu_fan_rpm;
  }
  if (!state->thermal.has_gpu_fan_rpm && amw0_state->thermal.has_gpu_fan_rpm) {
    state->thermal.has_gpu_fan_rpm = true;
    state->thermal.gpu_fan_rpm = amw0_state->thermal.gpu_fan_rpm;
  }
}

static void append_operation(char *buffer, size_t buffer_len, bool *first,
                             const char *name) {
  size_t used = 0;

  if (buffer == NULL || buffer_len == 0u || first == NULL || name == NULL ||
      name[0] == '\0') {
    return;
  }

  used = strlen(buffer);
  if (used >= buffer_len - 1u) {
    return;
  }

  (void)snprintf(buffer + used, buffer_len - used, "%s%s", *first ? "" : ", ",
                 name);
  *first = false;
}

static void build_selection_metadata(lcc_converged_backend_t *converged) {
  char fallback_ops[128];
  bool first = true;

  if (converged == NULL) {
    return;
  }

  converged->backend_selected[0] = '\0';
  converged->backend_fallback_reason[0] = '\0';
  lcc_backend_execution_clear(&converged->execution);

  if (converged->standard_available) {
    (void)lcc_backend_copy_text(converged->backend_selected,
                                sizeof(converged->backend_selected),
                                "standard");
  } else if (converged->amw0_available) {
    (void)lcc_backend_copy_text(converged->backend_selected,
                                sizeof(converged->backend_selected), "amw0");
  } else {
    return;
  }

  (void)lcc_backend_execution_set(
      &converged->execution,
      route_backend_name(
          route_primary_backend(converged, LCC_CONVERGED_ROUTE_READ_STATE)),
      route_backend_name(
          route_primary_backend(converged, LCC_CONVERGED_ROUTE_APPLY_PROFILE)),
      route_backend_name(
          route_primary_backend(converged, LCC_CONVERGED_ROUTE_APPLY_MODE)),
      route_backend_name(route_primary_backend(
          converged, LCC_CONVERGED_ROUTE_APPLY_POWER_LIMITS)),
      route_backend_name(route_primary_backend(
          converged, LCC_CONVERGED_ROUTE_APPLY_FAN_TABLE)));

  if (strcmp(converged->backend_selected, "standard") == 0) {
    fallback_ops[0] = '\0';
    if (strcmp(converged->execution.apply_profile, "amw0") == 0) {
      append_operation(fallback_ops, sizeof(fallback_ops), &first,
                       "apply_profile");
    }
    if (strcmp(converged->execution.apply_mode, "amw0") == 0) {
      append_operation(fallback_ops, sizeof(fallback_ops), &first,
                       "apply_mode");
    }
    if (strcmp(converged->execution.apply_power_limits, "amw0") == 0) {
      append_operation(fallback_ops, sizeof(fallback_ops), &first,
                       "apply_power_limits");
    }
    if (strcmp(converged->execution.apply_fan_table, "amw0") == 0) {
      append_operation(fallback_ops, sizeof(fallback_ops), &first,
                       "apply_fan_table");
    }

    if (fallback_ops[0] != '\0') {
      (void)snprintf(converged->backend_fallback_reason,
                     sizeof(converged->backend_fallback_reason),
                     "amw0 handles %s because standard backend does not support them",
                     fallback_ops);
      return;
    }

    if (!converged->amw0_available &&
        (!converged->standard_capabilities.can_apply_power_limits ||
         !converged->standard_capabilities.can_apply_fan_table)) {
      first = true;
      fallback_ops[0] = '\0';
      if (!converged->standard_capabilities.can_apply_power_limits) {
        append_operation(fallback_ops, sizeof(fallback_ops), &first,
                         "apply_power_limits");
      }
      if (!converged->standard_capabilities.can_apply_fan_table) {
        append_operation(fallback_ops, sizeof(fallback_ops), &first,
                         "apply_fan_table");
      }
      if (fallback_ops[0] != '\0') {
        (void)snprintf(converged->backend_fallback_reason,
                       sizeof(converged->backend_fallback_reason),
                       "no amw0 fallback is available for %s", fallback_ops);
      }
    }
    return;
  }

  if (converged->standard_status != LCC_OK) {
    (void)snprintf(converged->backend_fallback_reason,
                   sizeof(converged->backend_fallback_reason),
                   "standard probe failed: %s",
                   lcc_status_string(converged->standard_status));
    return;
  }

  (void)snprintf(converged->backend_fallback_reason,
                 sizeof(converged->backend_fallback_reason),
                 "standard backend is unavailable for all requested capabilities");
}

static lcc_status_t converged_probe(void *ctx,
                                    lcc_backend_capabilities_t *capabilities,
                                    lcc_backend_result_t *result) {
  lcc_converged_backend_t *converged = ctx;

  if (converged == NULL || capabilities == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  memset(capabilities, 0, sizeof(*capabilities));
  capabilities->can_read_state =
      route_primary_backend(converged, LCC_CONVERGED_ROUTE_READ_STATE) != NULL;
  capabilities->can_apply_profile = route_primary_backend(
                                        converged,
                                        LCC_CONVERGED_ROUTE_APPLY_PROFILE) != NULL;
  capabilities->can_apply_mode =
      route_primary_backend(converged, LCC_CONVERGED_ROUTE_APPLY_MODE) != NULL;
  capabilities->can_apply_power_limits =
      route_primary_backend(converged,
                            LCC_CONVERGED_ROUTE_APPLY_POWER_LIMITS) != NULL;
  capabilities->can_apply_fan_table =
      route_primary_backend(converged,
                            LCC_CONVERGED_ROUTE_APPLY_FAN_TABLE) != NULL;
  capabilities->has_platform_profile =
      converged->standard_available &&
      converged->standard_capabilities.has_platform_profile;
  capabilities->has_powercap =
      converged->standard_available && converged->standard_capabilities.has_powercap;
  capabilities->needs_reboot_for_mux =
      converged->standard_capabilities.needs_reboot_for_mux ||
      converged->amw0_capabilities.needs_reboot_for_mux;

  lcc_backend_result_reset(result);
  if (!capabilities->can_read_state && !capabilities->can_apply_profile &&
      !capabilities->can_apply_mode && !capabilities->can_apply_power_limits &&
      !capabilities->can_apply_fan_table) {
    return LCC_ERR_NOT_FOUND;
  }

  return LCC_OK;
}

static lcc_status_t converged_read_state(void *ctx, lcc_state_snapshot_t *state,
                                         lcc_backend_result_t *result) {
  lcc_converged_backend_t *converged = ctx;
  lcc_state_snapshot_t amw0_state;
  lcc_backend_result_t amw0_result;
  const lcc_backend_t *primary_backend = NULL;
  const lcc_backend_t *secondary_backend = NULL;
  const char *used_backend_name = NULL;
  lcc_status_t status = LCC_OK;

  if (converged == NULL || state == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  primary_backend =
      route_primary_backend(converged, LCC_CONVERGED_ROUTE_READ_STATE);
  if (primary_backend == NULL) {
    lcc_backend_result_reset(result);
    lcc_backend_result_set_detail(result,
                                  "no converged backend can read state");
    return LCC_ERR_NOT_SUPPORTED;
  }

  status = lcc_backend_read_state(primary_backend, state, result);
  used_backend_name = lcc_backend_name(primary_backend);
  set_result_executor(result, primary_backend);
  if ((status == LCC_ERR_NOT_SUPPORTED || status == LCC_ERR_NOT_FOUND) &&
      (secondary_backend = route_secondary_backend(
           converged, LCC_CONVERGED_ROUTE_READ_STATE, primary_backend)) != NULL) {
    lcc_log_warn("backend route fallback operation=read_state from=%s to=%s reason=%s",
                 lcc_backend_name(primary_backend), lcc_backend_name(secondary_backend),
                 lcc_status_string(status));
    status = lcc_backend_read_state(secondary_backend, state, result);
    used_backend_name = lcc_backend_name(secondary_backend);
    set_result_executor(result, secondary_backend);
    if (result != NULL && result->detail[0] == '\0') {
      set_fallback_detail(result, "read_state", primary_backend,
                          secondary_backend, LCC_ERR_NOT_SUPPORTED);
    }
  } else if (status == LCC_ERR_NOT_SUPPORTED || status == LCC_ERR_NOT_FOUND) {
    set_fallback_detail(result, "read_state", primary_backend, NULL, status);
  }
  if (status != LCC_OK) {
    return status;
  }
  if (primary_backend != converged->amw0_backend && converged->amw0_available) {
    memset(&amw0_state, 0, sizeof(amw0_state));
    status = lcc_backend_read_state(converged->amw0_backend, &amw0_state,
                                    &amw0_result);
    if (status == LCC_OK) {
      overlay_amw0_state(state, &amw0_state, converged);
    }
  }

  return lcc_backend_state_set_metadata(state, used_backend_name,
                                        converged->backend_selected,
                                        converged->backend_fallback_reason,
                                        &converged->execution);
}

static lcc_status_t converged_apply_profile(void *ctx, const char *profile_name,
                                            lcc_backend_result_t *result) {
  lcc_converged_backend_t *converged = ctx;
  const lcc_backend_t *primary_backend = NULL;
  const lcc_backend_t *secondary_backend = NULL;
  lcc_status_t status = LCC_OK;

  if (converged == NULL || profile_name == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  primary_backend =
      route_primary_backend(converged, LCC_CONVERGED_ROUTE_APPLY_PROFILE);
  if (primary_backend == NULL) {
    lcc_backend_result_reset(result);
    lcc_backend_result_set_detail(
        result, "no converged backend can apply profiles");
    return LCC_ERR_NOT_SUPPORTED;
  }

  status = lcc_backend_apply_profile(primary_backend, profile_name, result);
  set_result_executor(result, primary_backend);
  if (status == LCC_ERR_NOT_SUPPORTED &&
      (secondary_backend = route_secondary_backend(
           converged, LCC_CONVERGED_ROUTE_APPLY_PROFILE, primary_backend)) != NULL) {
    lcc_log_warn("backend route fallback operation=apply_profile from=%s to=%s reason=%s",
                 lcc_backend_name(primary_backend), lcc_backend_name(secondary_backend),
                 lcc_status_string(status));
    status = lcc_backend_apply_profile(secondary_backend, profile_name, result);
    set_result_executor(result, secondary_backend);
    if (result != NULL && result->detail[0] == '\0') {
      set_fallback_detail(result, "apply_profile", primary_backend,
                          secondary_backend, LCC_ERR_NOT_SUPPORTED);
    }
    return status;
  }
  if (status == LCC_ERR_NOT_SUPPORTED) {
    set_fallback_detail(result, "apply_profile", primary_backend, NULL, status);
  }

  return status;
}

static lcc_status_t converged_apply_mode(void *ctx, lcc_operating_mode_t mode,
                                         lcc_backend_result_t *result) {
  lcc_converged_backend_t *converged = ctx;
  const lcc_backend_t *primary_backend = NULL;
  const lcc_backend_t *secondary_backend = NULL;
  lcc_status_t status = LCC_OK;

  if (converged == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  primary_backend =
      route_primary_backend(converged, LCC_CONVERGED_ROUTE_APPLY_MODE);
  if (primary_backend == NULL) {
    lcc_backend_result_reset(result);
    lcc_backend_result_set_detail(result, "no converged backend can apply modes");
    return LCC_ERR_NOT_SUPPORTED;
  }

  status = lcc_backend_apply_mode(primary_backend, mode, result);
  set_result_executor(result, primary_backend);
  if (status == LCC_ERR_NOT_SUPPORTED &&
      (secondary_backend = route_secondary_backend(
           converged, LCC_CONVERGED_ROUTE_APPLY_MODE, primary_backend)) != NULL) {
    lcc_log_warn("backend route fallback operation=apply_mode from=%s to=%s reason=%s",
                 lcc_backend_name(primary_backend), lcc_backend_name(secondary_backend),
                 lcc_status_string(status));
    status = lcc_backend_apply_mode(secondary_backend, mode, result);
    set_result_executor(result, secondary_backend);
    if (result != NULL && result->detail[0] == '\0') {
      set_fallback_detail(result, "apply_mode", primary_backend,
                          secondary_backend, LCC_ERR_NOT_SUPPORTED);
    }
    return status;
  }
  if (status == LCC_ERR_NOT_SUPPORTED) {
    set_fallback_detail(result, "apply_mode", primary_backend, NULL, status);
  }

  return status;
}

static lcc_status_t converged_apply_power_limits(
    void *ctx, const lcc_power_limits_t *limits, lcc_backend_result_t *result) {
  lcc_converged_backend_t *converged = ctx;
  const lcc_backend_t *primary_backend = NULL;
  const lcc_backend_t *secondary_backend = NULL;
  lcc_status_t status = LCC_OK;

  if (converged == NULL || limits == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  primary_backend =
      route_primary_backend(converged, LCC_CONVERGED_ROUTE_APPLY_POWER_LIMITS);
  if (primary_backend == NULL) {
    lcc_backend_result_reset(result);
    lcc_backend_result_set_detail(
        result, "no converged backend can apply power limits");
    return LCC_ERR_NOT_SUPPORTED;
  }

  status = lcc_backend_apply_power_limits(primary_backend, limits, result);
  set_result_executor(result, primary_backend);
  if (status == LCC_ERR_NOT_SUPPORTED &&
      (secondary_backend = route_secondary_backend(
           converged, LCC_CONVERGED_ROUTE_APPLY_POWER_LIMITS,
           primary_backend)) != NULL) {
    lcc_log_warn("backend route fallback operation=apply_power_limits from=%s to=%s reason=%s",
                 lcc_backend_name(primary_backend), lcc_backend_name(secondary_backend),
                 lcc_status_string(status));
    status = lcc_backend_apply_power_limits(secondary_backend, limits, result);
    set_result_executor(result, secondary_backend);
    if (result != NULL && result->detail[0] == '\0') {
      set_fallback_detail(result, "apply_power_limits", primary_backend,
                          secondary_backend, LCC_ERR_NOT_SUPPORTED);
    }
    return status;
  }
  if (status == LCC_ERR_NOT_SUPPORTED) {
    set_fallback_detail(result, "apply_power_limits", primary_backend, NULL,
                        status);
  }

  return status;
}

static lcc_status_t converged_apply_fan_table(void *ctx, const char *table_name,
                                              lcc_backend_result_t *result) {
  lcc_converged_backend_t *converged = ctx;
  const lcc_backend_t *primary_backend = NULL;
  const lcc_backend_t *secondary_backend = NULL;
  lcc_status_t status = LCC_OK;

  if (converged == NULL || table_name == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  primary_backend =
      route_primary_backend(converged, LCC_CONVERGED_ROUTE_APPLY_FAN_TABLE);
  if (primary_backend == NULL) {
    lcc_backend_result_reset(result);
    lcc_backend_result_set_detail(
        result, "no converged backend can apply fan tables");
    return LCC_ERR_NOT_SUPPORTED;
  }

  status = lcc_backend_apply_fan_table(primary_backend, table_name, result);
  set_result_executor(result, primary_backend);
  if (status == LCC_ERR_NOT_SUPPORTED &&
      (secondary_backend = route_secondary_backend(
           converged, LCC_CONVERGED_ROUTE_APPLY_FAN_TABLE,
           primary_backend)) != NULL) {
    lcc_log_warn("backend route fallback operation=apply_fan_table from=%s to=%s reason=%s",
                 lcc_backend_name(primary_backend), lcc_backend_name(secondary_backend),
                 lcc_status_string(status));
    status = lcc_backend_apply_fan_table(secondary_backend, table_name, result);
    set_result_executor(result, secondary_backend);
    if (result != NULL && result->detail[0] == '\0') {
      set_fallback_detail(result, "apply_fan_table", primary_backend,
                          secondary_backend, LCC_ERR_NOT_SUPPORTED);
    }
    return status;
  }
  if (status == LCC_ERR_NOT_SUPPORTED) {
    set_fallback_detail(result, "apply_fan_table", primary_backend, NULL,
                        status);
  }

  return status;
}

const lcc_backend_ops_t lcc_converged_standard_backend_ops = {
    .name = "standard",
    .kind = LCC_BACKEND_STANDARD,
    .probe = converged_probe,
    .read_state = converged_read_state,
    .apply_profile = converged_apply_profile,
    .apply_mode = converged_apply_mode,
    .apply_power_limits = converged_apply_power_limits,
    .apply_fan_table = converged_apply_fan_table,
};

const lcc_backend_ops_t lcc_converged_amw0_backend_ops = {
    .name = "amw0",
    .kind = LCC_BACKEND_AMW0,
    .probe = converged_probe,
    .read_state = converged_read_state,
    .apply_profile = converged_apply_profile,
    .apply_mode = converged_apply_mode,
    .apply_power_limits = converged_apply_power_limits,
    .apply_fan_table = converged_apply_fan_table,
};

lcc_status_t lcc_converged_backend_init(
    lcc_converged_backend_t *converged, lcc_backend_t *backend,
    lcc_backend_t *standard_backend, lcc_status_t standard_status,
    const lcc_backend_capabilities_t *standard_capabilities,
    lcc_backend_t *amw0_backend, lcc_status_t amw0_status,
    const lcc_backend_capabilities_t *amw0_capabilities) {
  if (converged == NULL || backend == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  memset(converged, 0, sizeof(*converged));
  converged->standard_backend = standard_backend;
  converged->amw0_backend = amw0_backend;
  converged->standard_status = standard_status;
  converged->amw0_status = amw0_status;
  converged->standard_available = standard_backend != NULL && standard_status == LCC_OK;
  converged->amw0_available = amw0_backend != NULL && amw0_status == LCC_OK;
  if (standard_capabilities != NULL) {
    converged->standard_capabilities = *standard_capabilities;
  }
  if (amw0_capabilities != NULL) {
    converged->amw0_capabilities = *amw0_capabilities;
  }

  build_selection_metadata(converged);
  if (converged->backend_selected[0] == '\0') {
    return LCC_ERR_NOT_FOUND;
  }

  lcc_backend_bind(
      backend,
      strcmp(converged->backend_selected, "standard") == 0
          ? &lcc_converged_standard_backend_ops
          : &lcc_converged_amw0_backend_ops,
      converged);
  return LCC_OK;
}
