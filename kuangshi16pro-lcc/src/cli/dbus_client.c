#include "cli/dbus_client.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <polkit/polkit.h>
#include <polkitagent/polkitagent.h>
#include <systemd/sd-bus.h>

static const char *const lcc_bus_name = "io.github.semcosm.Lcc1";
static const char *const lcc_object_path = "/io/github/semcosm/Lcc1";

typedef struct {
  PolkitAgentListener *listener;
  gpointer registration_handle;
} lcc_cli_polkit_agent_t;

static lcc_status_t dbus_error_to_status(const sd_bus_error *error, int r) {
  if (error != NULL && sd_bus_error_is_set(error) > 0) {
    if (strcmp(error->name, "io.github.semcosm.Lcc1.Error.InvalidArgument") ==
        0) {
      return LCC_ERR_INVALID_ARGUMENT;
    }
    if (strcmp(error->name, "io.github.semcosm.Lcc1.Error.NotFound") == 0) {
      return LCC_ERR_NOT_FOUND;
    }
    if (strcmp(error->name, "io.github.semcosm.Lcc1.Error.PermissionDenied") ==
        0) {
      return LCC_ERR_PERMISSION;
    }
    if (strcmp(error->name, "io.github.semcosm.Lcc1.Error.Parse") == 0) {
      return LCC_ERR_PARSE;
    }
    if (strcmp(error->name, "io.github.semcosm.Lcc1.Error.Range") == 0) {
      return LCC_ERR_RANGE;
    }
    if (strcmp(error->name, "io.github.semcosm.Lcc1.Error.NotSupported") == 0) {
      return LCC_ERR_NOT_SUPPORTED;
    }
    if (strcmp(error->name, "io.github.semcosm.Lcc1.Error.Unimplemented") ==
        0) {
      return LCC_ERR_UNIMPLEMENTED;
    }
    if (strcmp(error->name, "io.github.semcosm.Lcc1.Error.IO") == 0 ||
        strcmp(error->name, "org.freedesktop.DBus.Error.Failed") == 0) {
      return LCC_ERR_IO;
    }
    if (strcmp(error->name, "org.freedesktop.DBus.Error.AccessDenied") == 0) {
      return LCC_ERR_PERMISSION;
    }
    if (strcmp(error->name, "org.freedesktop.DBus.Error.ServiceUnknown") == 0 ||
        strcmp(error->name, "org.freedesktop.DBus.Error.NameHasNoOwner") == 0) {
      return LCC_ERR_NOT_FOUND;
    }
  }

  switch (r) {
    case -ENOENT:
    case -ENXIO:
      return LCC_ERR_NOT_FOUND;
    case -EACCES:
    case -EPERM:
      return LCC_ERR_PERMISSION;
    case -EINVAL:
      return LCC_ERR_INVALID_ARGUMENT;
    default:
      return LCC_ERR_IO;
  }
}

static lcc_status_t open_bus(bool use_user_bus, sd_bus **bus) {
  const int r =
      use_user_bus ? sd_bus_default_user(bus) : sd_bus_default_system(bus);

  if (r < 0) {
    return dbus_error_to_status(NULL, r);
  }

  return LCC_OK;
}

static lcc_status_t lcc_cli_register_polkit_agent(bool use_user_bus,
                                                  lcc_cli_polkit_agent_t *agent) {
  PolkitSubject *subject = NULL;
  GVariantBuilder options_builder;
  GVariant *options = NULL;
  GError *error = NULL;

  if (agent == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  agent->listener = NULL;
  agent->registration_handle = NULL;

  if (use_user_bus) {
    return LCC_OK;
  }
  if (isatty(STDIN_FILENO) == 0) {
    return LCC_OK;
  }

  subject = polkit_unix_session_new_for_process_sync((gint)getpid(), NULL, &error);
  if (subject == NULL) {
    if (error != NULL) {
      g_error_free(error);
    }
    return LCC_OK;
  }

  error = NULL;
  agent->listener = polkit_agent_text_listener_new(NULL, &error);
  if (agent->listener == NULL) {
    if (error != NULL) {
      g_error_free(error);
    }
    g_object_unref(subject);
    return LCC_OK;
  }

  g_variant_builder_init(&options_builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&options_builder, "{sv}", "fallback",
                        g_variant_new_boolean(TRUE));
  options = g_variant_builder_end(&options_builder);

  error = NULL;
  agent->registration_handle = polkit_agent_listener_register_with_options(
      agent->listener, POLKIT_AGENT_REGISTER_FLAGS_RUN_IN_THREAD, subject, NULL,
      options, NULL, &error);
  if (agent->registration_handle == NULL) {
    if (error != NULL) {
      g_error_free(error);
    }
    g_object_unref(agent->listener);
    agent->listener = NULL;
  }

  g_object_unref(subject);
  return LCC_OK;
}

static void lcc_cli_unregister_polkit_agent(lcc_cli_polkit_agent_t *agent) {
  if (agent == NULL) {
    return;
  }

  if (agent->registration_handle != NULL) {
    polkit_agent_listener_unregister(agent->registration_handle);
    agent->registration_handle = NULL;
  }
  if (agent->listener != NULL) {
    g_object_unref(agent->listener);
    agent->listener = NULL;
  }
}

static lcc_status_t call_string_method(bool use_user_bus, const char *interface,
                                       const char *method, char *buffer,
                                       size_t buffer_len) {
  sd_bus *bus = NULL;
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *reply = NULL;
  const char *payload = NULL;
  lcc_status_t status = LCC_OK;
  int r = 0;

  if (interface == NULL || method == NULL || buffer == NULL || buffer_len == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  status = open_bus(use_user_bus, &bus);
  if (status != LCC_OK) {
    return status;
  }

  r = sd_bus_call_method(bus, lcc_bus_name, lcc_object_path, interface, method,
                         &error, &reply, "");
  if (r < 0) {
    status = dbus_error_to_status(&error, r);
    goto out;
  }

  r = sd_bus_message_read(reply, "s", &payload);
  if (r < 0) {
    status = dbus_error_to_status(NULL, r);
    goto out;
  }

  if (strlen(payload) + 1u > buffer_len) {
    status = LCC_ERR_BUFFER_TOO_SMALL;
    goto out;
  }

  (void)snprintf(buffer, buffer_len, "%s", payload);

out:
  sd_bus_message_unref(reply);
  sd_bus_error_free(&error);
  sd_bus_unref(bus);
  return status;
}

static lcc_status_t call_string_setter(bool use_user_bus, const char *interface,
                                       const char *method, const char *value) {
  sd_bus *bus = NULL;
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *reply = NULL;
  lcc_cli_polkit_agent_t agent;
  lcc_status_t status = LCC_OK;
  int r = 0;

  if (interface == NULL || method == NULL || value == NULL || value[0] == '\0') {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  status = open_bus(use_user_bus, &bus);
  if (status != LCC_OK) {
    return status;
  }

  status = lcc_cli_register_polkit_agent(use_user_bus, &agent);
  if (status != LCC_OK) {
    sd_bus_unref(bus);
    return status;
  }

  r = sd_bus_call_method(bus, lcc_bus_name, lcc_object_path, interface, method,
                         &error, &reply, "s", value);
  if (r < 0) {
    status = dbus_error_to_status(&error, r);
  }

  lcc_cli_unregister_polkit_agent(&agent);
  sd_bus_message_unref(reply);
  sd_bus_error_free(&error);
  sd_bus_unref(bus);
  return status;
}

lcc_status_t lcc_dbus_get_capabilities_json(bool use_user_bus, char *buffer,
                                            size_t buffer_len) {
  return call_string_method(use_user_bus, "io.github.semcosm.Lcc1.Manager",
                            "GetCapabilities", buffer, buffer_len);
}

lcc_status_t lcc_dbus_get_state_json(bool use_user_bus, char *buffer,
                                     size_t buffer_len) {
  return call_string_method(use_user_bus, "io.github.semcosm.Lcc1.Manager",
                            "GetState", buffer, buffer_len);
}

lcc_status_t lcc_dbus_set_mode(bool use_user_bus, const char *mode_name) {
  return call_string_setter(use_user_bus, "io.github.semcosm.Lcc1.Manager",
                            "SetMode", mode_name);
}

lcc_status_t lcc_dbus_set_profile(bool use_user_bus, const char *profile_name) {
  return call_string_setter(use_user_bus, "io.github.semcosm.Lcc1.Manager",
                            "SetProfile", profile_name);
}

lcc_status_t lcc_dbus_apply_fan_table(bool use_user_bus, const char *table_name) {
  return call_string_setter(use_user_bus, "io.github.semcosm.Lcc1.Fan",
                            "ApplyFanTable", table_name);
}

lcc_status_t lcc_dbus_set_power_limits(bool use_user_bus,
                                       const lcc_power_limits_t *limits) {
  sd_bus *bus = NULL;
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *reply = NULL;
  lcc_cli_polkit_agent_t agent;
  lcc_status_t status = LCC_OK;
  int r = 0;
  int has_pl1 = 0;
  int has_pl2 = 0;
  int has_pl4 = 0;
  int has_tcc_offset = 0;

  if (limits == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  has_pl1 = limits->pl1.present ? 1 : 0;
  has_pl2 = limits->pl2.present ? 1 : 0;
  has_pl4 = limits->pl4.present ? 1 : 0;
  has_tcc_offset = limits->tcc_offset.present ? 1 : 0;
  if (has_pl1 == 0 && has_pl2 == 0 && has_pl4 == 0 && has_tcc_offset == 0) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  status = open_bus(use_user_bus, &bus);
  if (status != LCC_OK) {
    return status;
  }

  status = lcc_cli_register_polkit_agent(use_user_bus, &agent);
  if (status != LCC_OK) {
    sd_bus_unref(bus);
    return status;
  }

  r = sd_bus_call_method(
      bus, lcc_bus_name, lcc_object_path, "io.github.semcosm.Lcc1.Power",
      "SetPowerLimits", &error, &reply, "yyyybbbb", limits->pl1.value,
      limits->pl2.value, limits->pl4.value, limits->tcc_offset.value, has_pl1,
      has_pl2, has_pl4, has_tcc_offset);
  if (r < 0) {
    status = dbus_error_to_status(&error, r);
  }

  lcc_cli_unregister_polkit_agent(&agent);
  sd_bus_message_unref(reply);
  sd_bus_error_free(&error);
  sd_bus_unref(bus);
  return status;
}
