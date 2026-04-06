#ifndef LCC_CORE_CAPABILITIES_DETECT_H
#define LCC_CORE_CAPABILITIES_DETECT_H

#include <stddef.h>

#include "lcc/backend.h"
#include "lcc/error.h"

lcc_status_t lcc_capabilities_detect_json(
    const lcc_backend_t *backend,
    const lcc_backend_capabilities_t *backend_capabilities,
    const char *capabilities_path, char *buffer, size_t buffer_len);

#endif
