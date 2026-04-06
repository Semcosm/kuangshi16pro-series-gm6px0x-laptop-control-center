#include "cli.h"

#include <stdio.h>
#include <string.h>

#include "cli/dbus_client.h"
#include "lcc/profile.h"

int lcc_cmd_mode_set(int argc, char **argv) {
  lcc_operating_mode_t mode = LCC_MODE_OFFICE;
  lcc_apply_plan_t plan;
  bool use_user_bus = false;
  bool plan_only = false;
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
    if (strcmp(argv[index], "--plan") == 0) {
      plan_only = true;
      continue;
    }
    if (lcc_cli_parse_bus_flag(argv[index], &use_user_bus)) {
      continue;
    }
    return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  (void)printf("mode=%s\n", lcc_mode_name(mode));
  if (!plan_only) {
    return lcc_cli_exit_with_status(
        lcc_dbus_set_mode(use_user_bus, lcc_mode_name(mode)));
  }

  status = lcc_build_mode_plan(mode, &plan);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  return lcc_cli_print_plan_or_unimplemented(&plan, false);
}
