#include <assert.h>
#include <string.h>

#include "backends/backend.h"
#include "core/state/model.h"
#include "core/state/reader.h"

static void test_state_render_json_includes_last_apply_backend(void) {
  lcc_state_snapshot_t state;
  lcc_backend_capabilities_t capabilities;
  char json[4096];

  memset(&state, 0, sizeof(state));
  memset(&capabilities, 0, sizeof(capabilities));
  assert(lcc_backend_copy_text(state.backend_name, sizeof(state.backend_name),
                               "standard") == LCC_OK);
  assert(lcc_backend_copy_text(state.backend_selected,
                               sizeof(state.backend_selected),
                               "standard") == LCC_OK);
  assert(lcc_backend_copy_text(state.backend_fallback_reason,
                               sizeof(state.backend_fallback_reason),
                               "amw0 handles apply_fan_table because standard backend does not support them") ==
         LCC_OK);
  assert(lcc_backend_execution_set(&state.execution, "standard", "standard",
                                   "standard", "amw0", "amw0") == LCC_OK);
  assert(lcc_backend_copy_text(state.requested.profile,
                               sizeof(state.requested.profile),
                               "turbo") == LCC_OK);
  assert(lcc_backend_copy_text(state.effective.profile,
                               sizeof(state.effective.profile),
                               "turbo") == LCC_OK);
  assert(lcc_backend_copy_text(state.requested.fan_table,
                               sizeof(state.requested.fan_table),
                               "system-default") == LCC_OK);
  assert(lcc_backend_copy_text(state.effective.fan_table,
                               sizeof(state.effective.fan_table),
                               "system-default") == LCC_OK);
  assert(lcc_backend_copy_text(state.last_apply.stage,
                               sizeof(state.last_apply.stage),
                               "write-platform-profile") == LCC_OK);
  assert(lcc_backend_copy_text(state.last_apply.backend,
                               sizeof(state.last_apply.backend),
                               "standard") == LCC_OK);
  state.last_apply.has_target = true;
  assert(lcc_backend_copy_text(state.last_apply.target.profile,
                               sizeof(state.last_apply.target.profile),
                               "turbo") == LCC_OK);
  assert(lcc_backend_copy_text(state.last_apply.target.fan_table,
                               sizeof(state.last_apply.target.fan_table),
                               "system-default") == LCC_OK);

  capabilities.can_read_state = true;
  capabilities.can_apply_mode = true;
  capabilities.can_apply_profile = true;
  capabilities.can_apply_power_limits = true;
  capabilities.can_apply_fan_table = true;
  capabilities.has_platform_profile = true;
  capabilities.has_powercap = true;

  assert(lcc_state_render_json(&state, &capabilities, json, sizeof(json)) ==
         LCC_OK);
  assert(strstr(json, "\"backend\":\"standard\"") != NULL);
  assert(strstr(json, "\"execution\":{\"read_state\":\"standard\"") != NULL);
  assert(strstr(json, "\"last_apply_stage\":\"write-platform-profile\"") !=
         NULL);
  assert(strstr(json, "\"last_apply_backend\":\"standard\"") != NULL);
}

static void test_state_reader_refresh_preserves_diagnostic_snapshots(void) {
  lcc_backend_t backend;
  lcc_mock_backend_t mock_backend;
  lcc_state_snapshot_t state;
  lcc_state_target_t target;

  assert(lcc_mock_backend_init(&mock_backend, &backend) == LCC_OK);
  memset(&state, 0, sizeof(state));
  memset(&target, 0, sizeof(target));
  assert(lcc_backend_copy_text(state.backend_selected,
                               sizeof(state.backend_selected),
                               "standard") == LCC_OK);
  assert(lcc_backend_copy_text(state.backend_fallback_reason,
                               sizeof(state.backend_fallback_reason),
                               "amw0 handles apply_power_limits because standard backend does not support them") ==
         LCC_OK);
  assert(lcc_backend_execution_set(&state.execution, "standard", "standard",
                                   "standard", "amw0", "amw0") == LCC_OK);
  assert(lcc_backend_copy_text(state.last_apply.stage,
                               sizeof(state.last_apply.stage),
                               "write-pl1") == LCC_OK);
  assert(lcc_backend_copy_text(state.last_apply.backend,
                               sizeof(state.last_apply.backend),
                               "amw0") == LCC_OK);
  state.last_apply.has_target = true;
  assert(lcc_backend_copy_text(target.profile, sizeof(target.profile),
                               "turbo") == LCC_OK);
  state.last_apply.target = target;
  state.transaction.state = LCC_TRANSACTION_STATE_FAILED;
  assert(lcc_backend_copy_text(state.transaction.operation,
                               sizeof(state.transaction.operation),
                               "set-power-limits") == LCC_OK);
  assert(lcc_backend_copy_text(state.transaction.stage,
                               sizeof(state.transaction.stage),
                               "write-pl1") == LCC_OK);
  state.transaction.has_pending_target = true;
  state.transaction.pending_target = target;
  state.transaction.last_error = LCC_ERR_IO;

  assert(lcc_state_reader_refresh(&backend, &state) == LCC_OK);
  assert(strcmp(state.backend_name, "mock") == 0);
  assert(strcmp(state.backend_selected, "mock") == 0);
  assert(strcmp(state.last_apply.stage, "write-pl1") == 0);
  assert(strcmp(state.last_apply.backend, "amw0") == 0);
  assert(state.last_apply.has_target);
  assert(strcmp(state.last_apply.target.profile, "turbo") == 0);
  assert(state.transaction.state == LCC_TRANSACTION_STATE_FAILED);
  assert(strcmp(state.transaction.stage, "write-pl1") == 0);
  assert(state.transaction.last_error == LCC_ERR_IO);
}

void lcc_run_state_model_tests(void) {
  test_state_render_json_includes_last_apply_backend();
  test_state_reader_refresh_preserves_diagnostic_snapshots();
}
