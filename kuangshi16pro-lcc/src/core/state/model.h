#ifndef LCC_CORE_STATE_MODEL_H
#define LCC_CORE_STATE_MODEL_H

#include <stddef.h>

#include "lcc/backend.h"
#include "lcc/error.h"
#include "lcc/state.h"

lcc_status_t lcc_state_render_json(
    const lcc_state_snapshot_t *state,
    const lcc_backend_capabilities_t *backend_capabilities, char *buffer,
    size_t buffer_len);
lcc_status_t lcc_state_render_thermal_json(const lcc_state_snapshot_t *state,
                                           char *buffer, size_t buffer_len);

#endif
