#include "backends/standard/backend.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum {
  LCC_POWERCAP_MAX_CONSTRAINTS = 8
};

static lcc_status_t read_u32_file(const char *path, uint32_t *value) {
  FILE *stream = NULL;
  unsigned long parsed = 0;

  if (path == NULL || value == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  stream = fopen(path, "r");
  if (stream == NULL) {
    if (errno == ENOENT) {
      return LCC_ERR_NOT_FOUND;
    }
    if (errno == EACCES || errno == EPERM) {
      return LCC_ERR_PERMISSION;
    }
    return LCC_ERR_IO;
  }
  if (fscanf(stream, "%lu", &parsed) != 1) {
    (void)fclose(stream);
    return LCC_ERR_PARSE;
  }
  (void)fclose(stream);
  *value = (uint32_t)parsed;
  return LCC_OK;
}

static lcc_status_t write_u32_file(const char *path, uint32_t value) {
  FILE *stream = NULL;

  if (path == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  stream = fopen(path, "w");
  if (stream == NULL) {
    if (errno == ENOENT) {
      return LCC_ERR_NOT_FOUND;
    }
    if (errno == EACCES || errno == EPERM) {
      return LCC_ERR_PERMISSION;
    }
    return LCC_ERR_IO;
  }
  if (fprintf(stream, "%u\n", value) < 0) {
    (void)fclose(stream);
    return LCC_ERR_IO;
  }
  if (fclose(stream) != 0) {
    return LCC_ERR_IO;
  }

  return LCC_OK;
}

static bool read_text_file(const char *path, char *buffer, size_t buffer_len) {
  FILE *stream = NULL;

  if (path == NULL || buffer == NULL || buffer_len < 2u) {
    return false;
  }

  stream = fopen(path, "r");
  if (stream == NULL) {
    return false;
  }
  if (fgets(buffer, (int)buffer_len, stream) == NULL) {
    (void)fclose(stream);
    return false;
  }
  (void)fclose(stream);
  buffer[strcspn(buffer, "\r\n")] = '\0';
  return true;
}

static size_t count_char(const char *text, char needle) {
  size_t count = 0u;

  if (text == NULL) {
    return 0u;
  }
  for (; *text != '\0'; ++text) {
    if (*text == needle) {
      ++count;
    }
  }
  return count;
}

static int candidate_score(const char *entry_name, const char *zone_name) {
  int score = 0;

  if (entry_name == NULL) {
    return -1000000;
  }

  if (zone_name != NULL && strncmp(zone_name, "package-", 8u) == 0) {
    score += 1000;
  }
  if (strncmp(entry_name, "intel-rapl:", 11u) == 0) {
    score += 200;
  } else if (strncmp(entry_name, "intel-rapl-mmio:", 16u) == 0) {
    score += 100;
  }
  if (strstr(entry_name, "mmio") != NULL) {
    score -= 50;
  }
  if (count_char(entry_name, ':') > 1u) {
    score -= 300;
  }
  if (zone_name != NULL &&
      (strcmp(zone_name, "core") == 0 || strcmp(zone_name, "uncore") == 0 ||
       strcmp(zone_name, "dram") == 0 || strcmp(zone_name, "psys") == 0)) {
    score -= 500;
  }

  return score;
}

static lcc_status_t resolve_constraint_path(const char *base,
                                            const char *preferred_name,
                                            unsigned int fallback_index,
                                            const char *suffix, char *path,
                                            size_t path_len) {
  unsigned int index = 0u;

  if (base == NULL || suffix == NULL || path == NULL || path_len == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  if (preferred_name != NULL && preferred_name[0] != '\0') {
    for (index = 0u; index < LCC_POWERCAP_MAX_CONSTRAINTS; ++index) {
      char name_path[512];
      char constraint_name[64];

      (void)snprintf(name_path, sizeof(name_path), "%s/constraint_%u_name",
                     base, index);
      if (!read_text_file(name_path, constraint_name, sizeof(constraint_name))) {
        continue;
      }
      if (strcmp(constraint_name, preferred_name) != 0) {
        continue;
      }
      (void)snprintf(path, path_len, "%s/constraint_%u_%s", base, index,
                     suffix);
      if (access(path, F_OK) == 0) {
        return LCC_OK;
      }
    }
  }

  (void)snprintf(path, path_len, "%s/constraint_%u_%s", base, fallback_index,
                 suffix);
  if (access(path, F_OK) == 0) {
    return LCC_OK;
  }

  path[0] = '\0';
  return LCC_ERR_NOT_FOUND;
}

static lcc_status_t select_powercap_zone(const lcc_standard_backend_t *standard,
                                         char *base, size_t base_len) {
  DIR *dir = NULL;
  struct dirent *entry = NULL;
  int best_score = -1000000;

  if (standard == NULL || base == NULL || base_len == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  base[0] = '\0';
  dir = opendir(standard->powercap_dir);
  if (dir == NULL) {
    if (errno == ENOENT) {
      return LCC_ERR_NOT_FOUND;
    }
    if (errno == EACCES || errno == EPERM) {
      return LCC_ERR_PERMISSION;
    }
    return LCC_ERR_IO;
  }

  while ((entry = readdir(dir)) != NULL) {
    char candidate_base[512];
    char candidate_name[64];
    char long_term_path[512];
    char short_term_path[512];
    int score = 0;

    if (entry->d_name[0] == '.') {
      continue;
    }

    (void)snprintf(candidate_base, sizeof(candidate_base), "%s/%s",
                   standard->powercap_dir, entry->d_name);
    if (resolve_constraint_path(candidate_base, "long_term", 0u,
                                "power_limit_uw", long_term_path,
                                sizeof(long_term_path)) != LCC_OK &&
        resolve_constraint_path(candidate_base, "short_term", 1u,
                                "power_limit_uw", short_term_path,
                                sizeof(short_term_path)) != LCC_OK) {
      continue;
    }

    candidate_name[0] = '\0';
    {
      char name_path[512];
      (void)snprintf(name_path, sizeof(name_path), "%s/name", candidate_base);
      (void)read_text_file(name_path, candidate_name, sizeof(candidate_name));
    }

    score = candidate_score(entry->d_name, candidate_name);
    if (score > best_score) {
      best_score = score;
      (void)snprintf(base, base_len, "%s", candidate_base);
    }
  }

  (void)closedir(dir);
  return base[0] == '\0' ? LCC_ERR_NOT_FOUND : LCC_OK;
}

static bool read_constraint_from_zone(const char *base,
                                      const char *preferred_name,
                                      unsigned int fallback_index,
                                      lcc_optional_byte_t *target) {
  char path[512];
  uint32_t value = 0;
  uint8_t watts = 0;

  if (base == NULL || target == NULL) {
    return false;
  }

  if (resolve_constraint_path(base, preferred_name, fallback_index,
                              "power_limit_uw", path, sizeof(path)) !=
      LCC_OK) {
    return false;
  }
  if (read_u32_file(path, &value) != LCC_OK) {
    return false;
  }

  watts = (uint8_t)((value + 500000u) / 1000000u);
  if (watts == 0u) {
    return false;
  }

  target->present = true;
  target->value = watts;
  return true;
}

static lcc_status_t read_zone_limits(const char *base, lcc_power_limits_t *limits,
                                     bool *any) {
  if (base == NULL || limits == NULL || any == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  memset(limits, 0, sizeof(*limits));
  *any = false;
  *any |= read_constraint_from_zone(base, "long_term", 0u, &limits->pl1);
  *any |= read_constraint_from_zone(base, "short_term", 1u, &limits->pl2);
  return *any ? LCC_OK : LCC_ERR_NOT_FOUND;
}

lcc_status_t lcc_standard_powercap_probe(const lcc_standard_backend_t *standard,
                                         bool *available) {
  char base[512];
  lcc_status_t status = LCC_OK;

  if (standard == NULL || available == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  *available = false;
  status = select_powercap_zone(standard, base, sizeof(base));
  if (status == LCC_OK) {
    *available = true;
  }
  return status;
}

lcc_status_t lcc_standard_powercap_can_write(
    const lcc_standard_backend_t *standard, bool *writable) {
  char base[512];
  char path[512];
  lcc_status_t status = LCC_OK;

  if (standard == NULL || writable == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  *writable = false;
  status = select_powercap_zone(standard, base, sizeof(base));
  if (status != LCC_OK) {
    return status;
  }

  if (resolve_constraint_path(base, "long_term", 0u, "power_limit_uw", path,
                              sizeof(path)) == LCC_OK &&
      access(path, W_OK) == 0) {
    *writable = true;
    return LCC_OK;
  }
  if (resolve_constraint_path(base, "short_term", 1u, "power_limit_uw", path,
                              sizeof(path)) == LCC_OK &&
      access(path, W_OK) == 0) {
    *writable = true;
    return LCC_OK;
  }

  return LCC_OK;
}

lcc_status_t lcc_standard_powercap_read(const lcc_standard_backend_t *standard,
                                        lcc_state_snapshot_t *state) {
  char base[512];
  bool any = false;
  lcc_status_t status = LCC_OK;

  if (standard == NULL || state == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  status = select_powercap_zone(standard, base, sizeof(base));
  if (status != LCC_OK) {
    return status;
  }

  status = read_zone_limits(base, &state->requested.power_limits, &any);
  if (status != LCC_OK) {
    return status;
  }

  state->requested.has_power_limits = any;
  state->effective.power_limits = state->requested.power_limits;
  state->effective.has_power_limits = any;
  return LCC_OK;
}

lcc_status_t lcc_standard_powercap_apply(
    const lcc_standard_backend_t *standard, const lcc_power_limits_t *limits,
    lcc_backend_result_t *result) {
  char base[512];
  lcc_power_limits_t readback;
  bool any = false;
  lcc_status_t status = LCC_OK;

  if (standard == NULL || limits == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  if (limits->pl4.present || limits->tcc_offset.present) {
    if (result != NULL) {
      lcc_backend_result_set_detail(
          result, "standard powercap only supports PL1 and PL2");
    }
    return LCC_ERR_NOT_SUPPORTED;
  }
  if (!limits->pl1.present && !limits->pl2.present) {
    if (result != NULL) {
      lcc_backend_result_set_detail(
          result, "power limit write requires PL1 or PL2");
    }
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if ((limits->pl1.present && limits->pl1.value == 0u) ||
      (limits->pl2.present && limits->pl2.value == 0u)) {
    if (result != NULL) {
      lcc_backend_result_set_detail(
          result, "powercap PL1 and PL2 must be positive watts");
    }
    return LCC_ERR_RANGE;
  }

  status = select_powercap_zone(standard, base, sizeof(base));
  if (status != LCC_OK) {
    if (result != NULL) {
      lcc_backend_result_set_detail(
          result, "no authoritative powercap package zone found");
    }
    return status;
  }

  if (limits->pl1.present) {
    char path[512];

    lcc_backend_result_set_stage(result, "write-powercap-pl1");
    status = resolve_constraint_path(base, "long_term", 0u, "power_limit_uw",
                                     path, sizeof(path));
    if (status != LCC_OK) {
      if (result != NULL) {
        lcc_backend_result_set_detail(
            result, "powercap long_term constraint is unavailable");
      }
      return LCC_ERR_NOT_SUPPORTED;
    }
    status = write_u32_file(path, (uint32_t)limits->pl1.value * 1000000u);
    if (status != LCC_OK) {
      if (result != NULL) {
        lcc_backend_result_set_detail(result,
                                      "powercap long_term write failed");
      }
      return status;
    }
  }

  if (limits->pl2.present) {
    char path[512];

    lcc_backend_result_set_stage(result, "write-powercap-pl2");
    status = resolve_constraint_path(base, "short_term", 1u,
                                     "power_limit_uw", path, sizeof(path));
    if (status != LCC_OK) {
      if (result != NULL) {
        lcc_backend_result_set_detail(
            result, "powercap short_term constraint is unavailable");
      }
      return LCC_ERR_NOT_SUPPORTED;
    }
    status = write_u32_file(path, (uint32_t)limits->pl2.value * 1000000u);
    if (status != LCC_OK) {
      if (result != NULL) {
        lcc_backend_result_set_detail(result,
                                      "powercap short_term write failed");
      }
      return status;
    }
  }

  lcc_backend_result_set_stage(result, "verify-powercap");
  status = read_zone_limits(base, &readback, &any);
  if (status != LCC_OK || !any) {
    if (result != NULL) {
      lcc_backend_result_set_detail(result,
                                    "powercap readback failed after write");
    }
    return status == LCC_OK ? LCC_ERR_IO : status;
  }
  if ((limits->pl1.present &&
       (!readback.pl1.present || readback.pl1.value != limits->pl1.value)) ||
      (limits->pl2.present &&
       (!readback.pl2.present || readback.pl2.value != limits->pl2.value))) {
    if (result != NULL) {
      lcc_backend_result_set_detail(result,
                                    "powercap readback mismatch after write");
    }
    return LCC_ERR_IO;
  }

  if (result != NULL) {
    result->changed = true;
    result->hardware_write = true;
  }
  return LCC_OK;
}
