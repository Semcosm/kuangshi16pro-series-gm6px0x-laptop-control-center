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
  lcc_mock_backend_t mock_backend;
  lcc_amw0_backend_t amw0_backend;
  lcc_standard_backend_t standard_backend;
  lcc_backend_capabilities_t capabilities;
  lcc_manager_t manager;
  const char *capabilities_path = NULL;
  const char *amw0_call_node = NULL;
  const char *amw0_ecrr_path = NULL;
  const char *amw0_trace_file = NULL;
  const char *forced_backend = NULL;
  const char *standard_root = NULL;
  bool amw0_dry_run = false;
  bool use_user_bus = false;
  int index = 0;
  lcc_status_t status = LCC_OK;

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

  if (!backend_forced(forced_backend, "amw0") &&
      !backend_forced(forced_backend, "mock")) {
    if (standard_root != NULL && standard_root[0] != '\0') {
      status = lcc_standard_backend_init_at_root(&standard_backend, &backend,
                                                 standard_root);
    } else {
      status = lcc_standard_backend_init(&standard_backend, &backend);
    }
    if (status != LCC_OK) {
      lcc_log_error("standard backend init failed: %s",
                    lcc_status_string(status));
      return 1;
    }

    status = probe_backend(&backend, &capabilities);
    if (status == LCC_OK) {
      lcc_log_info("selected standard backend (%s)",
                   standard_root != NULL && standard_root[0] != '\0'
                       ? standard_root
                       : "/sys");
    } else if (backend_forced(forced_backend, "standard")) {
      lcc_log_error("forced standard backend unavailable: %s",
                    lcc_status_string(status));
      return 1;
    }
  } else {
    status = LCC_ERR_NOT_SUPPORTED;
  }

  if (status != LCC_OK && !backend_forced(forced_backend, "mock")) {
    status = lcc_amw0_backend_init(&amw0_backend, &backend,
                                   amw0_call_node != NULL &&
                                           amw0_call_node[0] != '\0'
                                       ? amw0_call_node
                                       : "/proc/acpi/call",
                                   amw0_ecrr_path, amw0_dry_run);
    if (status != LCC_OK) {
      lcc_log_error("amw0 backend init failed: %s", lcc_status_string(status));
      return 1;
    }
    if (amw0_trace_file != NULL && amw0_trace_file[0] != '\0') {
      status =
          amw0_backend_set_trace(&amw0_backend.transport, amw0_trace_file);
      if (status != LCC_OK) {
        lcc_log_error("amw0 trace init failed: %s", lcc_status_string(status));
        return 1;
      }
    }
    status = probe_backend(&backend, &capabilities);
    if (status == LCC_OK) {
      lcc_log_info("selected amw0 backend (%s%s)",
                   amw0_call_node != NULL && amw0_call_node[0] != '\0'
                       ? amw0_call_node
                       : "/proc/acpi/call",
                   amw0_dry_run ? ", dry-run" : "");
    } else if (backend_forced(forced_backend, "amw0")) {
      lcc_log_error("forced amw0 backend unavailable: %s",
                    lcc_status_string(status));
      return 1;
    }
  }

  if (status != LCC_OK) {
    status = lcc_mock_backend_init(&mock_backend, &backend);
    if (status != LCC_OK) {
      lcc_log_error("mock backend init failed: %s", lcc_status_string(status));
      return 1;
    }
    lcc_log_info("selected mock backend fallback");
  }

  status = lcc_manager_init(&manager, &backend, capabilities_path);
  if (status != LCC_OK) {
    lcc_log_error("manager init failed: %s", lcc_status_string(status));
    return 1;
  }

  lcc_log_info("starting lccd on %s bus", use_user_bus ? "user" : "system");
  return lcc_dbus_server_run(&manager, use_user_bus);
}
