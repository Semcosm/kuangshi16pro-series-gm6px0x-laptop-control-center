#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "backends/backend.h"
#include "daemon/manager.h"
#include "daemon/transaction.h"

static void make_dir(const char *path) {
  assert(path != NULL);
  assert(mkdir(path, 0700) == 0 || errno == EEXIST);
}

static void write_text_file(const char *path, const char *text) {
  FILE *stream = NULL;

  assert(path != NULL);
  assert(text != NULL);
  stream = fopen(path, "w");
  assert(stream != NULL);
  assert(fputs(text, stream) >= 0);
  assert(fclose(stream) == 0);
}

static void init_fake_sysfs(char *root, size_t root_len) {
  char path[512];
  const long timestamp = (long)time(NULL);
  const long pid = (long)getpid();

  assert(root != NULL);
  assert(root_len > 0u);
  assert(snprintf(root, root_len, "/tmp/lcc-transaction-test-%ld-%ld", pid,
                  timestamp) > 0);

  make_dir(root);
  (void)snprintf(path, sizeof(path), "%s/firmware", root);
  make_dir(path);
  (void)snprintf(path, sizeof(path), "%s/firmware/acpi", root);
  make_dir(path);
  (void)snprintf(path, sizeof(path), "%s/class", root);
  make_dir(path);
  (void)snprintf(path, sizeof(path), "%s/class/hwmon", root);
  make_dir(path);
  (void)snprintf(path, sizeof(path), "%s/class/thermal", root);
  make_dir(path);
  (void)snprintf(path, sizeof(path), "%s/class/powercap", root);
  make_dir(path);
  (void)snprintf(path, sizeof(path), "%s/firmware/acpi/platform_profile", root);
  write_text_file(path, "performance\n");
}

static void test_transaction_happy_path(void) {
  lcc_backend_t backend;
  lcc_mock_backend_t mock_backend;
  lcc_manager_t manager;
  lcc_transaction_request_t request;

  assert(lcc_mock_backend_init(&mock_backend, &backend) == LCC_OK);
  assert(lcc_manager_init(&manager, &backend, NULL) == LCC_OK);

  request.kind = LCC_TRANSACTION_MODE;
  request.input.mode_name = "turbo";
  assert(lcc_transaction_execute(&manager, &request) == LCC_OK);
  assert(manager.state_cache.transaction.state == LCC_TRANSACTION_STATE_IDLE);
  assert(!manager.state_cache.transaction.has_pending_target);
  assert(strcmp(manager.state_cache.requested.profile, "turbo") == 0);
  assert(strcmp(manager.state_cache.effective.profile, "turbo") == 0);
  assert(manager.state_cache.last_apply.has_target);
  assert(strcmp(manager.state_cache.last_apply.target.profile, "turbo") == 0);
  assert(manager.state_cache.last_apply.error == LCC_OK);
  assert(strcmp(manager.state_cache.last_apply.stage, "apply") == 0);
  assert(strcmp(manager.state_cache.last_apply.backend, "mock") == 0);
}

static void test_transaction_failure_path(void) {
  lcc_backend_t backend;
  lcc_mock_backend_t mock_backend;
  lcc_manager_t manager;
  lcc_transaction_request_t request;

  assert(lcc_mock_backend_init(&mock_backend, &backend) == LCC_OK);
  assert(lcc_manager_init(&manager, &backend, NULL) == LCC_OK);
  lcc_mock_backend_fail_next_mode(&mock_backend, LCC_ERR_IO);

  request.kind = LCC_TRANSACTION_MODE;
  request.input.mode_name = "turbo";
  assert(lcc_transaction_execute(&manager, &request) == LCC_ERR_IO);
  assert(manager.state_cache.transaction.state == LCC_TRANSACTION_STATE_FAILED);
  assert(manager.state_cache.transaction.has_pending_target);
  assert(strcmp(manager.state_cache.transaction.pending_target.profile,
                "turbo") == 0);
  assert(manager.state_cache.transaction.last_error == LCC_ERR_IO);
  assert(manager.state_cache.last_apply.has_target);
  assert(strcmp(manager.state_cache.last_apply.target.profile, "turbo") == 0);
  assert(manager.state_cache.last_apply.error == LCC_ERR_IO);
  assert(strcmp(manager.state_cache.last_apply.stage, "apply") == 0);
  assert(strcmp(manager.state_cache.last_apply.backend, "mock") == 0);
  assert(strcmp(manager.state_cache.effective.profile, "balanced") == 0);
  assert(strcmp(manager.state_cache.requested.profile, "balanced") == 0);
}

static void test_transaction_preflight_failure_records_state(void) {
  lcc_backend_t backend;
  lcc_mock_backend_t mock_backend;
  lcc_manager_t manager;
  lcc_transaction_request_t request;

  assert(lcc_mock_backend_init(&mock_backend, &backend) == LCC_OK);
  assert(lcc_manager_init(&manager, &backend, NULL) == LCC_OK);

  request.kind = LCC_TRANSACTION_MODE;
  request.input.mode_name = "balanced";
  assert(lcc_transaction_execute(&manager, &request) == LCC_ERR_PARSE);
  assert(manager.state_cache.transaction.state == LCC_TRANSACTION_STATE_FAILED);
  assert(strcmp(manager.state_cache.transaction.stage, "preflight-validate") ==
         0);
  assert(!manager.state_cache.last_apply.has_target);
  assert(strcmp(manager.state_cache.last_apply.stage, "preflight-validate") ==
         0);
  assert(manager.state_cache.last_apply.backend[0] == '\0');
  assert(manager.state_cache.last_apply.error == LCC_ERR_PARSE);
}

static void test_transaction_capability_gate_records_state(void) {
  char root[256];
  lcc_backend_t backend;
  lcc_standard_backend_t standard_backend;
  lcc_manager_t manager;
  lcc_transaction_request_t request;

  init_fake_sysfs(root, sizeof(root));
  assert(lcc_standard_backend_init_at_root(&standard_backend, &backend, root) ==
         LCC_OK);
  assert(lcc_manager_init(&manager, &backend, NULL) == LCC_OK);

  request.kind = LCC_TRANSACTION_FAN_TABLE;
  request.input.fan_table_name = "M4T1";
  assert(lcc_transaction_execute(&manager, &request) == LCC_ERR_NOT_SUPPORTED);
  assert(manager.state_cache.transaction.state == LCC_TRANSACTION_STATE_FAILED);
  assert(strcmp(manager.state_cache.transaction.stage, "capability-gate") == 0);
  assert(manager.state_cache.transaction.has_pending_target);
  assert(strcmp(manager.state_cache.transaction.pending_target.fan_table,
                "M4T1") == 0);
  assert(strcmp(manager.state_cache.last_apply.stage, "capability-gate") == 0);
  assert(manager.state_cache.last_apply.error == LCC_ERR_NOT_SUPPORTED);
  assert(manager.state_cache.last_apply.backend[0] == '\0');
}

void lcc_run_transaction_tests(void) {
  test_transaction_happy_path();
  test_transaction_failure_path();
  test_transaction_preflight_failure_records_state();
  test_transaction_capability_gate_records_state();
}
