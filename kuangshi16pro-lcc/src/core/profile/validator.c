#include "lcc/profile.h"

lcc_status_t lcc_profile_document_validate(
    const lcc_profile_document_t *document) {
  if (document == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  if (document->has_fan_table) {
    return lcc_validate_fan_table(&document->fan_table);
  }

  if (!document->has_mode && !document->has_power_limits &&
      !document->has_fan_table) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  return LCC_OK;
}
