#include "cli.h"

#include <stdio.h>
#include <string.h>

#include "backends/amw0/ec-addr-map.h"
#include "backends/amw0/ecmg.h"
#include "common/lcc_log.h"

static lcc_status_t print_observe_group(amw0_backend_t *backend,
                                        const char *ecrr_path,
                                        const char *group_name,
                                        const lcc_observe_item_t *items,
                                        size_t count) {
  char reply[AMW0_REPLY_MAX];
  size_t index = 0;

  if (backend == NULL || ecrr_path == NULL || group_name == NULL ||
      items == NULL || count == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  (void)printf("[%s]\n", group_name);
  for (index = 0; index < count; ++index) {
    const lcc_status_t status = amw0_backend_read_ecrr(
        backend, ecrr_path, items[index].offset, reply, sizeof(reply));
    if (status != LCC_OK) {
      return status;
    }
    (void)printf("%-16s addr=0x%04X value=%s\n", items[index].name,
                 (unsigned int)items[index].offset, reply);
  }

  return LCC_OK;
}

int lcc_cmd_state_status(int argc, char **argv) {
  amw0_backend_t backend;
  char ecrr_path[32];
  char ecrr_460[AMW0_REPLY_MAX];
  const char *call_node = lcc_cli_default_call_node();
  int index = 0;
  lcc_status_t status = LCC_OK;

  for (index = 0; index < argc; ++index) {
    if (strcmp(argv[index], "--call-node") == 0 && index + 1 < argc) {
      call_node = argv[++index];
      continue;
    }
    return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  status = lcc_cli_init_backend(&backend, call_node, false);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  status = lcc_cli_print_transport_snapshot(&backend);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  status = amw0_backend_probe_ecrr_path(&backend, ecrr_path, sizeof(ecrr_path));
  if (status == LCC_OK) {
    status = amw0_backend_read_ecrr(&backend, ecrr_path, LCC_AMW0_ADDR_FFAN,
                                    ecrr_460, sizeof(ecrr_460));
    if (status != LCC_OK) {
      return lcc_cli_exit_with_status(status);
    }
    (void)printf("ECRR_PATH=%s\nECRR_0460=%s\n", ecrr_path, ecrr_460);
    return 0;
  }

  lcc_log_warn("ECRR path probe not available on this setup");
  return 0;
}

int lcc_cmd_state_observe(int argc, char **argv) {
  amw0_backend_t backend;
  char ecrr_path[32];
  const char *group = NULL;
  const char *call_node = lcc_cli_default_call_node();
  const char *override_ecrr_path = NULL;
  const lcc_observe_item_t *items = NULL;
  size_t count = 0;
  lcc_status_t status = LCC_OK;
  int index = 0;

  if (argc < 1) {
    return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  group = argv[0];
  for (index = 1; index < argc; ++index) {
    if (strcmp(argv[index], "--call-node") == 0 && index + 1 < argc) {
      call_node = argv[++index];
      continue;
    }
    if (strcmp(argv[index], "--ecrr-path") == 0 && index + 1 < argc) {
      override_ecrr_path = argv[++index];
      continue;
    }
    return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  status = lcc_cli_init_backend(&backend, call_node, false);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status = lcc_cli_print_transport_snapshot(&backend);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  if (override_ecrr_path != NULL) {
    const int written =
        snprintf(ecrr_path, sizeof(ecrr_path), "%s", override_ecrr_path);
    if (written < 0 || (size_t)written >= sizeof(ecrr_path)) {
      return lcc_cli_exit_with_status(LCC_ERR_BUFFER_TOO_SMALL);
    }
  } else {
    status = amw0_backend_probe_ecrr_path(&backend, ecrr_path, sizeof(ecrr_path));
    if (status != LCC_OK) {
      return lcc_cli_exit_with_status(status);
    }
  }

  (void)printf("ECRR_PATH=%s\n", ecrr_path);

  if (strcmp(group, "mode") == 0) {
    items = lcc_amw0_observe_mode_items(&count);
    status = print_observe_group(&backend, ecrr_path, "mode", items, count);
    if (status != LCC_OK) {
      return lcc_cli_exit_with_status(status);
    }
    return lcc_cli_exit_with_status(
        lcc_amw0_print_mode_decode(&backend, ecrr_path));
  }
  if (strcmp(group, "power") == 0) {
    items = lcc_amw0_observe_power_items(&count);
    return lcc_cli_exit_with_status(
        print_observe_group(&backend, ecrr_path, "power", items, count));
  }
  if (strcmp(group, "fan") == 0) {
    items = lcc_amw0_observe_fan_items(&count);
    return lcc_cli_exit_with_status(
        print_observe_group(&backend, ecrr_path, "fan", items, count));
  }
  if (strcmp(group, "thermal") == 0) {
    items = lcc_amw0_observe_thermal_items(&count);
    status = print_observe_group(&backend, ecrr_path, "thermal", items, count);
    if (status != LCC_OK) {
      return lcc_cli_exit_with_status(status);
    }
    return lcc_cli_exit_with_status(
        lcc_amw0_print_thermal_decode(&backend, ecrr_path));
  }
  if (strcmp(group, "all") == 0) {
    items = lcc_amw0_observe_mode_items(&count);
    status = print_observe_group(&backend, ecrr_path, "mode", items, count);
    if (status != LCC_OK) {
      return lcc_cli_exit_with_status(status);
    }
    status = lcc_amw0_print_mode_decode(&backend, ecrr_path);
    if (status != LCC_OK) {
      return lcc_cli_exit_with_status(status);
    }

    items = lcc_amw0_observe_power_items(&count);
    status = print_observe_group(&backend, ecrr_path, "power", items, count);
    if (status != LCC_OK) {
      return lcc_cli_exit_with_status(status);
    }

    items = lcc_amw0_observe_fan_items(&count);
    status = print_observe_group(&backend, ecrr_path, "fan", items, count);
    if (status != LCC_OK) {
      return lcc_cli_exit_with_status(status);
    }

    items = lcc_amw0_observe_thermal_items(&count);
    status = print_observe_group(&backend, ecrr_path, "thermal", items, count);
    if (status != LCC_OK) {
      return lcc_cli_exit_with_status(status);
    }
    return lcc_cli_exit_with_status(
        lcc_amw0_print_thermal_decode(&backend, ecrr_path));
  }

  return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
}
