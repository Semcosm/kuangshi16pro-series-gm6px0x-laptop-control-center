#include "backends/amw0/ec-addr-map.h"

static const lcc_observe_item_t mode_items[] = {
    {"MAFAN_CTL", LCC_AMW0_ADDR_MODE_CONTROL},
    {"MYFANCCI_IDX", LCC_AMW0_ADDR_MODE_INDEX},
    {"PROFILE1", LCC_AMW0_ADDR_MODE_PROFILE1},
    {"PROFILE2", LCC_AMW0_ADDR_MODE_PROFILE2},
    {"PROFILE3", LCC_AMW0_ADDR_MODE_PROFILE3},
    {"MODE_HELPER", LCC_AMW0_ADDR_MODE_HELPER},
};

static const lcc_observe_item_t power_items[] = {
    {"PL1", LCC_AMW0_ADDR_PL1},
    {"PL2", LCC_AMW0_ADDR_PL2},
    {"PL4", LCC_AMW0_ADDR_PL4},
    {"TCC_OFFSET", LCC_AMW0_ADDR_TCC_OFFSET},
    {"FAN_SWITCH_SPEED", LCC_AMW0_ADDR_FAN_SWITCH_SPEED},
};

static const lcc_observe_item_t fan_items[] = {
    {"CPU_UP0", LCC_AMW0_ADDR_CPU_UP_BASE},
    {"CPU_DOWN0", LCC_AMW0_ADDR_CPU_DOWN_BASE},
    {"CPU_DUTY0", LCC_AMW0_ADDR_CPU_DUTY_BASE},
    {"GPU_UP0", LCC_AMW0_ADDR_GPU_UP_BASE},
    {"GPU_DOWN0", LCC_AMW0_ADDR_GPU_DOWN_BASE},
    {"GPU_DUTY0", LCC_AMW0_ADDR_GPU_DUTY_BASE},
    {"TABLE_STATUS1", LCC_AMW0_ADDR_FAN_TABLE_STATUS1},
    {"TABLE_STATUS2", LCC_AMW0_ADDR_FAN_TABLE_STATUS2},
    {"TABLE_CTRL", LCC_AMW0_ADDR_FAN_TABLE_CTRL},
};

static const lcc_observe_item_t thermal_items[] = {
    {"FFAN", LCC_AMW0_ADDR_FFAN}, {"CPUT", LCC_AMW0_ADDR_CPUT},
    {"PCHT", LCC_AMW0_ADDR_PCHT}, {"F1SH", LCC_AMW0_ADDR_F1SH},
    {"F1SL", LCC_AMW0_ADDR_F1SL}, {"F1DC", LCC_AMW0_ADDR_F1DC},
    {"F2DC", LCC_AMW0_ADDR_F2DC},
};

const lcc_observe_item_t *lcc_amw0_observe_mode_items(size_t *count) {
  if (count != NULL) {
    *count = sizeof(mode_items) / sizeof(mode_items[0]);
  }
  return mode_items;
}

const lcc_observe_item_t *lcc_amw0_observe_power_items(size_t *count) {
  if (count != NULL) {
    *count = sizeof(power_items) / sizeof(power_items[0]);
  }
  return power_items;
}

const lcc_observe_item_t *lcc_amw0_observe_fan_items(size_t *count) {
  if (count != NULL) {
    *count = sizeof(fan_items) / sizeof(fan_items[0]);
  }
  return fan_items;
}

const lcc_observe_item_t *lcc_amw0_observe_thermal_items(size_t *count) {
  if (count != NULL) {
    *count = sizeof(thermal_items) / sizeof(thermal_items[0]);
  }
  return thermal_items;
}
