#ifndef LCC_BACKENDS_AMW0_WMBC_PACK_H
#define LCC_BACKENDS_AMW0_WMBC_PACK_H

#include "lcc/backend.h"

lcc_status_t amw0_packet_format_expr(const amw0_packet_t *packet, char *buffer,
                                     size_t buffer_len);

#endif
