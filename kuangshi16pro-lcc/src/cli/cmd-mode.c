#include "cli.h"

#include <stdio.h>
#include <string.h>

#include "lcc/profile.h"

int lcc_cmd_mode_set(int argc, char **argv) {
  lcc_operating_mode_t mode = LCC_MODE_OFFICE;
  lcc_apply_plan_t plan;
  bool execute = false;
  int index = 0;
  lcc_status_t status = LCC_OK;

  if (argc < 1) {
    return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  status = lcc_mode_from_string(argv[0], &mode);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  for (index = 1; index < argc; ++index) {
    if (strcmp(argv[index], "--execute") == 0) {
      execute = true;
      continue;
    }
    return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  status = lcc_build_mode_plan(mode, &plan);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  (void)printf("mode=%s\n", lcc_mode_name(mode));
  return lcc_cli_print_plan_or_unimplemented(&plan, execute);
}
