#include "daemon/errors.h"

#include <errno.h>

int lcc_dbus_error_set(sd_bus_error *ret_error, lcc_status_t status) {
  switch (status) {
    case LCC_ERR_INVALID_ARGUMENT:
      return sd_bus_error_set_const(
          ret_error, "io.github.semcosm.Lcc1.Error.InvalidArgument",
          "invalid argument");
    case LCC_ERR_NOT_FOUND:
      return sd_bus_error_set_const(
          ret_error, "io.github.semcosm.Lcc1.Error.NotFound", "not found");
    case LCC_ERR_PERMISSION:
      return sd_bus_error_set_const(
          ret_error, "io.github.semcosm.Lcc1.Error.PermissionDenied",
          "permission denied");
    case LCC_ERR_PARSE:
      return sd_bus_error_set_const(
          ret_error, "io.github.semcosm.Lcc1.Error.Parse", "parse error");
    case LCC_ERR_RANGE:
      return sd_bus_error_set_const(
          ret_error, "io.github.semcosm.Lcc1.Error.Range", "value out of range");
    case LCC_ERR_NOT_SUPPORTED:
      return sd_bus_error_set_const(
          ret_error, "io.github.semcosm.Lcc1.Error.NotSupported",
          "not supported");
    case LCC_ERR_UNIMPLEMENTED:
      return sd_bus_error_set_const(
          ret_error, "io.github.semcosm.Lcc1.Error.Unimplemented",
          "not implemented");
    case LCC_ERR_IO:
    case LCC_ERR_BUFFER_TOO_SMALL:
      return sd_bus_error_set_const(
          ret_error, "io.github.semcosm.Lcc1.Error.IO", "io error");
    case LCC_OK:
      return 0;
  }

  return -EINVAL;
}
