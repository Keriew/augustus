#ifndef BUILDING_DATA_TRANSFER_H
#define BUILDING_DATA_TRANSFER_H

#include "building/building.h"

typedef enum {
    DATA_TYPE_NOT_SUPPORTED,
    DATA_TYPE_WAREHOUSE,
    DATA_TYPE_GRANARY,
    DATA_TYPE_DOCK,
    DATA_TYPE_MARKET,
    DATA_TYPE_TAVERN,
    DATA_TYPE_ROADBLOCK,
    DATA_TYPE_DEPOT,
    DATA_TYPE_RAW_RESOURCE_PRODUCER
} building_data_type;

void building_data_transfer_clear(int backup);
void building_data_transfer_backup(void);
void building_data_transfer_restore(void);
void building_data_transfer_restore_and_clear_backup(void);

int building_data_transfer_copy(building *b, int supress_warnings);

int building_data_transfer_paste(building *b, int supress_warnings);

int building_data_transfer_possible(building *b, int supress_warnings);

building_data_type building_data_transfer_data_type_from_building_type(building_type type);

#endif // BUILDING_DATA_TRANSFER_H
