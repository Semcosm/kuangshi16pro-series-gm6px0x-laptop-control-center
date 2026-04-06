#include "lcc/profile.h"

#include <string.h>

#include "core/profile/plan.h"

const char *lcc_mode_name(lcc_operating_mode_t mode) {
  switch (mode) {
    case LCC_MODE_GAMING:
      return "gaming";
    case LCC_MODE_OFFICE:
      return "office";
    case LCC_MODE_TURBO:
      return "turbo";
    case LCC_MODE_CUSTOM:
      return "custom";
  }

  return "unknown";
}

lcc_status_t lcc_mode_from_string(const char *text, lcc_operating_mode_t *mode) {
  if (text == NULL || mode == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  if (strcmp(text, "gaming") == 0) {
    *mode = LCC_MODE_GAMING;
    return LCC_OK;
  }
  if (strcmp(text, "office") == 0) {
    *mode = LCC_MODE_OFFICE;
    return LCC_OK;
  }
  if (strcmp(text, "turbo") == 0) {
    *mode = LCC_MODE_TURBO;
    return LCC_OK;
  }
  if (strcmp(text, "custom") == 0) {
    *mode = LCC_MODE_CUSTOM;
    return LCC_OK;
  }

  return LCC_ERR_PARSE;
}

lcc_status_t lcc_build_mode_plan(lcc_operating_mode_t mode,
                                 lcc_apply_plan_t *plan) {
  lcc_status_t status = LCC_OK;
  const char *mode_index_hint = NULL;
  const char *mode_hint = NULL;
  const char *helper_hint = NULL;

  if (plan == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  lcc_plan_reset(plan);

  status = lcc_plan_append_stage(
      plan, "SetModeSwitchChangeThread -> candidate sequence");
  if (status != LCC_OK) {
    return status;
  }

  status = lcc_plan_append_stage(
      plan, "candidate: inspect mode/profile bytes 0x7AB and 0x7B0..0x7B2");
  if (status != LCC_OK) {
    return status;
  }

  switch (mode) {
    case LCC_MODE_GAMING:
      mode_index_hint = "candidate: mode index write 0x07AB = 0x00 (gaming)";
      mode_hint =
          "candidate: clear 0x0751 TBME/UFME bits for gaming, but do not treat 0x751 as a literal mode byte";
      helper_hint =
          "candidate: compare 0x07B0..0x07B2 and 0x07C7 against office while switching";
      break;
    case LCC_MODE_OFFICE:
      mode_index_hint = "candidate: mode index write 0x07AB = 0x01 (office)";
      mode_hint =
          "candidate: clear 0x0751 TBME/UFME bits for office; office vs gaming likely differs outside 0x751";
      helper_hint =
          "candidate: compare 0x07B0..0x07B2 and 0x07C7 against gaming while switching";
      break;
    case LCC_MODE_TURBO:
      mode_index_hint = "candidate: mode index write 0x07AB = 0x02 (turbo)";
      mode_hint = "candidate: set 0x0751 bit4 mask 0x10 (TBME) for turbo";
      helper_hint =
          "candidate: inspect 0x07C7 helper bits while entering turbo";
      break;
    case LCC_MODE_CUSTOM:
      mode_index_hint = "candidate: mode index write 0x07AB = 0x03 (custom)";
      mode_hint = "candidate: set 0x0751 bit7 mask 0x80 (UFME) for custom";
      helper_hint =
          "candidate: inspect 0x07C7 helper bits and custom profile slots 0x07B0..0x07B2";
      break;
    default:
      return LCC_ERR_INVALID_ARGUMENT;
  }

  status = lcc_plan_append_stage(plan, mode_index_hint);
  if (status != LCC_OK) {
    return status;
  }

  status = lcc_plan_append_stage(plan, mode_hint);
  if (status != LCC_OK) {
    return status;
  }

  return lcc_plan_append_stage(plan, helper_hint);
}

lcc_status_t lcc_build_profile_plan(const lcc_profile_document_t *document,
                                    lcc_apply_plan_t *plan) {
  lcc_apply_plan_t sub_plan;
  lcc_status_t status = LCC_OK;

  if (document == NULL || plan == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  lcc_plan_reset(plan);

  if (document->has_mode) {
    status = lcc_build_mode_plan(document->mode, &sub_plan);
    if (status != LCC_OK) {
      return status;
    }
    status = lcc_plan_append_all(plan, &sub_plan);
    if (status != LCC_OK) {
      return status;
    }
  }

  if (document->has_power_limits) {
    status = lcc_build_power_plan(&document->power_limits, &sub_plan);
    if (status != LCC_OK) {
      return status;
    }
    status = lcc_plan_append_all(plan, &sub_plan);
    if (status != LCC_OK) {
      return status;
    }
  }

  if (document->has_fan_table) {
    status = lcc_build_fan_plan(&document->fan_table, &sub_plan);
    if (status != LCC_OK) {
      return status;
    }
    status = lcc_plan_append_all(plan, &sub_plan);
    if (status != LCC_OK) {
      return status;
    }
  }

  if (plan->count == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  return LCC_OK;
}
