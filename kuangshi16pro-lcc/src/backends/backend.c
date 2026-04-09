#include "backends/backend.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static lcc_status_t unsupported_operation(lcc_backend_result_t *result) {
  lcc_backend_result_reset(result);
  lcc_backend_result_set_detail(result, "backend operation unavailable");
  return LCC_ERR_NOT_SUPPORTED;
}

lcc_status_t lcc_backend_copy_text(char *buffer, size_t buffer_len,
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

static bool has_text(const char *value) {
  return value != NULL && value[0] != '\0';
}

static void clear_text(char *buffer, size_t buffer_len) {
  if (buffer != NULL && buffer_len > 0u) {
    buffer[0] = '\0';
  }
}

static bool component_has_attribution(
    const lcc_state_component_attribution_t *component) {
  return component != NULL &&
         (has_text(component->source) || has_text(component->freshness));
}

static void merge_text(char *buffer, size_t buffer_len, const char *value,
                       const char *mixed_value) {
  if (buffer == NULL || buffer_len == 0u || !has_text(value) ||
      !has_text(mixed_value)) {
    return;
  }

  if (!has_text(buffer)) {
    (void)lcc_backend_copy_text(buffer, buffer_len, value);
    return;
  }
  if (strcmp(buffer, value) == 0) {
    return;
  }

  clear_text(buffer, buffer_len);
  (void)lcc_backend_copy_text(buffer, buffer_len, mixed_value);
}

static lcc_state_component_attribution_t *power_field_component(
    lcc_effective_state_metadata_t *effective_meta,
    lcc_power_field_kind_t field_kind) {
  if (effective_meta == NULL) {
    return NULL;
  }

  switch (field_kind) {
    case LCC_POWER_FIELD_PL1:
      return &effective_meta->power_fields.pl1;
    case LCC_POWER_FIELD_PL2:
      return &effective_meta->power_fields.pl2;
    case LCC_POWER_FIELD_PL4:
      return &effective_meta->power_fields.pl4;
    case LCC_POWER_FIELD_TCC_OFFSET:
      return &effective_meta->power_fields.tcc_offset;
  }

  return NULL;
}

static const lcc_state_component_attribution_t *power_field_component_const(
    const lcc_effective_state_metadata_t *effective_meta,
    lcc_power_field_kind_t field_kind) {
  if (effective_meta == NULL) {
    return NULL;
  }

  switch (field_kind) {
    case LCC_POWER_FIELD_PL1:
      return &effective_meta->power_fields.pl1;
    case LCC_POWER_FIELD_PL2:
      return &effective_meta->power_fields.pl2;
    case LCC_POWER_FIELD_PL4:
      return &effective_meta->power_fields.pl4;
    case LCC_POWER_FIELD_TCC_OFFSET:
      return &effective_meta->power_fields.tcc_offset;
  }

  return NULL;
}

void lcc_backend_execution_clear(lcc_execution_snapshot_t *execution) {
  if (execution != NULL) {
    memset(execution, 0, sizeof(*execution));
  }
}

lcc_status_t lcc_backend_execution_set(lcc_execution_snapshot_t *execution,
                                       const char *read_state,
                                       const char *apply_profile,
                                       const char *apply_mode,
                                       const char *apply_power_limits,
                                       const char *apply_fan_table,
                                       const char *apply_fan_boost) {
  if (execution == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  lcc_backend_execution_clear(execution);
  if (read_state != NULL && read_state[0] != '\0' &&
      lcc_backend_copy_text(execution->read_state,
                            sizeof(execution->read_state), read_state) !=
          LCC_OK) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }
  if (apply_profile != NULL && apply_profile[0] != '\0' &&
      lcc_backend_copy_text(execution->apply_profile,
                            sizeof(execution->apply_profile), apply_profile) !=
          LCC_OK) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }
  if (apply_mode != NULL && apply_mode[0] != '\0' &&
      lcc_backend_copy_text(execution->apply_mode, sizeof(execution->apply_mode),
                            apply_mode) != LCC_OK) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }
  if (apply_power_limits != NULL && apply_power_limits[0] != '\0' &&
      lcc_backend_copy_text(execution->apply_power_limits,
                            sizeof(execution->apply_power_limits),
                            apply_power_limits) != LCC_OK) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }
  if (apply_fan_table != NULL && apply_fan_table[0] != '\0' &&
      lcc_backend_copy_text(execution->apply_fan_table,
                            sizeof(execution->apply_fan_table),
                            apply_fan_table) != LCC_OK) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }
  if (apply_fan_boost != NULL && apply_fan_boost[0] != '\0' &&
      lcc_backend_copy_text(execution->apply_fan_boost,
                            sizeof(execution->apply_fan_boost),
                            apply_fan_boost) != LCC_OK) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return LCC_OK;
}

lcc_status_t lcc_backend_execution_set_all(lcc_execution_snapshot_t *execution,
                                           const char *backend_name) {
  return lcc_backend_execution_set(execution, backend_name, backend_name,
                                   backend_name, backend_name, backend_name,
                                   backend_name);
}

lcc_status_t lcc_backend_state_set_metadata(
    lcc_state_snapshot_t *state, const char *backend_name,
    const char *backend_selected, const char *fallback_reason,
    const lcc_execution_snapshot_t *execution) {
  if (state == NULL || backend_selected == NULL || backend_selected[0] == '\0' ||
      execution == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  if (backend_name != NULL && backend_name[0] != '\0' &&
      lcc_backend_copy_text(state->backend_name, sizeof(state->backend_name),
                            backend_name) != LCC_OK) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }
  if (lcc_backend_copy_text(state->backend_selected,
                            sizeof(state->backend_selected),
                            backend_selected) != LCC_OK) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  state->execution = *execution;
  if (fallback_reason != NULL && fallback_reason[0] != '\0') {
    if (lcc_backend_copy_text(state->backend_fallback_reason,
                              sizeof(state->backend_fallback_reason),
                              fallback_reason) != LCC_OK) {
      return LCC_ERR_BUFFER_TOO_SMALL;
    }
  } else {
    state->backend_fallback_reason[0] = '\0';
  }

  return LCC_OK;
}

void lcc_backend_effective_meta_clear(
    lcc_effective_state_metadata_t *effective_meta) {
  if (effective_meta != NULL) {
    memset(effective_meta, 0, sizeof(*effective_meta));
  }
}

void lcc_backend_effective_power_clear(
    lcc_effective_state_metadata_t *effective_meta) {
  if (effective_meta == NULL) {
    return;
  }

  memset(&effective_meta->power, 0, sizeof(effective_meta->power));
  memset(&effective_meta->power_fields, 0, sizeof(effective_meta->power_fields));
}

lcc_state_component_attribution_t *lcc_backend_effective_power_field(
    lcc_effective_state_metadata_t *effective_meta,
    lcc_power_field_kind_t field_kind) {
  return power_field_component(effective_meta, field_kind);
}

const lcc_state_component_attribution_t *lcc_backend_effective_power_field_const(
    const lcc_effective_state_metadata_t *effective_meta,
    lcc_power_field_kind_t field_kind) {
  return power_field_component_const(effective_meta, field_kind);
}

lcc_status_t lcc_backend_effective_component_set(
    lcc_state_component_attribution_t *component, const char *source,
    const char *freshness) {
  if (component == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  memset(component, 0, sizeof(*component));
  if (has_text(source) &&
      lcc_backend_copy_text(component->source, sizeof(component->source),
                            source) != LCC_OK) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }
  if (has_text(freshness) &&
      lcc_backend_copy_text(component->freshness,
                            sizeof(component->freshness),
                            freshness) != LCC_OK) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return LCC_OK;
}

lcc_status_t lcc_backend_effective_power_field_set(
    lcc_effective_state_metadata_t *effective_meta,
    lcc_power_field_kind_t field_kind, const char *source,
    const char *freshness) {
  return lcc_backend_effective_component_set(
      power_field_component(effective_meta, field_kind), source, freshness);
}

lcc_status_t lcc_backend_effective_component_merge(
    lcc_state_component_attribution_t *component, const char *source,
    const char *freshness) {
  if (component == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  merge_text(component->source, sizeof(component->source), source, "mixed");
  merge_text(component->freshness, sizeof(component->freshness), freshness,
             "mixed");
  return LCC_OK;
}

lcc_status_t lcc_backend_effective_power_field_merge(
    lcc_effective_state_metadata_t *effective_meta,
    lcc_power_field_kind_t field_kind, const char *source,
    const char *freshness) {
  return lcc_backend_effective_component_merge(
      power_field_component(effective_meta, field_kind), source, freshness);
}

void lcc_backend_effective_power_set_from_limits(
    lcc_effective_state_metadata_t *effective_meta,
    const lcc_power_limits_t *limits, const char *source,
    const char *freshness) {
  if (effective_meta == NULL) {
    return;
  }

  lcc_backend_effective_power_clear(effective_meta);
  if (limits == NULL) {
    return;
  }

  if (limits->pl1.present) {
    (void)lcc_backend_effective_power_field_set(effective_meta,
                                                LCC_POWER_FIELD_PL1, source,
                                                freshness);
  }
  if (limits->pl2.present) {
    (void)lcc_backend_effective_power_field_set(effective_meta,
                                                LCC_POWER_FIELD_PL2, source,
                                                freshness);
  }
  if (limits->pl4.present) {
    (void)lcc_backend_effective_power_field_set(effective_meta,
                                                LCC_POWER_FIELD_PL4, source,
                                                freshness);
  }
  if (limits->tcc_offset.present) {
    (void)lcc_backend_effective_power_field_set(
        effective_meta, LCC_POWER_FIELD_TCC_OFFSET, source, freshness);
  }
}

void lcc_backend_effective_meta_finalize(
    lcc_effective_state_metadata_t *effective_meta) {
  lcc_state_component_attribution_t power_snapshot;
  const lcc_power_field_kind_t power_fields[] = {
      LCC_POWER_FIELD_PL1, LCC_POWER_FIELD_PL2, LCC_POWER_FIELD_PL4,
      LCC_POWER_FIELD_TCC_OFFSET};
  const lcc_state_component_attribution_t *field_component = NULL;
  bool has_power_field_attribution = false;
  size_t index = 0;

  if (effective_meta == NULL) {
    return;
  }

  power_snapshot = effective_meta->power;
  clear_text(effective_meta->power.source, sizeof(effective_meta->power.source));
  clear_text(effective_meta->power.freshness,
             sizeof(effective_meta->power.freshness));
  for (index = 0; index < sizeof(power_fields) / sizeof(power_fields[0]);
       ++index) {
    field_component =
        power_field_component_const(effective_meta, power_fields[index]);
    if (!component_has_attribution(field_component)) {
      continue;
    }

    has_power_field_attribution = true;
    merge_text(effective_meta->power.source, sizeof(effective_meta->power.source),
               field_component->source, "mixed");
    merge_text(effective_meta->power.freshness,
               sizeof(effective_meta->power.freshness),
               field_component->freshness, "mixed");
  }
  if (!has_power_field_attribution) {
    effective_meta->power = power_snapshot;
  }

  clear_text(effective_meta->source, sizeof(effective_meta->source));
  clear_text(effective_meta->freshness, sizeof(effective_meta->freshness));

  merge_text(effective_meta->source, sizeof(effective_meta->source),
             effective_meta->profile.source, "mixed");
  merge_text(effective_meta->source, sizeof(effective_meta->source),
             effective_meta->fan_table.source, "mixed");
  merge_text(effective_meta->source, sizeof(effective_meta->source),
             effective_meta->fan_boost.source, "mixed");
  merge_text(effective_meta->source, sizeof(effective_meta->source),
             effective_meta->power.source, "mixed");
  merge_text(effective_meta->source, sizeof(effective_meta->source),
             effective_meta->thermal.source, "mixed");

  merge_text(effective_meta->freshness, sizeof(effective_meta->freshness),
             effective_meta->profile.freshness, "mixed");
  merge_text(effective_meta->freshness, sizeof(effective_meta->freshness),
             effective_meta->fan_table.freshness, "mixed");
  merge_text(effective_meta->freshness, sizeof(effective_meta->freshness),
             effective_meta->fan_boost.freshness, "mixed");
  merge_text(effective_meta->freshness, sizeof(effective_meta->freshness),
             effective_meta->power.freshness, "mixed");
  merge_text(effective_meta->freshness, sizeof(effective_meta->freshness),
             effective_meta->thermal.freshness, "mixed");
}

void lcc_backend_state_finalize_effective_meta(lcc_state_snapshot_t *state) {
  if (state != NULL) {
    lcc_backend_effective_meta_finalize(&state->effective_meta);
  }
}

void lcc_backend_state_mark_effective_cached(lcc_state_snapshot_t *state) {
  bool has_thermal = false;

  if (state == NULL) {
    return;
  }

  has_thermal = state->thermal.has_cpu_temp_c || state->thermal.has_gpu_temp_c ||
                state->thermal.has_cpu_fan_rpm ||
                state->thermal.has_gpu_fan_rpm ||
                state->thermal.has_vendor_fan_level;

  (void)lcc_backend_effective_component_set(
      &state->effective_meta.profile,
      state->effective.profile[0] != '\0' ? "cache" : NULL,
      state->effective.profile[0] != '\0' ? "cache" : NULL);
  (void)lcc_backend_effective_component_set(
      &state->effective_meta.fan_table,
      state->effective.fan_table[0] != '\0' ? "cache" : NULL,
      state->effective.fan_table[0] != '\0' ? "cache" : NULL);
  (void)lcc_backend_effective_component_set(
      &state->effective_meta.fan_boost,
      state->effective.has_fan_boost ? "cache" : NULL,
      state->effective.has_fan_boost ? "cache" : NULL);
  lcc_backend_effective_power_set_from_limits(
      &state->effective_meta,
      state->effective.has_power_limits ? &state->effective.power_limits : NULL,
      "cache", "cache");
  (void)lcc_backend_effective_component_set(
      &state->effective_meta.thermal, has_thermal ? "cache" : NULL,
      has_thermal ? "cache" : NULL);
  lcc_backend_effective_meta_finalize(&state->effective_meta);
}

void lcc_backend_result_reset(lcc_backend_result_t *result) {
  if (result != NULL) {
    result->changed = false;
    result->hardware_write = false;
    result->reboot_required = false;
    result->stage[0] = '\0';
    result->executor_backend[0] = '\0';
    result->detail[0] = '\0';
  }
}

void lcc_backend_result_set_stage(lcc_backend_result_t *result,
                                  const char *stage) {
  if (result == NULL || stage == NULL || stage[0] == '\0') {
    return;
  }

  (void)lcc_backend_copy_text(result->stage, sizeof(result->stage), stage);
}

void lcc_backend_result_set_executor(lcc_backend_result_t *result,
                                     const char *backend_name) {
  if (result == NULL || backend_name == NULL || backend_name[0] == '\0') {
    return;
  }

  (void)lcc_backend_copy_text(result->executor_backend,
                              sizeof(result->executor_backend), backend_name);
}

void lcc_backend_result_set_detail(lcc_backend_result_t *result,
                                   const char *detail) {
  if (result == NULL || detail == NULL || detail[0] == '\0') {
    return;
  }

  (void)lcc_backend_copy_text(result->detail, sizeof(result->detail), detail);
}

void lcc_backend_bind(lcc_backend_t *backend, const lcc_backend_ops_t *ops,
                      void *ctx) {
  if (backend == NULL) {
    return;
  }

  backend->ops = ops;
  backend->ctx = ctx;
}

const char *lcc_backend_name(const lcc_backend_t *backend) {
  if (backend == NULL || backend->ops == NULL || backend->ops->name == NULL) {
    return "unknown";
  }

  return backend->ops->name;
}

lcc_status_t lcc_backend_probe(const lcc_backend_t *backend,
                               lcc_backend_capabilities_t *capabilities,
                               lcc_backend_result_t *result) {
  if (backend == NULL || capabilities == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->probe == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->probe(backend->ctx, capabilities, result);
}

lcc_status_t lcc_backend_read_state(const lcc_backend_t *backend,
                                    lcc_state_snapshot_t *state,
                                    lcc_backend_result_t *result) {
  if (backend == NULL || state == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->read_state == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->read_state(backend->ctx, state, result);
}

lcc_status_t lcc_backend_apply_profile(const lcc_backend_t *backend,
                                       const char *profile_name,
                                       lcc_backend_result_t *result) {
  if (backend == NULL || profile_name == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->apply_profile == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->apply_profile(backend->ctx, profile_name, result);
}

lcc_status_t lcc_backend_apply_mode(const lcc_backend_t *backend,
                                    lcc_operating_mode_t mode,
                                    lcc_backend_result_t *result) {
  if (backend == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->apply_mode == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->apply_mode(backend->ctx, mode, result);
}

lcc_status_t lcc_backend_apply_power_limits(const lcc_backend_t *backend,
                                            const lcc_power_limits_t *limits,
                                            lcc_backend_result_t *result) {
  if (backend == NULL || limits == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->apply_power_limits == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->apply_power_limits(backend->ctx, limits, result);
}

lcc_status_t lcc_backend_apply_fan_table(const lcc_backend_t *backend,
                                         const char *table_name,
                                         lcc_backend_result_t *result) {
  if (backend == NULL || table_name == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->apply_fan_table == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->apply_fan_table(backend->ctx, table_name, result);
}

lcc_status_t lcc_backend_apply_fan_boost(const lcc_backend_t *backend,
                                         bool enabled,
                                         lcc_backend_result_t *result) {
  if (backend == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->apply_fan_boost == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->apply_fan_boost(backend->ctx, enabled, result);
}
