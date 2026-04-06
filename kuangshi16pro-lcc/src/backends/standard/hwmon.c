#include "backends/standard/backend.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool path_exists(const char *path) {
  struct stat st;

  return path != NULL && stat(path, &st) == 0;
}

static lcc_status_t read_u32_file(const char *path, uint32_t *value) {
  FILE *stream = NULL;
  unsigned long parsed = 0;

  if (path == NULL || value == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  stream = fopen(path, "r");
  if (stream == NULL) {
    return errno == ENOENT ? LCC_ERR_NOT_FOUND : LCC_ERR_IO;
  }
  if (fscanf(stream, "%lu", &parsed) != 1) {
    (void)fclose(stream);
    return LCC_ERR_PARSE;
  }
  (void)fclose(stream);
  *value = (uint32_t)parsed;
  return LCC_OK;
}

static void maybe_read_temp(const char *path, bool *present, uint8_t *target) {
  uint32_t value = 0;

  if (present == NULL || target == NULL) {
    return;
  }
  if (read_u32_file(path, &value) == LCC_OK) {
    *present = true;
    *target = (uint8_t)(value / 1000u);
  }
}

static void maybe_read_fan(const char *path, bool *present, uint16_t *target) {
  uint32_t value = 0;

  if (present == NULL || target == NULL) {
    return;
  }
  if (read_u32_file(path, &value) == LCC_OK) {
    *present = true;
    *target = (uint16_t)value;
  }
}

lcc_status_t lcc_standard_hwmon_probe(const lcc_standard_backend_t *standard,
                                      bool *available) {
  DIR *dir = NULL;

  if (standard == NULL || available == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  *available = false;
  dir = opendir(standard->hwmon_dir);
  if (dir == NULL) {
    return errno == ENOENT ? LCC_ERR_NOT_FOUND : LCC_ERR_IO;
  }
  *available = true;
  (void)closedir(dir);
  return LCC_OK;
}

lcc_status_t lcc_standard_hwmon_read(const lcc_standard_backend_t *standard,
                                     lcc_state_snapshot_t *state) {
  DIR *dir = NULL;
  struct dirent *entry = NULL;
  bool any = false;

  if (standard == NULL || state == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  dir = opendir(standard->hwmon_dir);
  if (dir == NULL) {
    return errno == ENOENT ? LCC_ERR_NOT_FOUND : LCC_ERR_IO;
  }

  while ((entry = readdir(dir)) != NULL) {
    char base[512];
    char path[512];

    if (entry->d_name[0] == '.') {
      continue;
    }
    (void)snprintf(base, sizeof(base), "%s/%s", standard->hwmon_dir,
                   entry->d_name);

    (void)snprintf(path, sizeof(path), "%s/temp1_input", base);
    if (path_exists(path)) {
      maybe_read_temp(path, &state->thermal.has_cpu_temp_c,
                      &state->thermal.cpu_temp_c);
      any = true;
    }
    (void)snprintf(path, sizeof(path), "%s/temp2_input", base);
    if (path_exists(path)) {
      maybe_read_temp(path, &state->thermal.has_gpu_temp_c,
                      &state->thermal.gpu_temp_c);
      any = true;
    }
    (void)snprintf(path, sizeof(path), "%s/fan1_input", base);
    if (path_exists(path)) {
      maybe_read_fan(path, &state->thermal.has_cpu_fan_rpm,
                     &state->thermal.cpu_fan_rpm);
      any = true;
    }
    (void)snprintf(path, sizeof(path), "%s/fan2_input", base);
    if (path_exists(path)) {
      maybe_read_fan(path, &state->thermal.has_gpu_fan_rpm,
                     &state->thermal.gpu_fan_rpm);
      any = true;
    }
  }

  (void)closedir(dir);
  return any ? LCC_OK : LCC_ERR_NOT_FOUND;
}
