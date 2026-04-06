#include "lcc/backend.h"

#include <stddef.h>

static lcc_status_t unsupported_operation(lcc_backend_result_t *result) {
  lcc_backend_result_reset(result);
  return LCC_ERR_NOT_SUPPORTED;
}

void lcc_backend_result_reset(lcc_backend_result_t *result) {
  if (result != NULL) {
    result->changed = false;
    result->hardware_write = false;
    result->reboot_required = false;
    result->stage[0] = '\0';
  }
}

void lcc_backend_bind(lcc_backend_t *backend, const lcc_backend_ops_t *ops,
                      void *ctx) {
  if (backend == NULL) {
    return;
  }

  backend->ops = ops;
  backend->ctx = ctx;
}

const char *lcc_backend_name(const lcc_backend_t *backend) {
  if (backend == NULL || backend->ops == NULL || backend->ops->name == NULL) {
    return "unknown";
  }

  return backend->ops->name;
}

lcc_status_t lcc_backend_probe(const lcc_backend_t *backend,
                               lcc_backend_capabilities_t *capabilities,
                               lcc_backend_result_t *result) {
  if (backend == NULL || capabilities == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->probe == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->probe(backend->ctx, capabilities, result);
}

lcc_status_t lcc_backend_read_state(const lcc_backend_t *backend,
                                    lcc_state_snapshot_t *state,
                                    lcc_backend_result_t *result) {
  if (backend == NULL || state == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->read_state == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->read_state(backend->ctx, state, result);
}

lcc_status_t lcc_backend_apply_profile(const lcc_backend_t *backend,
                                       const char *profile_name,
                                       lcc_backend_result_t *result) {
  if (backend == NULL || profile_name == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->apply_profile == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->apply_profile(backend->ctx, profile_name, result);
}

lcc_status_t lcc_backend_apply_mode(const lcc_backend_t *backend,
                                    lcc_operating_mode_t mode,
                                    lcc_backend_result_t *result) {
  if (backend == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->apply_mode == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->apply_mode(backend->ctx, mode, result);
}

lcc_status_t lcc_backend_apply_power_limits(const lcc_backend_t *backend,
                                            const lcc_power_limits_t *limits,
                                            lcc_backend_result_t *result) {
  if (backend == NULL || limits == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->apply_power_limits == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->apply_power_limits(backend->ctx, limits, result);
}

lcc_status_t lcc_backend_apply_fan_table(const lcc_backend_t *backend,
                                         const char *table_name,
                                         lcc_backend_result_t *result) {
  if (backend == NULL || table_name == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (backend->ops == NULL || backend->ops->apply_fan_table == NULL) {
    return unsupported_operation(result);
  }

  return backend->ops->apply_fan_table(backend->ctx, table_name, result);
}
