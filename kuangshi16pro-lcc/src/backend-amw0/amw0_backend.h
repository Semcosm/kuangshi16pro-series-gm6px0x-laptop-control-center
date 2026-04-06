#ifndef AMW0_BACKEND_H
#define AMW0_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/lcc_error.h"

#define AMW0_EXPR_MAX 256
#define AMW0_REPLY_MAX 128

typedef enum {
  AMW0_ROUTE_WKBC0 = 0x0000u,
  AMW0_ROUTE_WKBC1 = 0x0001u,
  AMW0_ROUTE_RKBC = 0x0100u,
  AMW0_ROUTE_SCMD = 0x0200u
} amw0_route_t;

typedef struct {
  uint8_t slot;
  uint8_t sa[4];
  uint16_t sac1;
} amw0_packet_t;

typedef struct {
  char call_node[128];
  bool dry_run;
} amw0_backend_t;

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
