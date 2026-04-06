#include "backends/standard/backend.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

lcc_status_t lcc_standard_thermal_probe(const lcc_standard_backend_t *standard,
                                        bool *available) {
  DIR *dir = NULL;

  if (standard == NULL || available == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  *available = false;
  dir = opendir(standard->thermal_dir);
  if (dir == NULL) {
    return errno == ENOENT ? LCC_ERR_NOT_FOUND : LCC_ERR_IO;
  }
  *available = true;
  (void)closedir(dir);
  return LCC_OK;
}

lcc_status_t lcc_standard_thermal_read(const lcc_standard_backend_t *standard,
                                       lcc_state_snapshot_t *state) {
  DIR *dir = NULL;
  struct dirent *entry = NULL;
  bool any = false;

  if (standard == NULL || state == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  dir = opendir(standard->thermal_dir);
  if (dir == NULL) {
    return errno == ENOENT ? LCC_ERR_NOT_FOUND : LCC_ERR_IO;
  }

  while ((entry = readdir(dir)) != NULL) {
    char path[512];
    uint32_t temp = 0;

    if (strncmp(entry->d_name, "thermal_zone", 12) != 0) {
      continue;
    }
    (void)snprintf(path, sizeof(path), "%s/%s/temp", standard->thermal_dir,
                   entry->d_name);
    if (read_u32_file(path, &temp) != LCC_OK) {
      continue;
    }
    if (!state->thermal.has_cpu_temp_c) {
      state->thermal.has_cpu_temp_c = true;
      state->thermal.cpu_temp_c = (uint8_t)(temp / 1000u);
    } else if (!state->thermal.has_gpu_temp_c) {
      state->thermal.has_gpu_temp_c = true;
      state->thermal.gpu_temp_c = (uint8_t)(temp / 1000u);
    }
    any = true;
  }

  (void)closedir(dir);
  return any ? LCC_OK : LCC_ERR_NOT_FOUND;
}
