#ifndef LCC_DAEMON_MANAGER_H
#define LCC_DAEMON_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lcc/error.h"
#include "lcc/power.h"

#define LCC_MANAGER_JSON_MAX 4096u

typedef struct {
  char capabilities_json[LCC_MANAGER_JSON_MAX];
  char requested_profile[64];
  char effective_profile[64];
  char requested_fan_table[64];
  char effective_fan_table[64];
  bool has_requested_power;
  bool has_effective_power;
  lcc_power_limits_t requested_power;
  lcc_power_limits_t effective_power;
} lcc_manager_t;

lcc_status_t lcc_manager_init(lcc_manager_t *manager,
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
lcc_status_t lcc_manager_set_power_limits(lcc_manager_t *manager,
                                          const lcc_power_limits_t *limits);

#endif
