#ifndef LCC_PROFILE_H
#define LCC_PROFILE_H

#include <stdbool.h>

#include "lcc/error.h"
#include "lcc/fan.h"
#include "lcc/power.h"

typedef enum {
  LCC_MODE_GAMING = 0u,
  LCC_MODE_OFFICE = 1u,
  LCC_MODE_TURBO = 2u,
  LCC_MODE_CUSTOM = 3u
} lcc_operating_mode_t;

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
lcc_status_t lcc_build_profile_plan(const lcc_profile_document_t *document,
                                    lcc_apply_plan_t *plan);
lcc_status_t lcc_profile_document_load(const char *path,
                                       lcc_profile_document_t *document);
lcc_status_t lcc_profile_document_validate(
    const lcc_profile_document_t *document);

#endif
