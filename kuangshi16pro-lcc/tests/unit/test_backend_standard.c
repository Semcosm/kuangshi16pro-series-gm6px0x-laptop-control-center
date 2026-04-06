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
  assert(snprintf(root, root_len, "/tmp/lcc-standard-test-%ld-%ld", pid,
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
  (void)snprintf(path, sizeof(path), "%s/class/powercap/intel-rapl:0/constraint_0_power_limit_uw",
                 root);
  write_text_file(path, "45000000\n");
  (void)snprintf(path, sizeof(path), "%s/class/powercap/intel-rapl:0/constraint_1_power_limit_uw",
                 root);
  write_text_file(path, "90000000\n");
  (void)snprintf(path, sizeof(path), "%s/firmware/acpi/platform_profile", root);
  write_text_file(path, "performance\n");
}

void lcc_run_backend_standard_tests(void) {
  char root[256];
  char path[512];
  char profile_value[64];
  lcc_backend_t backend;
  lcc_standard_backend_t standard;
  lcc_backend_capabilities_t capabilities;
  lcc_backend_result_t result;
  lcc_state_snapshot_t state;
  lcc_power_limits_t limits;

  init_fake_sysfs(root, sizeof(root));

  assert(lcc_standard_backend_init_at_root(&standard, &backend, root) ==
         LCC_OK);

  memset(&capabilities, 0, sizeof(capabilities));
  assert(lcc_backend_probe(&backend, &capabilities, &result) == LCC_OK);
  assert(capabilities.can_read_state);
  assert(capabilities.can_apply_mode);
  assert(capabilities.can_apply_profile);
  assert(!capabilities.can_apply_power_limits);
  assert(!capabilities.can_apply_fan_table);
  assert(capabilities.has_platform_profile);
  assert(capabilities.has_powercap);

  memset(&state, 0, sizeof(state));
  assert(lcc_backend_read_state(&backend, &state, &result) == LCC_OK);
  assert(strcmp(state.backend_name, "standard") == 0);
  assert(strcmp(state.requested.profile, "turbo") == 0);
  assert(strcmp(state.effective.profile, "turbo") == 0);
  assert(strcmp(state.requested.fan_table, "system-default") == 0);
  assert(state.thermal.has_cpu_temp_c);
  assert(state.thermal.cpu_temp_c == 61u);
  assert(state.thermal.has_gpu_temp_c);
  assert(state.thermal.gpu_temp_c == 56u);
  assert(state.thermal.has_cpu_fan_rpm);
  assert(state.thermal.cpu_fan_rpm == 2480u);
  assert(state.thermal.has_gpu_fan_rpm);
  assert(state.thermal.gpu_fan_rpm == 2310u);
  assert(state.requested.has_power_limits);
  assert(state.requested.power_limits.pl1.present);
  assert(state.requested.power_limits.pl1.value == 45u);
  assert(state.requested.power_limits.pl2.present);
  assert(state.requested.power_limits.pl2.value == 90u);

  assert(lcc_backend_apply_mode(&backend, LCC_MODE_OFFICE, &result) == LCC_OK);
  assert(result.changed);
  assert(result.hardware_write);

  (void)snprintf(path, sizeof(path), "%s/firmware/acpi/platform_profile", root);
  read_text_file(path, profile_value, sizeof(profile_value));
  assert(strcmp(profile_value, "low-power\n") == 0);

  memset(&state, 0, sizeof(state));
  assert(lcc_backend_read_state(&backend, &state, &result) == LCC_OK);
  assert(strcmp(state.effective.profile, "office") == 0);

  memset(&limits, 0, sizeof(limits));
  limits.pl1.present = true;
  limits.pl1.value = 60u;
  assert(lcc_backend_apply_power_limits(&backend, &limits, &result) ==
         LCC_ERR_NOT_SUPPORTED);
  assert(lcc_backend_apply_fan_table(&backend, "M4T1", &result) ==
         LCC_ERR_NOT_SUPPORTED);
}
