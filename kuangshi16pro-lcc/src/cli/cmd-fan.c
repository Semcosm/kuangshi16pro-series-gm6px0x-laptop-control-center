#include "cli.h"

#include <stdio.h>
#include <string.h>

#include "lcc/fan.h"

int lcc_cmd_fan_apply(int argc, char **argv) {
  lcc_fan_table_t table;
  lcc_apply_plan_t plan;
  const char *preset = "demo";
  const char *file_path = NULL;
  bool execute = false;
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
    if (strcmp(argv[index], "--execute") == 0) {
      execute = true;
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
    status = lcc_fan_table_build_demo(&table, preset);
    if (status != LCC_OK) {
      return lcc_cli_exit_with_status(status);
    }
  }

  status = lcc_build_fan_plan(&table, &plan);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  (void)printf("fan-table=%s\n", table.name);
  return lcc_cli_print_plan_or_unimplemented(&plan, execute);
}
