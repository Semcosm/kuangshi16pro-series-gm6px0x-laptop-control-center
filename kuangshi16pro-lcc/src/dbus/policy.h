#ifndef LCC_DBUS_POLICY_H
#define LCC_DBUS_POLICY_H

#include <stdbool.h>
#include <systemd/sd-bus.h>

#include "lcc/error.h"

typedef enum {
  LCC_DBUS_ACCESS_READ = 0,
  LCC_DBUS_ACCESS_WRITE
} lcc_dbus_access_t;

#define LCC_DBUS_ACTION_SET_MODE "io.github.semcosm.Lcc1.set-mode"
#define LCC_DBUS_ACTION_SET_PROFILE "io.github.semcosm.Lcc1.set-profile"
#define LCC_DBUS_ACTION_SET_FAN_TABLE "io.github.semcosm.Lcc1.set-fan-table"
#define LCC_DBUS_ACTION_SET_FAN_BOOST "io.github.semcosm.Lcc1.set-fan-boost"
#define LCC_DBUS_ACTION_SET_POWER_LIMITS "io.github.semcosm.Lcc1.set-power-limits"

lcc_status_t lcc_dbus_authorize(sd_bus_message *message, bool use_user_bus,
                                lcc_dbus_access_t access,
                                const char *action_id);

#endif
