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

typedef struct {
  lcc_state_snapshot_t state;
} lcc_route_failure_backend_t;

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
  (void)snprintf(path, sizeof(path), "%s/class/powercap/intel-rapl:0", root);
  make_dir(path);
  (void)snprintf(path, sizeof(path), "%s/class/powercap/intel-rapl-mmio:0",
                 root);
  make_dir(path);
  (void)snprintf(path, sizeof(path), "%s/firmware/acpi/platform_profile", root);
  write_text_file(path, "performance\n");
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl:0/constraint_0_power_limit_uw",
                 root);
  write_text_file(path, "45000000\n");
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl:0/constraint_1_power_limit_uw",
                 root);
  write_text_file(path, "90000000\n");
  (void)snprintf(path, sizeof(path), "%s/class/powercap/intel-rapl:0/name",
                 root);
  write_text_file(path, "package-0\n");
  (void)snprintf(path, sizeof(path), "%s/class/powercap/intel-rapl:0/constraint_0_name",
                 root);
  write_text_file(path, "long_term\n");
  (void)snprintf(path, sizeof(path), "%s/class/powercap/intel-rapl:0/constraint_1_name",
                 root);
  write_text_file(path, "short_term\n");
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl-mmio:0/constraint_0_power_limit_uw",
                 root);
  write_text_file(path, "140000000\n");
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl-mmio:0/constraint_1_power_limit_uw",
                 root);
  write_text_file(path, "140000000\n");
  (void)snprintf(path, sizeof(path), "%s/class/powercap/intel-rapl-mmio:0/name",
                 root);
  write_text_file(path, "package-0\n");
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl-mmio:0/constraint_0_name", root);
  write_text_file(path, "long_term\n");
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl-mmio:0/constraint_1_name", root);
  write_text_file(path, "short_term\n");
}

static lcc_status_t route_failure_probe(void *ctx,
                                        lcc_backend_capabilities_t *capabilities,
                                        lcc_backend_result_t *result) {
  (void)ctx;

  assert(capabilities != NULL);
  memset(capabilities, 0, sizeof(*capabilities));
  capabilities->can_read_state = true;
  capabilities->can_apply_mode = true;
  lcc_backend_result_reset(result);
  return LCC_OK;
}

static lcc_status_t route_failure_read_state(void *ctx,
                                             lcc_state_snapshot_t *state,
                                             lcc_backend_result_t *result) {
  lcc_route_failure_backend_t *backend = ctx;

  assert(backend != NULL);
  assert(state != NULL);
  memset(state, 0, sizeof(*state));
  *state = backend->state;
  lcc_backend_result_reset(result);
  return LCC_OK;
}

static lcc_status_t route_failure_apply_mode(void *ctx,
                                             lcc_operating_mode_t mode,
                                             lcc_backend_result_t *result) {
  (void)ctx;
  (void)mode;

  lcc_backend_result_reset(result);
  lcc_backend_result_set_detail(result,
                                "route test backend rejected mode apply");
  return LCC_ERR_NOT_SUPPORTED;
}

static const lcc_backend_ops_t route_failure_backend_ops = {
    .name = "route-test",
    .kind = LCC_BACKEND_MOCK,
    .probe = route_failure_probe,
    .read_state = route_failure_read_state,
    .apply_profile = NULL,
    .apply_mode = route_failure_apply_mode,
    .apply_power_limits = NULL,
    .apply_fan_table = NULL,
};

static void init_route_failure_backend(lcc_route_failure_backend_t *route_backend,
                                       lcc_backend_t *backend) {
  assert(route_backend != NULL);
  assert(backend != NULL);

  memset(route_backend, 0, sizeof(*route_backend));
  assert(lcc_backend_copy_text(route_backend->state.backend_name,
                               sizeof(route_backend->state.backend_name),
                               "route-test") == LCC_OK);
  assert(lcc_backend_copy_text(route_backend->state.backend_selected,
                               sizeof(route_backend->state.backend_selected),
                               "route-test") == LCC_OK);
  assert(lcc_backend_execution_set_all(&route_backend->state.execution,
                                       "route-test") == LCC_OK);
  assert(lcc_backend_copy_text(route_backend->state.requested.profile,
                               sizeof(route_backend->state.requested.profile),
                               "balanced") == LCC_OK);
  assert(lcc_backend_copy_text(route_backend->state.effective.profile,
                               sizeof(route_backend->state.effective.profile),
                               "balanced") == LCC_OK);
  lcc_backend_bind(backend, &route_failure_backend_ops, route_backend);
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
  assert(strcmp(manager.state_cache.effective_meta.source, "mock") == 0);
  assert(strcmp(manager.state_cache.effective_meta.freshness, "live") == 0);
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

static void test_transaction_backend_route_failure_records_state(void) {
  lcc_backend_t backend;
  lcc_route_failure_backend_t route_backend;
  lcc_manager_t manager;
  lcc_transaction_request_t request;

  init_route_failure_backend(&route_backend, &backend);
  assert(lcc_manager_init(&manager, &backend, NULL) == LCC_OK);

  request.kind = LCC_TRANSACTION_MODE;
  request.input.mode_name = "turbo";
  assert(lcc_transaction_execute(&manager, &request) == LCC_ERR_NOT_SUPPORTED);
  assert(manager.state_cache.transaction.state == LCC_TRANSACTION_STATE_FAILED);
  assert(strcmp(manager.state_cache.transaction.stage, "backend-route") == 0);
  assert(manager.state_cache.transaction.has_pending_target);
  assert(strcmp(manager.state_cache.transaction.pending_target.profile,
                "turbo") == 0);
  assert(strcmp(manager.state_cache.last_apply.stage, "backend-route") == 0);
  assert(manager.state_cache.last_apply.has_target);
  assert(strcmp(manager.state_cache.last_apply.target.profile, "turbo") == 0);
  assert(manager.state_cache.last_apply.error == LCC_ERR_NOT_SUPPORTED);
  assert(manager.state_cache.last_apply.backend[0] == '\0');
  assert(strcmp(manager.state_cache.requested.profile, "balanced") == 0);
  assert(strcmp(manager.state_cache.effective.profile, "balanced") == 0);
}

static void test_transaction_converged_route_attribution_records_executor(void) {
  char root[256];
  lcc_backend_t standard_handle;
  lcc_backend_t amw0_handle;
  lcc_backend_t converged_handle;
  lcc_standard_backend_t standard_backend;
  lcc_amw0_backend_t amw0_backend;
  lcc_converged_backend_t converged_backend;
  lcc_backend_capabilities_t standard_capabilities;
  lcc_backend_capabilities_t amw0_capabilities;
  lcc_backend_result_t result;
  lcc_manager_t manager;
  lcc_transaction_request_t request;
  lcc_power_limits_t limits;

  init_fake_sysfs(root, sizeof(root));
  assert(lcc_standard_backend_init_at_root(&standard_backend, &standard_handle,
                                           root) == LCC_OK);
  assert(lcc_backend_probe(&standard_handle, &standard_capabilities, &result) ==
         LCC_OK);
  assert(lcc_amw0_backend_init(&amw0_backend, &amw0_handle, "/proc/acpi/call",
                               NULL, true) == LCC_OK);
  assert(lcc_backend_probe(&amw0_handle, &amw0_capabilities, &result) ==
         LCC_OK);
  assert(lcc_converged_backend_init(
             &converged_backend, &converged_handle, &standard_handle, LCC_OK,
             &standard_capabilities, &amw0_handle, LCC_OK,
             &amw0_capabilities) == LCC_OK);
  assert(lcc_manager_init(&manager, &converged_handle, NULL) == LCC_OK);

  memset(&limits, 0, sizeof(limits));
  limits.pl1.present = true;
  limits.pl1.value = 70u;
  limits.pl2.present = true;
  limits.pl2.value = 120u;

  memset(&request, 0, sizeof(request));
  request.kind = LCC_TRANSACTION_POWER_LIMITS;
  request.input.power_limits = &limits;

  assert(lcc_transaction_execute(&manager, &request) == LCC_OK);
  assert(manager.state_cache.transaction.state == LCC_TRANSACTION_STATE_IDLE);
  assert(strcmp(manager.state_cache.backend_name, "standard") == 0);
  assert(strcmp(manager.state_cache.backend_selected, "standard") == 0);
  assert(strcmp(manager.state_cache.execution.read_state, "standard") == 0);
  assert(strcmp(manager.state_cache.execution.apply_power_limits, "standard") == 0);
  assert(strcmp(manager.state_cache.last_apply.stage, "verify-powercap") == 0);
  assert(strcmp(manager.state_cache.last_apply.backend, "standard") == 0);
  assert(manager.state_cache.last_apply.error == LCC_OK);
  assert(manager.state_cache.last_apply.has_target);
  assert(manager.state_cache.effective.has_power_limits);
  assert(manager.state_cache.effective.power_limits.pl1.present);
  assert(manager.state_cache.effective.power_limits.pl1.value == 70u);
  assert(manager.state_cache.effective.power_limits.pl2.present);
  assert(manager.state_cache.effective.power_limits.pl2.value == 120u);
  assert(strcmp(manager.state_cache.effective_meta.source, "mixed") == 0);
  assert(strcmp(manager.state_cache.effective_meta.freshness, "mixed") == 0);
  assert(strcmp(manager.state_cache.effective_meta.power.source, "standard") == 0);
  assert(strcmp(manager.state_cache.effective_meta.power_fields.pl1.source,
                "standard") == 0);
}

void lcc_run_transaction_tests(void) {
  test_transaction_happy_path();
  test_transaction_failure_path();
  test_transaction_preflight_failure_records_state();
  test_transaction_capability_gate_records_state();
  test_transaction_backend_route_failure_records_state();
  test_transaction_converged_route_attribution_records_executor();
}
