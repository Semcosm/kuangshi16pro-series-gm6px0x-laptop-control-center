#ifndef LCC_BACKENDS_BACKEND_H
#define LCC_BACKENDS_BACKEND_H

#include "lcc/backend.h"

typedef struct {
  lcc_backend_capabilities_t capabilities;
  lcc_state_snapshot_t state;
  lcc_status_t fail_profile_status;
  lcc_status_t fail_mode_status;
  lcc_status_t fail_power_status;
  lcc_status_t fail_fan_status;
} lcc_mock_backend_t;

void lcc_mock_backend_seed_defaults(lcc_mock_backend_t *mock);
lcc_status_t lcc_mock_backend_init(lcc_mock_backend_t *mock,
                                   lcc_backend_t *backend);
void lcc_mock_backend_fail_next_profile(lcc_mock_backend_t *mock,
                                        lcc_status_t status);
void lcc_mock_backend_fail_next_mode(lcc_mock_backend_t *mock,
                                     lcc_status_t status);
void lcc_mock_backend_fail_next_power(lcc_mock_backend_t *mock,
                                      lcc_status_t status);
void lcc_mock_backend_fail_next_fan(lcc_mock_backend_t *mock,
                                    lcc_status_t status);

typedef struct {
  char root[256];
  char hwmon_dir[256];
  char thermal_dir[256];
  char platform_profile_path[256];
  char powercap_dir[256];
} lcc_standard_backend_t;

lcc_status_t lcc_standard_backend_init(lcc_standard_backend_t *standard,
                                       lcc_backend_t *backend);
lcc_status_t lcc_standard_backend_init_at_root(lcc_standard_backend_t *standard,
                                               lcc_backend_t *backend,
                                               const char *root);

extern const lcc_backend_ops_t lcc_mock_backend_ops;
extern const lcc_backend_ops_t lcc_standard_backend_ops;

#endif
