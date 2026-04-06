#include <assert.h>
#include <string.h>

#include "lcc/backend.h"
#include "lcc/profile.h"

void lcc_run_manager_tests(void);

static void test_mode_plan(void) {
  lcc_operating_mode_t mode = LCC_MODE_OFFICE;
  lcc_apply_plan_t plan;

  assert(lcc_mode_from_string("turbo", &mode) == LCC_OK);
  assert(mode == LCC_MODE_TURBO);
  assert(lcc_build_mode_plan(mode, &plan) == LCC_OK);
  assert(plan.count == 5u);
  assert(plan.actions[0].kind == LCC_ACTION_STAGE);
  assert(plan.actions[1].kind == LCC_ACTION_STAGE);
  assert(plan.actions[2].kind == LCC_ACTION_STAGE);
  assert(plan.actions[3].kind == LCC_ACTION_STAGE);
  assert(plan.actions[4].kind == LCC_ACTION_STAGE);
}

static void test_power_plan(void) {
  lcc_power_limits_t limits;
  lcc_apply_plan_t plan;

  memset(&limits, 0, sizeof(limits));
  limits.pl1.present = true;
  limits.pl1.value = 55u;
  limits.pl2.present = true;
  limits.pl2.value = 95u;

  assert(lcc_build_power_plan(&limits, &plan) == LCC_OK);
  assert(plan.count == 3u);
  assert(plan.actions[0].kind == LCC_ACTION_STAGE);
  assert(plan.actions[1].addr == 0x0783u);
  assert(plan.actions[1].value == 55u);
  assert(plan.actions[2].addr == 0x0784u);
  assert(plan.actions[2].value == 95u);
}

static void test_fan_plan(void) {
  lcc_fan_table_t table;
  lcc_apply_plan_t plan;

  assert(lcc_fan_table_build_demo(&table, "demo") == LCC_OK);
  assert(lcc_validate_fan_table(&table) == LCC_OK);
  assert(lcc_build_fan_plan(&table, &plan) == LCC_OK);
  assert(plan.count == 100u);
  assert(plan.actions[0].kind == LCC_ACTION_STAGE);
  assert(plan.actions[1].kind == LCC_ACTION_CUSTOM_MODE);
  assert(plan.actions[2].kind == LCC_ACTION_STAGE);
  assert(plan.actions[3].addr == 0x0F00u);
  assert(plan.actions[18].addr == 0x0F0Fu);
  assert(plan.actions[19].addr == 0x0F10u);
  assert(plan.actions[51].kind == LCC_ACTION_STAGE);
  assert(plan.actions[99].addr == 0x0F5Fu);
}

static void test_amw0_expr_format(void) {
  amw0_packet_t packet;
  char expr[AMW0_EXPR_MAX];

  memset(&packet, 0, sizeof(packet));
  packet.sac1 = AMW0_ROUTE_WKBC1;
  packet.sa[0] = 0x49u;
  packet.sa[1] = 0x00u;
  packet.sa[2] = 0x1Eu;
  packet.sa[3] = 0x00u;

  assert(amw0_packet_format_expr(&packet, expr, sizeof(expr)) == LCC_OK);
  assert(strstr(expr, "\\_SB.AMW0.WMBC 0x0 0x4") != NULL);
  assert(strstr(expr, "0x49,0x00,0x1E,0x00,0x01,0x00,0x00,0x00") != NULL);
}

static void test_profile_document_load(void) {
  lcc_profile_document_t document;
  lcc_apply_plan_t plan;

  assert(lcc_profile_document_load("tests/fixtures/demo-profile.ini",
                                   &document) == LCC_OK);
  assert(document.has_mode);
  assert(document.mode == LCC_MODE_CUSTOM);
  assert(document.has_power_limits);
  assert(document.power_limits.pl1.value == 55u);
  assert(document.has_fan_table);
  assert(strcmp(document.fan_table.name, "balanced-demo") == 0);
  assert(document.fan_table.cpu[0].up_temp == 40u);
  assert(document.fan_table.gpu[15].duty == 95u);
  assert(lcc_build_profile_plan(&document, &plan) == LCC_OK);
  assert(plan.count == 110u);
}

int main(void) {
  test_mode_plan();
  test_power_plan();
  test_fan_plan();
  test_amw0_expr_format();
  test_profile_document_load();
  lcc_run_manager_tests();
  return 0;
}
