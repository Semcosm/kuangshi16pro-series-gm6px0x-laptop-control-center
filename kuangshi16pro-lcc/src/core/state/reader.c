#include "core/state/reader.h"

#include <stdio.h>
#include <string.h>

static lcc_status_t copy_name(char *buffer, size_t buffer_len,
                              const char *value) {
  const int written = snprintf(buffer, buffer_len, "%s", value);

  if (buffer == NULL || value == NULL || value[0] == '\0') {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (written < 0 || (size_t)written >= buffer_len) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }
  return LCC_OK;
}

lcc_status_t lcc_state_reader_refresh(const lcc_backend_t *backend,
                                      lcc_state_snapshot_t *state) {
  lcc_backend_result_t result;
  lcc_transaction_snapshot_t transaction;
  lcc_last_apply_snapshot_t last_apply;
  lcc_execution_snapshot_t execution;
  char backend_selected[LCC_STATE_BACKEND_NAME_MAX];
  char backend_fallback_reason[LCC_STATE_REASON_MAX];
  lcc_status_t status = LCC_OK;

  if (backend == NULL || state == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  transaction = state->transaction;
  last_apply = state->last_apply;
  execution = state->execution;
  memcpy(backend_selected, state->backend_selected, sizeof(backend_selected));
  memcpy(backend_fallback_reason, state->backend_fallback_reason,
         sizeof(backend_fallback_reason));
  memset(state, 0, sizeof(*state));
  status = lcc_backend_read_state(backend, state, &result);
  if (status != LCC_OK) {
    return status;
  }

  if (state->backend_name[0] == '\0') {
    status = copy_name(state->backend_name, sizeof(state->backend_name),
                       lcc_backend_name(backend));
    if (status != LCC_OK) {
      return status;
    }
  }
  if (result.hardware_write) {
    state->hardware_write = true;
  }
  state->transaction = transaction;
  state->last_apply = last_apply;
  if (state->backend_selected[0] == '\0' && backend_selected[0] != '\0') {
    (void)copy_name(state->backend_selected, sizeof(state->backend_selected),
                    backend_selected);
  }
  if (state->backend_fallback_reason[0] == '\0' &&
      backend_fallback_reason[0] != '\0') {
    (void)copy_name(state->backend_fallback_reason,
                    sizeof(state->backend_fallback_reason),
                    backend_fallback_reason);
  }
  if (state->execution.read_state[0] == '\0' && execution.read_state[0] != '\0') {
    state->execution = execution;
  }

  return LCC_OK;
}
