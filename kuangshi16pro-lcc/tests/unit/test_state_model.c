#include <assert.h>
#include <string.h>

#include "backends/backend.h"
#include "core/state/model.h"
#include "core/state/reader.h"

typedef struct {
  lcc_status_t read_state_status;
  lcc_state_snapshot_t state;
} lcc_state_reader_failure_backend_t;

static lcc_status_t state_reader_failure_probe(
    void *ctx, lcc_backend_capabilities_t *capabilities,
    lcc_backend_result_t *result) {
  (void)ctx;

  assert(capabilities != NULL);
  memset(capabilities, 0, sizeof(*capabilities));
  capabilities->can_read_state = true;
  lcc_backend_result_reset(result);
  lcc_backend_result_set_executor(result, "failing-reader");
  return LCC_OK;
}

static lcc_status_t state_reader_failure_read_state(void *ctx,
                                                    lcc_state_snapshot_t *state,
                                                    lcc_backend_result_t *result) {
  lcc_state_reader_failure_backend_t *backend = ctx;

  assert(backend != NULL);
  assert(state != NULL);
  lcc_backend_result_reset(result);
  lcc_backend_result_set_executor(result, "failing-reader");
  if (backend->read_state_status != LCC_OK) {
    lcc_backend_result_set_detail(result, "state refresh intentionally failed");
    return backend->read_state_status;
  }

  *state = backend->state;
  return LCC_OK;
}

static const lcc_backend_ops_t state_reader_failure_backend_ops = {
    .name = "failing-reader",
    .kind = LCC_BACKEND_MOCK,
    .probe = state_reader_failure_probe,
    .read_state = state_reader_failure_read_state,
    .apply_profile = NULL,
    .apply_mode = NULL,
    .apply_power_limits = NULL,
    .apply_fan_table = NULL,
};

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
                                   "standard", "amw0", "amw0", "amw0") ==
         LCC_OK);
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
  state.last_apply.has_hardware_write = true;
  state.last_apply.hardware_write = true;
  (void)lcc_backend_effective_component_set(&state.effective_meta.profile,
                                            "standard", "live");
  (void)lcc_backend_effective_component_set(&state.effective_meta.fan_table,
                                            "cache", "cache");
  lcc_backend_state_finalize_effective_meta(&state);
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
  assert(strstr(json, "\"last_apply_hardware_write\":true") != NULL);
  assert(strstr(json, "\"effective_meta\":{\"source\":\"mixed\",\"freshness\":\"mixed\"") !=
         NULL);
}

static void test_state_render_json_renders_unknown_power_fields_as_null(void) {
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
  assert(lcc_backend_execution_set(&state.execution, "standard", "amw0", "amw0",
                                   "amw0", "amw0", "amw0") == LCC_OK);
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
  state.requested.has_power_limits = true;
  state.requested.power_limits.pl1.present = true;
  state.requested.power_limits.pl1.value = 55u;
  state.effective.has_power_limits = true;
  state.effective.power_limits.pl1.present = true;
  state.effective.power_limits.pl1.value = 55u;
  (void)lcc_backend_effective_component_set(&state.effective_meta.fan_table,
                                            "cache", "cache");
  lcc_backend_effective_power_set_from_limits(&state.effective_meta,
                                              &state.effective.power_limits,
                                              "standard", "live");
  lcc_backend_state_finalize_effective_meta(&state);

  capabilities.can_read_state = true;
  capabilities.can_apply_profile = true;
  capabilities.can_apply_mode = true;
  capabilities.can_apply_power_limits = true;
  capabilities.can_apply_fan_table = true;

  assert(lcc_state_render_json(&state, &capabilities, json, sizeof(json)) ==
         LCC_OK);
  assert(strstr(json,
                "\"power\":{\"pl1\":55,\"pl2\":null,\"pl4\":null,\"tcc_offset\":null}") !=
         NULL);
  assert(strstr(json,
                "\"power\":{\"source\":\"standard\",\"freshness\":\"live\",\"fields\":{\"pl1\":{\"source\":\"standard\",\"freshness\":\"live\"},\"pl2\":{\"source\":null,\"freshness\":null},\"pl4\":{\"source\":null,\"freshness\":null},\"tcc_offset\":{\"source\":null,\"freshness\":null}}}") !=
         NULL);
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
                                   "standard", "amw0", "amw0", "amw0") ==
         LCC_OK);
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
  assert(strcmp(state.effective_meta.source, "mock") == 0);
  assert(strcmp(state.effective_meta.freshness, "live") == 0);
  assert(strcmp(state.last_apply.stage, "write-pl1") == 0);
  assert(strcmp(state.last_apply.backend, "amw0") == 0);
  assert(state.last_apply.has_target);
  assert(strcmp(state.last_apply.target.profile, "turbo") == 0);
  assert(state.transaction.state == LCC_TRANSACTION_STATE_FAILED);
  assert(strcmp(state.transaction.stage, "write-pl1") == 0);
  assert(state.transaction.last_error == LCC_ERR_IO);
}

static void test_state_reader_refresh_marks_cached_values_on_failure(void) {
  lcc_backend_t backend;
  lcc_state_reader_failure_backend_t failure_backend;
  lcc_state_snapshot_t state;

  memset(&failure_backend, 0, sizeof(failure_backend));
  failure_backend.read_state_status = LCC_ERR_IO;
  lcc_backend_bind(&backend, &state_reader_failure_backend_ops, &failure_backend);

  memset(&state, 0, sizeof(state));
  assert(lcc_backend_copy_text(state.backend_name, sizeof(state.backend_name),
                               "mock") == LCC_OK);
  assert(lcc_backend_copy_text(state.effective.profile,
                               sizeof(state.effective.profile),
                               "turbo") == LCC_OK);
  assert(lcc_backend_copy_text(state.effective.fan_table,
                               sizeof(state.effective.fan_table),
                               "M4T1") == LCC_OK);
  state.effective.has_power_limits = true;
  state.effective.power_limits.pl1.present = true;
  state.effective.power_limits.pl1.value = 55u;
  state.thermal.has_cpu_temp_c = true;
  state.thermal.cpu_temp_c = 61u;
  (void)lcc_backend_effective_component_set(&state.effective_meta.profile,
                                            "mock", "live");
  (void)lcc_backend_effective_component_set(&state.effective_meta.fan_table,
                                            "mock", "live");
  lcc_backend_effective_power_set_from_limits(&state.effective_meta,
                                              &state.effective.power_limits,
                                              "mock", "live");
  (void)lcc_backend_effective_component_set(&state.effective_meta.thermal,
                                            "mock", "live");
  lcc_backend_state_finalize_effective_meta(&state);

  assert(lcc_state_reader_refresh(&backend, &state) == LCC_ERR_IO);
  assert(strcmp(state.effective.profile, "turbo") == 0);
  assert(strcmp(state.effective.fan_table, "M4T1") == 0);
  assert(state.effective.has_power_limits);
  assert(state.thermal.has_cpu_temp_c);
  assert(strcmp(state.effective_meta.source, "cache") == 0);
  assert(strcmp(state.effective_meta.freshness, "cache") == 0);
  assert(strcmp(state.effective_meta.profile.source, "cache") == 0);
  assert(strcmp(state.effective_meta.power.freshness, "cache") == 0);
  assert(strcmp(state.effective_meta.power_fields.pl1.source, "cache") == 0);
  assert(strcmp(state.effective_meta.power_fields.pl1.freshness, "cache") == 0);
}

static void test_state_render_json_includes_vendor_fan_level(void) {
  lcc_state_snapshot_t state;
  lcc_backend_capabilities_t capabilities;
  char json[2048];

  memset(&state, 0, sizeof(state));
  memset(&capabilities, 0, sizeof(capabilities));
  capabilities.can_read_state = true;
  assert(lcc_backend_copy_text(state.backend_name, sizeof(state.backend_name),
                               "amw0") == LCC_OK);
  assert(lcc_backend_copy_text(state.backend_selected,
                               sizeof(state.backend_selected),
                               "standard") == LCC_OK);
  assert(lcc_backend_copy_text(state.requested.profile,
                               sizeof(state.requested.profile),
                               "turbo") == LCC_OK);
  assert(lcc_backend_copy_text(state.effective.profile,
                               sizeof(state.effective.profile),
                               "turbo") == LCC_OK);
  state.thermal.has_vendor_fan_level = true;
  state.thermal.vendor_fan_level = 10u;
  (void)lcc_backend_effective_component_set(&state.effective_meta.thermal,
                                            "amw0", "live");
  lcc_backend_state_finalize_effective_meta(&state);

  assert(lcc_state_render_json(&state, &capabilities, json, sizeof(json)) ==
         LCC_OK);
  assert(strstr(json, "\"vendor_fan_level\":10") != NULL);
}

static void test_state_render_json_includes_fan_boost(void) {
  lcc_state_snapshot_t state;
  lcc_backend_capabilities_t capabilities;
  char json[2048];

  memset(&state, 0, sizeof(state));
  memset(&capabilities, 0, sizeof(capabilities));
  capabilities.can_read_state = true;
  capabilities.can_apply_fan_boost = true;
  state.requested.has_fan_boost = true;
  state.requested.fan_boost_enabled = true;
  state.effective.has_fan_boost = true;
  state.effective.fan_boost_enabled = true;
  (void)lcc_backend_effective_component_set(&state.effective_meta.fan_boost,
                                            "amw0", "live");
  lcc_backend_state_finalize_effective_meta(&state);

  assert(lcc_state_render_json(&state, &capabilities, json, sizeof(json)) ==
         LCC_OK);
  assert(strstr(json, "\"apply_fan_boost\":true") != NULL);
  assert(strstr(json, "\"fan_boost\":true") != NULL);
  assert(strstr(json, "\"fan_boost\":{\"source\":\"amw0\",\"freshness\":\"live\"}") !=
         NULL);
}

void lcc_run_state_model_tests(void) {
  test_state_render_json_includes_last_apply_backend();
  test_state_render_json_renders_unknown_power_fields_as_null();
  test_state_render_json_includes_vendor_fan_level();
  test_state_render_json_includes_fan_boost();
  test_state_reader_refresh_preserves_diagnostic_snapshots();
  test_state_reader_refresh_marks_cached_values_on_failure();
}
