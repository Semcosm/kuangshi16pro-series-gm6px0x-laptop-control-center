#include "cli.h"

#include <stdio.h>
#include <string.h>

#include "backends/amw0/wmbc-pack.h"
#include "cli/amw0_cli.h"

int lcc_cmd_debug_raw_wmbc(int argc, char **argv) {
  amw0_backend_t backend;
  amw0_packet_t packet;
  char expr[AMW0_EXPR_MAX];
  char reply[AMW0_REPLY_MAX];
  const char *call_node = lcc_cli_default_call_node();
  bool dry_run = false;
  int index = 0;
  lcc_status_t status = LCC_OK;

  if (argc < 6) {
    lcc_cli_print_usage(stderr);
    return 1;
  }

  memset(&packet, 0, sizeof(packet));

  status = lcc_cli_parse_u8(argv[0], &packet.slot);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status = lcc_cli_parse_u16(argv[1], &packet.sac1);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status = lcc_cli_parse_u8(argv[2], &packet.sa[0]);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status = lcc_cli_parse_u8(argv[3], &packet.sa[1]);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status = lcc_cli_parse_u8(argv[4], &packet.sa[2]);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status = lcc_cli_parse_u8(argv[5], &packet.sa[3]);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  for (index = 6; index < argc; ++index) {
    if (strcmp(argv[index], "--call-node") == 0 && index + 1 < argc) {
      call_node = argv[++index];
      continue;
    }
    if (strcmp(argv[index], "--dry-run") == 0) {
      dry_run = true;
      continue;
    }
    return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  status = lcc_cli_init_amw0_backend(&backend, call_node, dry_run);
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  status = amw0_packet_format_expr(&packet, expr, sizeof(expr));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  (void)printf("expr=%s\n", expr);

  status = amw0_backend_send_packet(&backend, &packet, reply, sizeof(reply));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  (void)printf("reply=%s\n", reply);
  return 0;
}
