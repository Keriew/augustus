#ifndef BUILDING_STORAGE_RUNTIME_API_H
#define BUILDING_STORAGE_RUNTIME_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct building building;

void storage_runtime_reset(void);
void storage_runtime_initialize_city(void);
int storage_runtime_get_slot_count(building *b);
const char *storage_runtime_get_slot_type_path(building *b, int slot_index);

#ifdef __cplusplus
}
#endif

#endif // BUILDING_STORAGE_RUNTIME_API_H
