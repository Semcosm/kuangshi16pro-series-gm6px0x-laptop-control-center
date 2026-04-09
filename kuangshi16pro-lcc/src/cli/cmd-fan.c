#include "cli.h"

#include <stdio.h>
#include <string.h>

#include "cli/dbus_client.h"
#include "lcc/fan.h"

int lcc_cmd_fan_apply(int argc, char **argv) {
  lcc_fan_table_t table;
  lcc_apply_plan_t plan;
  const char *preset = "demo";
  const char *file_path = NULL;
  bool use_user_bus = false;
  bool plan_only = false;
  int index = 0;
  lcc_status_t status = LCC_OK;

  for (index = 0; index < argc; ++index) {
    if (strcmp(argv[index], "--preset") == 0 && index + 1 < argc) {
      preset = argv[++index];
      continue;
    }
    if (strcmp(argv[index], "--file") == 0 && index + 1 < argc) {
      file_path = argv[++index];
      continue;
    }
    if (strcmp(argv[index], "--plan") == 0) {
      plan_only = true;
      continue;
    }
    if (lcc_cli_parse_bus_flag(argv[index], &use_user_bus)) {
      continue;
    }
    return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  if (file_path != NULL) {
    if (strcmp(preset, "demo") != 0) {
      return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
    }
    status = lcc_fan_table_load_file(file_path, &table);
    if (status != LCC_OK) {
      return lcc_cli_exit_with_status(status);
    }
  } else {
    status = lcc_fan_table_load_named(preset, &table);
    if (status != LCC_OK) {
      return lcc_cli_exit_with_status(status);
    }
  }

  status = lcc_build_fan_plan(&table, &plan);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  (void)printf("fan-table=%s\n", table.name);
  if (plan_only) {
    return lcc_cli_print_plan_or_unimplemented(&plan, false);
  }

  return lcc_cli_exit_with_status(
      lcc_dbus_apply_fan_table(use_user_bus, table.name));
}

int lcc_cmd_fan_boost(int argc, char **argv) {
  const char *value = NULL;
  bool enabled = false;
  bool use_user_bus = false;
  int index = 0;

  for (index = 0; index < argc; ++index) {
    if (lcc_cli_parse_bus_flag(argv[index], &use_user_bus)) {
      continue;
    }
    if (value == NULL) {
      value = argv[index];
      continue;
    }
    return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  if (value == NULL) {
    return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }
  if (strcmp(value, "on") == 0 || strcmp(value, "true") == 0 ||
      strcmp(value, "1") == 0) {
    enabled = true;
  } else if (strcmp(value, "off") == 0 || strcmp(value, "false") == 0 ||
             strcmp(value, "0") == 0) {
    enabled = false;
  } else {
    return lcc_cli_exit_with_status(LCC_ERR_PARSE);
  }

  (void)printf("fan-boost=%s\n", enabled ? "on" : "off");
  return lcc_cli_exit_with_status(
      lcc_dbus_set_fan_boost(use_user_bus, enabled));
}
