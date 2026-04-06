#ifndef LCC_STATE_H
#define LCC_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "lcc/power.h"

#define LCC_STATE_NAME_MAX 64u
#define LCC_STATE_BACKEND_NAME_MAX 32u

typedef struct {
  char profile[LCC_STATE_NAME_MAX];
  char fan_table[LCC_STATE_NAME_MAX];
  bool has_power_limits;
  lcc_power_limits_t power_limits;
} lcc_state_target_t;

typedef struct {
  bool has_cpu_fan_rpm;
  bool has_gpu_fan_rpm;
  bool has_cpu_temp_c;
  bool has_gpu_temp_c;
  uint16_t cpu_fan_rpm;
  uint16_t gpu_fan_rpm;
  uint8_t cpu_temp_c;
  uint8_t gpu_temp_c;
} lcc_thermal_state_t;

typedef struct {
  char backend_name[LCC_STATE_BACKEND_NAME_MAX];
  bool hardware_write;
  lcc_state_target_t requested;
  lcc_state_target_t effective;
  lcc_thermal_state_t thermal;
} lcc_state_snapshot_t;

#endif
