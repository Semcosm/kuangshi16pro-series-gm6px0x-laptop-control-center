#include "backends/standard/backend.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  bool present;
  uint32_t score;
  uint32_t value;
} lcc_hwmon_candidate_t;

enum { LCC_HWMON_CHANNEL_MAX = 32u };

static void trim_text(char *text) {
  size_t len = 0;

  if (text == NULL) {
    return;
  }

  len = strlen(text);
  while (len > 0u && isspace((unsigned char)text[len - 1u]) != 0) {
    text[len - 1u] = '\0';
    --len;
  }
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

static lcc_status_t read_text_file(const char *path, char *buffer,
                                   size_t buffer_len) {
  FILE *stream = NULL;

  if (path == NULL || buffer == NULL || buffer_len == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  stream = fopen(path, "r");
  if (stream == NULL) {
    return errno == ENOENT ? LCC_ERR_NOT_FOUND : LCC_ERR_IO;
  }
  if (fgets(buffer, (int)buffer_len, stream) == NULL) {
    (void)fclose(stream);
    return LCC_ERR_PARSE;
  }
  (void)fclose(stream);
  trim_text(buffer);
  return LCC_OK;
}

static bool text_contains_ci(const char *haystack, const char *needle) {
  size_t haystack_len = 0;
  size_t needle_len = 0;
  size_t start = 0;

  if (haystack == NULL || needle == NULL || needle[0] == '\0') {
    return false;
  }

  haystack_len = strlen(haystack);
  needle_len = strlen(needle);
  if (needle_len > haystack_len) {
    return false;
  }

  for (start = 0u; start + needle_len <= haystack_len; ++start) {
    size_t index = 0u;

    while (index < needle_len) {
      const char lhs =
          (char)tolower((unsigned char)haystack[start + index]);
      const char rhs = (char)tolower((unsigned char)needle[index]);
      if (lhs != rhs) {
        break;
      }
      ++index;
    }
    if (index == needle_len) {
      return true;
    }
  }

  return false;
}

static bool text_matches_any_ci(const char *text, const char *const *patterns,
                                size_t count) {
  size_t index = 0u;

  if (text == NULL || text[0] == '\0' || patterns == NULL) {
    return false;
  }

  for (index = 0u; index < count; ++index) {
    if (patterns[index] != NULL && text_contains_ci(text, patterns[index])) {
      return true;
    }
  }

  return false;
}

static bool node_looks_cpu(const char *node_name) {
  static const char *const patterns[] = {"coretemp", "cpu", "peci", "k10temp",
                                         "zenpower"};

  return text_matches_any_ci(node_name, patterns,
                             sizeof(patterns) / sizeof(patterns[0]));
}

static bool node_looks_gpu(const char *node_name) {
  static const char *const patterns[] = {"amdgpu",  "gpu",   "nvidia",
                                         "radeon",  "nouveau", "graphics",
                                         "vga"};

  return text_matches_any_ci(node_name, patterns,
                             sizeof(patterns) / sizeof(patterns[0]));
}

static bool label_looks_cpu_temp(const char *label) {
  static const char *const patterns[] = {"cpu", "package", "pkg",
                                         "processor", "tdie", "tctl",
                                         "peci"};

  return text_matches_any_ci(label, patterns,
                             sizeof(patterns) / sizeof(patterns[0]));
}

static bool label_looks_gpu_temp(const char *label) {
  static const char *const patterns[] = {"gpu", "graphics", "vga",
                                         "edge", "junction", "hotspot"};

  return text_matches_any_ci(label, patterns,
                             sizeof(patterns) / sizeof(patterns[0]));
}

static bool label_looks_cpu_fan(const char *label) {
  static const char *const patterns[] = {"cpu", "processor"};

  return text_matches_any_ci(label, patterns,
                             sizeof(patterns) / sizeof(patterns[0]));
}

static bool label_looks_gpu_fan(const char *label) {
  static const char *const patterns[] = {"gpu", "graphics", "vga"};

  return text_matches_any_ci(label, patterns,
                             sizeof(patterns) / sizeof(patterns[0]));
}

static uint32_t score_cpu_temp_channel(const char *node_name, const char *label,
                                       unsigned int index) {
  if (label_looks_cpu_temp(label)) {
    return 120u;
  }
  if (label_looks_gpu_temp(label) || node_looks_gpu(node_name)) {
    return 0u;
  }
  if (node_looks_cpu(node_name)) {
    return 80u + (index == 1u ? 1u : 0u);
  }
  return index == 1u ? 20u : 0u;
}

static uint32_t score_gpu_temp_channel(const char *node_name, const char *label,
                                       unsigned int index) {
  if (label_looks_gpu_temp(label)) {
    return 120u;
  }
  if (label_looks_cpu_temp(label) || node_looks_cpu(node_name)) {
    return 0u;
  }
  if (node_looks_gpu(node_name)) {
    return 80u + (index == 1u ? 1u : 0u);
  }
  return index == 2u ? 20u : 0u;
}

static uint32_t score_cpu_fan_channel(const char *node_name, const char *label,
                                      unsigned int index) {
  if (label_looks_cpu_fan(label)) {
    return 120u;
  }
  if (label_looks_gpu_fan(label) || node_looks_gpu(node_name)) {
    return 0u;
  }
  if (node_looks_cpu(node_name)) {
    return 70u + (index == 1u ? 1u : 0u);
  }
  return index == 1u ? 20u : 0u;
}

static uint32_t score_gpu_fan_channel(const char *node_name, const char *label,
                                      unsigned int index) {
  if (label_looks_gpu_fan(label)) {
    return 120u;
  }
  if (label_looks_cpu_fan(label) || node_looks_cpu(node_name)) {
    return 0u;
  }
  if (node_looks_gpu(node_name)) {
    return 70u + (index == 1u ? 1u : 0u);
  }
  return index == 2u ? 20u : 0u;
}

static void capture_candidate(lcc_hwmon_candidate_t *candidate, uint32_t score,
                              uint32_t value) {
  if (candidate == NULL || score == 0u) {
    return;
  }
  if (!candidate->present || score > candidate->score) {
    candidate->present = true;
    candidate->score = score;
    candidate->value = value;
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
  lcc_hwmon_candidate_t cpu_temp = {0};
  lcc_hwmon_candidate_t gpu_temp = {0};
  lcc_hwmon_candidate_t cpu_fan = {0};
  lcc_hwmon_candidate_t gpu_fan = {0};
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
    char node_name_path[512];
    char node_name[128] = {0};
    unsigned int channel = 0u;

    if (entry->d_name[0] == '.') {
      continue;
    }
    (void)snprintf(base, sizeof(base), "%s/%s", standard->hwmon_dir,
                   entry->d_name);
    (void)snprintf(node_name_path, sizeof(node_name_path), "%s/name", base);
    (void)read_text_file(node_name_path, node_name, sizeof(node_name));

    for (channel = 1u; channel <= LCC_HWMON_CHANNEL_MAX; ++channel) {
      char path[512];
      char label_path[512];
      char label[128] = {0};
      uint32_t value = 0;

      (void)snprintf(path, sizeof(path), "%s/temp%u_input", base, channel);
      if (read_u32_file(path, &value) == LCC_OK) {
        (void)snprintf(label_path, sizeof(label_path), "%s/temp%u_label", base,
                       channel);
        (void)read_text_file(label_path, label, sizeof(label));
        capture_candidate(&cpu_temp,
                          score_cpu_temp_channel(node_name, label, channel),
                          value);
        capture_candidate(&gpu_temp,
                          score_gpu_temp_channel(node_name, label, channel),
                          value);
      }

      memset(label, 0, sizeof(label));
      (void)snprintf(path, sizeof(path), "%s/fan%u_input", base, channel);
      if (read_u32_file(path, &value) == LCC_OK) {
        (void)snprintf(label_path, sizeof(label_path), "%s/fan%u_label", base,
                       channel);
        (void)read_text_file(label_path, label, sizeof(label));
        capture_candidate(&cpu_fan,
                          score_cpu_fan_channel(node_name, label, channel),
                          value);
        capture_candidate(&gpu_fan,
                          score_gpu_fan_channel(node_name, label, channel),
                          value);
      }
    }
  }

  (void)closedir(dir);
  if (cpu_temp.present) {
    state->thermal.has_cpu_temp_c = true;
    state->thermal.cpu_temp_c = (uint8_t)(cpu_temp.value / 1000u);
    any = true;
  }
  if (gpu_temp.present) {
    state->thermal.has_gpu_temp_c = true;
    state->thermal.gpu_temp_c = (uint8_t)(gpu_temp.value / 1000u);
    any = true;
  }
  if (cpu_fan.present) {
    state->thermal.has_cpu_fan_rpm = true;
    state->thermal.cpu_fan_rpm = (uint16_t)cpu_fan.value;
    any = true;
  }
  if (gpu_fan.present) {
    state->thermal.has_gpu_fan_rpm = true;
    state->thermal.gpu_fan_rpm = (uint16_t)gpu_fan.value;
    any = true;
  }
  return any ? LCC_OK : LCC_ERR_NOT_FOUND;
}
