#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "backends/backend.h"

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

static void init_fake_sysfs(char *root, size_t root_len) {
  char path[512];
  const long timestamp = (long)time(NULL);
  const long pid = (long)getpid();

  assert(root != NULL);
  assert(root_len > 0u);
  assert(snprintf(root, root_len, "/tmp/lcc-converged-test-%ld-%ld", pid,
                  timestamp) > 0);

  make_dir(root);

  (void)snprintf(path, sizeof(path), "%s/class", root);
  make_dir(path);
  (void)snprintf(path, sizeof(path), "%s/class/hwmon", root);
  make_dir(path);
  (void)snprintf(path, sizeof(path), "%s/class/hwmon/hwmon0", root);
  make_dir(path);
  (void)snprintf(path, sizeof(path), "%s/class/thermal", root);
  make_dir(path);
  (void)snprintf(path, sizeof(path), "%s/class/thermal/thermal_zone0", root);
  make_dir(path);
  (void)snprintf(path, sizeof(path), "%s/class/powercap", root);
  make_dir(path);
  (void)snprintf(path, sizeof(path), "%s/class/powercap/intel-rapl:0", root);
  make_dir(path);
  (void)snprintf(path, sizeof(path), "%s/firmware", root);
  make_dir(path);
  (void)snprintf(path, sizeof(path), "%s/firmware/acpi", root);
  make_dir(path);

  (void)snprintf(path, sizeof(path), "%s/class/hwmon/hwmon0/temp1_input", root);
  write_text_file(path, "61000\n");
  (void)snprintf(path, sizeof(path), "%s/class/hwmon/hwmon0/temp2_input", root);
  write_text_file(path, "56000\n");
  (void)snprintf(path, sizeof(path), "%s/class/hwmon/hwmon0/fan1_input", root);
  write_text_file(path, "2480\n");
  (void)snprintf(path, sizeof(path), "%s/class/hwmon/hwmon0/fan2_input", root);
  write_text_file(path, "2310\n");
  (void)snprintf(path, sizeof(path), "%s/class/thermal/thermal_zone0/temp",
                 root);
  write_text_file(path, "59000\n");
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl:0/constraint_0_power_limit_uw",
                 root);
  write_text_file(path, "45000000\n");
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl:0/constraint_1_power_limit_uw",
                 root);
  write_text_file(path, "90000000\n");
  (void)snprintf(path, sizeof(path), "%s/firmware/acpi/platform_profile", root);
  write_text_file(path, "performance\n");
}

void lcc_run_backend_converged_tests(void) {
  char root[256];
  char path[512];
  char profile_value[64];
  lcc_backend_t standard_handle;
  lcc_backend_t amw0_handle;
  lcc_backend_t converged_handle;
  lcc_standard_backend_t standard_backend;
  lcc_amw0_backend_t amw0_backend;
  lcc_converged_backend_t converged_backend;
  lcc_backend_capabilities_t standard_capabilities;
  lcc_backend_capabilities_t amw0_capabilities;
  lcc_backend_capabilities_t merged_capabilities;
  lcc_backend_result_t result;
  lcc_state_snapshot_t state;
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

  memset(&merged_capabilities, 0, sizeof(merged_capabilities));
  assert(lcc_backend_probe(&converged_handle, &merged_capabilities, &result) ==
         LCC_OK);
  assert(merged_capabilities.can_read_state);
  assert(merged_capabilities.can_apply_mode);
  assert(merged_capabilities.can_apply_power_limits);
  assert(merged_capabilities.can_apply_fan_table);

  memset(&state, 0, sizeof(state));
  assert(lcc_backend_read_state(&converged_handle, &state, &result) == LCC_OK);
  assert(strcmp(state.backend_name, "standard") == 0);
  assert(strcmp(state.backend_selected, "standard") == 0);
  assert(strcmp(state.execution.read_state, "standard") == 0);
  assert(strcmp(state.execution.apply_mode, "standard") == 0);
  assert(strcmp(state.execution.apply_power_limits, "amw0") == 0);
  assert(strcmp(state.execution.apply_fan_table, "amw0") == 0);
  assert(strstr(state.backend_fallback_reason, "apply_power_limits") != NULL);
  assert(strstr(state.backend_fallback_reason, "apply_fan_table") != NULL);

  assert(lcc_backend_apply_mode(&converged_handle, LCC_MODE_OFFICE, &result) ==
         LCC_OK);
  assert(result.hardware_write);
  (void)snprintf(path, sizeof(path), "%s/firmware/acpi/platform_profile", root);
  read_text_file(path, profile_value, sizeof(profile_value));
  assert(strcmp(profile_value, "low-power\n") == 0);

  memset(&limits, 0, sizeof(limits));
  limits.pl1.present = true;
  limits.pl1.value = 70u;
  limits.pl2.present = true;
  limits.pl2.value = 120u;
  assert(lcc_backend_apply_power_limits(&converged_handle, &limits, &result) ==
         LCC_OK);
  assert(!result.hardware_write);

  memset(&state, 0, sizeof(state));
  assert(lcc_backend_read_state(&converged_handle, &state, &result) == LCC_OK);
  assert(state.effective.has_power_limits);
  assert(state.effective.power_limits.pl1.value == 70u);
  assert(state.effective.power_limits.pl2.value == 120u);

  assert(lcc_backend_apply_fan_table(&converged_handle, "M4T1", &result) ==
         LCC_OK);
  assert(!result.hardware_write);

  memset(&state, 0, sizeof(state));
  assert(lcc_backend_read_state(&converged_handle, &state, &result) == LCC_OK);
  assert(strcmp(state.effective.fan_table, "M4T1") == 0);
  assert(strcmp(state.effective.profile, "custom") == 0);
}
