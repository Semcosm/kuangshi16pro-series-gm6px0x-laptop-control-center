#include "core/profile/plan.h"

void lcc_plan_reset(lcc_apply_plan_t *plan) {
  if (plan != NULL) {
    plan->count = 0u;
  }
}

lcc_status_t lcc_plan_append_stage(lcc_apply_plan_t *plan, const char *label) {
  if (plan == NULL || label == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (plan->count >= LCC_PLAN_MAX_ACTIONS) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  plan->actions[plan->count].kind = LCC_ACTION_STAGE;
  plan->actions[plan->count].addr = 0u;
  plan->actions[plan->count].value = 0u;
  plan->actions[plan->count].enabled = false;
  plan->actions[plan->count].label = label;
  ++plan->count;
  return LCC_OK;
}

lcc_status_t lcc_plan_append_custom_mode(lcc_apply_plan_t *plan, bool enabled,
                                         const char *label) {
  if (plan == NULL || label == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (plan->count >= LCC_PLAN_MAX_ACTIONS) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  plan->actions[plan->count].kind = LCC_ACTION_CUSTOM_MODE;
  plan->actions[plan->count].addr = 0u;
  plan->actions[plan->count].value = 0u;
  plan->actions[plan->count].enabled = enabled;
  plan->actions[plan->count].label = label;
  ++plan->count;
  return LCC_OK;
}

lcc_status_t lcc_plan_append_write(lcc_apply_plan_t *plan, uint16_t addr,
                                   uint8_t value, const char *label) {
  if (plan == NULL || label == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (plan->count >= LCC_PLAN_MAX_ACTIONS) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  plan->actions[plan->count].kind = LCC_ACTION_EC_WRITE;
  plan->actions[plan->count].addr = addr;
  plan->actions[plan->count].value = value;
  plan->actions[plan->count].enabled = false;
  plan->actions[plan->count].label = label;
  ++plan->count;
  return LCC_OK;
}

lcc_status_t lcc_plan_append_all(lcc_apply_plan_t *target,
                                 const lcc_apply_plan_t *source) {
  size_t index = 0;

  if (target == NULL || source == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (target->count + source->count > LCC_PLAN_MAX_ACTIONS) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  for (index = 0; index < source->count; ++index) {
    target->actions[target->count] = source->actions[index];
    ++target->count;
  }

  return LCC_OK;
}
