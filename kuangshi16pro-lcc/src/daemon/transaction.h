#ifndef LCC_DAEMON_TRANSACTION_H
#define LCC_DAEMON_TRANSACTION_H

#include "daemon/manager.h"

typedef enum {
  LCC_TRANSACTION_PROFILE = 0,
  LCC_TRANSACTION_MODE,
  LCC_TRANSACTION_POWER_LIMITS,
  LCC_TRANSACTION_FAN_TABLE
} lcc_transaction_kind_t;

typedef struct {
  lcc_transaction_kind_t kind;
  union {
    const char *profile_name;
    const char *mode_name;
    const char *fan_table_name;
    const lcc_power_limits_t *power_limits;
  } input;
} lcc_transaction_request_t;

lcc_status_t lcc_transaction_refresh_state(lcc_manager_t *manager);
lcc_status_t lcc_transaction_execute(lcc_manager_t *manager,
                                     const lcc_transaction_request_t *request);

#endif
