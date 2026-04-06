#ifndef LCC_BACKEND_H
#define LCC_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lcc/error.h"
#include "lcc/fan.h"
#include "lcc/profile.h"
#include "lcc/power.h"
#include "lcc/state.h"

#define AMW0_EXPR_MAX 256
#define AMW0_REPLY_MAX 128

typedef enum {
  LCC_BACKEND_STANDARD = 0,
  LCC_BACKEND_AMW0,
  LCC_BACKEND_UEFI,
  LCC_BACKEND_MOCK
} lcc_backend_kind_t;

typedef struct {
  bool can_read_state;
  bool can_set_mode;
  bool can_set_power;
  bool can_set_fan_table;
  bool has_platform_profile;
  bool has_powercap;
  bool needs_reboot_for_mux;
} lcc_backend_capabilities_t;

typedef struct {
  uint8_t slot;
  uint8_t sa[4];
  uint16_t sac1;
} amw0_packet_t;

typedef enum {
  AMW0_ROUTE_WKBC0 = 0x0000u,
  AMW0_ROUTE_WKBC1 = 0x0001u,
  AMW0_ROUTE_RKBC = 0x0100u,
  AMW0_ROUTE_SCMD = 0x0200u
} amw0_route_t;

typedef struct {
  char call_node[128];
  bool dry_run;
} amw0_backend_t;

typedef struct {
  const char *name;
  lcc_backend_kind_t kind;
  lcc_status_t (*probe)(void *ctx, lcc_backend_capabilities_t *capabilities);
  lcc_status_t (*read_state)(void *ctx, lcc_state_snapshot_t *state);
  lcc_status_t (*apply_profile)(void *ctx,
                                const lcc_profile_document_t *document,
                                lcc_apply_plan_t *plan);
  lcc_status_t (*apply_power)(void *ctx, const lcc_power_limits_t *limits,
                              lcc_apply_plan_t *plan);
  lcc_status_t (*apply_fan_table)(void *ctx, const lcc_fan_table_t *table,
                                  lcc_apply_plan_t *plan);
} lcc_backend_ops_t;

lcc_status_t amw0_backend_init(amw0_backend_t *backend, const char *call_node,
                               bool dry_run);
lcc_status_t amw0_packet_format_expr(const amw0_packet_t *packet, char *buffer,
                                     size_t buffer_len);
lcc_status_t amw0_backend_eval(amw0_backend_t *backend, const char *expr,
                               char *reply, size_t reply_len);
lcc_status_t amw0_backend_send_packet(amw0_backend_t *backend,
                                      const amw0_packet_t *packet, char *reply,
                                      size_t reply_len);
lcc_status_t amw0_backend_read_wqac(amw0_backend_t *backend, uint8_t index,
                                    char *reply, size_t reply_len);
lcc_status_t amw0_backend_read_ecrr(amw0_backend_t *backend,
                                    const char *ecrr_path, uint16_t offset,
                                    char *reply, size_t reply_len);
lcc_status_t amw0_backend_probe_ecrr_path(amw0_backend_t *backend,
                                          char *buffer, size_t buffer_len);

#endif
