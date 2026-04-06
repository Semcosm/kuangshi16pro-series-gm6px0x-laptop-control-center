#ifndef PROFILE_SERVICE_H
#define PROFILE_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "common/lcc_error.h"

#define LCC_FAN_POINTS 16u
#define LCC_PLAN_MAX_ACTIONS 128u

typedef enum {
  LCC_MODE_GAMING = 0u,
  LCC_MODE_OFFICE = 1u,
  LCC_MODE_TURBO = 2u,
  LCC_MODE_CUSTOM = 3u
} lcc_operating_mode_t;

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

typedef struct {
  bool has_mode;
  lcc_operating_mode_t mode;
  bool has_power_limits;
  lcc_power_limits_t power_limits;
  bool has_fan_table;
  lcc_fan_table_t fan_table;
} lcc_profile_document_t;

const char *lcc_mode_name(lcc_operating_mode_t mode);
lcc_status_t lcc_mode_from_string(const char *text, lcc_operating_mode_t *mode);
lcc_status_t lcc_build_mode_plan(lcc_operating_mode_t mode,
                                 lcc_apply_plan_t *plan);
lcc_status_t lcc_build_power_plan(const lcc_power_limits_t *limits,
                                  lcc_apply_plan_t *plan);
lcc_status_t lcc_build_profile_plan(const lcc_profile_document_t *document,
                                    lcc_apply_plan_t *plan);
lcc_status_t lcc_validate_fan_table(const lcc_fan_table_t *table);
lcc_status_t lcc_build_fan_plan(const lcc_fan_table_t *table,
                                lcc_apply_plan_t *plan);
lcc_status_t lcc_fan_table_build_demo(lcc_fan_table_t *table,
                                      const char *name);
lcc_status_t lcc_profile_document_load(const char *path,
                                       lcc_profile_document_t *document);
lcc_status_t lcc_fan_table_load_file(const char *path, lcc_fan_table_t *table);
void lcc_apply_plan_print(FILE *stream, const lcc_apply_plan_t *plan);

#endif
