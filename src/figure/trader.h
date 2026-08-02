#ifndef FIGURE_TRADER_H
#define FIGURE_TRADER_H

#include "core/buffer.h"
#include "game/resource.h"

/**
 * @file
 * Trade figure extra info
 */

enum trader_type {
    TRADER_SEA = 0,
    TRADER_LAND = 1,
    TRADER_NATIVE = 2, // subtype of land, for most intents and purposes
};

/**
  * Clears all traders
  */
void traders_clear(void);

/**
 * Creates a trader
 * @return ID of the new trader
 */
int trader_create(void);

/**
 * Check whether a trader is a land or sea trader
 * @param figure_id Figure ID of the trader
 * @return TRADER_LAND, TRADER_SEA, or TRADER_NATIVE
 */
int trader_is_land_by_figure_id(int figure_id);

/**
 * Check whether city is a land or sea route by empire city ID
 * @param empire_city_id Empire city ID to check
 * @return TRADER_LAND, TRADER_SEA, or TRADER_NATIVE
 */
int trader_is_land_by_empire_city_id(unsigned short empire_city_id);

/**
 * Record that the trader has bought a resource from the city
 * @param trader_id Trader
 * @param resource Resource bought
 * @param storage_id Storage building ID where the trade took place
 */
void trader_record_bought_resource(int figure_id, unsigned short trader_id, resource_type resource, int storage_id);

/**
 * Record that the trader has sold a resource to the city
 * @param trader_id Trader
 * @param resource Resource sold
 * @param storage_id Storage building ID where the trade took place
 */
void trader_record_sold_resource(int figure_id, unsigned short trader_id, resource_type resource, int storage_id);

/**
 * Gets the amount bought of the given resource
 * @param trader_id Trader
 * @param resource Resource
 * @return Amount of resource bought by the trader from the city
 */
int trader_bought_resources(int trader_id, resource_type resource);

/**
 * Gets the amount sold of the given resource
 * @param trader_id Trader
 * @param resource Resource
 * @return Amount of resource sold by the trader to the city
 */
int trader_sold_resources(int trader_id, resource_type resource);

/**
 * Check whether this trader has bought/sold any items
 * @param trader_id Trader
 * @return True if the trader has bought or sold at least one item
 */
int trader_has_traded(int trader_id);

/**
 * Check whether a trade ship has traded the maximum amount
 * @param trader_id Trader
 * @return True if the trader has either bought or sold the max amount (12 or 16 with mercury monument)
 */
int trader_has_traded_max(int trader_id);

/**
 * Check whether a trade ship has bought the maximum amount
 * @param trader_id Trader
 * @return True if the trader has bought the max amount (12 or 16 with mercury monument)
 */
int trader_has_bought_max(int trader_id);


/**
 * Check whether a trade ship has sold the maximum amount
 * @param trader_id Trader
 * @return True if the trader has sold the max amount (12 or 16 with mercury monument)
 */
int trader_has_sold_max(int trader_id);

/**
 * Save state to buffer
 * @param buf Buffer
 */
void traders_save_state(buffer *buf);

/**
 * Load state from buffer
 * @param buf Buffer
 */
void traders_load_state(buffer *buf);

#endif // FIGURE_TRADE_INFO_H
