#include "core/state/reader.h"

#include <stdio.h>
#include <string.h>

#include "backends/backend.h"

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
  lcc_state_snapshot_t previous;
  lcc_state_snapshot_t refreshed;
  lcc_transaction_snapshot_t transaction;
  lcc_last_apply_snapshot_t last_apply;
  lcc_execution_snapshot_t execution;
  char backend_selected[LCC_STATE_BACKEND_NAME_MAX];
  char backend_fallback_reason[LCC_STATE_REASON_MAX];
  lcc_status_t status = LCC_OK;

  if (backend == NULL || state == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  previous = *state;
  transaction = state->transaction;
  last_apply = state->last_apply;
  execution = state->execution;
  memcpy(backend_selected, state->backend_selected, sizeof(backend_selected));
  memcpy(backend_fallback_reason, state->backend_fallback_reason,
         sizeof(backend_fallback_reason));
  memset(&refreshed, 0, sizeof(refreshed));
  status = lcc_backend_read_state(backend, &refreshed, &result);
  if (status != LCC_OK) {
    *state = previous;
    lcc_backend_state_mark_effective_cached(state);
    return status;
  }

  if (refreshed.backend_name[0] == '\0') {
    status = copy_name(refreshed.backend_name, sizeof(refreshed.backend_name),
                       lcc_backend_name(backend));
    if (status != LCC_OK) {
      return status;
    }
  }
  if (result.hardware_write) {
    refreshed.hardware_write = true;
  }
  refreshed.transaction = transaction;
  refreshed.last_apply = last_apply;
  if (refreshed.backend_selected[0] == '\0' && backend_selected[0] != '\0') {
    (void)copy_name(refreshed.backend_selected,
                    sizeof(refreshed.backend_selected),
                    backend_selected);
  }
  if (refreshed.backend_fallback_reason[0] == '\0' &&
      backend_fallback_reason[0] != '\0') {
    (void)copy_name(refreshed.backend_fallback_reason,
                    sizeof(refreshed.backend_fallback_reason),
                    backend_fallback_reason);
  }
  if (refreshed.execution.read_state[0] == '\0' &&
      execution.read_state[0] != '\0') {
    refreshed.execution = execution;
  }
  *state = refreshed;

  return LCC_OK;
}
