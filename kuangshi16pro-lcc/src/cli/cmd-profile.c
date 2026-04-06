#include "cli.h"

#include <stdio.h>
#include <string.h>

#include "lcc/profile.h"

int lcc_cmd_profile_apply(int argc, char **argv) {
  lcc_profile_document_t document;
  lcc_apply_plan_t plan;
  const char *file_path = NULL;
  bool execute = false;
  int index = 0;
  lcc_status_t status = LCC_OK;

  for (index = 0; index < argc; ++index) {
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

  if (file_path == NULL) {
    return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  status = lcc_profile_document_load(file_path, &document);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  status = lcc_build_profile_plan(&document, &plan);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  if (document.has_mode) {
    (void)printf("mode=%s\n", lcc_mode_name(document.mode));
  }
  if (document.has_fan_table) {
    (void)printf("fan-table=%s\n", document.fan_table.name);
  }
  return lcc_cli_print_plan_or_unimplemented(&plan, execute);
}
