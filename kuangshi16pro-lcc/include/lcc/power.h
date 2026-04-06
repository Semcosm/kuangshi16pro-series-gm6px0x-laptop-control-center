#ifndef LCC_POWER_H
#define LCC_POWER_H

#include <stdbool.h>
#include <stdint.h>

#include "lcc/error.h"
#include "lcc/plan.h"

typedef struct {
  bool present;
  uint8_t value;
} lcc_optional_byte_t;

typedef struct {
  lcc_optional_byte_t pl1;
  lcc_optional_byte_t pl2;
  lcc_optional_byte_t pl4;
  lcc_optional_byte_t tcc_offset;
} lcc_power_limits_t;

lcc_status_t lcc_build_power_plan(const lcc_power_limits_t *limits,
                                  lcc_apply_plan_t *plan);

#endif
