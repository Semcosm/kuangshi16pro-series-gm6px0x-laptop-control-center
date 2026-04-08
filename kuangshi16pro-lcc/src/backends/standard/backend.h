#ifndef LCC_BACKENDS_STANDARD_BACKEND_H
#define LCC_BACKENDS_STANDARD_BACKEND_H

#include <stdbool.h>

#include "backends/backend.h"

lcc_status_t lcc_standard_hwmon_probe(const lcc_standard_backend_t *standard,
                                      bool *available);
lcc_status_t lcc_standard_hwmon_read(const lcc_standard_backend_t *standard,
                                     lcc_state_snapshot_t *state);
lcc_status_t lcc_standard_thermal_probe(const lcc_standard_backend_t *standard,
                                        bool *available);
lcc_status_t lcc_standard_thermal_read(const lcc_standard_backend_t *standard,
                                       lcc_state_snapshot_t *state);
lcc_status_t lcc_standard_platform_profile_probe(
    const lcc_standard_backend_t *standard, bool *available);
lcc_status_t lcc_standard_platform_profile_read(
    const lcc_standard_backend_t *standard, lcc_state_snapshot_t *state);
lcc_status_t lcc_standard_platform_profile_apply_mode(
    const lcc_standard_backend_t *standard, lcc_operating_mode_t mode,
    lcc_backend_result_t *result);
lcc_status_t lcc_standard_platform_profile_apply_profile(
    const lcc_standard_backend_t *standard, const char *profile_name,
    lcc_backend_result_t *result);
lcc_status_t lcc_standard_powercap_probe(const lcc_standard_backend_t *standard,
                                         bool *available);
lcc_status_t lcc_standard_powercap_can_write(
    const lcc_standard_backend_t *standard, bool *writable);
lcc_status_t lcc_standard_powercap_read(const lcc_standard_backend_t *standard,
                                        lcc_state_snapshot_t *state);
lcc_status_t lcc_standard_powercap_apply(
    const lcc_standard_backend_t *standard, const lcc_power_limits_t *limits,
    lcc_backend_result_t *result);

#endif
