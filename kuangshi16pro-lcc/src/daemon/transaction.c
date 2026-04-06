#include "daemon/transaction.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

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

static lcc_status_t copy_name(char *buffer, size_t buffer_len,
                              const char *value) {
  const int written = snprintf(buffer, buffer_len, "%s", value);

  if (value == NULL || value[0] == '\0') {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (written < 0 || (size_t)written >= buffer_len) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return LCC_OK;
}

static void merge_optional_byte(lcc_optional_byte_t *target,
                                lcc_optional_byte_t source) {
  if (target != NULL && source.present) {
    *target = source;
  }
}

static lcc_status_t mode_to_profile_name(const char *mode_name,
                                         const char **profile_name,
                                         lcc_operating_mode_t *mode) {
  if (mode_name == NULL || mode == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (strcmp(mode_name, "gaming") == 0) {
    if (profile_name != NULL) {
      *profile_name = "gaming";
    }
    *mode = LCC_MODE_GAMING;
    return LCC_OK;
  }
  if (strcmp(mode_name, "office") == 0) {
    if (profile_name != NULL) {
      *profile_name = "office";
    }
    *mode = LCC_MODE_OFFICE;
    return LCC_OK;
  }
  if (strcmp(mode_name, "turbo") == 0) {
    if (profile_name != NULL) {
      *profile_name = "turbo";
    }
    *mode = LCC_MODE_TURBO;
    return LCC_OK;
  }
  if (strcmp(mode_name, "custom") == 0) {
    if (profile_name != NULL) {
      *profile_name = "custom";
    }
    *mode = LCC_MODE_CUSTOM;
    return LCC_OK;
  }

  return LCC_ERR_PARSE;
}

static const char *transaction_operation_name(lcc_transaction_kind_t kind) {
  switch (kind) {
    case LCC_TRANSACTION_PROFILE:
      return "set-profile";
    case LCC_TRANSACTION_MODE:
      return "set-mode";
    case LCC_TRANSACTION_POWER_LIMITS:
      return "set-power-limits";
    case LCC_TRANSACTION_FAN_TABLE:
      return "apply-fan-table";
  }

  return "unknown";
}

static void transaction_clear(lcc_manager_t *manager) {
  if (manager == NULL) {
    return;
  }

  manager->state_cache.transaction.state = LCC_TRANSACTION_STATE_IDLE;
  manager->state_cache.transaction.operation[0] = '\0';
  manager->state_cache.transaction.has_pending_target = false;
  memset(&manager->state_cache.transaction.pending_target, 0,
         sizeof(manager->state_cache.transaction.pending_target));
  manager->state_cache.transaction.last_error = LCC_OK;
}

static void transaction_fail(lcc_manager_t *manager, const char *operation_name,
                             const lcc_state_target_t *pending_target,
                             lcc_status_t status) {
  if (manager == NULL || operation_name == NULL || pending_target == NULL) {
    return;
  }

  manager->state_cache.transaction.state = LCC_TRANSACTION_STATE_FAILED;
  (void)copy_name(manager->state_cache.transaction.operation,
                  sizeof(manager->state_cache.transaction.operation),
                  operation_name);
  manager->state_cache.transaction.has_pending_target = true;
  manager->state_cache.transaction.pending_target = *pending_target;
  manager->state_cache.transaction.last_error = status;
}

static lcc_status_t transaction_stage_target(
    const lcc_manager_t *manager, const lcc_transaction_request_t *request,
    lcc_state_target_t *target, lcc_operating_mode_t *resolved_mode) {
  const char *resolved_profile = NULL;
  lcc_operating_mode_t mode = LCC_MODE_OFFICE;

  if (manager == NULL || request == NULL || target == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  *target = manager->state_cache.requested;
  if (resolved_mode != NULL) {
    *resolved_mode = LCC_MODE_OFFICE;
  }

  switch (request->kind) {
    case LCC_TRANSACTION_PROFILE:
      if (!name_is_safe(request->input.profile_name)) {
        return LCC_ERR_INVALID_ARGUMENT;
      }
      return copy_name(target->profile, sizeof(target->profile),
                       request->input.profile_name);
    case LCC_TRANSACTION_MODE:
      if (mode_to_profile_name(request->input.mode_name, &resolved_profile,
                               &mode) != LCC_OK) {
        return LCC_ERR_PARSE;
      }
      if (resolved_mode != NULL) {
        *resolved_mode = mode;
      }
      return copy_name(target->profile, sizeof(target->profile),
                       resolved_profile);
    case LCC_TRANSACTION_POWER_LIMITS:
      if (request->input.power_limits == NULL) {
        return LCC_ERR_INVALID_ARGUMENT;
      }
      if (!request->input.power_limits->pl1.present &&
          !request->input.power_limits->pl2.present &&
          !request->input.power_limits->pl4.present &&
          !request->input.power_limits->tcc_offset.present) {
        return LCC_ERR_INVALID_ARGUMENT;
      }
      merge_optional_byte(&target->power_limits.pl1,
                          request->input.power_limits->pl1);
      merge_optional_byte(&target->power_limits.pl2,
                          request->input.power_limits->pl2);
      merge_optional_byte(&target->power_limits.pl4,
                          request->input.power_limits->pl4);
      merge_optional_byte(&target->power_limits.tcc_offset,
                          request->input.power_limits->tcc_offset);
      target->has_power_limits = true;
      return LCC_OK;
    case LCC_TRANSACTION_FAN_TABLE:
      if (!name_is_safe(request->input.fan_table_name)) {
        return LCC_ERR_INVALID_ARGUMENT;
      }
      return copy_name(target->fan_table, sizeof(target->fan_table),
                       request->input.fan_table_name);
  }

  return LCC_ERR_INVALID_ARGUMENT;
}

static void transaction_begin(lcc_manager_t *manager, const char *operation_name,
                              const lcc_state_target_t *pending_target) {
  if (manager == NULL || operation_name == NULL || pending_target == NULL) {
    return;
  }

  manager->state_cache.transaction.state = LCC_TRANSACTION_STATE_PENDING;
  (void)copy_name(manager->state_cache.transaction.operation,
                  sizeof(manager->state_cache.transaction.operation),
                  operation_name);
  manager->state_cache.transaction.has_pending_target = true;
  manager->state_cache.transaction.pending_target = *pending_target;
  manager->state_cache.transaction.last_error = LCC_OK;
}

static lcc_status_t transaction_apply(lcc_manager_t *manager,
                                      const lcc_transaction_request_t *request,
                                      lcc_operating_mode_t resolved_mode,
                                      lcc_backend_result_t *result) {
  lcc_status_t status = LCC_OK;

  if (manager == NULL || request == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  switch (request->kind) {
    case LCC_TRANSACTION_PROFILE:
      status = lcc_backend_apply_profile(manager->backend,
                                         request->input.profile_name, result);
      if (status == LCC_ERR_NOT_SUPPORTED &&
          mode_to_profile_name(request->input.profile_name, NULL,
                               &resolved_mode) == LCC_OK) {
        return lcc_backend_apply_mode(manager->backend, resolved_mode, result);
      }
      return status;
    case LCC_TRANSACTION_MODE:
      return lcc_backend_apply_mode(manager->backend, resolved_mode, result);
    case LCC_TRANSACTION_POWER_LIMITS:
      return lcc_backend_apply_power_limits(manager->backend,
                                            request->input.power_limits, result);
    case LCC_TRANSACTION_FAN_TABLE:
      return lcc_backend_apply_fan_table(manager->backend,
                                         request->input.fan_table_name, result);
  }

  return LCC_ERR_INVALID_ARGUMENT;
}

lcc_status_t lcc_transaction_refresh_state(lcc_manager_t *manager) {
  lcc_state_snapshot_t backend_state;
  lcc_transaction_snapshot_t transaction_state;
  lcc_backend_result_t result;
  lcc_status_t status = LCC_OK;

  if (manager == NULL || manager->backend == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  transaction_state = manager->state_cache.transaction;
  memset(&backend_state, 0, sizeof(backend_state));
  status = lcc_backend_read_state(manager->backend, &backend_state, &result);
  if (status != LCC_OK) {
    return status;
  }

  if (backend_state.backend_name[0] == '\0') {
    (void)copy_name(backend_state.backend_name, sizeof(backend_state.backend_name),
                    lcc_backend_name(manager->backend));
  }
  if (result.hardware_write) {
    backend_state.hardware_write = true;
  }

  manager->state_cache = backend_state;
  manager->state_cache.transaction = transaction_state;
  return LCC_OK;
}

lcc_status_t lcc_transaction_execute(lcc_manager_t *manager,
                                     const lcc_transaction_request_t *request) {
  lcc_state_target_t pending_target;
  lcc_backend_result_t result;
  lcc_operating_mode_t resolved_mode = LCC_MODE_OFFICE;
  const char *operation_name = NULL;
  lcc_status_t status = LCC_OK;

  if (manager == NULL || request == NULL || manager->backend == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  operation_name = transaction_operation_name(request->kind);
  status = transaction_stage_target(manager, request, &pending_target,
                                    &resolved_mode);
  if (status != LCC_OK) {
    return status;
  }

  transaction_begin(manager, operation_name, &pending_target);
  status = transaction_apply(manager, request, resolved_mode, &result);
  if (status != LCC_OK) {
    (void)lcc_transaction_refresh_state(manager);
    transaction_fail(manager, operation_name, &pending_target, status);
    return status;
  }

  status = lcc_transaction_refresh_state(manager);
  if (status != LCC_OK) {
    transaction_fail(manager, operation_name, &pending_target, status);
    return status;
  }

  if (result.hardware_write) {
    manager->state_cache.hardware_write = true;
  }
  transaction_clear(manager);
  return LCC_OK;
}
