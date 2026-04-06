#ifndef LCC_CORE_CAPABILITIES_MODEL_MAP_H
#define LCC_CORE_CAPABILITIES_MODEL_MAP_H

#include <stdbool.h>

#define LCC_MODEL_NAME_MAX 32u
#define LCC_GPU_MUX_MODE_MAX 32u

typedef struct {
  char model[LCC_MODEL_NAME_MAX];
  bool platform_profile_preferred;
  bool platform_profile_fallback_required;
  bool feature_fan_table_1p5;
  bool feature_smart_apc;
  char gpu_mux[LCC_GPU_MUX_MODE_MAX];
} lcc_model_map_t;

void lcc_model_map_seed_defaults(lcc_model_map_t *map);

#endif
