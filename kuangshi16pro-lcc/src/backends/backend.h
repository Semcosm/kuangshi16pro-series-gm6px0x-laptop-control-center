#ifndef LCC_BACKENDS_BACKEND_H
#define LCC_BACKENDS_BACKEND_H

#include <stddef.h>

#include "backends/amw0/transport.h"
#include "lcc/backend.h"

typedef struct {
  lcc_backend_capabilities_t capabilities;
  lcc_state_snapshot_t state;
  lcc_status_t fail_profile_status;
  lcc_status_t fail_mode_status;
  lcc_status_t fail_power_status;
  lcc_status_t fail_fan_status;
} lcc_mock_backend_t;

void lcc_mock_backend_seed_defaults(lcc_mock_backend_t *mock);
lcc_status_t lcc_mock_backend_init(lcc_mock_backend_t *mock,
                                   lcc_backend_t *backend);
void lcc_mock_backend_fail_next_profile(lcc_mock_backend_t *mock,
                                        lcc_status_t status);
void lcc_mock_backend_fail_next_mode(lcc_mock_backend_t *mock,
                                     lcc_status_t status);
void lcc_mock_backend_fail_next_power(lcc_mock_backend_t *mock,
                                      lcc_status_t status);
void lcc_mock_backend_fail_next_fan(lcc_mock_backend_t *mock,
                                    lcc_status_t status);

lcc_status_t lcc_backend_copy_text(char *buffer, size_t buffer_len,
                                   const char *value);
void lcc_backend_execution_clear(lcc_execution_snapshot_t *execution);
lcc_status_t lcc_backend_execution_set(lcc_execution_snapshot_t *execution,
                                       const char *read_state,
                                       const char *apply_profile,
                                       const char *apply_mode,
                                       const char *apply_power_limits,
                                       const char *apply_fan_table);
lcc_status_t lcc_backend_execution_set_all(lcc_execution_snapshot_t *execution,
                                           const char *backend_name);
lcc_status_t lcc_backend_state_set_metadata(
    lcc_state_snapshot_t *state, const char *backend_name,
    const char *backend_selected, const char *fallback_reason,
    const lcc_execution_snapshot_t *execution);
void lcc_backend_effective_meta_clear(
    lcc_effective_state_metadata_t *effective_meta);
lcc_status_t lcc_backend_effective_component_set(
    lcc_state_component_attribution_t *component, const char *source,
    const char *freshness);
lcc_status_t lcc_backend_effective_component_merge(
    lcc_state_component_attribution_t *component, const char *source,
    const char *freshness);
void lcc_backend_effective_meta_finalize(
    lcc_effective_state_metadata_t *effective_meta);
void lcc_backend_state_finalize_effective_meta(lcc_state_snapshot_t *state);
void lcc_backend_state_mark_effective_cached(lcc_state_snapshot_t *state);

typedef enum {
  LCC_POWER_FIELD_PL1 = 0,
  LCC_POWER_FIELD_PL2,
  LCC_POWER_FIELD_PL4,
  LCC_POWER_FIELD_TCC_OFFSET,
} lcc_power_field_kind_t;

void lcc_backend_effective_power_clear(
    lcc_effective_state_metadata_t *effective_meta);
lcc_state_component_attribution_t *lcc_backend_effective_power_field(
    lcc_effective_state_metadata_t *effective_meta,
    lcc_power_field_kind_t field_kind);
const lcc_state_component_attribution_t *lcc_backend_effective_power_field_const(
    const lcc_effective_state_metadata_t *effective_meta,
    lcc_power_field_kind_t field_kind);
lcc_status_t lcc_backend_effective_power_field_set(
    lcc_effective_state_metadata_t *effective_meta,
    lcc_power_field_kind_t field_kind, const char *source,
    const char *freshness);
lcc_status_t lcc_backend_effective_power_field_merge(
    lcc_effective_state_metadata_t *effective_meta,
    lcc_power_field_kind_t field_kind, const char *source,
    const char *freshness);
void lcc_backend_effective_power_set_from_limits(
    lcc_effective_state_metadata_t *effective_meta,
    const lcc_power_limits_t *limits, const char *source,
    const char *freshness);

typedef struct {
  char root[256];
  char hwmon_dir[256];
  char thermal_dir[256];
  char platform_profile_path[256];
  char powercap_dir[256];
} lcc_standard_backend_t;

lcc_status_t lcc_standard_backend_init(lcc_standard_backend_t *standard,
                                       lcc_backend_t *backend);
lcc_status_t lcc_standard_backend_init_at_root(lcc_standard_backend_t *standard,
                                               lcc_backend_t *backend,
                                               const char *root);

typedef struct {
  amw0_backend_t transport;
  char ecrr_path[64];
  bool has_ecrr_path;
  amw0_route_t route;
  size_t fail_after_writes;
  size_t write_count;
  lcc_state_snapshot_t shadow_state;
} lcc_amw0_backend_t;

lcc_status_t lcc_amw0_backend_init(lcc_amw0_backend_t *amw0,
                                   lcc_backend_t *backend,
                                   const char *call_node,
                                   const char *ecrr_path,
                                   bool dry_run);
void lcc_amw0_backend_fail_after_writes(lcc_amw0_backend_t *amw0,
                                        size_t write_count);

typedef struct {
  lcc_backend_t *standard_backend;
  lcc_backend_t *amw0_backend;
  bool standard_available;
  bool amw0_available;
  lcc_status_t standard_status;
  lcc_status_t amw0_status;
  lcc_backend_capabilities_t standard_capabilities;
  lcc_backend_capabilities_t amw0_capabilities;
  lcc_execution_snapshot_t execution;
  char backend_selected[LCC_STATE_BACKEND_NAME_MAX];
  char backend_fallback_reason[LCC_STATE_REASON_MAX];
} lcc_converged_backend_t;

lcc_status_t lcc_converged_backend_init(
    lcc_converged_backend_t *converged, lcc_backend_t *backend,
    lcc_backend_t *standard_backend, lcc_status_t standard_status,
    const lcc_backend_capabilities_t *standard_capabilities,
    lcc_backend_t *amw0_backend, lcc_status_t amw0_status,
    const lcc_backend_capabilities_t *amw0_capabilities);

extern const lcc_backend_ops_t lcc_mock_backend_ops;
extern const lcc_backend_ops_t lcc_standard_backend_ops;
extern const lcc_backend_ops_t lcc_amw0_backend_ops;
extern const lcc_backend_ops_t lcc_converged_standard_backend_ops;
extern const lcc_backend_ops_t lcc_converged_amw0_backend_ops;

#endif
