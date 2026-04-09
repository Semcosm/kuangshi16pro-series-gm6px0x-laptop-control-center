#include "lcc/fan.h"

#include "backends/amw0/ec-addr-map.h"
#include "core/profile/plan.h"

static uint8_t fan_tail_status1(const lcc_fan_table_t *table) {
  return table != NULL && table->activated ? 0x01u : 0x00u;
}

static uint8_t fan_tail_status2(const lcc_fan_table_t *table) {
  return table != NULL && table->fan_control_respective ? 0x01u : 0x00u;
}

static uint8_t fan_tail_control(const lcc_fan_table_t *table) {
  if (table == NULL || !table->activated) {
    return 0x00u;
  }

  return table->fan_control_respective ? 0x03u : 0x01u;
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

  lcc_plan_reset(plan);

  status = lcc_plan_append_stage(
      plan,
      "SetFanTableThread -> MyFanTableCtrl::SetFanTable (confirmed call edge)");
  if (status != LCC_OK) {
    return status;
  }

  status = lcc_plan_append_stage(plan, "FanTable_Manager1p5::SetEcFanTable_Cpu");
  if (status != LCC_OK) {
    return status;
  }

  for (index = 0; index < LCC_FAN_POINTS; ++index) {
    status = lcc_plan_append_write(
        plan, (uint16_t)(LCC_AMW0_ADDR_CPU_UP_BASE + index),
        table->cpu[index].up_temp, "cpu up temp");
    if (status != LCC_OK) {
      return status;
    }
  }
  for (index = 0; index < LCC_FAN_POINTS; ++index) {
    status = lcc_plan_append_write(
        plan, (uint16_t)(LCC_AMW0_ADDR_CPU_DOWN_BASE + index),
        table->cpu[index].down_temp, "cpu down temp");
    if (status != LCC_OK) {
      return status;
    }
  }
  for (index = 0; index < LCC_FAN_POINTS; ++index) {
    status = lcc_plan_append_write(
        plan, (uint16_t)(LCC_AMW0_ADDR_CPU_DUTY_BASE + index),
        table->cpu[index].duty, "cpu duty");
    if (status != LCC_OK) {
      return status;
    }
  }

  status = lcc_plan_append_stage(plan, "FanTable_Manager1p5::SetEcFanTable_Gpu");
  if (status != LCC_OK) {
    return status;
  }

  for (index = 0; index < LCC_FAN_POINTS; ++index) {
    status = lcc_plan_append_write(
        plan, (uint16_t)(LCC_AMW0_ADDR_GPU_UP_BASE + index),
        table->gpu[index].up_temp, "gpu up temp");
    if (status != LCC_OK) {
      return status;
    }
  }
  for (index = 0; index < LCC_FAN_POINTS; ++index) {
    status = lcc_plan_append_write(
        plan, (uint16_t)(LCC_AMW0_ADDR_GPU_DOWN_BASE + index),
        table->gpu[index].down_temp, "gpu down temp");
    if (status != LCC_OK) {
      return status;
    }
  }
  /*
   * 0xF5D..0xF5F are reserved tail bytes, so staged GPU duty writes stop at
   * 0xF5C instead of flattening all 16 logical points over the tail region.
   */
  for (index = 0; index < LCC_FAN_POINTS - 3u; ++index) {
    status = lcc_plan_append_write(
        plan, (uint16_t)(LCC_AMW0_ADDR_GPU_DUTY_BASE + index),
        table->gpu[index].duty, "gpu duty");
    if (status != LCC_OK) {
      return status;
    }
  }

  status = lcc_plan_append_stage(plan, "FanTable_Manager1p5::FinalizeTailBytes");
  if (status != LCC_OK) {
    return status;
  }

  status =
      lcc_plan_append_write(plan, LCC_AMW0_ADDR_FAN_TABLE_STATUS1,
                            fan_tail_status1(table), "fan table status1");
  if (status != LCC_OK) {
    return status;
  }
  status =
      lcc_plan_append_write(plan, LCC_AMW0_ADDR_FAN_TABLE_STATUS2,
                            fan_tail_status2(table), "fan table status2");
  if (status != LCC_OK) {
    return status;
  }
  status =
      lcc_plan_append_write(plan, LCC_AMW0_ADDR_FAN_TABLE_CTRL,
                            fan_tail_control(table), "fan table control");
  if (status != LCC_OK) {
    return status;
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
