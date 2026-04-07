#include "backends/backend.h"

#include <stdio.h>
#include <string.h>

#include "backends/amw0/ec-addr-map.h"
#include "backends/amw0/ecmg.h"
#include "lcc/fan.h"

static lcc_status_t copy_name(char *buffer, size_t buffer_len,
                              const char *value) {
  int written = 0;

  if (buffer == NULL || value == NULL || value[0] == '\0') {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  written = snprintf(buffer, buffer_len, "%s", value);
  if (written < 0 || (size_t)written >= buffer_len) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return LCC_OK;
}

static const char *profile_name_for_mode(lcc_operating_mode_t mode) {
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

  return NULL;
}

static lcc_status_t profile_name_to_mode(const char *profile_name,
                                         lcc_operating_mode_t *mode) {
  if (profile_name == NULL || mode == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (strcmp(profile_name, "gaming") == 0 || strcmp(profile_name, "balanced") == 0) {
    *mode = LCC_MODE_GAMING;
    return LCC_OK;
  }
  if (strcmp(profile_name, "office") == 0 ||
      strcmp(profile_name, "low-power") == 0) {
    *mode = LCC_MODE_OFFICE;
    return LCC_OK;
  }
  if (strcmp(profile_name, "turbo") == 0 ||
      strcmp(profile_name, "performance") == 0) {
    *mode = LCC_MODE_TURBO;
    return LCC_OK;
  }
  if (strcmp(profile_name, "custom") == 0) {
    *mode = LCC_MODE_CUSTOM;
    return LCC_OK;
  }

  return LCC_ERR_NOT_SUPPORTED;
}

static uint8_t mode_index_for_mode(lcc_operating_mode_t mode) {
  switch (mode) {
    case LCC_MODE_GAMING:
      return 0u;
    case LCC_MODE_OFFICE:
      return 1u;
    case LCC_MODE_TURBO:
      return 2u;
    case LCC_MODE_CUSTOM:
      return 3u;
  }

  return 0u;
}

static uint8_t mode_control_for_mode(lcc_operating_mode_t mode) {
  switch (mode) {
    case LCC_MODE_GAMING:
      return 0x20u;
    case LCC_MODE_OFFICE:
      return 0x00u;
    case LCC_MODE_TURBO:
      return 0x10u;
    case LCC_MODE_CUSTOM:
      return 0xA0u;
  }

  return 0u;
}

static lcc_operating_mode_t mode_from_control(uint8_t control) {
  /*
   * 0x751 is not a pure enum byte. For the first executable path, keep the
   * proven turbo/custom bits and map the observed 0x20 helper family to gaming.
   */
  if ((control & 0x80u) != 0u) {
    return LCC_MODE_CUSTOM;
  }
  if ((control & 0x10u) != 0u) {
    return LCC_MODE_TURBO;
  }
  if ((control & 0x20u) != 0u) {
    return LCC_MODE_GAMING;
  }

  return LCC_MODE_OFFICE;
}

static void seed_shadow_state(lcc_amw0_backend_t *amw0) {
  if (amw0 == NULL) {
    return;
  }

  memset(&amw0->shadow_state, 0, sizeof(amw0->shadow_state));
  (void)copy_name(amw0->shadow_state.backend_name,
                  sizeof(amw0->shadow_state.backend_name), "amw0");
  (void)copy_name(amw0->shadow_state.backend_selected,
                  sizeof(amw0->shadow_state.backend_selected), "amw0");
  (void)lcc_backend_execution_set_all(&amw0->shadow_state.execution, "amw0");
  (void)copy_name(amw0->shadow_state.requested.profile,
                  sizeof(amw0->shadow_state.requested.profile), "office");
  (void)copy_name(amw0->shadow_state.effective.profile,
                  sizeof(amw0->shadow_state.effective.profile), "office");
  (void)copy_name(amw0->shadow_state.requested.fan_table,
                  sizeof(amw0->shadow_state.requested.fan_table),
                  "system-default");
  (void)copy_name(amw0->shadow_state.effective.fan_table,
                  sizeof(amw0->shadow_state.effective.fan_table),
                  "system-default");
}

static lcc_status_t ensure_ecrr_path(lcc_amw0_backend_t *amw0) {
  if (amw0 == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (amw0->has_ecrr_path) {
    return LCC_OK;
  }
  if (amw0->transport.dry_run) {
    return LCC_OK;
  }

  if (amw0_backend_probe_ecrr_path(&amw0->transport, amw0->ecrr_path,
                                   sizeof(amw0->ecrr_path)) != LCC_OK) {
    return LCC_ERR_NOT_FOUND;
  }

  amw0->has_ecrr_path = true;
  return LCC_OK;
}

static lcc_status_t read_live_byte(lcc_amw0_backend_t *amw0, uint16_t offset,
                                   uint8_t *value) {
  lcc_status_t status = LCC_OK;

  if (amw0 == NULL || value == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (amw0->transport.dry_run) {
    return LCC_ERR_NOT_SUPPORTED;
  }

  status = ensure_ecrr_path(amw0);
  if (status != LCC_OK) {
    return status;
  }

  return lcc_amw0_read_ecrr_u8(&amw0->transport, amw0->ecrr_path, offset, value);
}

static void set_mode_profile_state(lcc_state_snapshot_t *state,
                                   lcc_operating_mode_t mode) {
  const char *profile_name = profile_name_for_mode(mode);

  if (state == NULL || profile_name == NULL) {
    return;
  }

  (void)copy_name(state->requested.profile, sizeof(state->requested.profile),
                  profile_name);
  (void)copy_name(state->effective.profile, sizeof(state->effective.profile),
                  profile_name);
}

static lcc_status_t amw0_probe(void *ctx,
                               lcc_backend_capabilities_t *capabilities,
                               lcc_backend_result_t *result) {
  lcc_amw0_backend_t *amw0 = ctx;
  lcc_status_t status = LCC_OK;

  if (amw0 == NULL || capabilities == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  memset(capabilities, 0, sizeof(*capabilities));
  capabilities->can_read_state = true;
  capabilities->can_apply_profile = true;
  capabilities->can_apply_mode = true;
  capabilities->can_apply_power_limits = true;
  capabilities->can_apply_fan_table = true;
  lcc_backend_result_reset(result);
  lcc_backend_result_set_executor(result, "amw0");

  if (!amw0->transport.dry_run) {
    status = ensure_ecrr_path(amw0);
    if (status != LCC_OK) {
      lcc_backend_result_set_detail(result, "amw0 read helper path unavailable");
      return status;
    }
  }

  return LCC_OK;
}

static lcc_status_t amw0_read_state(void *ctx, lcc_state_snapshot_t *state,
                                    lcc_backend_result_t *result) {
  lcc_amw0_backend_t *amw0 = ctx;
  bool any_live_value = false;
  uint8_t raw = 0;

  if (amw0 == NULL || state == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  lcc_backend_result_reset(result);
  lcc_backend_result_set_executor(result, "amw0");
  *state = amw0->shadow_state;
  (void)copy_name(state->backend_name, sizeof(state->backend_name), "amw0");
  (void)lcc_backend_state_set_metadata(state, "amw0", "amw0", NULL,
                                       &amw0->shadow_state.execution);

  if (amw0->transport.dry_run) {
    return LCC_OK;
  }

  if (read_live_byte(amw0, LCC_AMW0_ADDR_MODE_CONTROL, &raw) == LCC_OK) {
    set_mode_profile_state(state, mode_from_control(raw));
    any_live_value = true;
  }
  if (read_live_byte(amw0, LCC_AMW0_ADDR_PL1, &raw) == LCC_OK) {
    state->requested.power_limits.pl1.present = true;
    state->requested.power_limits.pl1.value = raw;
    state->effective.power_limits.pl1 = state->requested.power_limits.pl1;
    any_live_value = true;
  }
  if (read_live_byte(amw0, LCC_AMW0_ADDR_PL2, &raw) == LCC_OK) {
    state->requested.power_limits.pl2.present = true;
    state->requested.power_limits.pl2.value = raw;
    state->effective.power_limits.pl2 = state->requested.power_limits.pl2;
    any_live_value = true;
  }
  if (read_live_byte(amw0, LCC_AMW0_ADDR_PL4, &raw) == LCC_OK) {
    state->requested.power_limits.pl4.present = true;
    state->requested.power_limits.pl4.value = raw;
    state->effective.power_limits.pl4 = state->requested.power_limits.pl4;
    any_live_value = true;
  }
  if (read_live_byte(amw0, LCC_AMW0_ADDR_TCC_OFFSET, &raw) == LCC_OK) {
    state->requested.power_limits.tcc_offset.present = true;
    state->requested.power_limits.tcc_offset.value = raw;
    state->effective.power_limits.tcc_offset =
        state->requested.power_limits.tcc_offset;
    any_live_value = true;
  }
  state->requested.has_power_limits = state->requested.power_limits.pl1.present ||
                                      state->requested.power_limits.pl2.present ||
                                      state->requested.power_limits.pl4.present ||
                                      state->requested.power_limits.tcc_offset.present;
  state->effective.has_power_limits = state->requested.has_power_limits;

  if (read_live_byte(amw0, LCC_AMW0_ADDR_CPUT, &raw) == LCC_OK && raw > 0u) {
    state->thermal.has_cpu_temp_c = true;
    state->thermal.cpu_temp_c = raw;
    any_live_value = true;
  }
  if (read_live_byte(amw0, LCC_AMW0_ADDR_PCHT, &raw) == LCC_OK && raw > 0u) {
    state->thermal.has_gpu_temp_c = true;
    state->thermal.gpu_temp_c = raw;
    any_live_value = true;
  }

  amw0->shadow_state = *state;
  if (!any_live_value) {
    lcc_backend_result_set_detail(result,
                                  "amw0 state read returned no live values");
    return LCC_ERR_NOT_FOUND;
  }
  return LCC_OK;
}

static lcc_status_t write_ec_byte(lcc_amw0_backend_t *amw0, uint16_t offset,
                                  uint8_t value) {
  char reply[AMW0_REPLY_MAX];

  if (amw0 == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  ++amw0->write_count;
  if (amw0->fail_after_writes > 0u &&
      amw0->write_count >= amw0->fail_after_writes) {
    return LCC_ERR_IO;
  }

  return amw0_backend_send_ec_write(&amw0->transport, amw0->route, offset, value,
                                    reply, sizeof(reply));
}

static lcc_status_t apply_custom_enable(lcc_amw0_backend_t *amw0,
                                        lcc_backend_result_t *result) {
  lcc_status_t status = LCC_OK;

  lcc_backend_result_set_stage(result, "custom-enable");
  (void)amw0_backend_trace_note(
      &amw0->transport,
      "stage=custom-enable label=keep custom fan-control path enabled while programming table");

  status = write_ec_byte(amw0, LCC_AMW0_ADDR_MODE_INDEX,
                         mode_index_for_mode(LCC_MODE_CUSTOM));
  if (status != LCC_OK) {
    return status;
  }

  return write_ec_byte(amw0, LCC_AMW0_ADDR_MODE_CONTROL,
                       mode_control_for_mode(LCC_MODE_CUSTOM));
}

static lcc_status_t apply_plan_ec_write(lcc_amw0_backend_t *amw0,
                                        const lcc_write_action_t *action,
                                        const char *stage_name,
                                        lcc_backend_result_t *result) {
  char note[160];

  if (amw0 == NULL || action == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  lcc_backend_result_set_stage(result, stage_name);
  (void)snprintf(note, sizeof(note),
                 "stage=%s write=%s addr=0x%04X value=0x%02X",
                 stage_name != NULL ? stage_name : "fan-write", action->label,
                 (unsigned int)action->addr, (unsigned int)action->value);
  (void)amw0_backend_trace_note(&amw0->transport, note);
  return write_ec_byte(amw0, action->addr, action->value);
}

static lcc_status_t apply_fan_plan(lcc_amw0_backend_t *amw0,
                                   const lcc_apply_plan_t *plan,
                                   lcc_backend_result_t *result) {
  size_t index = 0;
  lcc_status_t status = LCC_OK;
  const char *current_stage = "fan-apply";

  if (amw0 == NULL || plan == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  amw0->write_count = 0u;
  for (index = 0; index < plan->count; ++index) {
    const lcc_write_action_t *action = &plan->actions[index];

    switch (action->kind) {
      case LCC_ACTION_STAGE:
        current_stage = action->label;
        lcc_backend_result_set_stage(result, action->label);
        (void)amw0_backend_trace_note(&amw0->transport, action->label);
        break;
      case LCC_ACTION_CUSTOM_MODE:
        if (action->enabled) {
          status = apply_custom_enable(amw0, result);
          if (status != LCC_OK) {
            return status;
          }
        }
        break;
      case LCC_ACTION_EC_WRITE:
        status = apply_plan_ec_write(amw0, action, current_stage, result);
        if (status != LCC_OK) {
          return status;
        }
        break;
    }
  }

  return LCC_OK;
}

static lcc_status_t amw0_apply_mode(void *ctx, lcc_operating_mode_t mode,
                                    lcc_backend_result_t *result) {
  lcc_amw0_backend_t *amw0 = ctx;
  const char *profile_name = profile_name_for_mode(mode);
  bool changed = false;
  lcc_status_t status = LCC_OK;

  if (amw0 == NULL || profile_name == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  lcc_backend_result_reset(result);
  lcc_backend_result_set_executor(result, "amw0");
  changed = strcmp(amw0->shadow_state.effective.profile, profile_name) != 0;
  lcc_backend_result_set_stage(result, "set-mode-index");

  status = write_ec_byte(amw0, LCC_AMW0_ADDR_MODE_INDEX, mode_index_for_mode(mode));
  if (status != LCC_OK) {
    lcc_backend_result_set_detail(result, "amw0 mode index write failed");
    return status;
  }
  lcc_backend_result_set_stage(result, "set-mode-control");
  status = write_ec_byte(amw0, LCC_AMW0_ADDR_MODE_CONTROL,
                         mode_control_for_mode(mode));
  if (status != LCC_OK) {
    lcc_backend_result_set_detail(result, "amw0 mode control write failed");
    return status;
  }

  set_mode_profile_state(&amw0->shadow_state, mode);
  if (result != NULL) {
    result->changed = changed;
    result->hardware_write = !amw0->transport.dry_run;
  }
  return LCC_OK;
}

static lcc_status_t amw0_apply_profile(void *ctx, const char *profile_name,
                                       lcc_backend_result_t *result) {
  lcc_operating_mode_t mode = LCC_MODE_OFFICE;
  lcc_status_t status = LCC_OK;

  if (profile_name == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  status = profile_name_to_mode(profile_name, &mode);
  if (status != LCC_OK) {
    return status;
  }

  return amw0_apply_mode(ctx, mode, result);
}

static bool power_limits_equal(const lcc_power_limits_t *left,
                               const lcc_power_limits_t *right) {
  return left->pl1.present == right->pl1.present &&
         left->pl1.value == right->pl1.value &&
         left->pl2.present == right->pl2.present &&
         left->pl2.value == right->pl2.value &&
         left->pl4.present == right->pl4.present &&
         left->pl4.value == right->pl4.value &&
         left->tcc_offset.present == right->tcc_offset.present &&
         left->tcc_offset.value == right->tcc_offset.value;
}

static void merge_optional_byte(lcc_optional_byte_t *target,
                                lcc_optional_byte_t source) {
  if (target != NULL && source.present) {
    *target = source;
  }
}

static lcc_status_t amw0_apply_power_limits(void *ctx,
                                            const lcc_power_limits_t *limits,
                                            lcc_backend_result_t *result) {
  lcc_amw0_backend_t *amw0 = ctx;
  lcc_power_limits_t merged;
  bool changed = false;
  lcc_status_t status = LCC_OK;

  if (amw0 == NULL || limits == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (!limits->pl1.present && !limits->pl2.present && !limits->pl4.present &&
      !limits->tcc_offset.present) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  lcc_backend_result_reset(result);
  lcc_backend_result_set_executor(result, "amw0");
  merged = amw0->shadow_state.effective.power_limits;
  merge_optional_byte(&merged.pl1, limits->pl1);
  merge_optional_byte(&merged.pl2, limits->pl2);
  merge_optional_byte(&merged.pl4, limits->pl4);
  merge_optional_byte(&merged.tcc_offset, limits->tcc_offset);
  changed = !amw0->shadow_state.effective.has_power_limits ||
            !power_limits_equal(&amw0->shadow_state.effective.power_limits, &merged);

  if (limits->pl1.present) {
    lcc_backend_result_set_stage(result, "write-pl1");
    status = write_ec_byte(amw0, LCC_AMW0_ADDR_PL1, limits->pl1.value);
    if (status != LCC_OK) {
      lcc_backend_result_set_detail(result, "amw0 PL1 write failed");
      return status;
    }
  }
  if (limits->pl2.present) {
    lcc_backend_result_set_stage(result, "write-pl2");
    status = write_ec_byte(amw0, LCC_AMW0_ADDR_PL2, limits->pl2.value);
    if (status != LCC_OK) {
      lcc_backend_result_set_detail(result, "amw0 PL2 write failed");
      return status;
    }
  }
  if (limits->pl4.present) {
    lcc_backend_result_set_stage(result, "write-pl4");
    status = write_ec_byte(amw0, LCC_AMW0_ADDR_PL4, limits->pl4.value);
    if (status != LCC_OK) {
      lcc_backend_result_set_detail(result, "amw0 PL4 write failed");
      return status;
    }
  }
  if (limits->tcc_offset.present) {
    lcc_backend_result_set_stage(result, "write-tcc-offset");
    status =
        write_ec_byte(amw0, LCC_AMW0_ADDR_TCC_OFFSET, limits->tcc_offset.value);
    if (status != LCC_OK) {
      lcc_backend_result_set_detail(result, "amw0 TCC offset write failed");
      return status;
    }
  }

  amw0->shadow_state.requested.power_limits = merged;
  amw0->shadow_state.effective.power_limits = merged;
  amw0->shadow_state.requested.has_power_limits = true;
  amw0->shadow_state.effective.has_power_limits = true;
  if (result != NULL) {
    result->changed = changed;
    result->hardware_write = !amw0->transport.dry_run;
  }
  return LCC_OK;
}

static lcc_status_t amw0_apply_fan_table(void *ctx, const char *table_name,
                                         lcc_backend_result_t *result) {
  lcc_amw0_backend_t *amw0 = ctx;
  lcc_fan_table_t table;
  lcc_apply_plan_t plan;
  bool changed = false;
  lcc_status_t status = LCC_OK;

  if (amw0 == NULL || table_name == NULL || table_name[0] == '\0') {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  lcc_backend_result_reset(result);
  lcc_backend_result_set_executor(result, "amw0");
  status = lcc_fan_table_load_named(table_name, &table);
  if (status != LCC_OK) {
    lcc_backend_result_set_detail(result, "fan table fixture could not be loaded");
    return status;
  }
  status = lcc_build_fan_plan(&table, &plan);
  if (status != LCC_OK) {
    lcc_backend_result_set_detail(result, "fan table could not be translated into an apply plan");
    return status;
  }

  changed = strcmp(amw0->shadow_state.effective.fan_table, table.name) != 0;
  status = apply_fan_plan(amw0, &plan, result);
  if (status != LCC_OK) {
    if (result != NULL && result->detail[0] == '\0') {
      lcc_backend_result_set_detail(result, "amw0 fan table programming failed");
    }
    return status;
  }

  (void)copy_name(amw0->shadow_state.requested.fan_table,
                  sizeof(amw0->shadow_state.requested.fan_table), table.name);
  (void)copy_name(amw0->shadow_state.effective.fan_table,
                  sizeof(amw0->shadow_state.effective.fan_table), table.name);
  if (table.activated) {
    (void)copy_name(amw0->shadow_state.requested.profile,
                    sizeof(amw0->shadow_state.requested.profile), "custom");
    (void)copy_name(amw0->shadow_state.effective.profile,
                    sizeof(amw0->shadow_state.effective.profile), "custom");
  }
  if (result != NULL) {
    result->changed = changed;
    result->hardware_write = !amw0->transport.dry_run;
  }
  return LCC_OK;
}

const lcc_backend_ops_t lcc_amw0_backend_ops = {
    .name = "amw0",
    .kind = LCC_BACKEND_AMW0,
    .probe = amw0_probe,
    .read_state = amw0_read_state,
    .apply_profile = amw0_apply_profile,
    .apply_mode = amw0_apply_mode,
    .apply_power_limits = amw0_apply_power_limits,
    .apply_fan_table = amw0_apply_fan_table,
};

lcc_status_t lcc_amw0_backend_init(lcc_amw0_backend_t *amw0,
                                   lcc_backend_t *backend,
                                   const char *call_node,
                                   const char *ecrr_path, bool dry_run) {
  int written = 0;
  lcc_status_t status = LCC_OK;

  if (amw0 == NULL || backend == NULL || call_node == NULL ||
      call_node[0] == '\0') {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  memset(amw0, 0, sizeof(*amw0));
  status = amw0_backend_init(&amw0->transport, call_node, dry_run);
  if (status != LCC_OK) {
    return status;
  }

  if (ecrr_path != NULL && ecrr_path[0] != '\0') {
    written = snprintf(amw0->ecrr_path, sizeof(amw0->ecrr_path), "%s", ecrr_path);
    if (written < 0 || (size_t)written >= sizeof(amw0->ecrr_path)) {
      return LCC_ERR_BUFFER_TOO_SMALL;
    }
    amw0->has_ecrr_path = true;
  }

  amw0->route = AMW0_ROUTE_WKBC1;
  seed_shadow_state(amw0);
  lcc_backend_bind(backend, &lcc_amw0_backend_ops, amw0);
  return LCC_OK;
}

void lcc_amw0_backend_fail_after_writes(lcc_amw0_backend_t *amw0,
                                        size_t write_count) {
  if (amw0 == NULL) {
    return;
  }

  amw0->fail_after_writes = write_count;
}
