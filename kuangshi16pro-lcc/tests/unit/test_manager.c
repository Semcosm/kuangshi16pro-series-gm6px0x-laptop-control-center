#include <assert.h>
#include <string.h>

#include "daemon/manager.h"

static void test_manager_init_and_capabilities(void) {
  lcc_manager_t manager;

  assert(lcc_manager_init(&manager, "data/capabilities/gm6px0x.json") ==
         LCC_OK);
  assert(strstr(lcc_manager_capabilities_json(&manager), "\"GM6PX0X\"") != NULL);
}

static void test_manager_profile_and_fan_updates(void) {
  lcc_manager_t manager;
  char json[LCC_MANAGER_JSON_MAX];

  assert(lcc_manager_init(&manager, NULL) == LCC_OK);
  assert(lcc_manager_set_profile(&manager, "turbo") == LCC_OK);
  assert(lcc_manager_apply_fan_table(&manager, "M4T1") == LCC_OK);
  assert(lcc_manager_get_state_json(&manager, json, sizeof(json)) == LCC_OK);
  assert(strstr(json, "\"profile\":\"turbo\"") != NULL);
  assert(strstr(json, "\"fan_table\":\"M4T1\"") != NULL);
}

static void test_manager_power_update(void) {
  lcc_manager_t manager;
  char json[LCC_MANAGER_JSON_MAX];

  assert(lcc_manager_init(&manager, NULL) == LCC_OK);
  assert(lcc_manager_set_power_limits(&manager, 75u, 130u, 200u, 5u) == LCC_OK);
  assert(lcc_manager_get_state_json(&manager, json, sizeof(json)) == LCC_OK);
  assert(strstr(json, "\"pl1\":75") != NULL);
  assert(strstr(json, "\"pl2\":130") != NULL);
  assert(strstr(json, "\"pl4\":200") != NULL);
  assert(strstr(json, "\"tcc_offset\":5") != NULL);
}

static void test_manager_rejects_unsafe_names(void) {
  lcc_manager_t manager;

  assert(lcc_manager_init(&manager, NULL) == LCC_OK);
  assert(lcc_manager_set_profile(&manager, "bad name") ==
         LCC_ERR_INVALID_ARGUMENT);
  assert(lcc_manager_apply_fan_table(&manager, "bad/name") ==
         LCC_ERR_INVALID_ARGUMENT);
}

void lcc_run_manager_tests(void) {
  test_manager_init_and_capabilities();
  test_manager_profile_and_fan_updates();
  test_manager_power_update();
  test_manager_rejects_unsafe_names();
}
