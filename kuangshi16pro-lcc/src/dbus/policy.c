#include "dbus/policy.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <polkit/polkit.h>

#include "common/lcc_log.h"

static lcc_status_t map_errno_status(int saved_errno) {
  switch (saved_errno) {
    case EACCES:
    case EPERM:
      return LCC_ERR_PERMISSION;
    case ENOENT:
      return LCC_ERR_NOT_FOUND;
    default:
      return LCC_ERR_IO;
  }
}

static lcc_status_t read_process_start_time(pid_t pid, guint64 *start_time) {
  char path[64];
  char buffer[4096];
  char *close_paren = NULL;
  char *save = NULL;
  char *token = NULL;
  FILE *stream = NULL;
  size_t bytes = 0;
  int written = 0;
  int index = 0;
  unsigned long long parsed = 0;

  if (pid <= 0 || start_time == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  written = snprintf(path, sizeof(path), "/proc/%ld/stat", (long)pid);
  if (written < 0 || (size_t)written >= sizeof(path)) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  stream = fopen(path, "r");
  if (stream == NULL) {
    return map_errno_status(errno);
  }

  bytes = fread(buffer, 1u, sizeof(buffer) - 1u, stream);
  if (ferror(stream) != 0) {
    const int saved_errno = errno;

    (void)fclose(stream);
    return map_errno_status(saved_errno);
  }
  if (fclose(stream) != 0) {
    return map_errno_status(errno);
  }

  buffer[bytes] = '\0';
  close_paren = strrchr(buffer, ')');
  if (close_paren == NULL || close_paren[1] == '\0') {
    return LCC_ERR_IO;
  }

  token = strtok_r(close_paren + 2, " ", &save);
  while (token != NULL) {
    if (index == 19) {
      char *end = NULL;

      errno = 0;
      parsed = strtoull(token, &end, 10);
      if (errno != 0 || end == token || *end != '\0') {
        return LCC_ERR_IO;
      }
      *start_time = (guint64)parsed;
      return LCC_OK;
    }

    ++index;
    token = strtok_r(NULL, " ", &save);
  }

  return LCC_ERR_IO;
}

static const char *polkit_detail_lookup(PolkitAuthorizationResult *result,
                                        const char *key) {
  PolkitDetails *details = NULL;

  if (result == NULL || key == NULL || key[0] == '\0') {
    return NULL;
  }

  details = polkit_authorization_result_get_details(result);
  if (details == NULL) {
    return NULL;
  }

  return polkit_details_lookup(details, key);
}

static void log_polkit_denial(const char *action_id, pid_t pid, uid_t uid,
                              PolkitAuthorizationResult *result) {
  const gboolean is_challenge =
      polkit_authorization_result_get_is_challenge(result);
  const gboolean dismissed = polkit_authorization_result_get_dismissed(result);
  const char *polkit_result =
      polkit_detail_lookup(result, "polkit.result");

  if (dismissed) {
    lcc_log_warn(
        "polkit authorization dismissed action=%s pid=%ld uid=%ld "
        "challenge=%s polkit_result=%s",
        action_id, (long)pid, (long)uid, is_challenge ? "true" : "false",
        polkit_result != NULL ? polkit_result : "unknown");
    return;
  }

  if (is_challenge) {
    lcc_log_warn(
        "polkit authorization challenge not completed action=%s pid=%ld "
        "uid=%ld polkit_result=%s hint=use an active desktop session or "
        "pkttyagent --process %ld --fallback",
        action_id, (long)pid, (long)uid,
        polkit_result != NULL ? polkit_result : "unknown", (long)pid);
    return;
  }

  lcc_log_warn("polkit authorization denied action=%s pid=%ld uid=%ld "
               "polkit_result=%s",
               action_id, (long)pid, (long)uid,
               polkit_result != NULL ? polkit_result : "unknown");
}

static lcc_status_t polkit_check_authorization(sd_bus_message *message,
                                               const char *action_id) {
  sd_bus_creds *creds = NULL;
  PolkitAuthority *authority = NULL;
  PolkitSubject *subject = NULL;
  PolkitAuthorizationResult *result = NULL;
  GError *error = NULL;
  pid_t pid = 0;
  uid_t uid = 0;
  guint64 start_time = 0;
  lcc_status_t status = LCC_OK;
  int r = 0;

  if (message == NULL || action_id == NULL || action_id[0] == '\0') {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  r = sd_bus_query_sender_creds(message, SD_BUS_CREDS_PID | SD_BUS_CREDS_EUID,
                                &creds);
  if (r < 0) {
    return LCC_ERR_IO;
  }
  r = sd_bus_creds_get_pid(creds, &pid);
  if (r < 0) {
    status = LCC_ERR_IO;
    goto out;
  }
  r = sd_bus_creds_get_euid(creds, &uid);
  if (r < 0) {
    status = LCC_ERR_IO;
    goto out;
  }

  status = read_process_start_time(pid, &start_time);
  if (status != LCC_OK) {
    goto out;
  }

  authority = polkit_authority_get_sync(NULL, &error);
  if (authority == NULL) {
    lcc_log_error("failed to connect to polkit authority action=%s error=%s",
                  action_id,
                  error != NULL && error->message != NULL ? error->message
                                                          : "unknown");
    status = LCC_ERR_IO;
    goto out;
  }

  subject = polkit_unix_process_new_for_owner((gint)pid, start_time, (gint)uid);
  if (subject == NULL) {
    status = LCC_ERR_IO;
    goto out;
  }

  result = polkit_authority_check_authorization_sync(
      authority, subject, action_id, NULL,
      POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION, NULL, &error);
  if (result == NULL) {
    lcc_log_error("polkit authorization check failed action=%s pid=%ld uid=%ld "
                  "error=%s",
                  action_id, (long)pid, (long)uid,
                  error != NULL && error->message != NULL ? error->message
                                                          : "unknown");
    status = LCC_ERR_IO;
    goto out;
  }
  if (!polkit_authorization_result_get_is_authorized(result)) {
    log_polkit_denial(action_id, pid, uid, result);
    status = LCC_ERR_PERMISSION;
    goto out;
  }

out:
  if (error != NULL) {
    g_error_free(error);
  }
  if (result != NULL) {
    g_object_unref(result);
  }
  if (subject != NULL) {
    g_object_unref(subject);
  }
  if (authority != NULL) {
    g_object_unref(authority);
  }
  sd_bus_creds_unref(creds);
  return status;
}

static lcc_status_t authorize_write(sd_bus_message *message, bool use_user_bus,
                                    const char *action_id) {
  if (message == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (use_user_bus) {
    return LCC_OK;
  }

  return polkit_check_authorization(message, action_id);
}

lcc_status_t lcc_dbus_authorize(sd_bus_message *message, bool use_user_bus,
                                lcc_dbus_access_t access,
                                const char *action_id) {
  if (message == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  switch (access) {
    case LCC_DBUS_ACCESS_READ:
      return LCC_OK;
    case LCC_DBUS_ACCESS_WRITE:
      return authorize_write(message, use_user_bus, action_id);
  }

  return LCC_ERR_INVALID_ARGUMENT;
}
