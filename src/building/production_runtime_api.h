#ifndef BUILDING_PRODUCTION_RUNTIME_API_H
#define BUILDING_PRODUCTION_RUNTIME_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct building building;

void production_runtime_reset(void);
void production_runtime_initialize_city(void);
int production_runtime_has_native_production(building *b);
int production_runtime_get_method_count(building *b);
int production_runtime_building_has_raw_materials(building *b);
int production_runtime_get_max_progress(building *b);
int production_runtime_get_efficiency(building *b);
int production_runtime_update_building(building *b, int new_day, int *out_is_striking);
int production_runtime_has_produced_resource(building *b);
int production_runtime_start_new_production(building *b);
void production_runtime_advance_stats(building *b);
void production_runtime_bless_farm(building *b);
void production_runtime_curse_farm(building *b, int big_curse);
void production_runtime_bless_industry(building *b);

#ifdef __cplusplus
}
#endif

#endif // BUILDING_PRODUCTION_RUNTIME_API_H
