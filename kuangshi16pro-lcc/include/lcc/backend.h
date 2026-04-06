#ifndef LCC_BACKEND_H
#define LCC_BACKEND_H

#include <stdbool.h>

#include "lcc/error.h"
#include "lcc/profile.h"
#include "lcc/power.h"
#include "lcc/state.h"

typedef enum {
  LCC_BACKEND_STANDARD = 0,
  LCC_BACKEND_AMW0,
  LCC_BACKEND_UEFI,
  LCC_BACKEND_MOCK
} lcc_backend_kind_t;

typedef struct {
  bool can_read_state;
  bool can_apply_profile;
  bool can_apply_mode;
  bool can_apply_power_limits;
  bool can_apply_fan_table;
  bool has_platform_profile;
  bool has_powercap;
  bool needs_reboot_for_mux;
} lcc_backend_capabilities_t;

typedef struct {
  bool changed;
  bool hardware_write;
  bool reboot_required;
  char stage[LCC_STATE_STAGE_MAX];
} lcc_backend_result_t;

struct lcc_backend_ops;

typedef struct {
  const struct lcc_backend_ops *ops;
  void *ctx;
} lcc_backend_t;

typedef struct lcc_backend_ops {
  const char *name;
  lcc_backend_kind_t kind;
  lcc_status_t (*probe)(void *ctx, lcc_backend_capabilities_t *capabilities,
                        lcc_backend_result_t *result);
  lcc_status_t (*read_state)(void *ctx, lcc_state_snapshot_t *state,
                             lcc_backend_result_t *result);
  lcc_status_t (*apply_profile)(void *ctx, const char *profile_name,
                                lcc_backend_result_t *result);
  lcc_status_t (*apply_mode)(void *ctx, lcc_operating_mode_t mode,
                             lcc_backend_result_t *result);
  lcc_status_t (*apply_power_limits)(void *ctx,
                                     const lcc_power_limits_t *limits,
                                     lcc_backend_result_t *result);
  lcc_status_t (*apply_fan_table)(void *ctx, const char *table_name,
                                  lcc_backend_result_t *result);
} lcc_backend_ops_t;

void lcc_backend_result_reset(lcc_backend_result_t *result);
void lcc_backend_bind(lcc_backend_t *backend, const lcc_backend_ops_t *ops,
                      void *ctx);
const char *lcc_backend_name(const lcc_backend_t *backend);
lcc_status_t lcc_backend_probe(const lcc_backend_t *backend,
                               lcc_backend_capabilities_t *capabilities,
                               lcc_backend_result_t *result);
lcc_status_t lcc_backend_read_state(const lcc_backend_t *backend,
                                    lcc_state_snapshot_t *state,
                                    lcc_backend_result_t *result);
lcc_status_t lcc_backend_apply_profile(const lcc_backend_t *backend,
                                       const char *profile_name,
                                       lcc_backend_result_t *result);
lcc_status_t lcc_backend_apply_mode(const lcc_backend_t *backend,
                                    lcc_operating_mode_t mode,
                                    lcc_backend_result_t *result);
lcc_status_t lcc_backend_apply_power_limits(const lcc_backend_t *backend,
                                            const lcc_power_limits_t *limits,
                                            lcc_backend_result_t *result);
lcc_status_t lcc_backend_apply_fan_table(const lcc_backend_t *backend,
                                         const char *table_name,
                                         lcc_backend_result_t *result);

#endif
