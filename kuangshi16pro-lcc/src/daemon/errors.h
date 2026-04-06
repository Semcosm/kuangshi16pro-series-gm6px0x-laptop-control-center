#ifndef LCC_DAEMON_ERRORS_H
#define LCC_DAEMON_ERRORS_H

#include <systemd/sd-bus.h>

#include "lcc/error.h"

int lcc_dbus_error_set(sd_bus_error *ret_error, lcc_status_t status);

#endif
