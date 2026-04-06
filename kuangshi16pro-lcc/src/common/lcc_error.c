#include "lcc/error.h"

const char *lcc_status_string(lcc_status_t status) {
  switch (status) {
    case LCC_OK:
      return "ok";
    case LCC_ERR_INVALID_ARGUMENT:
      return "invalid argument";
    case LCC_ERR_IO:
      return "io error";
    case LCC_ERR_NOT_FOUND:
      return "not found";
    case LCC_ERR_PERMISSION:
      return "permission denied";
    case LCC_ERR_PARSE:
      return "parse error";
    case LCC_ERR_RANGE:
      return "value out of range";
    case LCC_ERR_NOT_SUPPORTED:
      return "not supported";
    case LCC_ERR_UNIMPLEMENTED:
      return "not implemented";
    case LCC_ERR_BUFFER_TOO_SMALL:
      return "buffer too small";
  }

  return "unknown error";
}
