#include <assert.h>
#include <string.h>

#include "backends/backend.h"

void lcc_run_backend_mock_tests(void) {
  lcc_backend_t backend;
  lcc_mock_backend_t mock_backend;
  lcc_backend_capabilities_t capabilities;
  lcc_backend_result_t result;
  lcc_state_snapshot_t state;
  lcc_power_limits_t limits;

  assert(lcc_mock_backend_init(&mock_backend, &backend) == LCC_OK);

  memset(&capabilities, 0, sizeof(capabilities));
  assert(lcc_backend_probe(&backend, &capabilities, &result) == LCC_OK);
  assert(capabilities.can_read_state);
  assert(capabilities.can_apply_mode);
  assert(capabilities.can_apply_fan_boost);
  assert(!result.changed);

  memset(&state, 0, sizeof(state));
  assert(lcc_backend_read_state(&backend, &state, &result) == LCC_OK);
  assert(strcmp(state.backend_name, "mock") == 0);
  assert(strcmp(state.backend_selected, "mock") == 0);
  assert(strcmp(state.execution.apply_mode, "mock") == 0);
  assert(strcmp(state.requested.profile, "balanced") == 0);
  assert(strcmp(state.effective.fan_table, "M4T1") == 0);
  assert(state.requested.has_fan_boost);
  assert(!state.requested.fan_boost_enabled);

  assert(lcc_backend_apply_mode(&backend, LCC_MODE_TURBO, &result) == LCC_OK);
  assert(result.changed);
  assert(lcc_backend_read_state(&backend, &state, &result) == LCC_OK);
  assert(strcmp(state.effective.profile, "turbo") == 0);

  memset(&limits, 0, sizeof(limits));
  limits.pl1.present = true;
  limits.pl1.value = 70u;
  limits.pl2.present = true;
  limits.pl2.value = 120u;
  assert(lcc_backend_apply_power_limits(&backend, &limits, &result) == LCC_OK);
  assert(result.changed);
  assert(lcc_backend_read_state(&backend, &state, &result) == LCC_OK);
  assert(state.effective.has_power_limits);
  assert(state.effective.power_limits.pl1.value == 70u);
  assert(state.effective.power_limits.pl2.value == 120u);

  assert(lcc_backend_apply_fan_table(&backend, "demo-custom", &result) ==
         LCC_OK);
  assert(result.changed);
  assert(lcc_backend_read_state(&backend, &state, &result) == LCC_OK);
  assert(strcmp(state.requested.fan_table, "demo-custom") == 0);

  assert(lcc_backend_apply_fan_boost(&backend, true, &result) == LCC_OK);
  assert(result.changed);
  assert(lcc_backend_read_state(&backend, &state, &result) == LCC_OK);
  assert(state.effective.has_fan_boost);
  assert(state.effective.fan_boost_enabled);
}
