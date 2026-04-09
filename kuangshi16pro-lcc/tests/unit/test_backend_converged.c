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
  (void)snprintf(path, sizeof(path), "%s/class/powercap/intel-rapl-mmio:0",
                 root);
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
  (void)snprintf(path, sizeof(path), "%s/class/powercap/intel-rapl:0/name",
                 root);
  write_text_file(path, "package-0\n");
  (void)snprintf(path, sizeof(path), "%s/class/powercap/intel-rapl:0/enabled",
                 root);
  write_text_file(path, "1\n");
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
                 "%s/class/powercap/intel-rapl-mmio:0/enabled", root);
  write_text_file(path, "1\n");
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl-mmio:0/constraint_0_name", root);
  write_text_file(path, "long_term\n");
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl-mmio:0/constraint_1_name", root);
  write_text_file(path, "short_term\n");
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
  amw0_backend.shadow_state.thermal.has_vendor_fan_level = true;
  amw0_backend.shadow_state.thermal.vendor_fan_level = 7u;
  (void)lcc_backend_effective_component_set(
      &amw0_backend.shadow_state.effective_meta.thermal, "cache", "cache");
  lcc_backend_state_finalize_effective_meta(&amw0_backend.shadow_state);
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
  assert(strcmp(state.execution.apply_power_limits, "standard") == 0);
  assert(strcmp(state.execution.apply_fan_table, "amw0") == 0);
  assert(strcmp(result.executor_backend, "standard") == 0);
  assert(strstr(state.backend_fallback_reason, "apply_fan_table") != NULL);
  assert(strcmp(state.effective_meta.source, "mixed") == 0);
  assert(strcmp(state.effective_meta.freshness, "mixed") == 0);
  assert(strcmp(state.effective_meta.profile.source, "cache") == 0);
  assert(strcmp(state.effective_meta.power.source, "standard") == 0);
  assert(strcmp(state.effective_meta.power.freshness, "live") == 0);
  assert(strcmp(state.effective_meta.power_fields.pl1.source, "standard") == 0);
  assert(strcmp(state.effective_meta.power_fields.pl2.source, "standard") == 0);
  assert(state.effective_meta.power_fields.pl4.source[0] == '\0');
  assert(state.effective_meta.power_fields.tcc_offset.source[0] == '\0');
  assert(state.thermal.has_vendor_fan_level);
  assert(state.thermal.vendor_fan_level == 7u);
  assert(strcmp(state.effective_meta.thermal.source, "mixed") == 0);
  assert(strcmp(state.effective_meta.thermal.freshness, "mixed") == 0);

  assert(lcc_backend_apply_mode(&converged_handle, LCC_MODE_OFFICE, &result) ==
         LCC_OK);
  assert(result.hardware_write);
  assert(strcmp(result.executor_backend, "standard") == 0);
  assert(strcmp(result.stage, "write-platform-profile") == 0);
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
  assert(strcmp(result.executor_backend, "standard") == 0);
  assert(result.hardware_write);
  assert(strcmp(result.stage, "verify-powercap") == 0);
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl:0/constraint_0_power_limit_uw",
                 root);
  read_text_file(path, profile_value, sizeof(profile_value));
  assert(strcmp(profile_value, "70000000\n") == 0);
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl-mmio:0/constraint_0_power_limit_uw",
                 root);
  read_text_file(path, profile_value, sizeof(profile_value));
  assert(strcmp(profile_value, "140000000\n") == 0);

  memset(&state, 0, sizeof(state));
  assert(lcc_backend_read_state(&converged_handle, &state, &result) == LCC_OK);
  assert(state.effective.has_power_limits);
  assert(state.effective.power_limits.pl1.value == 70u);
  assert(state.effective.power_limits.pl2.value == 120u);
  assert(strcmp(state.effective_meta.power.source, "standard") == 0);
  assert(strcmp(state.effective_meta.power.freshness, "live") == 0);
  assert(strcmp(state.effective_meta.power_fields.pl1.source, "standard") == 0);
  assert(strcmp(state.effective_meta.power_fields.pl2.source, "standard") == 0);
  assert(state.effective_meta.power_fields.tcc_offset.source[0] == '\0');

  memset(&limits, 0, sizeof(limits));
  limits.pl1.present = true;
  limits.pl1.value = 60u;
  limits.pl2.present = true;
  limits.pl2.value = 110u;
  limits.pl4.present = true;
  limits.pl4.value = 140u;
  limits.tcc_offset.present = true;
  limits.tcc_offset.value = 5u;
  assert(lcc_backend_apply_power_limits(&converged_handle, &limits, &result) ==
         LCC_OK);
  assert(strcmp(result.executor_backend, "mixed") == 0);
  assert(result.hardware_write);
  assert(strcmp(result.stage, "mixed-power-apply") == 0);
  assert(strstr(result.detail, "standard_stage=verify-powercap") != NULL);
  assert(strstr(result.detail, "amw0_stage=write-tcc-offset") != NULL);
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl:0/constraint_0_power_limit_uw",
                 root);
  read_text_file(path, profile_value, sizeof(profile_value));
  assert(strcmp(profile_value, "60000000\n") == 0);
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl:0/constraint_1_power_limit_uw",
                 root);
  read_text_file(path, profile_value, sizeof(profile_value));
  assert(strcmp(profile_value, "110000000\n") == 0);
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl-mmio:0/constraint_1_power_limit_uw",
                 root);
  read_text_file(path, profile_value, sizeof(profile_value));
  assert(strcmp(profile_value, "140000000\n") == 0);

  memset(&state, 0, sizeof(state));
  assert(lcc_backend_read_state(&converged_handle, &state, &result) == LCC_OK);
  assert(state.effective.has_power_limits);
  assert(state.effective.power_limits.pl1.present);
  assert(state.effective.power_limits.pl1.value == 60u);
  assert(state.effective.power_limits.pl2.present);
  assert(state.effective.power_limits.pl2.value == 110u);
  assert(state.effective.power_limits.pl4.present);
  assert(state.effective.power_limits.pl4.value == 140u);
  assert(state.effective.power_limits.tcc_offset.present);
  assert(state.effective.power_limits.tcc_offset.value == 5u);
  assert(strcmp(state.effective_meta.power.source, "mixed") == 0);
  assert(strcmp(state.effective_meta.power.freshness, "mixed") == 0);
  assert(strcmp(state.effective_meta.power_fields.pl1.source, "standard") == 0);
  assert(strcmp(state.effective_meta.power_fields.pl2.source, "standard") == 0);
  assert(strcmp(state.effective_meta.power_fields.pl4.source, "cache") == 0);
  assert(strcmp(state.effective_meta.power_fields.pl4.freshness, "cache") == 0);
  assert(strcmp(state.effective_meta.power_fields.tcc_offset.source, "cache") ==
         0);
  assert(strcmp(state.effective_meta.power_fields.tcc_offset.freshness,
                "cache") == 0);

  assert(lcc_backend_apply_fan_table(&converged_handle, "M4T1", &result) ==
         LCC_OK);
  assert(strcmp(result.executor_backend, "amw0") == 0);
  assert(!result.hardware_write);

  memset(&state, 0, sizeof(state));
  assert(lcc_backend_read_state(&converged_handle, &state, &result) == LCC_OK);
  assert(strcmp(state.effective.fan_table, "M4T1") == 0);
  assert(strcmp(state.effective.profile, "office") == 0);
  assert(strcmp(state.effective_meta.fan_table.source, "cache") == 0);

  (void)snprintf(path, sizeof(path), "%s/firmware/acpi/platform_profile", root);
  assert(unlink(path) == 0);
  (void)snprintf(path, sizeof(path), "%s/class/hwmon/hwmon0/temp1_input", root);
  assert(unlink(path) == 0);
  (void)snprintf(path, sizeof(path), "%s/class/hwmon/hwmon0/temp2_input", root);
  assert(unlink(path) == 0);
  (void)snprintf(path, sizeof(path), "%s/class/hwmon/hwmon0/fan1_input", root);
  assert(unlink(path) == 0);
  (void)snprintf(path, sizeof(path), "%s/class/hwmon/hwmon0/fan2_input", root);
  assert(unlink(path) == 0);
  (void)snprintf(path, sizeof(path), "%s/class/thermal/thermal_zone0/temp",
                 root);
  assert(unlink(path) == 0);
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl:0/constraint_0_power_limit_uw",
                 root);
  assert(unlink(path) == 0);
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl:0/constraint_1_power_limit_uw",
                 root);
  assert(unlink(path) == 0);
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl-mmio:0/constraint_0_power_limit_uw",
                 root);
  assert(unlink(path) == 0);
  (void)snprintf(path, sizeof(path),
                 "%s/class/powercap/intel-rapl-mmio:0/constraint_1_power_limit_uw",
                 root);
  assert(unlink(path) == 0);

  memset(&state, 0, sizeof(state));
  assert(lcc_backend_read_state(&converged_handle, &state, &result) == LCC_OK);
  assert(strcmp(state.backend_name, "amw0") == 0);
  assert(strcmp(result.executor_backend, "amw0") == 0);
  assert(strstr(result.detail, "fell back from standard to amw0") != NULL);
  assert(strcmp(state.effective_meta.source, "cache") == 0);
  assert(strcmp(state.effective_meta.freshness, "cache") == 0);
}
