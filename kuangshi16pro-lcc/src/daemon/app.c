#include "daemon/app.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "backends/backend.h"
#include "common/lcc_log.h"
#include "daemon/manager.h"
#include "dbus/server.h"

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
                "  LCC_STANDARD_ROOT    override sysfs root for standard backend\n");
}

int lcc_daemon_main(int argc, char **argv) {
  lcc_backend_t backend;
  lcc_mock_backend_t mock_backend;
  lcc_standard_backend_t standard_backend;
  lcc_backend_result_t probe_result;
  lcc_backend_capabilities_t capabilities;
  lcc_manager_t manager;
  const char *capabilities_path = NULL;
  const char *standard_root = NULL;
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

  standard_root = getenv("LCC_STANDARD_ROOT");
  if (standard_root != NULL && standard_root[0] != '\0') {
    status =
        lcc_standard_backend_init_at_root(&standard_backend, &backend, standard_root);
  } else {
    status = lcc_standard_backend_init(&standard_backend, &backend);
  }
  if (status != LCC_OK) {
    lcc_log_error("standard backend init failed: %s", lcc_status_string(status));
    return 1;
  }

  status = lcc_backend_probe(&backend, &capabilities, &probe_result);
  if (status == LCC_OK) {
    lcc_log_info("selected standard backend (%s)",
                 standard_root != NULL && standard_root[0] != '\0' ? standard_root
                                                                   : "/sys");
  } else {
    if (status != LCC_ERR_NOT_FOUND && status != LCC_ERR_NOT_SUPPORTED) {
      lcc_log_warn("standard backend unavailable: %s",
                   lcc_status_string(status));
    }
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
