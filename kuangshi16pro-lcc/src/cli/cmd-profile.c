#include "cli.h"

#include <stdio.h>
#include <string.h>

#include "cli/dbus_client.h"
#include "lcc/profile.h"

int lcc_cmd_profile_apply(int argc, char **argv) {
  lcc_profile_document_t document;
  lcc_apply_plan_t plan;
  const char *file_path = NULL;
  bool use_user_bus = false;
  bool plan_only = false;
  int index = 0;
  lcc_status_t status = LCC_OK;

  for (index = 0; index < argc; ++index) {
    if (strcmp(argv[index], "--file") == 0 && index + 1 < argc) {
      file_path = argv[++index];
      continue;
    }
    if (strcmp(argv[index], "--plan") == 0) {
      plan_only = true;
      continue;
    }
    if (strcmp(argv[index], "--execute") == 0) {
      continue;
    }
    if (lcc_cli_parse_bus_flag(argv[index], &use_user_bus)) {
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

  if (!plan_only) {
    if (document.has_mode) {
      status = lcc_dbus_set_mode(use_user_bus, lcc_mode_name(document.mode));
      if (status != LCC_OK) {
        return lcc_cli_exit_with_status(status);
      }
    }
    if (document.has_power_limits) {
      status = lcc_dbus_set_power_limits(use_user_bus, &document.power_limits);
      if (status != LCC_OK) {
        return lcc_cli_exit_with_status(status);
      }
    }
    if (document.has_fan_table) {
      status =
          lcc_dbus_apply_fan_table(use_user_bus, document.fan_table.name);
      if (status != LCC_OK) {
        return lcc_cli_exit_with_status(status);
      }
    }
  }

  if (document.has_mode) {
    (void)printf("mode=%s\n", lcc_mode_name(document.mode));
  }
  if (document.has_fan_table) {
    (void)printf("fan-table=%s\n", document.fan_table.name);
  }
  if (document.has_power_limits) {
    if (document.power_limits.pl1.present) {
      (void)printf("pl1=%u\n", (unsigned int)document.power_limits.pl1.value);
    }
    if (document.power_limits.pl2.present) {
      (void)printf("pl2=%u\n", (unsigned int)document.power_limits.pl2.value);
    }
    if (document.power_limits.pl4.present) {
      (void)printf("pl4=%u\n", (unsigned int)document.power_limits.pl4.value);
    }
    if (document.power_limits.tcc_offset.present) {
      (void)printf("tcc_offset=%u\n",
                   (unsigned int)document.power_limits.tcc_offset.value);
    }
  }
  if (plan_only) {
    return lcc_cli_print_plan_or_unimplemented(&plan, false);
  }

  return 0;
}
