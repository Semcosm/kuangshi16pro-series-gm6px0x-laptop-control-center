#ifndef LCC_STATE_H
#define LCC_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "lcc/error.h"
#include "lcc/power.h"

#define LCC_STATE_NAME_MAX 64u
#define LCC_STATE_BACKEND_NAME_MAX 32u
#define LCC_STATE_OPERATION_MAX 32u

typedef struct {
  char profile[LCC_STATE_NAME_MAX];
  char fan_table[LCC_STATE_NAME_MAX];
  bool has_power_limits;
  lcc_power_limits_t power_limits;
} lcc_state_target_t;

typedef struct {
  bool has_cpu_fan_rpm;
  bool has_gpu_fan_rpm;
  bool has_cpu_temp_c;
  bool has_gpu_temp_c;
  uint16_t cpu_fan_rpm;
  uint16_t gpu_fan_rpm;
  uint8_t cpu_temp_c;
  uint8_t gpu_temp_c;
} lcc_thermal_state_t;

typedef enum {
  LCC_TRANSACTION_STATE_IDLE = 0,
  LCC_TRANSACTION_STATE_PENDING,
  LCC_TRANSACTION_STATE_FAILED
} lcc_transaction_state_t;

typedef struct {
  lcc_transaction_state_t state;
  char operation[LCC_STATE_OPERATION_MAX];
  bool has_pending_target;
  lcc_state_target_t pending_target;
  lcc_status_t last_error;
} lcc_transaction_snapshot_t;

typedef struct {
  char backend_name[LCC_STATE_BACKEND_NAME_MAX];
  bool hardware_write;
  lcc_state_target_t requested;
  lcc_state_target_t effective;
  lcc_transaction_snapshot_t transaction;
  lcc_thermal_state_t thermal;
} lcc_state_snapshot_t;

#endif
