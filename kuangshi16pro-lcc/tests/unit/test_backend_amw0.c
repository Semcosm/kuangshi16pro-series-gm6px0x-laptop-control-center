#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "backends/backend.h"
#include "daemon/manager.h"
#include "daemon/transaction.h"

static void make_trace_path(char *buffer, size_t buffer_len) {
  const int written = snprintf(buffer, buffer_len, "/tmp/lcc-amw0-trace-%ld.log",
                               (long)getpid());

  assert(written > 0);
  assert((size_t)written < buffer_len);
}

static void read_text_file(const char *path, char *buffer, size_t buffer_len) {
  FILE *stream = NULL;
  size_t bytes = 0;

  assert(path != NULL);
  assert(buffer != NULL);
  assert(buffer_len > 1u);
  stream = fopen(path, "r");
  assert(stream != NULL);
  bytes = fread(buffer, 1u, buffer_len - 1u, stream);
  assert(fclose(stream) == 0);
  buffer[bytes] = '\0';
}

static void test_amw0_backend_dry_run_apply(void) {
  char trace_path[256];
  char trace_contents[4096];
  lcc_backend_t backend;
  lcc_amw0_backend_t amw0;
  lcc_backend_capabilities_t capabilities;
  lcc_backend_result_t result;
  lcc_state_snapshot_t state;
  lcc_power_limits_t limits;

  make_trace_path(trace_path, sizeof(trace_path));
  (void)unlink(trace_path);

  assert(lcc_amw0_backend_init(&amw0, &backend, "/proc/acpi/call", NULL, true) ==
         LCC_OK);
  assert(amw0_backend_set_trace(&amw0.transport, trace_path) == LCC_OK);

  memset(&capabilities, 0, sizeof(capabilities));
  assert(lcc_backend_probe(&backend, &capabilities, &result) == LCC_OK);
  assert(capabilities.can_read_state);
  assert(capabilities.can_apply_profile);
  assert(capabilities.can_apply_mode);
  assert(capabilities.can_apply_power_limits);
  assert(capabilities.can_apply_fan_table);

  memset(&state, 0, sizeof(state));
  assert(lcc_backend_read_state(&backend, &state, &result) == LCC_OK);
  assert(strcmp(state.backend_name, "amw0") == 0);
  assert(strcmp(state.effective.profile, "office") == 0);

  assert(lcc_backend_apply_mode(&backend, LCC_MODE_TURBO, &result) == LCC_OK);
  assert(result.changed);
  assert(!result.hardware_write);

  memset(&state, 0, sizeof(state));
  assert(lcc_backend_read_state(&backend, &state, &result) == LCC_OK);
  assert(strcmp(state.effective.profile, "turbo") == 0);

  memset(&limits, 0, sizeof(limits));
  limits.pl1.present = true;
  limits.pl1.value = 75u;
  limits.pl2.present = true;
  limits.pl2.value = 130u;
  limits.pl4.present = true;
  limits.pl4.value = 140u;
  limits.tcc_offset.present = true;
  limits.tcc_offset.value = 5u;
  assert(lcc_backend_apply_power_limits(&backend, &limits, &result) == LCC_OK);
  assert(result.changed);
  assert(!result.hardware_write);

  memset(&state, 0, sizeof(state));
  assert(lcc_backend_read_state(&backend, &state, &result) == LCC_OK);
  assert(state.effective.has_power_limits);
  assert(state.effective.power_limits.pl1.present);
  assert(state.effective.power_limits.pl1.value == 75u);
  assert(state.effective.power_limits.pl2.present);
  assert(state.effective.power_limits.pl2.value == 130u);
  assert(state.effective.power_limits.pl4.present);
  assert(state.effective.power_limits.pl4.value == 140u);
  assert(state.effective.power_limits.tcc_offset.present);
  assert(state.effective.power_limits.tcc_offset.value == 5u);

  read_text_file(trace_path, trace_contents, sizeof(trace_contents));
  assert(strstr(trace_contents, "\\_SB.AMW0.WMBC 0x0 0x4") != NULL);
  assert(strstr(trace_contents, "0xAB,0x07,0x02,0x00") != NULL);
  assert(strstr(trace_contents, "0x51,0x07,0x10,0x00") != NULL);
  assert(strstr(trace_contents, "0x83,0x07,0x4B,0x00") != NULL);
  assert(strstr(trace_contents, "0x84,0x07,0x82,0x00") != NULL);

  (void)unlink(trace_path);
}

static void test_amw0_fan_table_dry_run_apply_order(void) {
  char trace_path[256];
  char trace_contents[65536];
  lcc_backend_t backend;
  lcc_amw0_backend_t amw0;
  lcc_backend_result_t result;
  lcc_state_snapshot_t state;

  make_trace_path(trace_path, sizeof(trace_path));
  (void)unlink(trace_path);

  assert(lcc_amw0_backend_init(&amw0, &backend, "/proc/acpi/call", NULL, true) ==
         LCC_OK);
  assert(amw0_backend_set_trace(&amw0.transport, trace_path) == LCC_OK);
  assert(lcc_backend_apply_fan_table(&backend, "M4T1", &result) == LCC_OK);
  assert(result.changed);
  assert(!result.hardware_write);

  memset(&state, 0, sizeof(state));
  assert(lcc_backend_read_state(&backend, &state, &result) == LCC_OK);
  assert(strcmp(state.effective.fan_table, "M4T1") == 0);
  assert(strcmp(state.effective.profile, "custom") == 0);

  read_text_file(trace_path, trace_contents, sizeof(trace_contents));
  assert(strstr(trace_contents, "note=SetFanTableThread -> MyFanTableCtrl::SetFanTable (confirmed call edge)") != NULL);
  assert(strstr(trace_contents, "note=stage=custom-enable") != NULL);
  assert(strstr(trace_contents, "note=FanTable_Manager1p5::SetEcFanTable_Cpu") != NULL);
  assert(strstr(trace_contents, "note=FanTable_Manager1p5::SetEcFanTable_Gpu") != NULL);
  assert(strstr(trace_contents, "note=FanTable_Manager1p5::FinalizeTailBytes") != NULL);
  assert(strstr(trace_contents, "addr=0x0F00") != NULL);
  assert(strstr(trace_contents, "addr=0x0F30") != NULL);
  assert(strstr(trace_contents, "addr=0x0F5D") != NULL);
  assert(strstr(trace_contents, "addr=0x0F5F") != NULL);

  (void)unlink(trace_path);
}

static void test_amw0_transaction_dry_run_apply(void) {
  lcc_backend_t backend;
  lcc_amw0_backend_t amw0;
  lcc_manager_t manager;
  lcc_transaction_request_t request;
  lcc_power_limits_t limits;

  assert(lcc_amw0_backend_init(&amw0, &backend, "/proc/acpi/call", NULL, true) ==
         LCC_OK);
  assert(lcc_manager_init(&manager, &backend, NULL) == LCC_OK);

  memset(&request, 0, sizeof(request));
  request.kind = LCC_TRANSACTION_MODE;
  request.input.mode_name = "turbo";
  assert(lcc_transaction_execute(&manager, &request) == LCC_OK);
  assert(manager.state_cache.transaction.state == LCC_TRANSACTION_STATE_IDLE);
  assert(strcmp(manager.state_cache.effective.profile, "turbo") == 0);

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
  assert(manager.state_cache.effective.has_power_limits);
  assert(manager.state_cache.effective.power_limits.pl1.value == 70u);
  assert(manager.state_cache.effective.power_limits.pl2.value == 120u);
}

static void test_amw0_transaction_fan_failure_stage(void) {
  lcc_backend_t backend;
  lcc_amw0_backend_t amw0;
  lcc_manager_t manager;
  lcc_transaction_request_t request;

  assert(lcc_amw0_backend_init(&amw0, &backend, "/proc/acpi/call", NULL, true) ==
         LCC_OK);
  lcc_amw0_backend_fail_after_writes(&amw0, 51u);
  assert(lcc_manager_init(&manager, &backend, NULL) == LCC_OK);

  memset(&request, 0, sizeof(request));
  request.kind = LCC_TRANSACTION_FAN_TABLE;
  request.input.fan_table_name = "M4T1";
  assert(lcc_transaction_execute(&manager, &request) == LCC_ERR_IO);
  assert(manager.state_cache.transaction.state == LCC_TRANSACTION_STATE_FAILED);
  assert(strcmp(manager.state_cache.transaction.stage,
                "FanTable_Manager1p5::SetEcFanTable_Gpu") == 0);
  assert(strcmp(manager.state_cache.transaction.pending_target.fan_table,
                "M4T1") == 0);
  assert(strcmp(manager.state_cache.effective.fan_table, "system-default") == 0);
}

void lcc_run_backend_amw0_tests(void) {
  test_amw0_backend_dry_run_apply();
  test_amw0_fan_table_dry_run_apply_order();
  test_amw0_transaction_dry_run_apply();
  test_amw0_transaction_fan_failure_stage();
}
