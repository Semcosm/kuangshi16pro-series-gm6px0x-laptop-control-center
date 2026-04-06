#include "lcc/fan.h"

#include <stdio.h>
#include <string.h>

static bool point_is_monotonic(const lcc_fan_point_t *previous,
                               const lcc_fan_point_t *current) {
  if (previous == NULL || current == NULL) {
    return false;
  }

  return previous->up_temp <= current->up_temp &&
         previous->down_temp <= current->down_temp &&
         previous->duty <= current->duty;
}

lcc_status_t lcc_validate_fan_table(const lcc_fan_table_t *table) {
  size_t index = 0;

  if (table == NULL || table->name[0] == '\0') {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  for (index = 0; index < LCC_FAN_POINTS; ++index) {
    if (table->cpu[index].down_temp > table->cpu[index].up_temp ||
        table->gpu[index].down_temp > table->gpu[index].up_temp) {
      return LCC_ERR_RANGE;
    }
    if (table->cpu[index].duty > 100u || table->gpu[index].duty > 100u) {
      return LCC_ERR_RANGE;
    }
    if (index > 0u) {
      if (!point_is_monotonic(&table->cpu[index - 1u], &table->cpu[index]) ||
          !point_is_monotonic(&table->gpu[index - 1u], &table->gpu[index])) {
        return LCC_ERR_RANGE;
      }
    }
  }

  return LCC_OK;
}

lcc_status_t lcc_fan_table_build_demo(lcc_fan_table_t *table,
                                      const char *name) {
  size_t index = 0;

  if (table == NULL || name == NULL || name[0] == '\0') {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  memset(table, 0, sizeof(*table));
  (void)snprintf(table->name, sizeof(table->name), "%s", name);
  table->activated = true;
  table->fan_control_respective = true;

  for (index = 0; index < LCC_FAN_POINTS; ++index) {
    const uint8_t cpu_up = (uint8_t)(40u + (uint8_t)(index * 3u));
    const uint8_t gpu_up = (uint8_t)(45u + (uint8_t)(index * 3u));
    const uint8_t cpu_duty = (uint8_t)(18u + (uint8_t)(index * 5u));
    const uint8_t gpu_duty = (uint8_t)(20u + (uint8_t)(index * 5u));

    table->cpu[index].up_temp = cpu_up;
    table->cpu[index].down_temp = (uint8_t)(cpu_up - 3u);
    table->cpu[index].duty = cpu_duty > 100u ? 100u : cpu_duty;

    table->gpu[index].up_temp = gpu_up;
    table->gpu[index].down_temp = (uint8_t)(gpu_up - 3u);
    table->gpu[index].duty = gpu_duty > 100u ? 100u : gpu_duty;
  }

  return LCC_OK;
}
