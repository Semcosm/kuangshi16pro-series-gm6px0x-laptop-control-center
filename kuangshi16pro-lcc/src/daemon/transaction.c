#include "daemon/transaction.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "common/lcc_log.h"
#include "core/state/reader.h"

static const char *const LCC_TX_STAGE_PREFLIGHT = "preflight-validate";
static const char *const LCC_TX_STAGE_CAPABILITY_GATE = "capability-gate";
static const char *const LCC_TX_STAGE_BACKEND_ROUTE = "backend-route";
static const char *const LCC_TX_STAGE_APPLY = "apply";
static const char *const LCC_TX_STAGE_STATE_REFRESH = "state-refresh";
static const char *const LCC_TX_STAGE_COMPLETE = "complete";

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

static const char *transaction_capability_name(lcc_transaction_kind_t kind) {
  switch (kind) {
    case LCC_TRANSACTION_PROFILE:
      return "apply_profile";
    case LCC_TRANSACTION_MODE:
      return "apply_mode";
    case LCC_TRANSACTION_POWER_LIMITS:
      return "apply_power_limits";
    case LCC_TRANSACTION_FAN_TABLE:
      return "apply_fan_table";
  }

  return "unknown";
}

static bool transaction_capability_supported(
    const lcc_backend_capabilities_t *capabilities,
    lcc_transaction_kind_t kind) {
  if (capabilities == NULL) {
    return false;
  }

  switch (kind) {
    case LCC_TRANSACTION_PROFILE:
      return capabilities->can_apply_profile;
    case LCC_TRANSACTION_MODE:
      return capabilities->can_apply_mode;
    case LCC_TRANSACTION_POWER_LIMITS:
      return capabilities->can_apply_power_limits;
    case LCC_TRANSACTION_FAN_TABLE:
      return capabilities->can_apply_fan_table;
  }

  return false;
}

static void append_text_field(char *buffer, size_t buffer_len, bool *first,
                              const char *label, const char *value) {
  size_t used = 0;

  if (buffer == NULL || buffer_len == 0u || first == NULL || label == NULL ||
      label[0] == '\0' || value == NULL || value[0] == '\0') {
    return;
  }

  used = strlen(buffer);
  if (used >= buffer_len - 1u) {
    return;
  }

  (void)snprintf(buffer + used, buffer_len - used, "%s%s=%s",
                 *first ? "" : " ", label, value);
  *first = false;
}

static void append_optional_limit(char *buffer, size_t buffer_len, bool *first,
                                  const char *label,
                                  lcc_optional_byte_t value) {
  char number[32];

  if (!value.present) {
    return;
  }

  (void)snprintf(number, sizeof(number), "%u", (unsigned int)value.value);
  append_text_field(buffer, buffer_len, first, label, number);
}

static void format_target_summary(const lcc_state_target_t *target, char *buffer,
                                  size_t buffer_len) {
  bool first = true;

  if (buffer == NULL || buffer_len == 0u) {
    return;
  }

  buffer[0] = '\0';
  if (target == NULL) {
    (void)snprintf(buffer, buffer_len, "none");
    return;
  }

  append_text_field(buffer, buffer_len, &first, "profile", target->profile);
  append_text_field(buffer, buffer_len, &first, "fan_table", target->fan_table);
  if (target->has_power_limits) {
    append_optional_limit(buffer, buffer_len, &first, "pl1",
                          target->power_limits.pl1);
    append_optional_limit(buffer, buffer_len, &first, "pl2",
                          target->power_limits.pl2);
    append_optional_limit(buffer, buffer_len, &first, "pl4",
                          target->power_limits.pl4);
    append_optional_limit(buffer, buffer_len, &first, "tcc_offset",
                          target->power_limits.tcc_offset);
  }
  if (first) {
    (void)snprintf(buffer, buffer_len, "none");
  }
}

static const char *nonnull_text(const char *value) {
  return value != NULL && value[0] != '\0' ? value : "none";
}

static void last_apply_begin(lcc_manager_t *manager) {
  if (manager == NULL) {
    return;
  }

  manager->state_cache.last_apply.stage[0] = '\0';
  manager->state_cache.last_apply.backend[0] = '\0';
  manager->state_cache.last_apply.has_target = false;
  memset(&manager->state_cache.last_apply.target, 0,
         sizeof(manager->state_cache.last_apply.target));
  manager->state_cache.last_apply.error = LCC_OK;
}

static void last_apply_update_stage(lcc_manager_t *manager,
                                    const char *stage_name) {
  if (manager == NULL) {
    return;
  }

  if (stage_name != NULL && stage_name[0] != '\0') {
    (void)copy_name(manager->state_cache.last_apply.stage,
                    sizeof(manager->state_cache.last_apply.stage), stage_name);
  } else {
    manager->state_cache.last_apply.stage[0] = '\0';
  }
}

static void last_apply_update_backend(lcc_manager_t *manager,
                                      const char *backend_name) {
  if (manager == NULL) {
    return;
  }

  if (backend_name != NULL && backend_name[0] != '\0') {
    (void)copy_name(manager->state_cache.last_apply.backend,
                    sizeof(manager->state_cache.last_apply.backend),
                    backend_name);
  } else {
    manager->state_cache.last_apply.backend[0] = '\0';
  }
}

static void last_apply_set_target(lcc_manager_t *manager,
                                  const lcc_state_target_t *target) {
  if (manager == NULL) {
    return;
  }

  if (target != NULL) {
    manager->state_cache.last_apply.has_target = true;
    manager->state_cache.last_apply.target = *target;
  } else {
    manager->state_cache.last_apply.has_target = false;
    memset(&manager->state_cache.last_apply.target, 0,
           sizeof(manager->state_cache.last_apply.target));
  }
}

static void transaction_update_stage(lcc_manager_t *manager,
                                     const char *stage_name) {
  if (manager == NULL) {
    return;
  }

  if (stage_name != NULL && stage_name[0] != '\0') {
    (void)copy_name(manager->state_cache.transaction.stage,
                    sizeof(manager->state_cache.transaction.stage), stage_name);
  } else {
    manager->state_cache.transaction.stage[0] = '\0';
  }
}

static void transaction_mark_public_stage(lcc_manager_t *manager,
                                          const char *stage_name) {
  transaction_update_stage(manager, stage_name);
  last_apply_update_stage(manager, stage_name);
}

static void transaction_set_pending_target(lcc_manager_t *manager,
                                           const lcc_state_target_t *target) {
  if (manager == NULL) {
    return;
  }

  if (target != NULL) {
    manager->state_cache.transaction.has_pending_target = true;
    manager->state_cache.transaction.pending_target = *target;
  } else {
    manager->state_cache.transaction.has_pending_target = false;
    memset(&manager->state_cache.transaction.pending_target, 0,
           sizeof(manager->state_cache.transaction.pending_target));
  }
  last_apply_set_target(manager, target);
}

static void format_detail_summary(const char *backend_stage,
                                  const char *detail_text, char *buffer,
                                  size_t buffer_len) {
  if (buffer == NULL || buffer_len == 0u) {
    return;
  }

  if (backend_stage != NULL && backend_stage[0] != '\0' && detail_text != NULL &&
      detail_text[0] != '\0') {
    (void)snprintf(buffer, buffer_len, "backend_stage=%s reason=%s",
                   backend_stage, detail_text);
    return;
  }
  if (backend_stage != NULL && backend_stage[0] != '\0') {
    (void)snprintf(buffer, buffer_len, "backend_stage=%s", backend_stage);
    return;
  }
  if (detail_text != NULL && detail_text[0] != '\0') {
    (void)snprintf(buffer, buffer_len, "%s", detail_text);
    return;
  }

  (void)snprintf(buffer, buffer_len, "%s", "none");
}

static void log_transaction_event(const char *level,
                                  const char *operation_name,
                                  const char *stage_name,
                                  const char *backend_name,
                                  const lcc_state_target_t *target,
                                  lcc_status_t status,
                                  const char *detail_text) {
  char target_summary[256];

  if (operation_name == NULL || level == NULL) {
    return;
  }

  format_target_summary(target, target_summary, sizeof(target_summary));
  if (strcmp(level, "error") == 0) {
    lcc_log_error(
        "transaction operation=%s stage=%s backend=%s status=%s target=%s detail=%s",
        operation_name,
        (stage_name != NULL && stage_name[0] != '\0') ? stage_name : "none",
        nonnull_text(backend_name), lcc_status_string(status), target_summary,
        nonnull_text(detail_text));
    return;
  }
  if (strcmp(level, "warn") == 0) {
    lcc_log_warn(
        "transaction operation=%s stage=%s backend=%s status=%s target=%s detail=%s",
        operation_name,
        (stage_name != NULL && stage_name[0] != '\0') ? stage_name : "none",
        nonnull_text(backend_name), lcc_status_string(status), target_summary,
        nonnull_text(detail_text));
    return;
  }

  lcc_log_info(
      "transaction operation=%s stage=%s backend=%s status=%s target=%s detail=%s",
      operation_name,
      (stage_name != NULL && stage_name[0] != '\0') ? stage_name : "none",
      nonnull_text(backend_name), lcc_status_string(status), target_summary,
      nonnull_text(detail_text));
}

static void transaction_clear(lcc_manager_t *manager) {
  if (manager == NULL) {
    return;
  }

  manager->state_cache.transaction.state = LCC_TRANSACTION_STATE_IDLE;
  manager->state_cache.transaction.operation[0] = '\0';
  manager->state_cache.transaction.stage[0] = '\0';
  manager->state_cache.transaction.has_pending_target = false;
  memset(&manager->state_cache.transaction.pending_target, 0,
         sizeof(manager->state_cache.transaction.pending_target));
  manager->state_cache.transaction.last_error = LCC_OK;
}

static void transaction_fail(lcc_manager_t *manager, const char *operation_name,
                             const char *stage_name,
                             const char *backend_name,
                             const lcc_state_target_t *pending_target,
                             lcc_status_t status) {
  if (manager == NULL || operation_name == NULL) {
    return;
  }

  manager->state_cache.transaction.state = LCC_TRANSACTION_STATE_FAILED;
  (void)copy_name(manager->state_cache.transaction.operation,
                  sizeof(manager->state_cache.transaction.operation),
                  operation_name);
  transaction_update_stage(manager, stage_name);
  transaction_set_pending_target(manager, pending_target);
  manager->state_cache.transaction.last_error = status;
  manager->state_cache.last_apply.error = status;
  last_apply_update_stage(manager, stage_name);
  last_apply_update_backend(manager, backend_name);
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

static void transaction_begin(lcc_manager_t *manager,
                              const char *operation_name) {
  if (manager == NULL || operation_name == NULL) {
    return;
  }

  manager->state_cache.transaction.state = LCC_TRANSACTION_STATE_PENDING;
  (void)copy_name(manager->state_cache.transaction.operation,
                  sizeof(manager->state_cache.transaction.operation),
                  operation_name);
  manager->state_cache.transaction.stage[0] = '\0';
  manager->state_cache.transaction.has_pending_target = false;
  memset(&manager->state_cache.transaction.pending_target, 0,
         sizeof(manager->state_cache.transaction.pending_target));
  manager->state_cache.transaction.last_error = LCC_OK;
  last_apply_begin(manager);
}

static lcc_status_t transaction_capability_gate(
    const lcc_manager_t *manager, const lcc_transaction_request_t *request,
    char *detail, size_t detail_len) {
  if (manager == NULL || request == NULL || detail == NULL || detail_len == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  if (transaction_capability_supported(&manager->backend_capabilities,
                                       request->kind)) {
    detail[0] = '\0';
    return LCC_OK;
  }

  (void)snprintf(detail, detail_len,
                 "manager capability gate rejected %s for backend selection %s",
                 transaction_capability_name(request->kind),
                 nonnull_text(manager->state_cache.backend_selected));
  return LCC_ERR_NOT_SUPPORTED;
}

static lcc_status_t transaction_apply(lcc_manager_t *manager,
                                      const lcc_transaction_request_t *request,
                                      lcc_operating_mode_t resolved_mode,
                                      lcc_backend_result_t *result) {
  lcc_status_t status = LCC_OK;

  if (manager == NULL || request == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  lcc_backend_result_reset(result);
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
  lcc_transaction_snapshot_t transaction_state;
  lcc_status_t status = LCC_OK;

  if (manager == NULL || manager->backend == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  transaction_state = manager->state_cache.transaction;
  status = lcc_state_reader_refresh(manager->backend, &manager->state_cache);
  if (status != LCC_OK) {
    return status;
  }
  manager->state_cache.transaction = transaction_state;
  return LCC_OK;
}

lcc_status_t lcc_transaction_execute(lcc_manager_t *manager,
                                     const lcc_transaction_request_t *request) {
  lcc_state_target_t pending_target;
  lcc_backend_result_t result;
  char detail[LCC_STATE_REASON_MAX];
  const char *failure_stage = NULL;
  lcc_operating_mode_t resolved_mode = LCC_MODE_OFFICE;
  const char *operation_name = NULL;
  lcc_status_t status = LCC_OK;

  if (manager == NULL || request == NULL || manager->backend == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  operation_name = transaction_operation_name(request->kind);
  lcc_backend_result_reset(&result);
  detail[0] = '\0';
  memset(&pending_target, 0, sizeof(pending_target));

  transaction_begin(manager, operation_name);
  log_transaction_event("info", operation_name, "begin", NULL, NULL, LCC_OK,
                        NULL);

  transaction_mark_public_stage(manager, LCC_TX_STAGE_PREFLIGHT);
  status = transaction_stage_target(manager, request, &pending_target,
                                    &resolved_mode);
  if (status != LCC_OK) {
    transaction_fail(manager, operation_name, LCC_TX_STAGE_PREFLIGHT, NULL,
                     NULL, status);
    if (request->kind == LCC_TRANSACTION_MODE) {
      (void)snprintf(detail, sizeof(detail), "could not parse mode=%s",
                     request->input.mode_name != NULL ? request->input.mode_name
                                                      : "none");
    } else if (request->kind == LCC_TRANSACTION_PROFILE) {
      (void)snprintf(detail, sizeof(detail), "unsafe profile=%s",
                     request->input.profile_name != NULL
                         ? request->input.profile_name
                         : "none");
    } else if (request->kind == LCC_TRANSACTION_FAN_TABLE) {
      (void)snprintf(detail, sizeof(detail), "unsafe fan_table=%s",
                     request->input.fan_table_name != NULL
                         ? request->input.fan_table_name
                         : "none");
    } else {
      (void)snprintf(detail, sizeof(detail),
                     "power target requires at least one present limit");
    }
    log_transaction_event("error", operation_name, LCC_TX_STAGE_PREFLIGHT, NULL,
                          NULL, status, detail);
    return status;
  }
  transaction_set_pending_target(manager, &pending_target);

  transaction_mark_public_stage(manager, LCC_TX_STAGE_CAPABILITY_GATE);
  status = transaction_capability_gate(manager, request, detail, sizeof(detail));
  if (status != LCC_OK) {
    transaction_fail(manager, operation_name, LCC_TX_STAGE_CAPABILITY_GATE,
                     NULL, &pending_target, status);
    log_transaction_event("error", operation_name, LCC_TX_STAGE_CAPABILITY_GATE,
                          NULL, &pending_target, status, detail);
    return status;
  }

  transaction_update_stage(manager, LCC_TX_STAGE_BACKEND_ROUTE);
  status = transaction_apply(manager, request, resolved_mode, &result);
  if (result.executor_backend[0] != '\0') {
    last_apply_update_backend(manager, result.executor_backend);
  }
  if (result.stage[0] != '\0') {
    last_apply_update_stage(manager, result.stage);
    transaction_update_stage(manager, result.stage);
  } else {
    last_apply_update_stage(manager, LCC_TX_STAGE_APPLY);
  }
  if (status != LCC_OK) {
    failure_stage = result.stage[0] != '\0'
                        ? result.stage
                        : (status == LCC_ERR_NOT_SUPPORTED
                               ? LCC_TX_STAGE_BACKEND_ROUTE
                               : LCC_TX_STAGE_APPLY);
    format_detail_summary(result.stage[0] != '\0' ? result.stage : NULL,
                          result.detail, detail, sizeof(detail));
    transaction_fail(manager, operation_name, failure_stage,
                     result.executor_backend, &pending_target, status);
    (void)lcc_transaction_refresh_state(manager);
    log_transaction_event(
        "error", operation_name,
        status == LCC_ERR_NOT_SUPPORTED ? LCC_TX_STAGE_BACKEND_ROUTE
                                        : LCC_TX_STAGE_APPLY,
        result.executor_backend, &pending_target, status, detail);
    return status;
  }

  last_apply_set_target(manager, &pending_target);
  last_apply_update_backend(manager, result.executor_backend);
  manager->state_cache.last_apply.error = LCC_OK;
  transaction_update_stage(manager, LCC_TX_STAGE_STATE_REFRESH);
  status = lcc_transaction_refresh_state(manager);
  if (status != LCC_OK) {
    format_detail_summary(result.stage[0] != '\0' ? result.stage : NULL,
                          "state refresh failed after backend apply", detail,
                          sizeof(detail));
    transaction_fail(manager, operation_name, LCC_TX_STAGE_STATE_REFRESH,
                     result.executor_backend, &pending_target, status);
    log_transaction_event("error", operation_name, LCC_TX_STAGE_STATE_REFRESH,
                          result.executor_backend, &pending_target, status,
                          detail);
    return status;
  }

  if (result.hardware_write) {
    manager->state_cache.hardware_write = true;
  }
  manager->state_cache.last_apply.error = LCC_OK;
  if (result.stage[0] == '\0') {
    last_apply_update_stage(manager, LCC_TX_STAGE_APPLY);
  }
  format_detail_summary(result.stage[0] != '\0' ? result.stage : NULL,
                        result.detail, detail, sizeof(detail));
  log_transaction_event("info", operation_name, LCC_TX_STAGE_COMPLETE,
                        result.executor_backend, &pending_target, LCC_OK,
                        detail);
  transaction_clear(manager);
  return LCC_OK;
}
