#ifndef LCC_BACKENDS_AMW0_EC_ADDR_MAP_H
#define LCC_BACKENDS_AMW0_EC_ADDR_MAP_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
  const char *name;
  uint16_t offset;
} lcc_observe_item_t;

enum {
  LCC_AMW0_ADDR_MODE_CONTROL = 0x0751,
  LCC_AMW0_ADDR_MODE_INDEX = 0x07AB,
  LCC_AMW0_ADDR_MODE_PROFILE1 = 0x07B0,
  LCC_AMW0_ADDR_MODE_PROFILE2 = 0x07B1,
  LCC_AMW0_ADDR_MODE_PROFILE3 = 0x07B2,
  LCC_AMW0_ADDR_MODE_HELPER = 0x07C7,
  LCC_AMW0_ADDR_PL1 = 0x0783,
  LCC_AMW0_ADDR_PL2 = 0x0784,
  LCC_AMW0_ADDR_PL4 = 0x0785,
  LCC_AMW0_ADDR_TCC_OFFSET = 0x0786,
  LCC_AMW0_ADDR_FAN_SWITCH_SPEED = 0x0787,
  LCC_AMW0_ADDR_CPU_UP_BASE = 0x0F00,
  LCC_AMW0_ADDR_CPU_DOWN_BASE = 0x0F10,
  LCC_AMW0_ADDR_CPU_DUTY_BASE = 0x0F20,
  LCC_AMW0_ADDR_GPU_UP_BASE = 0x0F30,
  LCC_AMW0_ADDR_GPU_DOWN_BASE = 0x0F40,
  LCC_AMW0_ADDR_GPU_DUTY_BASE = 0x0F50,
  LCC_AMW0_ADDR_FAN_TABLE_STATUS1 = 0x0F5D,
  LCC_AMW0_ADDR_FAN_TABLE_STATUS2 = 0x0F5E,
  LCC_AMW0_ADDR_FAN_TABLE_CTRL = 0x0F5F,
  LCC_AMW0_ADDR_FFAN = 0x0460,
  LCC_AMW0_ADDR_CPUT = 0x0E0D,
  LCC_AMW0_ADDR_PCHT = 0x0E0E,
  LCC_AMW0_ADDR_F1SH = 0x0E1C,
  LCC_AMW0_ADDR_F1SL = 0x0E1D,
  LCC_AMW0_ADDR_F1DC = 0x0E8C,
  LCC_AMW0_ADDR_F2DC = 0x0E9D
};

const lcc_observe_item_t *lcc_amw0_observe_mode_items(size_t *count);
const lcc_observe_item_t *lcc_amw0_observe_power_items(size_t *count);
const lcc_observe_item_t *lcc_amw0_observe_fan_items(size_t *count);
const lcc_observe_item_t *lcc_amw0_observe_thermal_items(size_t *count);

#endif
