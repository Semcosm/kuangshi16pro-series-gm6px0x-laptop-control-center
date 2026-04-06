#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backend-amw0/amw0_backend.h"
#include "common/lcc_error.h"
#include "common/lcc_log.h"
#include "profile-service/profile_service.h"

static const char *default_call_node(void) { return "/proc/acpi/call"; }

typedef struct {
  const char *name;
  uint16_t offset;
} observe_item_t;

static const observe_item_t observe_mode_items[] = {
    {"MAFAN_CTL", 0x0751u},   {"MYFANCCI_IDX", 0x07ABu},
    {"PROFILE1", 0x07B0u},    {"PROFILE2", 0x07B1u},
    {"PROFILE3", 0x07B2u},    {"MODE_HELPER", 0x07C7u},
};

static const observe_item_t observe_power_items[] = {
    {"PL1", 0x0783u},          {"PL2", 0x0784u},
    {"PL4", 0x0785u},          {"TCC_OFFSET", 0x0786u},
    {"FAN_SWITCH_SPEED", 0x0787u},
};

static const observe_item_t observe_fan_items[] = {
    {"CPU_UP0", 0x0F00u},      {"CPU_DOWN0", 0x0F10u},
    {"CPU_DUTY0", 0x0F20u},    {"GPU_UP0", 0x0F30u},
    {"GPU_DOWN0", 0x0F40u},    {"GPU_DUTY0", 0x0F50u},
    {"TABLE_STATUS1", 0x0F5Du},{"TABLE_STATUS2", 0x0F5Eu},
    {"TABLE_CTRL", 0x0F5Fu},
};

static const observe_item_t observe_thermal_items[] = {
    {"FFAN", 0x0460u}, {"CPUT", 0x0E0Du}, {"PCHT", 0x0E0Eu},
    {"F1SH", 0x0E1Cu}, {"F1SL", 0x0E1Du}, {"F1DC", 0x0E8Cu},
    {"F2DC", 0x0E9Du},
};

static void print_usage(FILE *stream) {
  (void)fprintf(stream,
                "Usage:\n"
                "  lccctl status [--call-node PATH]\n"
                "  lccctl observe GROUP [--call-node PATH] [--ecrr-path PATH]\n"
                "  lccctl raw wmbc SLOT SAC1 SA00 SA01 SA02 SA03 "
                "[--call-node PATH] [--dry-run]\n"
                "  lccctl mode set MODE [--execute]\n"
                "  lccctl power set --pl1 N [--pl2 N] [--pl4 N] "
                "[--tcc-offset N] [--execute]\n"
                "  lccctl power set --file PATH [--execute]\n"
                "  lccctl fan apply [--preset demo | --file PATH] [--execute]\n"
                "  lccctl profile apply --file PATH [--execute]\n"
                "\n"
                "Profile file format:\n"
                "  [mode]  value=gaming|office|turbo|custom\n"
                "  [power] pl1=55 pl2=95 pl4=125 tcc_offset=10\n"
                "  [fan]   name=demo cpu.0=40,37,18 gpu.0=45,42,20\n"
                "\n"
                "Notes:\n"
                "  mode/power/fan/profile currently print a staged EC plan.\n"
                "  observe groups: mode, power, fan, thermal, all.\n"
                "  raw wmbc, status, and observe use the AMW0 backend directly.\n");
}

static int exit_with_status(lcc_status_t status) {
  if (status != LCC_OK) {
    lcc_log_error("%s", lcc_status_string(status));
  }
  return status == LCC_OK ? 0 : 1;
}

static lcc_status_t parse_u8(const char *text, uint8_t *value) {
  char *end = NULL;
  unsigned long parsed = 0;

  if (text == NULL || value == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  parsed = strtoul(text, &end, 0);
  if (end == text || *end != '\0') {
    return LCC_ERR_PARSE;
  }
  if (parsed > 0xfful) {
    return LCC_ERR_RANGE;
  }

  *value = (uint8_t)parsed;
  return LCC_OK;
}

static lcc_status_t parse_u16(const char *text, uint16_t *value) {
  char *end = NULL;
  unsigned long parsed = 0;

  if (text == NULL || value == NULL) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  parsed = strtoul(text, &end, 0);
  if (end == text || *end != '\0') {
    return LCC_ERR_PARSE;
  }
  if (parsed > 0xfffful) {
    return LCC_ERR_RANGE;
  }

  *value = (uint16_t)parsed;
  return LCC_OK;
}

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

static lcc_status_t init_backend(amw0_backend_t *backend, const char *call_node,
                                 bool dry_run) {
  return amw0_backend_init(backend, call_node != NULL ? call_node
                                                      : default_call_node(),
                           dry_run);
}

static lcc_status_t print_transport_snapshot(amw0_backend_t *backend) {
  char wqac0[AMW0_REPLY_MAX];
  char wqac1[AMW0_REPLY_MAX];
  char wed_d2[AMW0_REPLY_MAX];
  lcc_status_t status = LCC_OK;

  status = amw0_backend_read_wqac(backend, 0u, wqac0, sizeof(wqac0));
  if (status != LCC_OK) {
    return status;
  }
  status = amw0_backend_read_wqac(backend, 1u, wqac1, sizeof(wqac1));
  if (status != LCC_OK) {
    return status;
  }
  status =
      amw0_backend_eval(backend, "\\_SB.AMW0._WED 0xD2", wed_d2, sizeof(wed_d2));
  if (status != LCC_OK) {
    return status;
  }

  (void)printf("WQAC0=%s\nWQAC1=%s\nWED_D2=%s\n", wqac0, wqac1, wed_d2);
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

static lcc_status_t print_observe_group(amw0_backend_t *backend,
                                        const char *ecrr_path,
                                        const char *group_name,
                                        const observe_item_t *items,
                                        size_t count) {
  char reply[AMW0_REPLY_MAX];
  size_t index = 0;

  if (backend == NULL || ecrr_path == NULL || group_name == NULL ||
      items == NULL || count == 0u) {
    return LCC_ERR_INVALID_ARGUMENT;
  }

  (void)printf("[%s]\n", group_name);
  for (index = 0; index < count; ++index) {
    const lcc_status_t status = amw0_backend_read_ecrr(
        backend, ecrr_path, items[index].offset, reply, sizeof(reply));
    if (status != LCC_OK) {
      return status;
    }
    (void)printf("%-16s addr=0x%04X value=%s\n", items[index].name,
                 (unsigned int)items[index].offset, reply);
  }

  return LCC_OK;
}

static lcc_status_t print_mode_decode(amw0_backend_t *backend,
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

  status = read_ecrr_value(backend, ecrr_path, 0x0751u, &mafan_ctl);
  if (status != LCC_OK) {
    return status;
  }
  status = read_ecrr_value(backend, ecrr_path, 0x07ABu, &mode_index);
  if (status != LCC_OK) {
    return status;
  }
  status = read_ecrr_value(backend, ecrr_path, 0x07B0u, &profile1);
  if (status != LCC_OK) {
    return status;
  }
  status = read_ecrr_value(backend, ecrr_path, 0x07B1u, &profile2);
  if (status != LCC_OK) {
    return status;
  }
  status = read_ecrr_value(backend, ecrr_path, 0x07B2u, &profile3);
  if (status != LCC_OK) {
    return status;
  }
  status = read_ecrr_value(backend, ecrr_path, 0x07C7u, &helper);
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
               (unsigned int)(profile1 & 0xFFu), (unsigned int)(profile2 & 0xFFu),
               (unsigned int)(profile3 & 0xFFu));
  (void)printf("MODE_HELPER raw=0x%02X LCSE=%u OCPL=%u\n",
               (unsigned int)(helper & 0xFFu), lcse, ocpl);
  (void)printf(
      "note=0x07AB does not mirror the turbo/custom bits directly on this machine\n");
  (void)printf(
      "note=0x20 is likely the missing HiMode/helper bit because User_Fan_HiMode = 0xA0\n");
  return LCC_OK;
}

static lcc_status_t print_thermal_decode(amw0_backend_t *backend,
                                         const char *ecrr_path) {
  uint32_t ffan = 0;
  lcc_status_t status = LCC_OK;

  status = read_ecrr_value(backend, ecrr_path, 0x0460u, &ffan);
  if (status != LCC_OK) {
    return status;
  }

  (void)printf("[thermal-decode]\n");
  (void)printf("FFAN raw=0x%02X low_nibble=%u\n", (unsigned int)(ffan & 0xFFu),
               (unsigned int)(ffan & 0x0Fu));
  return LCC_OK;
}

static int print_plan_or_unimplemented(const lcc_apply_plan_t *plan,
                                       bool execute) {
  lcc_apply_plan_print(stdout, plan);

  if (execute) {
    lcc_log_warn("staged execution is not wired to final AMW0 write packets yet");
    return exit_with_status(LCC_ERR_UNIMPLEMENTED);
  }

  return 0;
}

static int cmd_status(int argc, char **argv) {
  amw0_backend_t backend;
  char ecrr_path[32];
  char ecrr_460[AMW0_REPLY_MAX];
  const char *call_node = default_call_node();
  int index = 0;
  lcc_status_t status = LCC_OK;

  for (index = 0; index < argc; ++index) {
    if (strcmp(argv[index], "--call-node") == 0 && index + 1 < argc) {
      call_node = argv[++index];
      continue;
    }
    return exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  status = init_backend(&backend, call_node, false);
  if (status != LCC_OK) {
    return exit_with_status(status);
  }

  status = print_transport_snapshot(&backend);
  if (status != LCC_OK) {
    return exit_with_status(status);
  }

  status =
      amw0_backend_probe_ecrr_path(&backend, ecrr_path, sizeof(ecrr_path));
  if (status == LCC_OK) {
    status = amw0_backend_read_ecrr(&backend, ecrr_path, 0x0460u, ecrr_460,
                                    sizeof(ecrr_460));
    if (status != LCC_OK) {
      return exit_with_status(status);
    }
    (void)printf("ECRR_PATH=%s\nECRR_0460=%s\n", ecrr_path, ecrr_460);
    return 0;
  }

  lcc_log_warn("ECRR path probe not available on this setup");
  return 0;
}

static int cmd_observe(int argc, char **argv) {
  amw0_backend_t backend;
  char ecrr_path[32];
  const char *group = NULL;
  const char *call_node = default_call_node();
  const char *override_ecrr_path = NULL;
  lcc_status_t status = LCC_OK;
  int index = 0;

  if (argc < 1) {
    return exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  group = argv[0];
  for (index = 1; index < argc; ++index) {
    if (strcmp(argv[index], "--call-node") == 0 && index + 1 < argc) {
      call_node = argv[++index];
      continue;
    }
    if (strcmp(argv[index], "--ecrr-path") == 0 && index + 1 < argc) {
      override_ecrr_path = argv[++index];
      continue;
    }
    return exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  status = init_backend(&backend, call_node, false);
  if (status != LCC_OK) {
    return exit_with_status(status);
  }

  status = print_transport_snapshot(&backend);
  if (status != LCC_OK) {
    return exit_with_status(status);
  }

  if (override_ecrr_path != NULL) {
    const int written = snprintf(ecrr_path, sizeof(ecrr_path), "%s",
                                 override_ecrr_path);
    if (written < 0 || (size_t)written >= sizeof(ecrr_path)) {
      return exit_with_status(LCC_ERR_BUFFER_TOO_SMALL);
    }
  } else {
    status =
        amw0_backend_probe_ecrr_path(&backend, ecrr_path, sizeof(ecrr_path));
    if (status != LCC_OK) {
      return exit_with_status(status);
    }
  }

  (void)printf("ECRR_PATH=%s\n", ecrr_path);

  if (strcmp(group, "mode") == 0) {
    status = print_observe_group(
        &backend, ecrr_path, "mode", observe_mode_items,
        sizeof(observe_mode_items) / sizeof(observe_mode_items[0]));
    if (status != LCC_OK) {
      return exit_with_status(status);
    }
    return exit_with_status(print_mode_decode(&backend, ecrr_path));
  }
  if (strcmp(group, "power") == 0) {
    return exit_with_status(print_observe_group(
        &backend, ecrr_path, "power", observe_power_items,
        sizeof(observe_power_items) / sizeof(observe_power_items[0])));
  }
  if (strcmp(group, "fan") == 0) {
    return exit_with_status(print_observe_group(
        &backend, ecrr_path, "fan", observe_fan_items,
        sizeof(observe_fan_items) / sizeof(observe_fan_items[0])));
  }
  if (strcmp(group, "thermal") == 0) {
    status = print_observe_group(
        &backend, ecrr_path, "thermal", observe_thermal_items,
        sizeof(observe_thermal_items) / sizeof(observe_thermal_items[0]));
    if (status != LCC_OK) {
      return exit_with_status(status);
    }
    return exit_with_status(print_thermal_decode(&backend, ecrr_path));
  }
  if (strcmp(group, "all") == 0) {
    status = print_observe_group(&backend, ecrr_path, "mode", observe_mode_items,
                                 sizeof(observe_mode_items) /
                                     sizeof(observe_mode_items[0]));
    if (status != LCC_OK) {
      return exit_with_status(status);
    }
    status = print_mode_decode(&backend, ecrr_path);
    if (status != LCC_OK) {
      return exit_with_status(status);
    }
    status = print_observe_group(
        &backend, ecrr_path, "power", observe_power_items,
        sizeof(observe_power_items) / sizeof(observe_power_items[0]));
    if (status != LCC_OK) {
      return exit_with_status(status);
    }
    status = print_observe_group(&backend, ecrr_path, "fan", observe_fan_items,
                                 sizeof(observe_fan_items) /
                                     sizeof(observe_fan_items[0]));
    if (status != LCC_OK) {
      return exit_with_status(status);
    }
    status = print_observe_group(
        &backend, ecrr_path, "thermal", observe_thermal_items,
        sizeof(observe_thermal_items) / sizeof(observe_thermal_items[0]));
    if (status != LCC_OK) {
      return exit_with_status(status);
    }
    return exit_with_status(print_thermal_decode(&backend, ecrr_path));
  }

  return exit_with_status(LCC_ERR_INVALID_ARGUMENT);
}

static int cmd_raw_wmbc(int argc, char **argv) {
  amw0_backend_t backend;
  amw0_packet_t packet;
  char expr[AMW0_EXPR_MAX];
  char reply[AMW0_REPLY_MAX];
  const char *call_node = default_call_node();
  bool dry_run = false;
  int index = 0;
  lcc_status_t status = LCC_OK;

  if (argc < 6) {
    print_usage(stderr);
    return 1;
  }

  memset(&packet, 0, sizeof(packet));

  status = parse_u8(argv[0], &packet.slot);
  if (status != LCC_OK) {
    return exit_with_status(status);
  }
  status = parse_u16(argv[1], &packet.sac1);
  if (status != LCC_OK) {
    return exit_with_status(status);
  }
  status = parse_u8(argv[2], &packet.sa[0]);
  if (status != LCC_OK) {
    return exit_with_status(status);
  }
  status = parse_u8(argv[3], &packet.sa[1]);
  if (status != LCC_OK) {
    return exit_with_status(status);
  }
  status = parse_u8(argv[4], &packet.sa[2]);
  if (status != LCC_OK) {
    return exit_with_status(status);
  }
  status = parse_u8(argv[5], &packet.sa[3]);
  if (status != LCC_OK) {
    return exit_with_status(status);
  }

  for (index = 6; index < argc; ++index) {
    if (strcmp(argv[index], "--call-node") == 0 && index + 1 < argc) {
      call_node = argv[++index];
      continue;
    }
    if (strcmp(argv[index], "--dry-run") == 0) {
      dry_run = true;
      continue;
    }
    return exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  status = init_backend(&backend, call_node, dry_run);
  if (status != LCC_OK) {
    return exit_with_status(status);
  }

  status = amw0_packet_format_expr(&packet, expr, sizeof(expr));
  if (status != LCC_OK) {
    return exit_with_status(status);
  }

  (void)printf("expr=%s\n", expr);

  status = amw0_backend_send_packet(&backend, &packet, reply, sizeof(reply));
  if (status != LCC_OK) {
    return exit_with_status(status);
  }

  (void)printf("reply=%s\n", reply);
  return 0;
}

static int cmd_mode_set(int argc, char **argv) {
  lcc_operating_mode_t mode = LCC_MODE_OFFICE;
  lcc_apply_plan_t plan;
  bool execute = false;
  int index = 0;
  lcc_status_t status = LCC_OK;

  if (argc < 1) {
    return exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  status = lcc_mode_from_string(argv[0], &mode);
  if (status != LCC_OK) {
    return exit_with_status(status);
  }

  for (index = 1; index < argc; ++index) {
    if (strcmp(argv[index], "--execute") == 0) {
      execute = true;
      continue;
    }
    return exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  status = lcc_build_mode_plan(mode, &plan);
  if (status != LCC_OK) {
    return exit_with_status(status);
  }

  (void)printf("mode=%s\n", lcc_mode_name(mode));
  return print_plan_or_unimplemented(&plan, execute);
}

static int cmd_power_set(int argc, char **argv) {
  lcc_power_limits_t limits;
  lcc_profile_document_t document;
  lcc_apply_plan_t plan;
  const char *file_path = NULL;
  bool execute = false;
  int index = 0;
  lcc_status_t status = LCC_OK;

  memset(&limits, 0, sizeof(limits));

  for (index = 0; index < argc; ++index) {
    if (strcmp(argv[index], "--file") == 0 && index + 1 < argc) {
      file_path = argv[++index];
    } else if (strcmp(argv[index], "--pl1") == 0 && index + 1 < argc) {
      limits.pl1.present = true;
      status = parse_u8(argv[++index], &limits.pl1.value);
    } else if (strcmp(argv[index], "--pl2") == 0 && index + 1 < argc) {
      limits.pl2.present = true;
      status = parse_u8(argv[++index], &limits.pl2.value);
    } else if (strcmp(argv[index], "--pl4") == 0 && index + 1 < argc) {
      limits.pl4.present = true;
      status = parse_u8(argv[++index], &limits.pl4.value);
    } else if (strcmp(argv[index], "--tcc-offset") == 0 && index + 1 < argc) {
      limits.tcc_offset.present = true;
      status = parse_u8(argv[++index], &limits.tcc_offset.value);
    } else if (strcmp(argv[index], "--execute") == 0) {
      execute = true;
      continue;
    } else {
      return exit_with_status(LCC_ERR_INVALID_ARGUMENT);
    }

    if (status != LCC_OK) {
      return exit_with_status(status);
    }
  }

  if (file_path != NULL) {
    status = lcc_profile_document_load(file_path, &document);
    if (status != LCC_OK) {
      return exit_with_status(status);
    }
    if (!document.has_power_limits) {
      return exit_with_status(LCC_ERR_PARSE);
    }
    status = lcc_build_power_plan(&document.power_limits, &plan);
  } else {
    status = lcc_build_power_plan(&limits, &plan);
  }
  if (status != LCC_OK) {
    return exit_with_status(status);
  }

  return print_plan_or_unimplemented(&plan, execute);
}

static int cmd_fan_apply(int argc, char **argv) {
  lcc_fan_table_t table;
  lcc_apply_plan_t plan;
  const char *preset = "demo";
  const char *file_path = NULL;
  bool execute = false;
  int index = 0;
  lcc_status_t status = LCC_OK;

  for (index = 0; index < argc; ++index) {
    if (strcmp(argv[index], "--preset") == 0 && index + 1 < argc) {
      preset = argv[++index];
      continue;
    }
    if (strcmp(argv[index], "--file") == 0 && index + 1 < argc) {
      file_path = argv[++index];
      continue;
    }
    if (strcmp(argv[index], "--execute") == 0) {
      execute = true;
      continue;
    }
    return exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  if (file_path != NULL) {
    if (strcmp(preset, "demo") != 0) {
      return exit_with_status(LCC_ERR_INVALID_ARGUMENT);
    }
    status = lcc_fan_table_load_file(file_path, &table);
    if (status != LCC_OK) {
      return exit_with_status(status);
    }
  } else {
    status = lcc_fan_table_build_demo(&table, preset);
    if (status != LCC_OK) {
      return exit_with_status(status);
    }
  }

  status = lcc_build_fan_plan(&table, &plan);
  if (status != LCC_OK) {
    return exit_with_status(status);
  }

  (void)printf("fan-table=%s\n", table.name);
  return print_plan_or_unimplemented(&plan, execute);
}

static int cmd_profile_apply(int argc, char **argv) {
  lcc_profile_document_t document;
  lcc_apply_plan_t plan;
  const char *file_path = NULL;
  bool execute = false;
  int index = 0;
  lcc_status_t status = LCC_OK;

  for (index = 0; index < argc; ++index) {
    if (strcmp(argv[index], "--file") == 0 && index + 1 < argc) {
      file_path = argv[++index];
      continue;
    }
    if (strcmp(argv[index], "--execute") == 0) {
      execute = true;
      continue;
    }
    return exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  if (file_path == NULL) {
    return exit_with_status(LCC_ERR_INVALID_ARGUMENT);
  }

  status = lcc_profile_document_load(file_path, &document);
  if (status != LCC_OK) {
    return exit_with_status(status);
  }

  status = lcc_build_profile_plan(&document, &plan);
  if (status != LCC_OK) {
    return exit_with_status(status);
  }

  if (document.has_mode) {
    (void)printf("mode=%s\n", lcc_mode_name(document.mode));
  }
  if (document.has_fan_table) {
    (void)printf("fan-table=%s\n", document.fan_table.name);
  }
  return print_plan_or_unimplemented(&plan, execute);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage(stderr);
    return 1;
  }

  if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
    print_usage(stdout);
    return 0;
  }

  if (strcmp(argv[1], "status") == 0) {
    return cmd_status(argc - 2, argv + 2);
  }
  if (strcmp(argv[1], "observe") == 0) {
    return cmd_observe(argc - 2, argv + 2);
  }
  if (strcmp(argv[1], "raw") == 0 && argc >= 3 && strcmp(argv[2], "wmbc") == 0) {
    return cmd_raw_wmbc(argc - 3, argv + 3);
  }
  if (strcmp(argv[1], "mode") == 0 && argc >= 3 &&
      strcmp(argv[2], "set") == 0) {
    return cmd_mode_set(argc - 3, argv + 3);
  }
  if (strcmp(argv[1], "power") == 0 && argc >= 3 &&
      strcmp(argv[2], "set") == 0) {
    return cmd_power_set(argc - 3, argv + 3);
  }
  if (strcmp(argv[1], "fan") == 0 && argc >= 3 &&
      strcmp(argv[2], "apply") == 0) {
    return cmd_fan_apply(argc - 3, argv + 3);
  }
  if (strcmp(argv[1], "profile") == 0 && argc >= 3 &&
      strcmp(argv[2], "apply") == 0) {
    return cmd_profile_apply(argc - 3, argv + 3);
  }

  print_usage(stderr);
  return 1;
}
