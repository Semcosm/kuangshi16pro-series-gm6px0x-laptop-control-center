#include "backends/backend.h"

const lcc_backend_ops_t lcc_mock_backend_ops = {
    .name = "mock",
    .kind = LCC_BACKEND_MOCK,
    .probe = NULL,
    .read_state = NULL,
    .apply_profile = NULL,
    .apply_power = NULL,
    .apply_fan_table = NULL,
};
