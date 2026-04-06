#include "backends/standard/backend.h"

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

static lcc_status_t standard_probe(void *ctx,
                                   lcc_backend_capabilities_t *capabilities,
                                   lcc_backend_result_t *result) {
  lcc_standard_backend_t *standard = ctx;
  bool hwmon_available = false;
  bool thermal_available = false;
  bool platform_profile_available = false;
  bool powercap_available = false;
  lcc_status_t status = LCC_OK;

  if (standard == NULL || capabilities == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  memset(capabilities, 0, sizeof(*capabilities));
  status = lcc_standard_hwmon_probe(standard, &hwmon_available);
  if (status != LCC_OK && status != LCC_ERR_NOT_FOUND) {
    return status;
  }
  status = lcc_standard_thermal_probe(standard, &thermal_available);
  if (status != LCC_OK && status != LCC_ERR_NOT_FOUND) {
    return status;
  }
  status = lcc_standard_platform_profile_probe(standard,
                                               &platform_profile_available);
  if (status != LCC_OK && status != LCC_ERR_NOT_FOUND) {
    return status;
  }
  status = lcc_standard_powercap_probe(standard, &powercap_available);
  if (status != LCC_OK && status != LCC_ERR_NOT_FOUND) {
    return status;
  }

  capabilities->can_read_state =
      hwmon_available || thermal_available || platform_profile_available ||
      powercap_available;
  capabilities->has_platform_profile = platform_profile_available;
  capabilities->has_powercap = powercap_available;
  capabilities->can_apply_mode = platform_profile_available;
  capabilities->can_apply_profile = platform_profile_available;
  capabilities->can_apply_power_limits = false;
  capabilities->can_apply_fan_table = false;
  capabilities->needs_reboot_for_mux = false;

  lcc_backend_result_reset(result);
  if (!capabilities->can_read_state) {
    return LCC_ERR_NOT_FOUND;
  }

  return LCC_OK;
}

static lcc_status_t standard_read_state(void *ctx, lcc_state_snapshot_t *state,
                                        lcc_backend_result_t *result) {
  lcc_standard_backend_t *standard = ctx;
  bool any_read = false;
  lcc_status_t status = LCC_OK;

  if (standard == NULL || state == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  memset(state, 0, sizeof(*state));
  status = copy_name(state->backend_name, sizeof(state->backend_name),
                     "standard");
  if (status != LCC_OK) {
    return status;
  }
  (void)copy_name(state->requested.fan_table, sizeof(state->requested.fan_table),
                  "system-default");
  (void)copy_name(state->effective.fan_table, sizeof(state->effective.fan_table),
                  "system-default");

  status = lcc_standard_platform_profile_read(standard, state);
  if (status == LCC_OK) {
    any_read = true;
  } else if (status != LCC_ERR_NOT_FOUND && status != LCC_ERR_NOT_SUPPORTED) {
    return status;
  }

  status = lcc_standard_hwmon_read(standard, state);
  if (status == LCC_OK) {
    any_read = true;
  } else if (status != LCC_ERR_NOT_FOUND && status != LCC_ERR_NOT_SUPPORTED) {
    return status;
  }

  status = lcc_standard_thermal_read(standard, state);
  if (status == LCC_OK) {
    any_read = true;
  } else if (status != LCC_ERR_NOT_FOUND && status != LCC_ERR_NOT_SUPPORTED) {
    return status;
  }

  status = lcc_standard_powercap_read(standard, state);
  if (status == LCC_OK) {
    any_read = true;
  } else if (status != LCC_ERR_NOT_FOUND && status != LCC_ERR_NOT_SUPPORTED) {
    return status;
  }

  lcc_backend_result_reset(result);
  if (!any_read) {
    return LCC_ERR_NOT_FOUND;
  }

  return LCC_OK;
}

static lcc_status_t standard_apply_profile(void *ctx, const char *profile_name,
                                           lcc_backend_result_t *result) {
  return lcc_standard_platform_profile_apply_profile(ctx, profile_name, result);
}

static lcc_status_t standard_apply_mode(void *ctx, lcc_operating_mode_t mode,
                                        lcc_backend_result_t *result) {
  return lcc_standard_platform_profile_apply_mode(ctx, mode, result);
}

static lcc_status_t standard_apply_power_limits(void *ctx,
                                                const lcc_power_limits_t *limits,
                                                lcc_backend_result_t *result) {
  (void)ctx;
  (void)limits;
  lcc_backend_result_reset(result);
  return LCC_ERR_NOT_SUPPORTED;
}

static lcc_status_t standard_apply_fan_table(void *ctx, const char *table_name,
                                             lcc_backend_result_t *result) {
  (void)ctx;
  (void)table_name;
  lcc_backend_result_reset(result);
  return LCC_ERR_NOT_SUPPORTED;
}

const lcc_backend_ops_t lcc_standard_backend_ops = {
    .name = "standard",
    .kind = LCC_BACKEND_STANDARD,
    .probe = standard_probe,
    .read_state = standard_read_state,
    .apply_profile = standard_apply_profile,
    .apply_mode = standard_apply_mode,
    .apply_power_limits = standard_apply_power_limits,
    .apply_fan_table = standard_apply_fan_table,
};

lcc_status_t lcc_standard_backend_init_at_root(lcc_standard_backend_t *standard,
                                               lcc_backend_t *backend,
                                               const char *root) {
  int written = 0;

  if (standard == NULL || backend == NULL || root == NULL || root[0] == '\0') {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  memset(standard, 0, sizeof(*standard));
  written = snprintf(standard->root, sizeof(standard->root), "%s", root);
  if (written < 0 || (size_t)written >= sizeof(standard->root)) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }
  (void)snprintf(standard->hwmon_dir, sizeof(standard->hwmon_dir), "%s/class/hwmon",
                 standard->root);
  (void)snprintf(standard->thermal_dir, sizeof(standard->thermal_dir),
                 "%s/class/thermal", standard->root);
  (void)snprintf(standard->platform_profile_path,
                 sizeof(standard->platform_profile_path),
                 "%s/firmware/acpi/platform_profile", standard->root);
  (void)snprintf(standard->powercap_dir, sizeof(standard->powercap_dir),
                 "%s/class/powercap", standard->root);

  lcc_backend_bind(backend, &lcc_standard_backend_ops, standard);
  return LCC_OK;
}

lcc_status_t lcc_standard_backend_init(lcc_standard_backend_t *standard,
                                       lcc_backend_t *backend) {
  return lcc_standard_backend_init_at_root(standard, backend, "/sys");
}
