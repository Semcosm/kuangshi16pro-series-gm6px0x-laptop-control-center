#include "backends/backend.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static lcc_status_t copy_name(char *buffer, size_t buffer_len,
                              const char *value) {
  const int written = snprintf(buffer, buffer_len, "%s", value);

  if (written < 0 || (size_t)written >= buffer_len) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return LCC_OK;
}

static const char *mode_name_for_backend(lcc_operating_mode_t mode) {
  switch (mode) {
    case LCC_MODE_GAMING:
      return "gaming";
    case LCC_MODE_OFFICE:
      return "office";
    case LCC_MODE_TURBO:
      return "turbo";
    case LCC_MODE_CUSTOM:
      return "custom";
  }

  return NULL;
}

static lcc_status_t consume_failure(lcc_status_t *status_slot) {
  lcc_status_t status = LCC_OK;

  if (status_slot == NULL) {
    return LCC_OK;
  }

  status = *status_slot;
  *status_slot = LCC_OK;
  return status;
}

static bool power_limits_equal(const lcc_power_limits_t *left,
                               const lcc_power_limits_t *right) {
  return left->pl1.present == right->pl1.present &&
         left->pl1.value == right->pl1.value &&
         left->pl2.present == right->pl2.present &&
         left->pl2.value == right->pl2.value &&
         left->pl4.present == right->pl4.present &&
         left->pl4.value == right->pl4.value &&
         left->tcc_offset.present == right->tcc_offset.present &&
         left->tcc_offset.value == right->tcc_offset.value;
}

static void merge_optional_byte(lcc_optional_byte_t *target,
                                lcc_optional_byte_t source) {
  if (target != NULL && source.present) {
    *target = source;
  }
}

static lcc_status_t mock_probe(void *ctx,
                               lcc_backend_capabilities_t *capabilities,
                               lcc_backend_result_t *result) {
  lcc_mock_backend_t *mock = ctx;

  if (mock == NULL || capabilities == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  *capabilities = mock->capabilities;
  lcc_backend_result_reset(result);
  lcc_backend_result_set_executor(result, "mock");
  return LCC_OK;
}

static lcc_status_t mock_read_state(void *ctx, lcc_state_snapshot_t *state,
                                    lcc_backend_result_t *result) {
  lcc_mock_backend_t *mock = ctx;

  if (mock == NULL || state == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  *state = mock->state;
  (void)lcc_backend_state_set_metadata(state, "mock", "mock", NULL,
                                       &mock->state.execution);
  lcc_backend_result_reset(result);
  lcc_backend_result_set_executor(result, "mock");
  return LCC_OK;
}

static lcc_status_t mock_apply_profile(void *ctx, const char *profile_name,
                                       lcc_backend_result_t *result) {
  lcc_mock_backend_t *mock = ctx;
  bool changed = false;
  lcc_status_t status = LCC_OK;

  if (mock == NULL || profile_name == NULL || profile_name[0] == '\0') {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  lcc_backend_result_reset(result);
  lcc_backend_result_set_executor(result, "mock");
  status = consume_failure(&mock->fail_profile_status);
  if (status != LCC_OK) {
    lcc_backend_result_set_detail(result, "mock apply_profile failed");
    return status;
  }

  changed = strcmp(mock->state.requested.profile, profile_name) != 0 ||
            strcmp(mock->state.effective.profile, profile_name) != 0;
  status = copy_name(mock->state.requested.profile,
                     sizeof(mock->state.requested.profile), profile_name);
  if (status != LCC_OK) {
    return status;
  }
  status = copy_name(mock->state.effective.profile,
                     sizeof(mock->state.effective.profile), profile_name);
  if (status != LCC_OK) {
    return status;
  }

  if (result != NULL) {
    result->changed = changed;
  }
  return LCC_OK;
}

static lcc_status_t mock_apply_mode(void *ctx, lcc_operating_mode_t mode,
                                    lcc_backend_result_t *result) {
  lcc_mock_backend_t *mock = ctx;
  const char *profile_name = mode_name_for_backend(mode);
  lcc_status_t status = LCC_OK;

  if (mock == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  if (profile_name == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  lcc_backend_result_reset(result);
  lcc_backend_result_set_executor(result, "mock");
  status = consume_failure(&mock->fail_mode_status);
  if (status != LCC_OK) {
    lcc_backend_result_set_detail(result, "mock apply_mode failed");
    return status;
  }

  return mock_apply_profile(ctx, profile_name, result);
}

static lcc_status_t mock_apply_power_limits(void *ctx,
                                            const lcc_power_limits_t *limits,
                                            lcc_backend_result_t *result) {
  lcc_mock_backend_t *mock = ctx;
  lcc_power_limits_t merged;
  bool changed = false;
  lcc_status_t status = LCC_OK;

  if (mock == NULL || limits == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  lcc_backend_result_reset(result);
  lcc_backend_result_set_executor(result, "mock");
  status = consume_failure(&mock->fail_power_status);
  if (status != LCC_OK) {
    lcc_backend_result_set_detail(result, "mock apply_power_limits failed");
    return status;
  }
  if (!limits->pl1.present && !limits->pl2.present && !limits->pl4.present &&
      !limits->tcc_offset.present) {
    lcc_backend_result_set_detail(result, "mock apply_power_limits requires at least one limit");
    return LCC_ERR_INVALID_ARGUMENT;
  }

  merged = mock->state.requested.power_limits;
  merge_optional_byte(&merged.pl1, limits->pl1);
  merge_optional_byte(&merged.pl2, limits->pl2);
  merge_optional_byte(&merged.pl4, limits->pl4);
  merge_optional_byte(&merged.tcc_offset, limits->tcc_offset);
  changed = !mock->state.requested.has_power_limits ||
            !power_limits_equal(&mock->state.requested.power_limits, &merged);

  mock->state.requested.power_limits = merged;
  mock->state.effective.power_limits = merged;
  mock->state.requested.has_power_limits = true;
  mock->state.effective.has_power_limits = true;

  if (result != NULL) {
    result->changed = changed;
  }
  return LCC_OK;
}

static lcc_status_t mock_apply_fan_table(void *ctx, const char *table_name,
                                         lcc_backend_result_t *result) {
  lcc_mock_backend_t *mock = ctx;
  bool changed = false;
  lcc_status_t status = LCC_OK;

  if (mock == NULL || table_name == NULL || table_name[0] == '\0') {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  lcc_backend_result_reset(result);
  lcc_backend_result_set_executor(result, "mock");
  status = consume_failure(&mock->fail_fan_status);
  if (status != LCC_OK) {
    lcc_backend_result_set_detail(result, "mock apply_fan_table failed");
    return status;
  }

  changed = strcmp(mock->state.requested.fan_table, table_name) != 0 ||
            strcmp(mock->state.effective.fan_table, table_name) != 0;
  status = copy_name(mock->state.requested.fan_table,
                     sizeof(mock->state.requested.fan_table), table_name);
  if (status != LCC_OK) {
    return status;
  }
  status = copy_name(mock->state.effective.fan_table,
                     sizeof(mock->state.effective.fan_table), table_name);
  if (status != LCC_OK) {
    return status;
  }

  if (result != NULL) {
    result->changed = changed;
  }
  return LCC_OK;
}

const lcc_backend_ops_t lcc_mock_backend_ops = {
    .name = "mock",
    .kind = LCC_BACKEND_MOCK,
    .probe = mock_probe,
    .read_state = mock_read_state,
    .apply_profile = mock_apply_profile,
    .apply_mode = mock_apply_mode,
    .apply_power_limits = mock_apply_power_limits,
    .apply_fan_table = mock_apply_fan_table,
};

void lcc_mock_backend_seed_defaults(lcc_mock_backend_t *mock) {
  if (mock == NULL) {
    return;
  }

  memset(mock, 0, sizeof(*mock));
  mock->capabilities.can_read_state = true;
  mock->capabilities.can_apply_profile = true;
  mock->capabilities.can_apply_mode = true;
  mock->capabilities.can_apply_power_limits = true;
  mock->capabilities.can_apply_fan_table = true;

  (void)copy_name(mock->state.backend_name, sizeof(mock->state.backend_name),
                  "mock");
  (void)copy_name(mock->state.backend_selected,
                  sizeof(mock->state.backend_selected), "mock");
  (void)lcc_backend_execution_set_all(&mock->state.execution, "mock");
  (void)copy_name(mock->state.requested.profile,
                  sizeof(mock->state.requested.profile), "balanced");
  (void)copy_name(mock->state.effective.profile,
                  sizeof(mock->state.effective.profile), "balanced");
  (void)copy_name(mock->state.requested.fan_table,
                  sizeof(mock->state.requested.fan_table), "M4T1");
  (void)copy_name(mock->state.effective.fan_table,
                  sizeof(mock->state.effective.fan_table), "M4T1");

  mock->state.requested.has_power_limits = true;
  mock->state.effective.has_power_limits = true;
  mock->state.requested.power_limits.pl1.present = true;
  mock->state.requested.power_limits.pl1.value = 55u;
  mock->state.requested.power_limits.pl2.present = true;
  mock->state.requested.power_limits.pl2.value = 95u;
  mock->state.requested.power_limits.pl4.present = true;
  mock->state.requested.power_limits.pl4.value = 125u;
  mock->state.requested.power_limits.tcc_offset.present = true;
  mock->state.requested.power_limits.tcc_offset.value = 10u;
  mock->state.effective.power_limits = mock->state.requested.power_limits;
  mock->state.thermal.has_cpu_temp_c = true;
  mock->state.thermal.cpu_temp_c = 61u;
  mock->state.thermal.has_gpu_temp_c = true;
  mock->state.thermal.gpu_temp_c = 56u;
  mock->state.thermal.has_cpu_fan_rpm = true;
  mock->state.thermal.cpu_fan_rpm = 2480u;
  mock->state.thermal.has_gpu_fan_rpm = true;
  mock->state.thermal.gpu_fan_rpm = 2310u;
}

lcc_status_t lcc_mock_backend_init(lcc_mock_backend_t *mock,
                                   lcc_backend_t *backend) {
  if (mock == NULL || backend == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  lcc_mock_backend_seed_defaults(mock);
  lcc_backend_bind(backend, &lcc_mock_backend_ops, mock);
  return LCC_OK;
}

void lcc_mock_backend_fail_next_profile(lcc_mock_backend_t *mock,
                                        lcc_status_t status) {
  if (mock != NULL) {
    mock->fail_profile_status = status;
  }
}

void lcc_mock_backend_fail_next_mode(lcc_mock_backend_t *mock,
                                     lcc_status_t status) {
  if (mock != NULL) {
    mock->fail_mode_status = status;
  }
}

void lcc_mock_backend_fail_next_power(lcc_mock_backend_t *mock,
                                      lcc_status_t status) {
  if (mock != NULL) {
    mock->fail_power_status = status;
  }
}

void lcc_mock_backend_fail_next_fan(lcc_mock_backend_t *mock,
                                    lcc_status_t status) {
  if (mock != NULL) {
    mock->fail_fan_status = status;
  }
}
