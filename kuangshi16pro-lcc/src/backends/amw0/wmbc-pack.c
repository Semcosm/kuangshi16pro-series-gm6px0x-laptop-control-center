#include "backends/amw0/wmbc-pack.h"

#include <stdio.h>

lcc_status_t amw0_packet_format_expr(const amw0_packet_t *packet, char *buffer,
                                     size_t buffer_len) {
  const unsigned int sac1_0 = (unsigned int)(packet->sac1 & 0xffu);
  const unsigned int sac1_1 =
      (unsigned int)((packet->sac1 >> 8) & 0xffu);
  int written = 0;

  if (packet == NULL || buffer == NULL || buffer_len == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  written = snprintf(
      buffer, buffer_len,
      "\\_SB.AMW0.WMBC 0x%X 0x4 "
      "{0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x00,0x00,"
      "0x02,0x02,0x02,0x02,0x03,0x03,0x03,0x03,0x04,0x04,0x04,0x04,"
      "0x05,0x05,0x05,0x05,0x06,0x06,0x06,0x06,0x07,0x07,0x07,0x07,"
      "0x08,0x08,0x08,0x08,0x09,0x09,0x09,0x09}",
      (unsigned int)packet->slot, (unsigned int)packet->sa[0],
      (unsigned int)packet->sa[1], (unsigned int)packet->sa[2],
      (unsigned int)packet->sa[3], sac1_0, sac1_1);
  if (written < 0 || (size_t)written >= buffer_len) {
    return LCC_ERR_BUFFER_TOO_SMALL;
  }

  return LCC_OK;
}
