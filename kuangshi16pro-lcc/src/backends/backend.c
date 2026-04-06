#include "lcc/backend.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static lcc_status_t unsupported_operation(lcc_backend_result_t *result) {
  lcc_backend_result_reset(result);
  return LCC_ERR_NOT_SUPPORTED;
}

lcc_status_t lcc_backend_copy_text(char *buffer, size_t buffer_len,
                                   const char *value) {
  int written = 0;

  if (buffer == NULL || value == NULL || value[0] == '\0') {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  written = snprintf(buffer, buffer_len, "%s", value);
  if (written < 0 || (size_t)written >= buffer_len) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return LCC_OK;
}

void lcc_backend_execution_clear(lcc_execution_snapshot_t *execution) {
  if (execution != NULL) {
    memset(execution, 0, sizeof(*execution));
  }
}

lcc_status_t lcc_backend_execution_set(lcc_execution_snapshot_t *execution,
                                       const char *read_state,
                                       const char *apply_profile,
                                       const char *apply_mode,
                                       const char *apply_power_limits,
                                       const char *apply_fan_table) {
  if (execution == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  lcc_backend_execution_clear(execution);
  if (read_state != NULL && read_state[0] != '\0' &&
      lcc_backend_copy_text(execution->read_state,
                            sizeof(execution->read_state), read_state) !=
          LCC_OK) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }
  if (apply_profile != NULL && apply_profile[0] != '\0' &&
      lcc_backend_copy_text(execution->apply_profile,
                            sizeof(execution->apply_profile), apply_profile) !=
          LCC_OK) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }
  if (apply_mode != NULL && apply_mode[0] != '\0' &&
      lcc_backend_copy_text(execution->apply_mode, sizeof(execution->apply_mode),
                            apply_mode) != LCC_OK) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }
  if (apply_power_limits != NULL && apply_power_limits[0] != '\0' &&
      lcc_backend_copy_text(execution->apply_power_limits,
                            sizeof(execution->apply_power_limits),
                            apply_power_limits) != LCC_OK) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }
  if (apply_fan_table != NULL && apply_fan_table[0] != '\0' &&
      lcc_backend_copy_text(execution->apply_fan_table,
                            sizeof(execution->apply_fan_table),
                            apply_fan_table) != LCC_OK) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return LCC_OK;
}

lcc_status_t lcc_backend_execution_set_all(lcc_execution_snapshot_t *execution,
                                           const char *backend_name) {
  return lcc_backend_execution_set(execution, backend_name, backend_name,
                                   backend_name, backend_name, backend_name);
}

lcc_status_t lcc_backend_state_set_metadata(
    lcc_state_snapshot_t *state, const char *backend_name,
    const char *backend_selected, const char *fallback_reason,
    const lcc_execution_snapshot_t *execution) {
  if (state == NULL || backend_selected == NULL || backend_selected[0] == '\0' ||
      execution == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  if (backend_name != NULL && backend_name[0] != '\0' &&
      lcc_backend_copy_text(state->backend_name, sizeof(state->backend_name),
                            backend_name) != LCC_OK) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }
  if (lcc_backend_copy_text(state->backend_selected,
                            sizeof(state->backend_selected),
                            backend_selected) != LCC_OK) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  state->execution = *execution;
  if (fallback_reason != NULL && fallback_reason[0] != '\0') {
    if (lcc_backend_copy_text(state->backend_fallback_reason,
                              sizeof(state->backend_fallback_reason),
                              fallback_reason) != LCC_OK) {
      return LCC_ERR_BUFFER_TOO_SMALL;
    }
  } else {
    state->backend_fallback_reason[0] = '\0';
  }

  return LCC_OK;
}

void lcc_backend_result_reset(lcc_backend_result_t *result) {
  if (result != NULL) {
    result->changed = false;
    result->hardware_write = false;
    result->reboot_required = false;
    result->stage[0] = '\0';
  }
}

void lcc_backend_bind(lcc_backend_t *backend, const lcc_backend_ops_t *ops,
                      void *ctx) {
  if (backend == NULL) {
    return;
  }

  backend->ops = ops;
  backend->ctx = ctx;
}

const char *lcc_backend_name(const lcc_backend_t *backend) {
  if (backend == NULL || backend->ops == NULL || backend->ops->name == NULL) {
    return "unknown";
  }

  return backend->ops->name;
}

lcc_status_t lcc_backend_probe(const lcc_backend_t *backend,
                               lcc_backend_capabilities_t *capabilities,
                               lcc_backend_result_t *result) {
  if (backend == NULL || capabilities == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->probe == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->probe(backend->ctx, capabilities, result);
}

lcc_status_t lcc_backend_read_state(const lcc_backend_t *backend,
                                    lcc_state_snapshot_t *state,
                                    lcc_backend_result_t *result) {
  if (backend == NULL || state == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->read_state == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->read_state(backend->ctx, state, result);
}

lcc_status_t lcc_backend_apply_profile(const lcc_backend_t *backend,
                                       const char *profile_name,
                                       lcc_backend_result_t *result) {
  if (backend == NULL || profile_name == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->apply_profile == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->apply_profile(backend->ctx, profile_name, result);
}

lcc_status_t lcc_backend_apply_mode(const lcc_backend_t *backend,
                                    lcc_operating_mode_t mode,
                                    lcc_backend_result_t *result) {
  if (backend == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->apply_mode == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->apply_mode(backend->ctx, mode, result);
}

lcc_status_t lcc_backend_apply_power_limits(const lcc_backend_t *backend,
                                            const lcc_power_limits_t *limits,
                                            lcc_backend_result_t *result) {
  if (backend == NULL || limits == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->apply_power_limits == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->apply_power_limits(backend->ctx, limits, result);
}

lcc_status_t lcc_backend_apply_fan_table(const lcc_backend_t *backend,
                                         const char *table_name,
                                         lcc_backend_result_t *result) {
  if (backend == NULL || table_name == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->apply_fan_table == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->apply_fan_table(backend->ctx, table_name, result);
}
