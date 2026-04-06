#include "daemon/app.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "backends/backend.h"
#include "common/lcc_log.h"
#include "daemon/manager.h"
#include "dbus/server.h"

static bool env_truthy(const char *value) {
  if (value == NULL || value[0] == '\0') {
    return false;
  }

  return strcmp(value, "1") == 0 || strcmp(value, "true") == 0 ||
         strcmp(value, "yes") == 0 || strcmp(value, "on") == 0;
}

static bool backend_forced(const char *selected, const char *name) {
  return selected != NULL && selected[0] != '\0' && strcmp(selected, name) == 0;
}

static lcc_status_t probe_backend(lcc_backend_t *backend,
                                  lcc_backend_capabilities_t *capabilities) {
  lcc_backend_result_t result;

  return lcc_backend_probe(backend, capabilities, &result);
}

static const char *nonnull_text(const char *value) {
  return value != NULL && value[0] != '\0' ? value : "none";
}

static void log_backend_probe(const char *backend_name, lcc_status_t status,
                              const lcc_backend_capabilities_t *capabilities,
                              const char *detail) {
  if (backend_name == NULL) {
    return;
  }

  if (status != LCC_OK) {
    lcc_log_warn("backend probe backend=%s status=%s detail=%s",
                 backend_name, lcc_status_string(status), nonnull_text(detail));
    return;
  }
  if (capabilities == NULL) {
    lcc_log_info("backend probe backend=%s status=ok detail=%s",
                 backend_name, nonnull_text(detail));
    return;
  }

  lcc_log_info(
      "backend probe backend=%s status=ok detail=%s support={read_state:%s,apply_profile:%s,apply_mode:%s,apply_power_limits:%s,apply_fan_table:%s}",
      backend_name, nonnull_text(detail),
      capabilities->can_read_state ? "true" : "false",
      capabilities->can_apply_profile ? "true" : "false",
      capabilities->can_apply_mode ? "true" : "false",
      capabilities->can_apply_power_limits ? "true" : "false",
      capabilities->can_apply_fan_table ? "true" : "false");
}

static void log_converged_selection(
    const lcc_converged_backend_t *converged) {
  if (converged == NULL) {
    return;
  }

  lcc_log_info(
      "backend selection selected=%s fallback_reason=%s execution={read_state:%s,apply_profile:%s,apply_mode:%s,apply_power_limits:%s,apply_fan_table:%s}",
      nonnull_text(converged->backend_selected),
      nonnull_text(converged->backend_fallback_reason),
      nonnull_text(converged->execution.read_state),
      nonnull_text(converged->execution.apply_profile),
      nonnull_text(converged->execution.apply_mode),
      nonnull_text(converged->execution.apply_power_limits),
      nonnull_text(converged->execution.apply_fan_table));
}

static void print_usage(FILE *stream) {
  (void)fprintf(stream,
                "Usage: lccd [--user] [--system] [--capabilities PATH]\n"
                "\n"
                "Options:\n"
                "  --user               connect to the user bus for development\n"
                "  --system             connect to the system bus (default)\n"
                "  --capabilities PATH  override capabilities JSON path\n"
                "\n"
                "Environment:\n"
                "  LCC_BACKEND          force backend: standard|amw0|mock\n"
                "  LCC_STANDARD_ROOT    override sysfs root for standard backend\n"
                "  LCC_AMW0_CALL_NODE   override acpi_call path for AMW0 backend\n"
                "  LCC_AMW0_ECRR_PATH   override detected ECRR path for AMW0 reads\n"
                "  LCC_AMW0_DRY_RUN     keep AMW0 writes in dry-run mode\n"
                "  LCC_AMW0_TRACE_FILE  append internal AMW0 transport trace\n");
}

int lcc_daemon_main(int argc, char **argv) {
  lcc_backend_t backend;
  lcc_backend_t standard_handle;
  lcc_backend_t amw0_handle;
  lcc_backend_t converged_handle;
  lcc_mock_backend_t mock_backend;
  lcc_amw0_backend_t amw0_backend;
  lcc_standard_backend_t standard_backend;
  lcc_converged_backend_t converged_backend;
  lcc_backend_capabilities_t standard_capabilities;
  lcc_backend_capabilities_t amw0_capabilities;
  lcc_manager_t manager;
  const char *capabilities_path = NULL;
  const char *amw0_call_node = NULL;
  const char *amw0_ecrr_path = NULL;
  const char *amw0_trace_file = NULL;
  const char *forced_backend = NULL;
  const char *standard_root = NULL;
  bool amw0_dry_run = false;
  bool use_user_bus = false;
  bool force_standard = false;
  bool force_amw0 = false;
  bool force_mock = false;
  int index = 0;
  lcc_status_t status = LCC_OK;
  lcc_status_t standard_status = LCC_ERR_NOT_FOUND;
  lcc_status_t amw0_status = LCC_ERR_NOT_FOUND;

  for (index = 1; index < argc; ++index) {
    if (strcmp(argv[index], "--help") == 0 || strcmp(argv[index], "-h") == 0) {
      print_usage(stdout);
      return 0;
    }
    if (strcmp(argv[index], "--user") == 0) {
      use_user_bus = true;
      continue;
    }
    if (strcmp(argv[index], "--system") == 0) {
      use_user_bus = false;
      continue;
    }
    if (strcmp(argv[index], "--capabilities") == 0 && index + 1 < argc) {
      capabilities_path = argv[++index];
      continue;
    }

    print_usage(stderr);
    return 1;
  }

  forced_backend = getenv("LCC_BACKEND");
  standard_root = getenv("LCC_STANDARD_ROOT");
  amw0_call_node = getenv("LCC_AMW0_CALL_NODE");
  amw0_ecrr_path = getenv("LCC_AMW0_ECRR_PATH");
  amw0_trace_file = getenv("LCC_AMW0_TRACE_FILE");
  amw0_dry_run = env_truthy(getenv("LCC_AMW0_DRY_RUN"));
  force_standard = backend_forced(forced_backend, "standard");
  force_amw0 = backend_forced(forced_backend, "amw0");
  force_mock = backend_forced(forced_backend, "mock");

  memset(&standard_capabilities, 0, sizeof(standard_capabilities));
  memset(&amw0_capabilities, 0, sizeof(amw0_capabilities));

  if (!force_amw0 && !force_mock) {
    if (standard_root != NULL && standard_root[0] != '\0') {
      standard_status = lcc_standard_backend_init_at_root(
          &standard_backend, &standard_handle, standard_root);
    } else {
      standard_status = lcc_standard_backend_init(&standard_backend,
                                                  &standard_handle);
    }
    if (standard_status != LCC_OK) {
      log_backend_probe("standard", standard_status, NULL,
                        standard_root != NULL && standard_root[0] != '\0'
                            ? standard_root
                            : "/sys");
      if (force_standard) {
        lcc_log_error("forced standard backend unavailable: %s",
                      lcc_status_string(standard_status));
        return 1;
      }
    } else {
      standard_status = probe_backend(&standard_handle, &standard_capabilities);
      log_backend_probe("standard", standard_status, &standard_capabilities,
                        standard_root != NULL && standard_root[0] != '\0'
                            ? standard_root
                            : "/sys");
      if (force_standard && standard_status != LCC_OK) {
        lcc_log_error("forced standard backend unavailable: %s",
                      lcc_status_string(standard_status));
        return 1;
      }
    }
  }

  if (!force_standard && !force_mock) {
    amw0_status =
        lcc_amw0_backend_init(&amw0_backend, &amw0_handle,
                              amw0_call_node != NULL &&
                                      amw0_call_node[0] != '\0'
                                  ? amw0_call_node
                                  : "/proc/acpi/call",
                              amw0_ecrr_path, amw0_dry_run);
    if (amw0_status == LCC_OK && amw0_trace_file != NULL &&
        amw0_trace_file[0] != '\0') {
      amw0_status =
          amw0_backend_set_trace(&amw0_backend.transport, amw0_trace_file);
    }
    if (amw0_status == LCC_OK) {
      amw0_status = probe_backend(&amw0_handle, &amw0_capabilities);
    }
    log_backend_probe(
        "amw0", amw0_status, amw0_status == LCC_OK ? &amw0_capabilities : NULL,
        amw0_call_node != NULL && amw0_call_node[0] != '\0' ? amw0_call_node
                                                             : "/proc/acpi/call");
    if (force_amw0 && amw0_status != LCC_OK) {
      lcc_log_error("forced amw0 backend unavailable: %s",
                    lcc_status_string(amw0_status));
      return 1;
    }
  }

  if (force_standard) {
    backend = standard_handle;
    lcc_log_info("backend selection selected=standard fallback_reason=none execution=direct");
  } else if (force_amw0) {
    backend = amw0_handle;
    lcc_log_info("backend selection selected=amw0 fallback_reason=none execution=direct");
  } else if (!force_mock &&
             lcc_converged_backend_init(&converged_backend, &converged_handle,
                                        standard_status == LCC_OK
                                            ? &standard_handle
                                            : NULL,
                                        standard_status, &standard_capabilities,
                                        amw0_status == LCC_OK ? &amw0_handle
                                                              : NULL,
                                        amw0_status, &amw0_capabilities) ==
                 LCC_OK) {
    backend = converged_handle;
    log_converged_selection(&converged_backend);
  } else {
    status = lcc_mock_backend_init(&mock_backend, &backend);
    if (status != LCC_OK) {
      lcc_log_error("mock backend init failed: %s", lcc_status_string(status));
      return 1;
    }
    lcc_log_info("backend selection selected=mock fallback_reason=standard=%s amw0=%s",
                 lcc_status_string(standard_status),
                 lcc_status_string(amw0_status));
  }

  status = lcc_manager_init(&manager, &backend, capabilities_path);
  if (status != LCC_OK) {
    lcc_log_error("manager init failed: %s", lcc_status_string(status));
    return 1;
  }

  lcc_log_info("starting lccd on %s bus", use_user_bus ? "user" : "system");
  return lcc_dbus_server_run(&manager, use_user_bus);
}
