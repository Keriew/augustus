#include "armoury.h"

#include "building/barracks.h"
#include "building/count.h"
#include "building/distribution.h"
#include "city/resource.h"
#include "figure/action.h"
#include "figure/figure.h"
#include "map/point.h"

static int is_weapons_destination(const building *b)
{
    return b->type == BUILDING_BARRACKS || b->type == BUILDING_GRAND_TEMPLE_MARS;
}

// Weapons already going to a barracks: carried there, or being fetched by an armoury supplier
static int weapons_on_the_way(void)
{
    int count = 0;
    for (unsigned int i = 1; i < figure_count(); i++) {
        figure *f = figure_get(i);
        if (f->state != FIGURE_STATE_ALIVE || f->type != FIGURE_WAREHOUSEMAN) {
            continue;
        }
        if (f->action_state == FIGURE_ACTION_248_ARMOURY_SUPPLIER_GETTING_WEAPONS ||
            f->action_state == FIGURE_ACTION_249_ARMOURY_SUPPLIER_AT_WAREHOUSE) {
            count++;
        } else if (f->resource_id == RESOURCE_WEAPONS &&
            (f->action_state == FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE ||
            f->action_state == FIGURE_ACTION_52_WAREHOUSEMAN_AT_DELIVERY_BUILDING) &&
            is_weapons_destination(building_get(f->destination_building_id))) {
            count++;
        } else if (f->resource_id == RESOURCE_WEAPONS &&
            building_get(f->building_id)->type == BUILDING_ARMOURY &&
            (f->action_state == FIGURE_ACTION_50_WAREHOUSEMAN_CREATED ||
            f->action_state == FIGURE_ACTION_233_WAREHOUSEMAN_RECONSIDER_TARGET)) {
            count++; // armoury supplier holding weapons, looking for a barracks
        }
    }
    return count;
}

int building_armoury_barracks_have_room(building *armoury)
{
    return building_barracks_free_weapon_slots(armoury->road_network_id) > weapons_on_the_way();
}

int building_armoury_is_needed(building *armoury)
{
    if (!building_count_active(BUILDING_BARRACKS) && !building_count_active(BUILDING_GRAND_TEMPLE_MARS)) {
        return 0;
    }

    if (!building_armoury_barracks_have_room(armoury)) {
        return 0;
    }

    resource_storage_info info[RESOURCE_MAX] = { 0 };
    info[RESOURCE_WEAPONS].needed = 1;

    return building_distribution_get_resource_storages_for_building(info, armoury, 10000);
}
