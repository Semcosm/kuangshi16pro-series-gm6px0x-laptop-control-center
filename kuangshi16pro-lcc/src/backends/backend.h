#ifndef LCC_BACKENDS_BACKEND_H
#define LCC_BACKENDS_BACKEND_H

#include "lcc/backend.h"

typedef struct {
  lcc_backend_capabilities_t capabilities;
  lcc_state_snapshot_t state;
} lcc_mock_backend_t;

void lcc_mock_backend_seed_defaults(lcc_mock_backend_t *mock);
lcc_status_t lcc_mock_backend_init(lcc_mock_backend_t *mock,
                                   lcc_backend_t *backend);

extern const lcc_backend_ops_t lcc_mock_backend_ops;

#endif
