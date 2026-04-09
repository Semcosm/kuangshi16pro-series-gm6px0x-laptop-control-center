#ifndef LCC_DAEMON_MANAGER_H
#define LCC_DAEMON_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lcc/backend.h"
#include "lcc/error.h"

#define LCC_MANAGER_JSON_MAX 4096u

typedef struct {
  lcc_backend_t *backend;
  lcc_backend_capabilities_t backend_capabilities;
  lcc_state_snapshot_t state_cache;
  char capabilities_path[256];
  char capabilities_json[LCC_MANAGER_JSON_MAX];
} lcc_manager_t;

lcc_status_t lcc_manager_init(lcc_manager_t *manager,
                              lcc_backend_t *backend,
                              const char *capabilities_path);
const char *lcc_manager_capabilities_json(const lcc_manager_t *manager);
lcc_status_t lcc_manager_get_state_json(const lcc_manager_t *manager,
                                        char *buffer, size_t buffer_len);
lcc_status_t lcc_manager_get_thermal_json(const lcc_manager_t *manager,
                                          char *buffer, size_t buffer_len);
lcc_status_t lcc_manager_set_mode(lcc_manager_t *manager,
                                  const char *mode_name);
lcc_status_t lcc_manager_set_profile(lcc_manager_t *manager,
                                     const char *profile_name);
lcc_status_t lcc_manager_apply_fan_table(lcc_manager_t *manager,
                                         const char *table_name);
lcc_status_t lcc_manager_set_fan_boost(lcc_manager_t *manager, bool enabled);
lcc_status_t lcc_manager_set_power_limits(lcc_manager_t *manager,
                                          const lcc_power_limits_t *limits);

#endif
