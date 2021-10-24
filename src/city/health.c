#include "health.h"

#include "building/destruction.h"
#include "building/granary.h"
#include "building/warehouse.h"
#include "city/data_private.h"
#include "city/message.h"
#include "core/calc.h"
#include "core/random.h"
#include "game/tutorial.h"
#include "scenario/property.h"

int city_health(void)
{
    return city_data.health.value;
}

void city_health_change(int amount)
{
    city_data.health.value = calc_bound(city_data.health.value + amount, 0, 100);
}

static void cause_plague_in_building(int building_id)
{
    building *b = building_get(building_id);
    if (!b->ruin_has_plague) {

        // remove all eatable resources from granary and warehouse
        if (b->type == BUILDING_GRANARY) {
            for (int resource = RESOURCE_MIN_FOOD; resource < RESOURCE_MAX_FOOD; resource++) {
                building_granary_remove_resource(b, resource, 3200);
            }
        }

        if (b->type == BUILDING_WAREHOUSE) {
            for (int resource = RESOURCE_MIN_FOOD; resource < RESOURCE_IRON; resource++) {
                building_warehouse_remove_resource(b, resource, 32);
            }
        }

        // set building to plague status and use fire process to manage plague on it
        b->ruin_has_plague = 1;
        b->fire_proof = 1;
        b->sickness_duration = 0;
    }
}

void city_health_update_sickness_level_in_building(int building_id)
{
    building *b = building_get(building_id);

    if (!b->ruin_has_plague && b->state == BUILDING_STATE_IN_USE) {
        b->sickness_level += 3;

        if (b->sickness_level > 100) {
            b->sickness_level = 100;
        }
    }
}

void city_health_dispatch_sickness(figure *f)
{
    building *b = building_get(f->building_id);
    building *dest_b = building_get(f->destination_building_id);

    // dispatch sickness level sub value between granaries, warehouses and docks
    if ((dest_b->type == BUILDING_GRANARY || dest_b->type == BUILDING_WAREHOUSE || dest_b->type == BUILDING_DOCK) &&
        b->sickness_level && b->sickness_level > dest_b->sickness_level) {
        int value = b->sickness_level == 1 ? 1 : b->sickness_level / 2;
        if (dest_b->sickness_level < value) {
            dest_b->sickness_level = value;
        }
    } else if((b->type == BUILDING_GRANARY || b->type == BUILDING_WAREHOUSE || b->type == BUILDING_DOCK) &&
        dest_b->sickness_level && dest_b->sickness_level > b->sickness_level) {
        int value = dest_b->sickness_level == 1 ? 1 : dest_b->sickness_level / 2;
        if (b->sickness_level < value) {
            b->sickness_level = value;
        }
    }
}

static int cause_plague()
{
    int sick_people = 0;
    // kill people who have sickness level to max in houses
    for (building_type type = BUILDING_HOUSE_SMALL_TENT; type <= BUILDING_HOUSE_LUXURY_PALACE; type++) {
        building *next_of_type = 0; // building_destroy_by_plague changes the building type
        for (building *b = building_first_of_type(type); b; b = next_of_type) {
            next_of_type = b->next_of_type;
            if (b->state == BUILDING_STATE_IN_USE && b->house_size && b->house_population) {
                if (b->sickness_level == 100) {
                    sick_people = 1;
                    building_destroy_by_plague(b);
                }
            }
        }
    }

    if (sick_people) {
        city_message_post(1, MESSAGE_HEALTH_PESTILENCE, 0, 0);
    }
    // cause plague in docks
    for (building *dock = building_first_of_type(BUILDING_DOCK); dock; dock = dock->next_of_type) {
        if (dock->sickness_level == 100) {
            cause_plague_in_building(dock->id);
        }
    }
    // cause plague in warehouses
    for (building *warehouse = building_first_of_type(BUILDING_WAREHOUSE); warehouse; warehouse = warehouse->next_of_type) {
        if (warehouse->sickness_level == 100) {
            cause_plague_in_building(warehouse->id);
        }
    }
    // cause plague in granaries
    for (building *granary = building_first_of_type(BUILDING_GRANARY); granary; granary = granary->next_of_type) {
        if (granary->sickness_level == 100) {
            cause_plague_in_building(granary->id);
        }
    }

    return sick_people;
}

static void cause_disease(int total_people)
{
    if (cause_plague()) {
        return;
    }

    if (city_data.health.value >= 40) {
        return;
    }
    int chance_value = random_byte() & 0x3f;
    if (city_data.religion.venus_curse_active) {
        // force plague
        chance_value = 0;
        city_data.religion.venus_curse_active = 0;
    }
    if (chance_value > 40 - city_data.health.value) {
        return;
    }

    int sick_people = calc_adjust_with_percentage(total_people, 7 + (random_byte() & 3));
    if (sick_people <= 0) {
        return;
    }
    city_health_change(10);
    int people_to_kill = sick_people - city_data.health.num_hospital_workers;
    if (people_to_kill <= 0) {
        city_message_post(1, MESSAGE_HEALTH_ILLNESS, 0, 0);
        return;
    }
    if (city_data.health.num_hospital_workers > 0) {
        city_message_post(1, MESSAGE_HEALTH_DISEASE, 0, 0);
    } else {
        city_message_post(1, MESSAGE_HEALTH_PESTILENCE, 0, 0);
    }
    tutorial_on_disease();
    // kill people who don't have access to a doctor
    for (building_type type = BUILDING_HOUSE_SMALL_TENT; type <= BUILDING_HOUSE_LUXURY_PALACE; type++) {
        building *next_of_type = 0; // building_destroy_by_plague changes the building type
        for (building *b = building_first_of_type(type); b; b = next_of_type) {
            next_of_type = b->next_of_type;
            if (b->state == BUILDING_STATE_IN_USE && b->house_size && b->house_population) {
                if (!b->data.house.clinic) {
                    people_to_kill -= b->house_population;
                    building_destroy_by_plague(b);
                    if (people_to_kill <= 0) {
                        return;
                    }
                }
            }
        }
    }
    // kill people in tents
    for (building_type type = BUILDING_HOUSE_SMALL_TENT; type <= BUILDING_HOUSE_LUXURY_PALACE; type++) {
        building *next_of_type = 0; // building_destroy_by_plague changes the building type
        for (building *b = building_first_of_type(type); b; b = next_of_type) {
            next_of_type = b->next_of_type;
            if (b->state == BUILDING_STATE_IN_USE && b->house_size && b->house_population) {
                people_to_kill -= b->house_population;
                building_destroy_by_plague(b);
                if (people_to_kill <= 0) {
                    return;
                }
            }
        }
    }
    // kill anyone
    for (building_type type = BUILDING_HOUSE_SMALL_TENT; type <= BUILDING_HOUSE_LUXURY_PALACE; type++) {
        building *next_of_type = 0; // building_destroy_by_plague changes the building type
        for (building *b = building_first_of_type(type); b; b = next_of_type) {
            next_of_type = b->next_of_type;
            if (b->state == BUILDING_STATE_IN_USE && b->house_size && b->house_population) {
                people_to_kill -= b->house_population;
                building_destroy_by_plague(b);
                if (people_to_kill <= 0) {
                    return;
                }
            }
        }
    }
}

static void increase_sickness_level_in_building(building *b)
{
    if (!b->ruin_has_plague) {
        if(b->sickness_level) {
            b->sickness_level += 5;

            if (b->sickness_level > 100) {
                b->sickness_level = 100;
            }
        }
    }
}

static void increase_sickness_level_in_buildings(void)
{
    for (building_type type = BUILDING_HOUSE_SMALL_TENT; type <= BUILDING_HOUSE_LUXURY_PALACE; type++) {
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            increase_sickness_level_in_building(b);
        }
    }

    for (building *warehouse = building_first_of_type(BUILDING_WAREHOUSE); warehouse; warehouse = warehouse->next_of_type) {
        increase_sickness_level_in_building(warehouse);
    }

    for (building *granary = building_first_of_type(BUILDING_GRANARY); granary; granary = granary->next_of_type) {
        increase_sickness_level_in_building(granary);
    }

    for (building *dock = building_first_of_type(BUILDING_GRANARY); dock; dock = dock->next_of_type) {
        increase_sickness_level_in_building(dock);
    }
}

void city_health_update(void)
{
    if (city_data.population.population < 200 || scenario_is_tutorial_1() || scenario_is_tutorial_2()) {
        city_data.health.value = 50;
        city_data.health.target_value = 50;
        return;
    }
    int total_population = 0;
    int healthy_population = 0;
    for (building_type type = BUILDING_HOUSE_SMALL_TENT; type <= BUILDING_HOUSE_LUXURY_PALACE; type++) {
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE || !b->house_size || !b->house_population) {
                continue;
            }
            total_population += b->house_population;
            if (b->subtype.house_level <= HOUSE_LARGE_TENT) {
                if (b->data.house.clinic) {
                    healthy_population += b->house_population;
                } else {
                    healthy_population += b->house_population / 4;
                }
            } else if (b->data.house.clinic) {
                if (b->house_days_without_food == 0) {
                    healthy_population += b->house_population;
                } else {
                    healthy_population += b->house_population / 4;
                }
            } else if (b->house_days_without_food == 0) {
                healthy_population += b->house_population / 4;
            }
        }
    }
    city_data.health.target_value = calc_percentage(healthy_population, total_population);
    if (city_data.health.value < city_data.health.target_value) {
        city_data.health.value += 2;
        if (city_data.health.value > city_data.health.target_value) {
            city_data.health.value = city_data.health.target_value;
        }
    } else if (city_data.health.value > city_data.health.target_value) {
        city_data.health.value -= 2;
        if (city_data.health.value < city_data.health.target_value) {
            city_data.health.value = city_data.health.target_value;
        }
    }
    city_data.health.value = calc_bound(city_data.health.value, 0, 100);

    increase_sickness_level_in_buildings();
    cause_disease(total_population);
}

void city_health_reset_hospital_workers(void)
{
    city_data.health.num_hospital_workers = 0;
}

void city_health_add_hospital_workers(int amount)
{
    city_data.health.num_hospital_workers += amount;
}

int city_health_get_global_sickness_level(void)
{
    int building_number = 0;
    int building_sickness_level = 0;
    int near_plague = 0;

    for (building_type type = BUILDING_HOUSE_SMALL_TENT; type <= BUILDING_HOUSE_LUXURY_PALACE; type++) {
        building *next_of_type;
        for (building *b = building_first_of_type(type); b; b = next_of_type) {
            next_of_type = b->next_of_type;
            if (b->state == BUILDING_STATE_IN_USE && b->house_size && b->house_population) {
                building_number++;
                building_sickness_level += b->sickness_level;

                if (b->sickness_level > 90) {
                    near_plague = 2;
                } else if (b->sickness_level > 60) {
                    near_plague = 1;
                }
            }
        }
    }
    for (building *dock = building_first_of_type(BUILDING_DOCK); dock; dock = dock->next_of_type) {
        building_number++;
        building_sickness_level += dock->sickness_level;
        if (dock->sickness_level > 90) {
            near_plague = 2;
        } else if (dock->sickness_level > 60) {
            near_plague = 1;
        }
    }
    for (building *warehouse = building_first_of_type(BUILDING_WAREHOUSE); warehouse; warehouse = warehouse->next_of_type) {
        building_number++;
        building_sickness_level += warehouse->sickness_level;
        if (warehouse->sickness_level > 90) {
            near_plague = 2;
        } else if (warehouse->sickness_level > 60) {
            near_plague = 1;
        }
    }
    for (building *granary = building_first_of_type(BUILDING_GRANARY); granary; granary = granary->next_of_type) {
        building_number++;
        building_sickness_level += granary->sickness_level;
        if (granary->sickness_level > 90) {
            near_plague = 2;
        } else if (granary->sickness_level > 60) {
            near_plague = 1;
        }
    }

    int global_rating = building_sickness_level / building_number;
    int global_sickness_level;

    if (global_rating < 30 && !near_plague) {
        global_sickness_level = SICKNESS_LEVEL_LOW;
    } else if (global_rating < 60 && !near_plague) {
        global_sickness_level = SICKNESS_LEVEL_MEDIUM;
    } else if (global_rating < 90 && near_plague == 1) {
        global_sickness_level = SICKNESS_LEVEL_HIGH;
    } else {
        global_sickness_level = SICKNESS_LEVEL_PLAGUE;
    }

    return global_sickness_level;
}
