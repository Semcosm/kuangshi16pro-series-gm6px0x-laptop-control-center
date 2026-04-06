#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli.h"
#include "common/lcc_log.h"

const char *lcc_cli_default_call_node(void) { return "/proc/acpi/call"; }

void lcc_cli_print_usage(FILE *stream) {
  (void)fprintf(stream,
                "Usage:\n"
                "  lccctl status [--call-node PATH]\n"
                "  lccctl observe GROUP [--call-node PATH] [--ecrr-path PATH]\n"
                "  lccctl raw wmbc SLOT SAC1 SA00 SA01 SA02 SA03 "
                "[--call-node PATH] [--dry-run]\n"
                "  lccctl mode set MODE [--execute]\n"
                "  lccctl power set --pl1 N [--pl2 N] [--pl4 N] "
                "[--tcc-offset N] [--execute]\n"
                "  lccctl power set --file PATH [--execute]\n"
                "  lccctl fan apply [--preset demo | --file PATH] [--execute]\n"
                "  lccctl profile apply --file PATH [--execute]\n"
                "\n"
                "Profile file format:\n"
                "  [mode]  value=gaming|office|turbo|custom\n"
                "  [power] pl1=55 pl2=95 pl4=125 tcc_offset=10\n"
                "  [fan]   name=demo cpu.0=40,37,18 gpu.0=45,42,20\n"
                "\n"
                "Notes:\n"
                "  mode/power/fan/profile currently print a staged EC plan.\n"
                "  observe groups: mode, power, fan, thermal, all.\n"
                "  raw wmbc, status, and observe use the AMW0 backend directly.\n");
}

int lcc_cli_exit_with_status(lcc_status_t status) {
  if (status != LCC_OK) {
    lcc_log_error("%s", lcc_status_string(status));
  }
  return status == LCC_OK ? 0 : 1;
}

lcc_status_t lcc_cli_parse_u8(const char *text, uint8_t *value) {
  char *end = NULL;
  unsigned long parsed = 0;

  if (text == NULL || value == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  parsed = strtoul(text, &end, 0);
  if (end == text || *end != '\0') {
    return LCC_ERR_PARSE;
  }
  if (parsed > 0xfful) {
    return LCC_ERR_RANGE;
  }

  *value = (uint8_t)parsed;
  return LCC_OK;
}

lcc_status_t lcc_cli_parse_u16(const char *text, uint16_t *value) {
  char *end = NULL;
  unsigned long parsed = 0;

  if (text == NULL || value == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  parsed = strtoul(text, &end, 0);
  if (end == text || *end != '\0') {
    return LCC_ERR_PARSE;
  }
  if (parsed > 0xfffful) {
    return LCC_ERR_RANGE;
  }

  *value = (uint16_t)parsed;
  return LCC_OK;
}

lcc_status_t lcc_cli_parse_u32_reply(const char *text, uint32_t *value) {
  char *end = NULL;
  unsigned long parsed = 0;

  if (text == NULL || value == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  parsed = strtoul(text, &end, 0);
  if (end == text || *end != '\0') {
    return LCC_ERR_PARSE;
  }

  *value = (uint32_t)parsed;
  return LCC_OK;
}

lcc_status_t lcc_cli_init_backend(amw0_backend_t *backend,
                                  const char *call_node, bool dry_run) {
  return amw0_backend_init(backend, call_node != NULL ? call_node
                                                      : lcc_cli_default_call_node(),
                           dry_run);
}

lcc_status_t lcc_cli_print_transport_snapshot(amw0_backend_t *backend) {
  char wqac0[AMW0_REPLY_MAX];
  char wqac1[AMW0_REPLY_MAX];
  char wed_d2[AMW0_REPLY_MAX];
  lcc_status_t status = LCC_OK;

  status = amw0_backend_read_wqac(backend, 0u, wqac0, sizeof(wqac0));
  if (status != LCC_OK) {
    return status;
  }
  status = amw0_backend_read_wqac(backend, 1u, wqac1, sizeof(wqac1));
  if (status != LCC_OK) {
    return status;
  }
  status =
      amw0_backend_eval(backend, "\\_SB.AMW0._WED 0xD2", wed_d2, sizeof(wed_d2));
  if (status != LCC_OK) {
    return status;
  }

  (void)printf("WQAC0=%s\nWQAC1=%s\nWED_D2=%s\n", wqac0, wqac1, wed_d2);
  return LCC_OK;
}

int lcc_cli_print_plan_or_unimplemented(const lcc_apply_plan_t *plan,
                                        bool execute) {
  lcc_apply_plan_print(stdout, plan);

  if (execute) {
    lcc_log_warn("staged execution is not wired to final AMW0 write packets yet");
    return lcc_cli_exit_with_status(LCC_ERR_UNIMPLEMENTED);
  }

  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    lcc_cli_print_usage(stderr);
    return 1;
  }

  if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
    lcc_cli_print_usage(stdout);
    return 0;
  }

  if (strcmp(argv[1], "status") == 0) {
    return lcc_cmd_state_status(argc - 2, argv + 2);
  }
  if (strcmp(argv[1], "observe") == 0) {
    return lcc_cmd_state_observe(argc - 2, argv + 2);
  }
  if (strcmp(argv[1], "raw") == 0 && argc >= 3 && strcmp(argv[2], "wmbc") == 0) {
    return lcc_cmd_debug_raw_wmbc(argc - 3, argv + 3);
  }
  if (strcmp(argv[1], "mode") == 0 && argc >= 3 &&
      strcmp(argv[2], "set") == 0) {
    return lcc_cmd_mode_set(argc - 3, argv + 3);
  }
  if (strcmp(argv[1], "power") == 0 && argc >= 3 &&
      strcmp(argv[2], "set") == 0) {
    return lcc_cmd_power_set(argc - 3, argv + 3);
  }
  if (strcmp(argv[1], "fan") == 0 && argc >= 3 &&
      strcmp(argv[2], "apply") == 0) {
    return lcc_cmd_fan_apply(argc - 3, argv + 3);
  }
  if (strcmp(argv[1], "profile") == 0 && argc >= 3 &&
      strcmp(argv[2], "apply") == 0) {
    return lcc_cmd_profile_apply(argc - 3, argv + 3);
  }

  lcc_cli_print_usage(stderr);
  return 1;
}
