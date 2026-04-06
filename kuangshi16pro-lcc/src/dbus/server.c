#include "dbus/server.h"

#include <errno.h>
#include <stdio.h>
#include <systemd/sd-bus.h>

#include "common/lcc_log.h"
#include "daemon/errors.h"
#include "dbus/policy.h"

static const char *const lcc_bus_name = "io.github.semcosm.Lcc1";
static const char *const lcc_object_path = "/io/github/semcosm/Lcc1";

typedef struct {
  lcc_manager_t *manager;
  bool use_user_bus;
} lcc_dbus_server_context_t;

static int authorize_method(sd_bus_message *message,
                            const lcc_dbus_server_context_t *context,
                            lcc_dbus_access_t access,
                            const char *action_id,
                            sd_bus_error *ret_error) {
  const lcc_status_t status = lcc_dbus_authorize(
      message, context->use_user_bus, access, action_id);

  if (status != LCC_OK) {
    return lcc_dbus_error_set(ret_error, status);
  }

  return 0;
}

static int method_get_capabilities(sd_bus_message *message, void *userdata,
                                   sd_bus_error *ret_error) {
  const lcc_dbus_server_context_t *context = userdata;
  int r = 0;

  r = authorize_method(message, context, LCC_DBUS_ACCESS_READ, NULL, ret_error);
  if (r < 0) {
    return r;
  }
  return sd_bus_reply_method_return(message, "s",
                                    lcc_manager_capabilities_json(context->manager));
}

static int method_get_state(sd_bus_message *message, void *userdata,
                            sd_bus_error *ret_error) {
  const lcc_dbus_server_context_t *context = userdata;
  char payload[LCC_MANAGER_JSON_MAX];
  int r = 0;
  const lcc_status_t status =
      lcc_manager_get_state_json(context->manager, payload, sizeof(payload));

  r = authorize_method(message, context, LCC_DBUS_ACCESS_READ, NULL, ret_error);
  if (r < 0) {
    return r;
  }

  if (status != LCC_OK) {
    return lcc_dbus_error_set(ret_error, status);
  }

  return sd_bus_reply_method_return(message, "s", payload);
}
static int method_set_profile(sd_bus_message *message, void *userdata,
                              sd_bus_error *ret_error) {
  lcc_dbus_server_context_t *context = userdata;
  const char *profile_name = NULL;
  lcc_status_t status = LCC_OK;
  int r = sd_bus_message_read(message, "s", &profile_name);

  if (r < 0) {
    return r;
  }
  r = authorize_method(message, context, LCC_DBUS_ACCESS_WRITE,
                       "io.github.semcosm.Lcc1.modify", ret_error);
  if (r < 0) {
    return r;
  }

  status = lcc_manager_set_profile(context->manager, profile_name);
  if (status != LCC_OK) {
    return lcc_dbus_error_set(ret_error, status);
  }

  return sd_bus_reply_method_return(message, "");
}

static int method_set_mode(sd_bus_message *message, void *userdata,
                           sd_bus_error *ret_error) {
  lcc_dbus_server_context_t *context = userdata;
  const char *mode_name = NULL;
  lcc_status_t status = LCC_OK;
  int r = sd_bus_message_read(message, "s", &mode_name);

  if (r < 0) {
    return r;
  }
  r = authorize_method(message, context, LCC_DBUS_ACCESS_WRITE,
                       "io.github.semcosm.Lcc1.modify", ret_error);
  if (r < 0) {
    return r;
  }

  status = lcc_manager_set_mode(context->manager, mode_name);
  if (status != LCC_OK) {
    return lcc_dbus_error_set(ret_error, status);
  }

  return sd_bus_reply_method_return(message, "");
}

static int method_apply_fan_table(sd_bus_message *message, void *userdata,
                                  sd_bus_error *ret_error) {
  lcc_dbus_server_context_t *context = userdata;
  const char *table_name = NULL;
  lcc_status_t status = LCC_OK;
  int r = sd_bus_message_read(message, "s", &table_name);

  if (r < 0) {
    return r;
  }
  r = authorize_method(message, context, LCC_DBUS_ACCESS_WRITE,
                       "io.github.semcosm.Lcc1.modify", ret_error);
  if (r < 0) {
    return r;
  }

  status = lcc_manager_apply_fan_table(context->manager, table_name);
  if (status != LCC_OK) {
    return lcc_dbus_error_set(ret_error, status);
  }

  return sd_bus_reply_method_return(message, "");
}

static int method_set_power_limits(sd_bus_message *message, void *userdata,
                                   sd_bus_error *ret_error) {
  lcc_dbus_server_context_t *context = userdata;
  lcc_power_limits_t limits;
  int has_pl1 = 0;
  int has_pl2 = 0;
  int has_pl4 = 0;
  int has_tcc_offset = 0;
  lcc_status_t status = LCC_OK;
  int r = sd_bus_message_read(message, "yyyybbbb", &limits.pl1.value,
                              &limits.pl2.value, &limits.pl4.value,
                              &limits.tcc_offset.value, &has_pl1, &has_pl2,
                              &has_pl4, &has_tcc_offset);

  if (r < 0) {
    return r;
  }
  r = authorize_method(message, context, LCC_DBUS_ACCESS_WRITE,
                       "io.github.semcosm.Lcc1.modify", ret_error);
  if (r < 0) {
    return r;
  }

  limits.pl1.present = has_pl1 != 0;
  limits.pl2.present = has_pl2 != 0;
  limits.pl4.present = has_pl4 != 0;
  limits.tcc_offset.present = has_tcc_offset != 0;

  status = lcc_manager_set_power_limits(context->manager, &limits);
  if (status != LCC_OK) {
    return lcc_dbus_error_set(ret_error, status);
  }

  return sd_bus_reply_method_return(message, "");
}

static const sd_bus_vtable manager_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("GetCapabilities", "", "s", method_get_capabilities,
                  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetState", "", "s", method_get_state,
                  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetMode", "s", "", method_set_mode,
                  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetProfile", "s", "", method_set_profile,
                  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END};

static const sd_bus_vtable fan_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("ApplyFanTable", "s", "", method_apply_fan_table,
                  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END};

static const sd_bus_vtable power_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("SetPowerLimits", "yyyybbbb", "", method_set_power_limits,
                  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END};

int lcc_dbus_server_run(lcc_manager_t *manager, bool use_user_bus) {
  lcc_dbus_server_context_t context;
  sd_bus *bus = NULL;
  int r = 0;

  if (manager == NULL) {
    return -EINVAL;
  }
  context.manager = manager;
  context.use_user_bus = use_user_bus;

  if (use_user_bus) {
    r = sd_bus_default_user(&bus);
  } else {
    r = sd_bus_default_system(&bus);
  }
  if (r < 0) {
    lcc_log_error("failed to connect to D-Bus: %d", r);
    return 1;
  }

  r = sd_bus_add_object_vtable(bus, NULL, lcc_object_path,
                               "io.github.semcosm.Lcc1.Manager",
                               manager_vtable, &context);
  if (r < 0) {
    lcc_log_error("failed to register Manager interface: %d", r);
    sd_bus_unref(bus);
    return 1;
  }

  r = sd_bus_add_object_vtable(bus, NULL, lcc_object_path,
                               "io.github.semcosm.Lcc1.Fan", fan_vtable,
                               &context);
  if (r < 0) {
    lcc_log_error("failed to register Fan interface: %d", r);
    sd_bus_unref(bus);
    return 1;
  }

  r = sd_bus_add_object_vtable(bus, NULL, lcc_object_path,
                               "io.github.semcosm.Lcc1.Power", power_vtable,
                               &context);
  if (r < 0) {
    lcc_log_error("failed to register Power interface: %d", r);
    sd_bus_unref(bus);
    return 1;
  }

  r = sd_bus_request_name(bus, lcc_bus_name, 0);
  if (r < 0) {
    lcc_log_error("failed to acquire bus name %s: %d", lcc_bus_name, r);
    sd_bus_unref(bus);
    return 1;
  }

  for (;;) {
    r = sd_bus_process(bus, NULL);
    if (r < 0) {
      lcc_log_error("D-Bus processing failed: %d", r);
      sd_bus_unref(bus);
      return 1;
    }
    if (r > 0) {
      continue;
    }

    r = sd_bus_wait(bus, (uint64_t)-1);
    if (r < 0) {
      lcc_log_error("D-Bus wait failed: %d", r);
      sd_bus_unref(bus);
      return 1;
    }
  }
}
