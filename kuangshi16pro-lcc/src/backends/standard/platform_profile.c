#include "backends/standard/backend.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static lcc_status_t read_text_file(const char *path, char *buffer,
                                   size_t buffer_len) {
  FILE *stream = NULL;
  size_t bytes = 0;

  if (path == NULL || buffer == NULL || buffer_len < 2u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  stream = fopen(path, "r");
  if (stream == NULL) {
    return errno == ENOENT ? LCC_ERR_NOT_FOUND : LCC_ERR_IO;
  }
  bytes = fread(buffer, 1u, buffer_len - 1u, stream);
  if (ferror(stream) != 0) {
    (void)fclose(stream);
    return LCC_ERR_IO;
  }
  buffer[bytes] = '\0';
  (void)fclose(stream);
  while (bytes > 0u &&
         (buffer[bytes - 1u] == '\n' || buffer[bytes - 1u] == '\r' ||
          buffer[bytes - 1u] == ' ' || buffer[bytes - 1u] == '\t')) {
    buffer[bytes - 1u] = '\0';
    --bytes;
  }
  return LCC_OK;
}

static lcc_status_t write_text_file(const char *path, const char *value) {
  FILE *stream = NULL;

  if (path == NULL || value == NULL || value[0] == '\0') {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  stream = fopen(path, "w");
  if (stream == NULL) {
    if (errno == EACCES || errno == EPERM) {
      return LCC_ERR_PERMISSION;
    }
    return errno == ENOENT ? LCC_ERR_NOT_FOUND : LCC_ERR_IO;
  }
  if (fprintf(stream, "%s\n", value) < 0) {
    (void)fclose(stream);
    return LCC_ERR_IO;
  }
  if (fclose(stream) != 0) {
    return LCC_ERR_IO;
  }

  return LCC_OK;
}

static const char *profile_from_mode(lcc_operating_mode_t mode) {
  switch (mode) {
    case LCC_MODE_OFFICE:
      return "low-power";
    case LCC_MODE_TURBO:
      return "performance";
    case LCC_MODE_GAMING:
      return "balanced";
    case LCC_MODE_CUSTOM:
      return "custom";
  }

  return NULL;
}

static const char *profile_from_alias(const char *profile_name) {
  if (profile_name == NULL) {
    return NULL;
  }
  if (strcmp(profile_name, "office") == 0 || strcmp(profile_name, "low-power") == 0) {
    return "low-power";
  }
  if (strcmp(profile_name, "turbo") == 0 ||
      strcmp(profile_name, "performance") == 0) {
    return "performance";
  }
  if (strcmp(profile_name, "gaming") == 0 ||
      strcmp(profile_name, "balanced") == 0) {
    return "balanced";
  }
  if (strcmp(profile_name, "custom") == 0) {
    return "custom";
  }

  return NULL;
}

static const char *alias_from_profile(const char *profile_name) {
  if (profile_name == NULL) {
    return "unknown";
  }
  if (strcmp(profile_name, "low-power") == 0) {
    return "office";
  }
  if (strcmp(profile_name, "performance") == 0) {
    return "turbo";
  }
  if (strcmp(profile_name, "custom") == 0) {
    return "custom";
  }
  if (strcmp(profile_name, "balanced") == 0) {
    return "balanced";
  }

  return profile_name;
}

lcc_status_t lcc_standard_platform_profile_probe(
    const lcc_standard_backend_t *standard, bool *available) {
  char buffer[64];
  lcc_status_t status = LCC_OK;

  if (standard == NULL || available == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  status = read_text_file(standard->platform_profile_path, buffer, sizeof(buffer));
  if (status == LCC_OK) {
    *available = true;
    return LCC_OK;
  }

  *available = false;
  return status;
}

lcc_status_t lcc_standard_platform_profile_read(
    const lcc_standard_backend_t *standard, lcc_state_snapshot_t *state) {
  char buffer[64];
  const char *alias = NULL;
  lcc_status_t status = LCC_OK;

  if (standard == NULL || state == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  status = read_text_file(standard->platform_profile_path, buffer, sizeof(buffer));
  if (status != LCC_OK) {
    return status;
  }

  alias = alias_from_profile(buffer);
  (void)snprintf(state->requested.profile, sizeof(state->requested.profile), "%s",
                 alias);
  (void)snprintf(state->effective.profile, sizeof(state->effective.profile), "%s",
                 alias);
  return LCC_OK;
}

lcc_status_t lcc_standard_platform_profile_apply_mode(
    const lcc_standard_backend_t *standard, lcc_operating_mode_t mode,
    lcc_backend_result_t *result) {
  const char *profile = profile_from_mode(mode);
  lcc_status_t status = LCC_OK;

  if (standard == NULL || profile == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  status = write_text_file(standard->platform_profile_path, profile);
  if (status != LCC_OK && result != NULL) {
    lcc_backend_result_set_detail(result,
                                  "platform_profile write failed");
  }
  if (status == LCC_OK && result != NULL) {
    result->changed = true;
    result->hardware_write = true;
  }
  return status;
}

lcc_status_t lcc_standard_platform_profile_apply_profile(
    const lcc_standard_backend_t *standard, const char *profile_name,
    lcc_backend_result_t *result) {
  const char *profile = profile_from_alias(profile_name);
  lcc_status_t status = LCC_OK;

  if (standard == NULL || profile_name == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (profile == NULL) {
    lcc_backend_result_set_detail(result,
                                  "platform_profile alias unsupported");
    return LCC_ERR_NOT_SUPPORTED;
  }

  status = write_text_file(standard->platform_profile_path, profile);
  if (status != LCC_OK && result != NULL) {
    lcc_backend_result_set_detail(result,
                                  "platform_profile write failed");
  }
  if (status == LCC_OK && result != NULL) {
    result->changed = true;
    result->hardware_write = true;
  }
  return status;
}
