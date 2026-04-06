#include "cli.h"

#include <stdio.h>
#include <string.h>

#include "cli/dbus_client.h"
#include "common/lcc_log.h"

static lcc_status_t extract_json_value(const char *json, const char *anchor,
                                       char *buffer, size_t buffer_len) {
  const char *value = NULL;
  const char *cursor = NULL;
  const char *end = NULL;
  int depth = 0;
  size_t len = 0;

  if (json == NULL || anchor == NULL || buffer == NULL || buffer_len == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  value = strstr(json, anchor);
  if (value == NULL) {
    return LCC_ERR_NOT_FOUND;
  }
  value = strchr(value, ':');
  if (value == NULL) {
    return LCC_ERR_PARSE;
  }
  ++value;
  while (*value == ' ' || *value == '\n' || *value == '\t') {
    ++value;
  }

  if (*value == '"') {
    end = strchr(value + 1, '"');
    if (end == NULL) {
      return LCC_ERR_PARSE;
    }
    ++end;
  } else if (*value == '{') {
    depth = 1;
    cursor = value + 1;
    while (*cursor != '\0' && depth > 0) {
      if (*cursor == '{') {
        ++depth;
      } else if (*cursor == '}') {
        --depth;
      }
      ++cursor;
    }
    if (depth != 0) {
      return LCC_ERR_PARSE;
    }
    end = cursor;
  } else {
    end = value;
    while (*end != '\0' && *end != ',' && *end != '}') {
      ++end;
    }
  }

  len = (size_t)(end - value);
  if (len + 1u > buffer_len) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }
  memcpy(buffer, value, len);
  buffer[len] = '\0';
  return LCC_OK;
}

static void trim_quotes(char *text) {
  size_t len = 0;

  if (text == NULL) {
    return;
  }
  len = strlen(text);
  if (len >= 2u && text[0] == '"' && text[len - 1] == '"') {
    memmove(text, text + 1, len - 2u);
    text[len - 2u] = '\0';
  }
}

static int print_observe_mode(const char *payload) {
  char backend[64];
  char requested[64];
  char effective[64];
  char pending[64];
  char transaction[64];
  lcc_status_t status = LCC_OK;

  status = extract_json_value(payload, "\"backend\"", backend, sizeof(backend));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status =
      extract_json_value(payload, "\"requested\":{\"profile\"", requested,
                         sizeof(requested));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status =
      extract_json_value(payload, "\"effective\":{\"profile\"", effective,
                         sizeof(effective));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status =
      extract_json_value(payload, "\"pending\"", pending, sizeof(pending));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status = extract_json_value(payload, "\"state\"", transaction,
                              sizeof(transaction));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  trim_quotes(backend);
  trim_quotes(requested);
  trim_quotes(effective);
  trim_quotes(transaction);
  (void)printf("[mode]\nbackend=%s\nrequested=%s\neffective=%s\npending=%s\ntransaction=%s\n",
               backend, requested, effective, pending, transaction);
  return 0;
}

static int print_observe_power(const char *payload) {
  char requested[128];
  char effective[128];
  char support[128];
  lcc_status_t status = LCC_OK;

  status = extract_json_value(payload, "\"support\"", support, sizeof(support));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status = extract_json_value(payload, "\"power\"", requested, sizeof(requested));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status = extract_json_value(strstr(payload, "\"effective\""), "\"power\"",
                              effective, sizeof(effective));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  (void)printf("[power]\nsupport=%s\nrequested=%s\neffective=%s\n", support,
               requested, effective);
  return 0;
}

static int print_observe_fan(const char *payload) {
  char requested[64];
  char effective[64];
  char thermal[128];
  lcc_status_t status = LCC_OK;

  status =
      extract_json_value(payload, "\"fan_table\"", requested, sizeof(requested));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status = extract_json_value(strstr(payload, "\"effective\""), "\"fan_table\"",
                              effective, sizeof(effective));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status = extract_json_value(payload, "\"thermal\"", thermal, sizeof(thermal));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  trim_quotes(requested);
  trim_quotes(effective);
  (void)printf("[fan]\nrequested_table=%s\neffective_table=%s\nthermal=%s\n",
               requested, effective, thermal);
  return 0;
}

static int print_observe_thermal(const char *payload) {
  char backend[64];
  char thermal[128];
  lcc_status_t status = LCC_OK;

  status = extract_json_value(payload, "\"backend\"", backend, sizeof(backend));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status = extract_json_value(payload, "\"thermal\"", thermal, sizeof(thermal));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  trim_quotes(backend);
  (void)printf("[thermal]\nbackend=%s\nsnapshot=%s\n", backend, thermal);
  return 0;
}

int lcc_cmd_state_status(int argc, char **argv) {
  char payload[4096];
  bool use_user_bus = false;
  int index = 0;
  lcc_status_t status = LCC_OK;

  for (index = 0; index < argc; ++index) {
    if (lcc_cli_parse_bus_flag(argv[index], &use_user_bus)) {
      continue;
    }
    return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  status = lcc_dbus_get_state_json(use_user_bus, payload, sizeof(payload));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  (void)printf("%s\n", payload);
  return 0;
}

int lcc_cmd_state_capabilities(int argc, char **argv) {
  char payload[4096];
  bool use_user_bus = false;
  int index = 0;
  lcc_status_t status = LCC_OK;

  for (index = 0; index < argc; ++index) {
    if (lcc_cli_parse_bus_flag(argv[index], &use_user_bus)) {
      continue;
    }
    return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  status =
      lcc_dbus_get_capabilities_json(use_user_bus, payload, sizeof(payload));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  (void)printf("%s\n", payload);
  return 0;
}

int lcc_cmd_state_observe(int argc, char **argv) {
  char payload[4096];
  const char *group = NULL;
  bool use_user_bus = false;
  lcc_status_t status = LCC_OK;
  int index = 0;

  if (argc < 1) {
    return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  group = argv[0];
  for (index = 1; index < argc; ++index) {
    if (lcc_cli_parse_bus_flag(argv[index], &use_user_bus)) {
      continue;
    }
    if ((strcmp(argv[index], "--call-node") == 0 ||
         strcmp(argv[index], "--ecrr-path") == 0) &&
        index + 1 < argc) {
      lcc_log_warn("observe now uses daemon state; ignoring legacy AMW0 flags");
      ++index;
      continue;
    }
    return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  status = lcc_dbus_get_state_json(use_user_bus, payload, sizeof(payload));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  if (strcmp(group, "mode") == 0) {
    return print_observe_mode(payload);
  }
  if (strcmp(group, "power") == 0) {
    return print_observe_power(payload);
  }
  if (strcmp(group, "fan") == 0) {
    return print_observe_fan(payload);
  }
  if (strcmp(group, "thermal") == 0) {
    return print_observe_thermal(payload);
  }
  if (strcmp(group, "all") == 0) {
    (void)printf("%s\n", payload);
    return 0;
  }

  return lcc_cli_exit_with_status(LCC_ERR_INVALID_ARGUMENT);
}
