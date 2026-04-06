#include "daemon/app.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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
                "  --capabilities PATH  override capabilities JSON path\n");
}

int lcc_daemon_main(int argc, char **argv) {
  lcc_manager_t manager;
  const char *capabilities_path = NULL;
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

  status = lcc_manager_init(&manager, capabilities_path);
  if (status != LCC_OK) {
    lcc_log_error("manager init failed: %s", lcc_status_string(status));
    return 1;
  }

  lcc_log_info("starting lccd on %s bus", use_user_bus ? "user" : "system");
  return lcc_dbus_server_run(&manager, use_user_bus);
}
