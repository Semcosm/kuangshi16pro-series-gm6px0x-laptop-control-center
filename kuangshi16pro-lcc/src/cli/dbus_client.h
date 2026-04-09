#ifndef LCC_CLI_DBUS_CLIENT_H
#define LCC_CLI_DBUS_CLIENT_H

#include <stdbool.h>
#include <stddef.h>

#include "lcc/error.h"
#include "lcc/power.h"

lcc_status_t lcc_dbus_get_capabilities_json(bool use_user_bus, char *buffer,
                                            size_t buffer_len);
lcc_status_t lcc_dbus_get_state_json(bool use_user_bus, char *buffer,
                                     size_t buffer_len);
lcc_status_t lcc_dbus_set_mode(bool use_user_bus, const char *mode_name);
lcc_status_t lcc_dbus_set_profile(bool use_user_bus, const char *profile_name);
lcc_status_t lcc_dbus_apply_fan_table(bool use_user_bus, const char *table_name);
lcc_status_t lcc_dbus_set_fan_boost(bool use_user_bus, bool enabled);
lcc_status_t lcc_dbus_set_power_limits(bool use_user_bus,
                                       const lcc_power_limits_t *limits);

#endif
