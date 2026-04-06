#include "backends/amw0/ecmg.h"

#include <stdio.h>
#include <stdlib.h>

#include "backends/amw0/ec-addr-map.h"

static lcc_status_t parse_u32_reply(const char *text, uint32_t *value) {
  char *end = NULL;
  unsigned long parsed = 0;

  if (text == NULL || value == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  parsed = strtoul(text, &end, 0);
  if (end == text || *end != '\0') {
    return LCC_ERR_PARSE;
  }

  *value = (uint32_t)parsed;
  return LCC_OK;
}

static lcc_status_t read_ecrr_value(amw0_backend_t *backend,
                                    const char *ecrr_path, uint16_t offset,
                                    uint32_t *value) {
  char reply[AMW0_REPLY_MAX];
  lcc_status_t status = LCC_OK;

  if (backend == NULL || ecrr_path == NULL || value == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  status =
      amw0_backend_read_ecrr(backend, ecrr_path, offset, reply, sizeof(reply));
  if (status != LCC_OK) {
    return status;
  }

  return parse_u32_reply(reply, value);
}

lcc_status_t lcc_amw0_print_mode_decode(amw0_backend_t *backend,
                                        const char *ecrr_path) {
  uint32_t mafan_ctl = 0;
  uint32_t mode_index = 0;
  uint32_t helper = 0;
  uint32_t profile1 = 0;
  uint32_t profile2 = 0;
  uint32_t profile3 = 0;
  lcc_status_t status = LCC_OK;
  const unsigned int turbo_bit = 0x10u;
  const unsigned int himode_bit = 0x20u;
  const unsigned int fanboost_bit = 0x40u;
  const unsigned int custom_bit = 0x80u;
  unsigned int ocpl = 0u;
  unsigned int lcse = 0u;

  status = read_ecrr_value(backend, ecrr_path, LCC_AMW0_ADDR_MODE_CONTROL,
                           &mafan_ctl);
  if (status != LCC_OK) {
    return status;
  }
  status =
      read_ecrr_value(backend, ecrr_path, LCC_AMW0_ADDR_MODE_INDEX, &mode_index);
  if (status != LCC_OK) {
    return status;
  }
  status = read_ecrr_value(backend, ecrr_path, LCC_AMW0_ADDR_MODE_PROFILE1,
                           &profile1);
  if (status != LCC_OK) {
    return status;
  }
  status = read_ecrr_value(backend, ecrr_path, LCC_AMW0_ADDR_MODE_PROFILE2,
                           &profile2);
  if (status != LCC_OK) {
    return status;
  }
  status = read_ecrr_value(backend, ecrr_path, LCC_AMW0_ADDR_MODE_PROFILE3,
                           &profile3);
  if (status != LCC_OK) {
    return status;
  }
  status =
      read_ecrr_value(backend, ecrr_path, LCC_AMW0_ADDR_MODE_HELPER, &helper);
  if (status != LCC_OK) {
    return status;
  }

  lcse = (unsigned int)((helper >> 1) & 0x1u);
  ocpl = (unsigned int)((helper >> 2) & 0x7u);

  (void)printf("[mode-decode]\n");
  (void)printf("MAFAN_CTL raw=0x%02X TBME=%u HIMODE=%u FANBOOST=%u UFME=%u\n",
               (unsigned int)(mafan_ctl & 0xFFu),
               ((mafan_ctl & turbo_bit) != 0u) ? 1u : 0u,
               ((mafan_ctl & himode_bit) != 0u) ? 1u : 0u,
               ((mafan_ctl & fanboost_bit) != 0u) ? 1u : 0u,
               ((mafan_ctl & custom_bit) != 0u) ? 1u : 0u);
  if ((mafan_ctl & custom_bit) != 0u && (mafan_ctl & himode_bit) != 0u) {
    (void)printf("candidate_state=user-fan-hi-mode-like\n");
  } else if ((mafan_ctl & custom_bit) != 0u) {
    (void)printf("candidate_state=custom-like\n");
  } else if ((mafan_ctl & turbo_bit) != 0u &&
             (mafan_ctl & himode_bit) != 0u) {
    (void)printf("candidate_state=turbo-hi-mode-like\n");
  } else if ((mafan_ctl & turbo_bit) != 0u) {
    (void)printf("candidate_state=turbo-like\n");
  } else if ((mafan_ctl & himode_bit) != 0u) {
    (void)printf("candidate_state=hi-mode-like\n");
  } else {
    (void)printf("candidate_state=non-turbo/non-custom\n");
  }
  (void)printf("MYFANCCI_IDX raw=0x%02X\n", (unsigned int)(mode_index & 0xFFu));
  (void)printf("PROFILE slots raw=0x%02X 0x%02X 0x%02X\n",
               (unsigned int)(profile1 & 0xFFu),
               (unsigned int)(profile2 & 0xFFu),
               (unsigned int)(profile3 & 0xFFu));
  (void)printf("MODE_HELPER raw=0x%02X LCSE=%u OCPL=%u\n",
               (unsigned int)(helper & 0xFFu), lcse, ocpl);
  (void)printf(
      "note=0x07AB does not mirror the turbo/custom bits directly on this machine\n");
  (void)printf(
      "note=0x20 is likely the missing HiMode/helper bit because User_Fan_HiMode = 0xA0\n");
  return LCC_OK;
}

lcc_status_t lcc_amw0_print_thermal_decode(amw0_backend_t *backend,
                                           const char *ecrr_path) {
  uint32_t ffan = 0;
  lcc_status_t status = LCC_OK;

  status = read_ecrr_value(backend, ecrr_path, LCC_AMW0_ADDR_FFAN, &ffan);
  if (status != LCC_OK) {
    return status;
  }

  (void)printf("[thermal-decode]\n");
  (void)printf("FFAN raw=0x%02X low_nibble=%u\n", (unsigned int)(ffan & 0xFFu),
               (unsigned int)(ffan & 0x0Fu));
  return LCC_OK;
}
