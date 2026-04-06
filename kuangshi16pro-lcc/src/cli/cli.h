#ifndef LCC_CLI_H
#define LCC_CLI_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "lcc/error.h"
#include "lcc/plan.h"

const char *lcc_cli_default_call_node(void);
void lcc_cli_print_usage(FILE *stream);
int lcc_cli_exit_with_status(lcc_status_t status);
bool lcc_cli_parse_bus_flag(const char *arg, bool *use_user_bus);
lcc_status_t lcc_cli_parse_u8(const char *text, uint8_t *value);
lcc_status_t lcc_cli_parse_u16(const char *text, uint16_t *value);
lcc_status_t lcc_cli_parse_u32_reply(const char *text, uint32_t *value);
int lcc_cli_print_plan_or_unimplemented(const lcc_apply_plan_t *plan,
                                        bool execute);

int lcc_cmd_state_status(int argc, char **argv);
int lcc_cmd_state_capabilities(int argc, char **argv);
int lcc_cmd_state_observe(int argc, char **argv);
int lcc_cmd_debug_raw_wmbc(int argc, char **argv);
int lcc_cmd_mode_set(int argc, char **argv);
int lcc_cmd_power_set(int argc, char **argv);
int lcc_cmd_fan_apply(int argc, char **argv);
int lcc_cmd_profile_apply(int argc, char **argv);

#endif
