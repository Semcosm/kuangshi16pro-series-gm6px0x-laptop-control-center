#ifndef LCC_PLAN_H
#define LCC_PLAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define LCC_PLAN_MAX_ACTIONS 128u

typedef enum {
  LCC_ACTION_STAGE = 0,
  LCC_ACTION_CUSTOM_MODE,
  LCC_ACTION_EC_WRITE
} lcc_action_kind_t;

typedef struct {
  lcc_action_kind_t kind;
  uint16_t addr;
  uint8_t value;
  bool enabled;
  const char *label;
} lcc_write_action_t;

typedef struct {
  lcc_write_action_t actions[LCC_PLAN_MAX_ACTIONS];
  size_t count;
} lcc_apply_plan_t;

void lcc_apply_plan_print(FILE *stream, const lcc_apply_plan_t *plan);

#endif
