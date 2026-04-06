#include "cli/amw0_cli.h"

#include <stdio.h>

#include "cli.h"

lcc_status_t lcc_cli_init_amw0_backend(amw0_backend_t *backend,
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
