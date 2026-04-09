#include "daemon/manager.h"

#include <stdio.h>
#include <string.h>

#include "core/capabilities/detect.h"
#include "core/state/model.h"
#include "core/state/reader.h"
#include "daemon/transaction.h"

static const char *const default_capability_paths[] = {
    "data/capabilities/gm6px0x.json",
    "kuangshi16pro-lcc/data/capabilities/gm6px0x.json",
    "/usr/share/kuangshi16pro-lcc/data/capabilities/gm6px0x.json",
};

static const char *fallback_capabilities_json(void) {
  return "{"
         "\"model\":\"GM6PX0X\","
         "\"service\":\"lccd\","
         "\"backend_order\":[\"standard\",\"amw0\",\"uefi\"],"
         "\"features\":{\"fan_table_1p5\":true,\"smart_apc\":true,"
         "\"gpu_mux\":\"experimental\"}"
         "}";
}

static void set_default_capabilities(lcc_manager_t *manager) {
  (void)snprintf(manager->capabilities_json, sizeof(manager->capabilities_json),
                 "%s", fallback_capabilities_json());
}

static lcc_status_t init_capabilities(lcc_manager_t *manager,
                                      const char *capabilities_path) {
  size_t index = 0;
  lcc_status_t status = LCC_OK;
  const char *backend_selected = NULL;

  backend_selected = manager->state_cache.backend_selected[0] != '\0'
                         ? manager->state_cache.backend_selected
                         : lcc_backend_name(manager->backend);

  if (capabilities_path != NULL) {
    status = lcc_capabilities_detect_json(
        backend_selected, &manager->backend_capabilities, capabilities_path,
        manager->capabilities_json, sizeof(manager->capabilities_json));
    if (status != LCC_OK) {
      set_default_capabilities(manager);
      return LCC_OK;
    }
    (void)snprintf(manager->capabilities_path, sizeof(manager->capabilities_path),
                   "%s", capabilities_path);
    return LCC_OK;
  }

  for (index = 0;
       index < sizeof(default_capability_paths) / sizeof(default_capability_paths[0]);
       ++index) {
    status = lcc_capabilities_detect_json(
        backend_selected, &manager->backend_capabilities,
        default_capability_paths[index], manager->capabilities_json,
        sizeof(manager->capabilities_json));
    if (status == LCC_OK) {
      (void)snprintf(manager->capabilities_path,
                     sizeof(manager->capabilities_path), "%s",
                     default_capability_paths[index]);
      return LCC_OK;
    }
  }

  manager->capabilities_path[0] = '\0';
  set_default_capabilities(manager);
  return LCC_OK;
}

lcc_status_t lcc_manager_init(lcc_manager_t *manager, lcc_backend_t *backend,
                              const char *capabilities_path) {
  lcc_backend_result_t result;
  lcc_status_t status = LCC_OK;

  if (manager == NULL || backend == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  memset(manager, 0, sizeof(*manager));
  manager->backend = backend;

  status = lcc_backend_probe(manager->backend, &manager->backend_capabilities,
                             &result);
  if (status != LCC_OK) {
    return status;
  }

  status = lcc_transaction_refresh_state(manager);
  if (status != LCC_OK) {
    return status;
  }

  return init_capabilities(manager, capabilities_path);
}

const char *lcc_manager_capabilities_json(const lcc_manager_t *manager) {
  if (manager == NULL || manager->capabilities_json[0] == '\0') {
    return fallback_capabilities_json();
  }

  return manager->capabilities_json;
}

lcc_status_t lcc_manager_get_state_json(const lcc_manager_t *manager,
                                        char *buffer, size_t buffer_len) {
  if (manager == NULL || buffer == NULL || buffer_len == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  return lcc_state_render_json(&manager->state_cache,
                               &manager->backend_capabilities, buffer,
                               buffer_len);
}

lcc_status_t lcc_manager_get_thermal_json(const lcc_manager_t *manager,
                                          char *buffer, size_t buffer_len) {
  if (manager == NULL || buffer == NULL || buffer_len == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  return lcc_state_render_thermal_json(&manager->state_cache, buffer,
                                       buffer_len);
}

lcc_status_t lcc_manager_set_mode(lcc_manager_t *manager,
                                  const char *mode_name) {
  const lcc_transaction_request_t request = {
      .kind = LCC_TRANSACTION_MODE,
      .input.mode_name = mode_name,
  };

  return lcc_transaction_execute(manager, &request);
}

lcc_status_t lcc_manager_set_profile(lcc_manager_t *manager,
                                     const char *profile_name) {
  const lcc_transaction_request_t request = {
      .kind = LCC_TRANSACTION_PROFILE,
      .input.profile_name = profile_name,
  };

  return lcc_transaction_execute(manager, &request);
}

lcc_status_t lcc_manager_apply_fan_table(lcc_manager_t *manager,
                                         const char *table_name) {
  const lcc_transaction_request_t request = {
      .kind = LCC_TRANSACTION_FAN_TABLE,
      .input.fan_table_name = table_name,
  };

  return lcc_transaction_execute(manager, &request);
}

lcc_status_t lcc_manager_set_fan_boost(lcc_manager_t *manager, bool enabled) {
  const lcc_transaction_request_t request = {
      .kind = LCC_TRANSACTION_FAN_BOOST,
      .input.fan_boost_enabled = enabled,
  };

  return lcc_transaction_execute(manager, &request);
}

lcc_status_t lcc_manager_set_power_limits(lcc_manager_t *manager,
                                          const lcc_power_limits_t *limits) {
  const lcc_transaction_request_t request = {
      .kind = LCC_TRANSACTION_POWER_LIMITS,
      .input.power_limits = limits,
  };

  return lcc_transaction_execute(manager, &request);
}
