#include "backends/standard/backend.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
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

static bool read_constraint(const char *base, const char *name,
                            lcc_optional_byte_t *target) {
  char path[512];
  uint32_t value = 0;

  if (base == NULL || name == NULL || target == NULL) {
    return false;
  }
  (void)snprintf(path, sizeof(path), "%s/%s", base, name);
  if (read_u32_file(path, &value) == LCC_OK) {
    target->present = true;
    target->value = (uint8_t)((value + 500000u) / 1000000u);
    return true;
  }

  return false;
}

lcc_status_t lcc_standard_powercap_probe(const lcc_standard_backend_t *standard,
                                         bool *available) {
  DIR *dir = NULL;

  if (standard == NULL || available == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  *available = false;
  dir = opendir(standard->powercap_dir);
  if (dir == NULL) {
    return errno == ENOENT ? LCC_ERR_NOT_FOUND : LCC_ERR_IO;
  }
  *available = true;
  (void)closedir(dir);
  return LCC_OK;
}

lcc_status_t lcc_standard_powercap_read(const lcc_standard_backend_t *standard,
                                        lcc_state_snapshot_t *state) {
  DIR *dir = NULL;
  struct dirent *entry = NULL;
  bool any = false;

  if (standard == NULL || state == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  dir = opendir(standard->powercap_dir);
  if (dir == NULL) {
    return errno == ENOENT ? LCC_ERR_NOT_FOUND : LCC_ERR_IO;
  }

  while ((entry = readdir(dir)) != NULL) {
    char base[512];

    if (entry->d_name[0] == '.') {
      continue;
    }
    (void)snprintf(base, sizeof(base), "%s/%s", standard->powercap_dir,
                   entry->d_name);
    any |= read_constraint(base, "constraint_0_power_limit_uw",
                           &state->requested.power_limits.pl1);
    any |= read_constraint(base, "constraint_1_power_limit_uw",
                           &state->requested.power_limits.pl2);
    if (state->requested.power_limits.pl1.present ||
        state->requested.power_limits.pl2.present) {
      state->requested.has_power_limits = true;
      state->effective.power_limits = state->requested.power_limits;
      state->effective.has_power_limits = true;
      break;
    }
  }

  (void)closedir(dir);
  return any ? LCC_OK : LCC_ERR_NOT_FOUND;
}
