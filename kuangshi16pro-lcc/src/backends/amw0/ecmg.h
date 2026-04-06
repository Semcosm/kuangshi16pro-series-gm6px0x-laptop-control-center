#ifndef LCC_BACKENDS_AMW0_ECMG_H
#define LCC_BACKENDS_AMW0_ECMG_H

#include "backends/amw0/transport.h"

lcc_status_t lcc_amw0_print_mode_decode(amw0_backend_t *backend,
                                        const char *ecrr_path);
lcc_status_t lcc_amw0_print_thermal_decode(amw0_backend_t *backend,
                                           const char *ecrr_path);

#endif
