#ifndef LCC_CLI_AMW0_CLI_H
#define LCC_CLI_AMW0_CLI_H

#include <stdbool.h>

#include "backends/amw0/transport.h"
#include "lcc/error.h"

lcc_status_t lcc_cli_init_amw0_backend(amw0_backend_t *backend,
                                       const char *call_node, bool dry_run);
lcc_status_t lcc_cli_print_transport_snapshot(amw0_backend_t *backend);

#endif
