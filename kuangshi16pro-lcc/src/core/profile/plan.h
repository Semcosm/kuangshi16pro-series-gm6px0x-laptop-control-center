#ifndef LCC_CORE_PROFILE_PLAN_H
#define LCC_CORE_PROFILE_PLAN_H

#include "lcc/error.h"
#include "lcc/plan.h"

void lcc_plan_reset(lcc_apply_plan_t *plan);
lcc_status_t lcc_plan_append_stage(lcc_apply_plan_t *plan, const char *label);
lcc_status_t lcc_plan_append_custom_mode(lcc_apply_plan_t *plan, bool enabled,
                                         const char *label);
lcc_status_t lcc_plan_append_write(lcc_apply_plan_t *plan, uint16_t addr,
                                   uint8_t value, const char *label);
lcc_status_t lcc_plan_append_all(lcc_apply_plan_t *target,
                                 const lcc_apply_plan_t *source);

#endif
