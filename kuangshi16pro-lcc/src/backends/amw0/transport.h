#ifndef LCC_BACKENDS_AMW0_TRANSPORT_H
#define LCC_BACKENDS_AMW0_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lcc/error.h"

#define AMW0_EXPR_MAX 256u
#define AMW0_REPLY_MAX 128u

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
  char trace_file[256];
  bool dry_run;
  bool trace_enabled;
} amw0_backend_t;

lcc_status_t amw0_backend_init(amw0_backend_t *backend, const char *call_node,
                               bool dry_run);
lcc_status_t amw0_backend_set_trace(amw0_backend_t *backend,
                                    const char *trace_file);
lcc_status_t amw0_backend_eval(amw0_backend_t *backend, const char *expr,
                               char *reply, size_t reply_len);
lcc_status_t amw0_backend_send_packet(amw0_backend_t *backend,
                                      const amw0_packet_t *packet, char *reply,
                                      size_t reply_len);
lcc_status_t amw0_backend_send_wkbc(amw0_backend_t *backend,
                                    amw0_route_t route, uint8_t sa00,
                                    uint8_t sa01, uint8_t sa02, uint8_t sa03,
                                    char *reply, size_t reply_len);
lcc_status_t amw0_backend_send_ec_write(amw0_backend_t *backend,
                                        amw0_route_t route, uint16_t offset,
                                        uint8_t value, char *reply,
                                        size_t reply_len);
lcc_status_t amw0_backend_read_wqac(amw0_backend_t *backend, uint8_t index,
                                    char *reply, size_t reply_len);
lcc_status_t amw0_backend_read_ecrr(amw0_backend_t *backend,
                                    const char *ecrr_path, uint16_t offset,
                                    char *reply, size_t reply_len);
lcc_status_t amw0_backend_probe_ecrr_path(amw0_backend_t *backend,
                                          char *buffer, size_t buffer_len);

#endif
