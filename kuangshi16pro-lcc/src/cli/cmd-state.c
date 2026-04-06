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

static void set_literal(char *buffer, size_t buffer_len, const char *value) {
  if (buffer != NULL && buffer_len > 0u && value != NULL) {
    (void)snprintf(buffer, buffer_len, "%s", value);
  }
}

static int print_observe_mode(const char *payload) {
  char backend[64];
  char backend_selected[64];
  char fallback_reason[256];
  char execution[256];
  char requested[64];
  char effective[64];
  char pending[64];
  char transaction[64];
  char last_apply_stage[128];
  char last_apply_error[64];
  char last_apply_target[256];
  lcc_status_t status = LCC_OK;

  status = extract_json_value(payload, "\"backend\"", backend, sizeof(backend));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status = extract_json_value(payload, "\"backend_selected\"", backend_selected,
                              sizeof(backend_selected));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status = extract_json_value(payload, "\"backend_fallback_reason\"",
                              fallback_reason, sizeof(fallback_reason));
  if (status != LCC_OK) {
    set_literal(fallback_reason, sizeof(fallback_reason), "null");
  }
  status =
      extract_json_value(payload, "\"execution\"", execution, sizeof(execution));
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
  status = extract_json_value(payload, "\"last_apply_stage\"", last_apply_stage,
                              sizeof(last_apply_stage));
  if (status != LCC_OK) {
    set_literal(last_apply_stage, sizeof(last_apply_stage), "null");
  }
  status = extract_json_value(payload, "\"last_apply_error\"", last_apply_error,
                              sizeof(last_apply_error));
  if (status != LCC_OK) {
    set_literal(last_apply_error, sizeof(last_apply_error), "null");
  }
  status = extract_json_value(payload, "\"last_apply_target\"",
                              last_apply_target, sizeof(last_apply_target));
  if (status != LCC_OK) {
    set_literal(last_apply_target, sizeof(last_apply_target), "null");
  }

  trim_quotes(backend);
  trim_quotes(backend_selected);
  trim_quotes(fallback_reason);
  trim_quotes(requested);
  trim_quotes(effective);
  trim_quotes(transaction);
  trim_quotes(last_apply_stage);
  trim_quotes(last_apply_error);
  (void)printf(
      "[mode]\nbackend=%s\nbackend_selected=%s\nfallback_reason=%s\nexecution=%s\nrequested=%s\neffective=%s\npending=%s\ntransaction=%s\nlast_apply_stage=%s\nlast_apply_error=%s\nlast_apply_target=%s\n",
      backend, backend_selected, fallback_reason, execution, requested,
      effective, pending, transaction, last_apply_stage, last_apply_error,
      last_apply_target);
  return 0;
}

static int print_observe_power(const char *payload) {
  char backend_selected[64];
  char fallback_reason[256];
  char execution[256];
  char requested[128];
  char effective[128];
  char support[128];
  char last_apply_stage[128];
  char last_apply_error[64];
  char last_apply_target[256];
  lcc_status_t status = LCC_OK;

  status = extract_json_value(payload, "\"backend_selected\"", backend_selected,
                              sizeof(backend_selected));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status = extract_json_value(payload, "\"backend_fallback_reason\"",
                              fallback_reason, sizeof(fallback_reason));
  if (status != LCC_OK) {
    set_literal(fallback_reason, sizeof(fallback_reason), "null");
  }
  status =
      extract_json_value(payload, "\"execution\"", execution, sizeof(execution));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
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
  status = extract_json_value(payload, "\"last_apply_stage\"", last_apply_stage,
                              sizeof(last_apply_stage));
  if (status != LCC_OK) {
    set_literal(last_apply_stage, sizeof(last_apply_stage), "null");
  }
  status = extract_json_value(payload, "\"last_apply_error\"", last_apply_error,
                              sizeof(last_apply_error));
  if (status != LCC_OK) {
    set_literal(last_apply_error, sizeof(last_apply_error), "null");
  }
  status = extract_json_value(payload, "\"last_apply_target\"",
                              last_apply_target, sizeof(last_apply_target));
  if (status != LCC_OK) {
    set_literal(last_apply_target, sizeof(last_apply_target), "null");
  }

  trim_quotes(backend_selected);
  trim_quotes(fallback_reason);
  trim_quotes(last_apply_stage);
  trim_quotes(last_apply_error);
  (void)printf(
      "[power]\nbackend_selected=%s\nfallback_reason=%s\nexecution=%s\nsupport=%s\nrequested=%s\neffective=%s\nlast_apply_stage=%s\nlast_apply_error=%s\nlast_apply_target=%s\n",
      backend_selected, fallback_reason, execution, support, requested,
      effective, last_apply_stage, last_apply_error, last_apply_target);
  return 0;
}

static int print_observe_fan(const char *payload) {
  char backend_selected[64];
  char fallback_reason[256];
  char execution[256];
  char requested[64];
  char effective[64];
  char thermal[128];
  char last_apply_stage[128];
  char last_apply_error[64];
  char last_apply_target[256];
  lcc_status_t status = LCC_OK;

  status = extract_json_value(payload, "\"backend_selected\"", backend_selected,
                              sizeof(backend_selected));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status = extract_json_value(payload, "\"backend_fallback_reason\"",
                              fallback_reason, sizeof(fallback_reason));
  if (status != LCC_OK) {
    set_literal(fallback_reason, sizeof(fallback_reason), "null");
  }
  status =
      extract_json_value(payload, "\"execution\"", execution, sizeof(execution));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
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
  status = extract_json_value(payload, "\"last_apply_stage\"", last_apply_stage,
                              sizeof(last_apply_stage));
  if (status != LCC_OK) {
    set_literal(last_apply_stage, sizeof(last_apply_stage), "null");
  }
  status = extract_json_value(payload, "\"last_apply_error\"", last_apply_error,
                              sizeof(last_apply_error));
  if (status != LCC_OK) {
    set_literal(last_apply_error, sizeof(last_apply_error), "null");
  }
  status = extract_json_value(payload, "\"last_apply_target\"",
                              last_apply_target, sizeof(last_apply_target));
  if (status != LCC_OK) {
    set_literal(last_apply_target, sizeof(last_apply_target), "null");
  }

  trim_quotes(backend_selected);
  trim_quotes(fallback_reason);
  trim_quotes(requested);
  trim_quotes(effective);
  trim_quotes(last_apply_stage);
  trim_quotes(last_apply_error);
  (void)printf(
      "[fan]\nbackend_selected=%s\nfallback_reason=%s\nexecution=%s\nrequested_table=%s\neffective_table=%s\nthermal=%s\nlast_apply_stage=%s\nlast_apply_error=%s\nlast_apply_target=%s\n",
      backend_selected, fallback_reason, execution, requested, effective,
      thermal, last_apply_stage, last_apply_error, last_apply_target);
  return 0;
}

static int print_observe_thermal(const char *payload) {
  char backend[64];
  char backend_selected[64];
  char fallback_reason[256];
  char execution[256];
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
  status = extract_json_value(payload, "\"backend_selected\"", backend_selected,
                              sizeof(backend_selected));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }
  status = extract_json_value(payload, "\"backend_fallback_reason\"",
                              fallback_reason, sizeof(fallback_reason));
  if (status != LCC_OK) {
    set_literal(fallback_reason, sizeof(fallback_reason), "null");
  }
  status =
      extract_json_value(payload, "\"execution\"", execution, sizeof(execution));
  if (status != LCC_OK) {
    return lcc_cli_exit_with_status(status);
  }

  trim_quotes(backend);
  trim_quotes(backend_selected);
  trim_quotes(fallback_reason);
  (void)printf(
      "[thermal]\nbackend=%s\nbackend_selected=%s\nfallback_reason=%s\nexecution=%s\nsnapshot=%s\n",
      backend, backend_selected, fallback_reason, execution, thermal);
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
