#ifndef LCC_STATE_H
#define LCC_STATE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool has_platform_profile;
  bool has_power_limits;
  bool has_fan_rpm;
  bool has_temperatures;
  uint8_t mode_index;
  uint8_t pl1;
  uint8_t pl2;
  uint8_t pl4;
  uint16_t cpu_fan_rpm;
  uint16_t gpu_fan_rpm;
  uint8_t cpu_temp;
  uint8_t gpu_temp;
} lcc_state_snapshot_t;

#endif
