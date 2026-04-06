#include "lcc/power.h"

#include "backends/amw0/ec-addr-map.h"
#include "core/profile/plan.h"

static bool power_limits_present(const lcc_power_limits_t *limits) {
  if (limits == NULL) {
    return false;
  }

  return limits->pl1.present || limits->pl2.present || limits->pl4.present ||
         limits->tcc_offset.present;
}

lcc_status_t lcc_build_power_plan(const lcc_power_limits_t *limits,
                                  lcc_apply_plan_t *plan) {
  lcc_status_t status = LCC_OK;

  if (limits == NULL || plan == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (!power_limits_present(limits)) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  lcc_plan_reset(plan);

  status = lcc_plan_append_stage(plan, "ModeProfile CPU power-limit writes");
  if (status != LCC_OK) {
    return status;
  }

  if (limits->pl1.present) {
    status =
        lcc_plan_append_write(plan, LCC_AMW0_ADDR_PL1, limits->pl1.value, "PL1");
    if (status != LCC_OK) {
      return status;
    }
  }
  if (limits->pl2.present) {
    status =
        lcc_plan_append_write(plan, LCC_AMW0_ADDR_PL2, limits->pl2.value, "PL2");
    if (status != LCC_OK) {
      return status;
    }
  }
  if (limits->pl4.present) {
    status =
        lcc_plan_append_write(plan, LCC_AMW0_ADDR_PL4, limits->pl4.value, "PL4");
    if (status != LCC_OK) {
      return status;
    }
  }
  if (limits->tcc_offset.present) {
    status = lcc_plan_append_write(plan, LCC_AMW0_ADDR_TCC_OFFSET,
                                   limits->tcc_offset.value, "TccOffset");
    if (status != LCC_OK) {
      return status;
    }
  }

  return LCC_OK;
}
