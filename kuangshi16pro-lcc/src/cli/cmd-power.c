#include "cli.h"

#include <string.h>

#include "lcc/profile.h"
#include "lcc/power.h"

int lcc_cmd_power_set(int argc, char **argv) {
  lcc_power_limits_t limits;
  lcc_profile_document_t document;
  lcc_apply_plan_t plan;
  const char *file_path = NULL;
  bool execute = false;
  int index = 0;
  lcc_status_t status = LCC_OK;

  memset(&limits, 0, sizeof(limits));

  for (index = 0; index < argc; ++index) {
    if (strcmp(argv[index], "--file") == 0 && index + 1 < argc) {
      file_path = argv[++index];
    } else if (strcmp(argv[index], "--pl1") == 0 && index + 1 < argc) {
      limits.pl1.present = true;
      status = lcc_cli_parse_u8(argv[++index], &limits.pl1.value);
    } else if (strcmp(argv[index], "--pl2") == 0 && index + 1 < argc) {
      limits.pl2.present = true;
      status = lcc_cli_parse_u8(argv[++index], &limits.pl2.value);
    } else if (strcmp(argv[index], "--pl4") == 0 && index + 1 < argc) {
      limits.pl4.present = true;
      status = lcc_cli_parse_u8(argv[++index], &limits.pl4.value);
    } else if (strcmp(argv[index], "--tcc-offset") == 0 && index + 1 < argc) {
      limits.tcc_offset.present = true;
      status = lcc_cli_parse_u8(argv[++index], &limits.tcc_offset.value);
    } else if (strcmp(argv[index], "--execute") == 0) {
      execute = true;
      continue;
    } else {
      return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
    }

    if (status != LCC_OK) {
      return lcc_cli_exit_with_status(status);
    }
  }

  if (file_path != NULL) {
    status = lcc_profile_document_load(file_path, &document);
    if (status != LCC_OK) {
      return lcc_cli_exit_with_status(status);
    }
    if (!document.has_power_limits) {
      return lcc_cli_exit_with_status(LCC_ERR_PARSE);
    }
    status = lcc_build_power_plan(&document.power_limits, &plan);
  } else {
    status = lcc_build_power_plan(&limits, &plan);
  }
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  return lcc_cli_print_plan_or_unimplemented(&plan, execute);
}
