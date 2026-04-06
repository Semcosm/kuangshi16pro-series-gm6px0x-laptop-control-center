#ifndef LCC_ERROR_H
#define LCC_ERROR_H

typedef enum {
  LCC_OK = 0,
  LCC_ERR_INVALID_ARGUMENT,
  LCC_ERR_IO,
  LCC_ERR_NOT_FOUND,
  LCC_ERR_PERMISSION,
  LCC_ERR_PARSE,
  LCC_ERR_RANGE,
  LCC_ERR_UNIMPLEMENTED,
  LCC_ERR_BUFFER_TOO_SMALL
} lcc_status_t;

const char *lcc_status_string(lcc_status_t status);

#endif
