#ifndef LCC_FAN_H
#define LCC_FAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lcc/error.h"
#include "lcc/plan.h"

#define LCC_FAN_POINTS 16u

typedef struct {
  uint8_t up_temp;
  uint8_t down_temp;
  uint8_t duty;
} lcc_fan_point_t;

typedef struct {
  char name[32];
  bool activated;
  bool fan_control_respective;
  lcc_fan_point_t cpu[LCC_FAN_POINTS];
  lcc_fan_point_t gpu[LCC_FAN_POINTS];
} lcc_fan_table_t;

lcc_status_t lcc_validate_fan_table(const lcc_fan_table_t *table);
lcc_status_t lcc_build_fan_plan(const lcc_fan_table_t *table,
                                lcc_apply_plan_t *plan);
lcc_status_t lcc_fan_table_build_demo(lcc_fan_table_t *table,
                                      const char *name);
lcc_status_t lcc_fan_table_load_file(const char *path, lcc_fan_table_t *table);

#endif
