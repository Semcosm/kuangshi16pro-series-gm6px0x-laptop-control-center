#include <assert.h>
#include <string.h>

#include "backends/backend.h"
#include "daemon/manager.h"
#include "daemon/transaction.h"

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
  assert(strcmp(manager.state_cache.effective.profile, "balanced") == 0);
  assert(strcmp(manager.state_cache.requested.profile, "balanced") == 0);
}

void lcc_run_transaction_tests(void) {
  test_transaction_happy_path();
  test_transaction_failure_path();
}
