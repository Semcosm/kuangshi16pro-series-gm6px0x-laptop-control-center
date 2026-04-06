#include "backends/amw0/transport.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "backends/amw0/wmbc-pack.h"

static lcc_status_t map_errno(int saved_errno) {
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

static void trim_reply(char *reply) {
  size_t length = 0;

  if (reply == NULL) {
    return;
  }

  length = strlen(reply);
  while (length > 0) {
    const unsigned char c = (unsigned char)reply[length - 1];
    if (c != '\0' && !isspace(c)) {
      break;
    }
    reply[length - 1] = '\0';
    --length;
  }
}

static bool amw0_reply_looks_numeric(const char *reply) {
  size_t index = 0;

  if (reply == NULL || reply[0] == '\0') {
    return false;
  }

  if (reply[0] == '0' && (reply[1] == 'x' || reply[1] == 'X')) {
    index = 2;
  }

  if (reply[index] == '\0') {
    return false;
  }

  for (; reply[index] != '\0'; ++index) {
    if (!isxdigit((unsigned char)reply[index])) {
      return false;
    }
  }

  return true;
}

lcc_status_t amw0_backend_init(amw0_backend_t *backend, const char *call_node,
                               bool dry_run) {
  int written = 0;

  if (backend == NULL || call_node == NULL || call_node[0] == '\0') {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  written = snprintf(backend->call_node, sizeof(backend->call_node), "%s",
                     call_node);
  if (written < 0 ||
      (size_t)written >= sizeof(backend->call_node)) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  backend->dry_run = dry_run;
  return LCC_OK;
}

lcc_status_t amw0_backend_eval(amw0_backend_t *backend, const char *expr,
                               char *reply, size_t reply_len) {
  FILE *stream = NULL;
  size_t read_bytes = 0;

  if (backend == NULL || expr == NULL || reply == NULL || reply_len < 2u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  if (backend->dry_run) {
    const int written = snprintf(reply, reply_len, "dry-run");
    if (written < 0 || (size_t)written >= reply_len) {
      return LCC_ERR_BUFFER_TOO_SMALL;
    }
    return LCC_OK;
  }

  stream = fopen(backend->call_node, "w");
  if (stream == NULL) {
    return map_errno(errno);
  }

  if (fprintf(stream, "%s\n", expr) < 0) {
    const int saved_errno = errno;
    (void)fclose(stream);
    return map_errno(saved_errno);
  }

  if (fclose(stream) != 0) {
    return map_errno(errno);
  }

  stream = fopen(backend->call_node, "r");
  if (stream == NULL) {
    return map_errno(errno);
  }

  read_bytes = fread(reply, 1u, reply_len - 1u, stream);
  if (ferror(stream) != 0) {
    const int saved_errno = errno;
    (void)fclose(stream);
    return map_errno(saved_errno);
  }

  reply[read_bytes] = '\0';
  trim_reply(reply);

  if (fclose(stream) != 0) {
    return map_errno(errno);
  }

  return LCC_OK;
}

lcc_status_t amw0_backend_send_packet(amw0_backend_t *backend,
                                      const amw0_packet_t *packet, char *reply,
                                      size_t reply_len) {
  char expr[AMW0_EXPR_MAX];
  lcc_status_t status = LCC_OK;

  status = amw0_packet_format_expr(packet, expr, sizeof(expr));
  if (status != LCC_OK) {
    return status;
  }

  return amw0_backend_eval(backend, expr, reply, reply_len);
}

lcc_status_t amw0_backend_read_wqac(amw0_backend_t *backend, uint8_t index,
                                    char *reply, size_t reply_len) {
  char expr[64];
  const int written =
      snprintf(expr, sizeof(expr), "\\_SB.AMW0.WQAC 0x%X", (unsigned int)index);

  if (written < 0 || (size_t)written >= sizeof(expr)) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return amw0_backend_eval(backend, expr, reply, reply_len);
}

lcc_status_t amw0_backend_read_ecrr(amw0_backend_t *backend,
                                    const char *ecrr_path, uint16_t offset,
                                    char *reply, size_t reply_len) {
  char expr[64];
  const int written = snprintf(expr, sizeof(expr), "%s 0x%X", ecrr_path,
                               (unsigned int)offset);

  if (backend == NULL || ecrr_path == NULL || ecrr_path[0] == '\0' ||
      reply == NULL || reply_len < 2u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (written < 0 || (size_t)written >= sizeof(expr)) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return amw0_backend_eval(backend, expr, reply, reply_len);
}

lcc_status_t amw0_backend_probe_ecrr_path(amw0_backend_t *backend,
                                          char *buffer, size_t buffer_len) {
  static const char *const candidates[] = {"\\_SB.INOU.ECRR",
                                           "\\_SB_.INOU.ECRR"};
  char reply[AMW0_REPLY_MAX];
  size_t index = 0;

  if (backend == NULL || buffer == NULL || buffer_len == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  for (index = 0; index < (sizeof(candidates) / sizeof(candidates[0]));
       ++index) {
    const lcc_status_t status = amw0_backend_read_ecrr(
        backend, candidates[index], 0x0460u, reply, sizeof(reply));
    if (status == LCC_OK && amw0_reply_looks_numeric(reply)) {
      const int written =
          snprintf(buffer, buffer_len, "%s", candidates[index]);
      if (written < 0 || (size_t)written >= buffer_len) {
        return LCC_ERR_BUFFER_TOO_SMALL;
      }
      return LCC_OK;
    }
  }

  return LCC_ERR_NOT_FOUND;
}
