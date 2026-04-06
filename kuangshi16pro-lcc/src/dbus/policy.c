#include "dbus/policy.h"

static lcc_status_t authorize_write(sd_bus_message *message, bool use_user_bus,
                                    const char *action_id) {
  int privileged = 0;

  (void)action_id;

  if (message == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }
  if (use_user_bus) {
    return LCC_OK;
  }

  privileged = sd_bus_query_sender_privilege(message, -1);
  if (privileged < 0) {
    return LCC_ERR_IO;
  }
  if (privileged == 0) {
    return LCC_ERR_PERMISSION;
  }

  return LCC_OK;
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
