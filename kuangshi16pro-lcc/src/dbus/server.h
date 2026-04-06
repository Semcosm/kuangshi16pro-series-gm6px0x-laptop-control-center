#ifndef LCC_DBUS_SERVER_H
#define LCC_DBUS_SERVER_H

#include <stdbool.h>

#include "daemon/manager.h"

int lcc_dbus_server_run(lcc_manager_t *manager, bool use_user_bus);

#endif
