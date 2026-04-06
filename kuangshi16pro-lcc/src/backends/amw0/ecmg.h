#ifndef LCC_BACKENDS_AMW0_ECMG_H
#define LCC_BACKENDS_AMW0_ECMG_H

#include <stdint.h>

#include "backends/amw0/transport.h"

lcc_status_t lcc_amw0_read_ecrr_u32(amw0_backend_t *backend,
                                    const char *ecrr_path, uint16_t offset,
                                    uint32_t *value);
lcc_status_t lcc_amw0_read_ecrr_u8(amw0_backend_t *backend,
                                   const char *ecrr_path, uint16_t offset,
                                   uint8_t *value);
lcc_status_t lcc_amw0_print_mode_decode(amw0_backend_t *backend,
                                        const char *ecrr_path);
lcc_status_t lcc_amw0_print_thermal_decode(amw0_backend_t *backend,
                                           const char *ecrr_path);

#endif
