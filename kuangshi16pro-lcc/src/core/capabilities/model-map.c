#include "core/capabilities/model-map.h"

#include <stdio.h>
#include <string.h>

void lcc_model_map_seed_defaults(lcc_model_map_t *map) {
  if (map == NULL) {
    return;
  }

  memset(map, 0, sizeof(*map));
  (void)snprintf(map->model, sizeof(map->model), "%s", "GM6PX0X");
  map->platform_profile_preferred = true;
  map->platform_profile_fallback_required = true;
  map->feature_fan_table_1p5 = true;
  map->feature_smart_apc = true;
  (void)snprintf(map->gpu_mux, sizeof(map->gpu_mux), "%s", "experimental");
}
