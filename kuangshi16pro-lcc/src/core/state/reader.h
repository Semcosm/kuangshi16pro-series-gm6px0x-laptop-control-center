#ifndef LCC_CORE_STATE_READER_H
#define LCC_CORE_STATE_READER_H

#include "lcc/backend.h"
#include "lcc/error.h"
#include "lcc/state.h"

lcc_status_t lcc_state_reader_refresh(const lcc_backend_t *backend,
                                      lcc_state_snapshot_t *state);

#endif
