#include "profile-service/profile_service.h"

#include <stdio.h>
#include <string.h>

enum {
  LCC_ADDR_MODE_CONTROL = 0x0751,
  LCC_ADDR_MODE_INDEX = 0x07AB,
  LCC_ADDR_MODE_PROFILE1 = 0x07B0,
  LCC_ADDR_MODE_PROFILE2 = 0x07B1,
  LCC_ADDR_MODE_PROFILE3 = 0x07B2,
  LCC_ADDR_MODE_HELPER = 0x07C7,
  LCC_ADDR_PL1 = 0x0783,
  LCC_ADDR_PL2 = 0x0784,
  LCC_ADDR_PL4 = 0x0785,
  LCC_ADDR_TCC_OFFSET = 0x0786,
  LCC_ADDR_CPU_UP_BASE = 0x0F00,
  LCC_ADDR_CPU_DOWN_BASE = 0x0F10,
  LCC_ADDR_CPU_DUTY_BASE = 0x0F20,
  LCC_ADDR_GPU_UP_BASE = 0x0F30,
  LCC_ADDR_GPU_DOWN_BASE = 0x0F40,
  LCC_ADDR_GPU_DUTY_BASE = 0x0F50
};

static void plan_reset(lcc_apply_plan_t *plan) {
  if (plan != NULL) {
    plan->count = 0u;
  }
}

static lcc_status_t plan_append_stage(lcc_apply_plan_t *plan,
                                      const char *label) {
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

static lcc_status_t plan_append_custom_mode(lcc_apply_plan_t *plan,
                                            bool enabled,
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

static lcc_status_t plan_append_write(lcc_apply_plan_t *plan, uint16_t addr,
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

static bool power_limits_present(const lcc_power_limits_t *limits) {
  if (limits == NULL) {
    return false;
  }

  return limits->pl1.present || limits->pl2.present || limits->pl4.present ||
         limits->tcc_offset.present;
}

static lcc_status_t plan_append_all(lcc_apply_plan_t *target,
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

static bool point_is_monotonic(const lcc_fan_point_t *previous,
                               const lcc_fan_point_t *current) {
  if (previous == NULL || current == NULL) {
    return false;
  }

  return previous->up_temp <= current->up_temp &&
         previous->down_temp <= current->down_temp &&
         previous->duty <= current->duty;
}

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

  plan_reset(plan);

  status = plan_append_stage(plan,
                             "SetModeSwitchChangeThread -> candidate sequence");
  if (status != LCC_OK) {
    return status;
  }

  status = plan_append_stage(
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

  status = plan_append_stage(plan, mode_index_hint);
  if (status != LCC_OK) {
    return status;
  }

  status = plan_append_stage(plan, mode_hint);
  if (status != LCC_OK) {
    return status;
  }

  return plan_append_stage(plan, helper_hint);
}

lcc_status_t lcc_build_power_plan(const lcc_power_limits_t *limits,
                                  lcc_apply_plan_t *plan) {
  lcc_status_t status = LCC_OK;

  if (limits == NULL || plan == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  plan_reset(plan);

  status = plan_append_stage(plan, "ModeProfile CPU power-limit writes");
  if (status != LCC_OK) {
    return status;
  }

  if (limits->pl1.present) {
    status = plan_append_write(plan, LCC_ADDR_PL1, limits->pl1.value, "PL1");
    if (status != LCC_OK) {
      return status;
    }
  }
  if (limits->pl2.present) {
    status = plan_append_write(plan, LCC_ADDR_PL2, limits->pl2.value, "PL2");
    if (status != LCC_OK) {
      return status;
    }
  }
  if (limits->pl4.present) {
    status = plan_append_write(plan, LCC_ADDR_PL4, limits->pl4.value, "PL4");
    if (status != LCC_OK) {
      return status;
    }
  }
  if (limits->tcc_offset.present) {
    status = plan_append_write(plan, LCC_ADDR_TCC_OFFSET,
                               limits->tcc_offset.value, "TccOffset");
    if (status != LCC_OK) {
      return status;
    }
  }

  if (!power_limits_present(limits)) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  return LCC_OK;
}

lcc_status_t lcc_build_profile_plan(const lcc_profile_document_t *document,
                                    lcc_apply_plan_t *plan) {
  lcc_apply_plan_t sub_plan;
  lcc_status_t status = LCC_OK;

  if (document == NULL || plan == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  plan_reset(plan);

  if (document->has_mode) {
    status = lcc_build_mode_plan(document->mode, &sub_plan);
    if (status != LCC_OK) {
      return status;
    }
    status = plan_append_all(plan, &sub_plan);
    if (status != LCC_OK) {
      return status;
    }
  }

  if (document->has_power_limits && power_limits_present(&document->power_limits)) {
    status = lcc_build_power_plan(&document->power_limits, &sub_plan);
    if (status != LCC_OK) {
      return status;
    }
    status = plan_append_all(plan, &sub_plan);
    if (status != LCC_OK) {
      return status;
    }
  }

  if (document->has_fan_table) {
    status = lcc_build_fan_plan(&document->fan_table, &sub_plan);
    if (status != LCC_OK) {
      return status;
    }
    status = plan_append_all(plan, &sub_plan);
    if (status != LCC_OK) {
      return status;
    }
  }

  if (plan->count == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  return LCC_OK;
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

lcc_status_t lcc_build_fan_plan(const lcc_fan_table_t *table,
                                lcc_apply_plan_t *plan) {
  lcc_status_t status = LCC_OK;
  size_t index = 0;

  if (table == NULL || plan == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  status = lcc_validate_fan_table(table);
  if (status != LCC_OK) {
    return status;
  }

  plan_reset(plan);

  status = plan_append_stage(
      plan,
      "SetFanTableThread -> MyFanTableCtrl::SetFanTable (confirmed call edge)");
  if (status != LCC_OK) {
    return status;
  }

  status = plan_append_custom_mode(
      plan, true,
      "keep custom fan-control path enabled while programming table");
  if (status != LCC_OK) {
    return status;
  }

  status = plan_append_stage(plan, "FanTable_Manager1p5::SetEcFanTable_Cpu");
  if (status != LCC_OK) {
    return status;
  }

  for (index = 0; index < LCC_FAN_POINTS; ++index) {
    status = plan_append_write(plan, (uint16_t)(LCC_ADDR_CPU_UP_BASE + index),
                               table->cpu[index].up_temp, "cpu up temp");
    if (status != LCC_OK) {
      return status;
    }
  }
  for (index = 0; index < LCC_FAN_POINTS; ++index) {
    status =
        plan_append_write(plan, (uint16_t)(LCC_ADDR_CPU_DOWN_BASE + index),
                          table->cpu[index].down_temp, "cpu down temp");
    if (status != LCC_OK) {
      return status;
    }
  }
  for (index = 0; index < LCC_FAN_POINTS; ++index) {
    status = plan_append_write(plan, (uint16_t)(LCC_ADDR_CPU_DUTY_BASE + index),
                               table->cpu[index].duty, "cpu duty");
    if (status != LCC_OK) {
      return status;
    }
  }

  status = plan_append_stage(plan, "FanTable_Manager1p5::SetEcFanTable_Gpu");
  if (status != LCC_OK) {
    return status;
  }

  for (index = 0; index < LCC_FAN_POINTS; ++index) {
    status = plan_append_write(plan, (uint16_t)(LCC_ADDR_GPU_UP_BASE + index),
                               table->gpu[index].up_temp, "gpu up temp");
    if (status != LCC_OK) {
      return status;
    }
  }
  for (index = 0; index < LCC_FAN_POINTS; ++index) {
    status =
        plan_append_write(plan, (uint16_t)(LCC_ADDR_GPU_DOWN_BASE + index),
                          table->gpu[index].down_temp, "gpu down temp");
    if (status != LCC_OK) {
      return status;
    }
  }
  for (index = 0; index < LCC_FAN_POINTS; ++index) {
    status = plan_append_write(plan, (uint16_t)(LCC_ADDR_GPU_DUTY_BASE + index),
                               table->gpu[index].duty, "gpu duty");
    if (status != LCC_OK) {
      return status;
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

void lcc_apply_plan_print(FILE *stream, const lcc_apply_plan_t *plan) {
  size_t index = 0;

  if (stream == NULL || plan == NULL) {
    return;
  }

  for (index = 0; index < plan->count; ++index) {
    const lcc_write_action_t *action = &plan->actions[index];

    switch (action->kind) {
      case LCC_ACTION_STAGE:
        (void)fprintf(stream, "%02zu  stage        %s\n", index, action->label);
        break;
      case LCC_ACTION_CUSTOM_MODE:
        (void)fprintf(stream, "%02zu  custom-mode  %s  %s\n", index,
                      action->enabled ? "on " : "off", action->label);
        break;
      case LCC_ACTION_EC_WRITE:
        (void)fprintf(stream,
                      "%02zu  ec-write     addr=0x%04X  value=0x%02X  %s\n",
                      index, (unsigned int)action->addr,
                      (unsigned int)action->value, action->label);
        break;
    }
  }
}
