#ifndef FIGURETYPE_TRADER_H
#define FIGURETYPE_TRADER_H

#include "figure/figure.h"

enum {
    TRADE_SHIP_NONE = 0,
    TRADE_SHIP_BUYING = 1,
    TRADE_SHIP_SELLING = 2,
};

int figure_create_trade_caravan(int x, int y, int city_id);

int figure_create_trade_ship(int x, int y, int city_id);

int figure_trade_caravan_can_buy(figure *trader, int warehouse_id, int city_id);

int figure_trade_caravan_can_sell(figure *trader, int warehouse_id, int city_id);

void figure_trade_caravan_action(figure *f);

void figure_trade_caravan_donkey_action(figure *f);

void figure_native_trader_action(figure *f);

int figure_trade_ship_is_trading(figure *ship);

void figure_trade_ship_action(figure *f);

int figure_trade_land_trade_units(void);

int figure_trade_sea_trade_units(void);

int figure_trader_ship_can_queue_for_import(figure *ship);

int figure_trader_ship_can_queue_for_export(figure *ship);

int figure_trader_ship_get_distance_to_dock(const figure *ship, unsigned int dock_id);

int figure_trader_ship_other_ship_closer_to_dock(unsigned int dock_id, int distance);

#define IMAGE_CAMEL 4922

#ifdef ENABLE_MULTIPLAYER
int figure_trade_is_authoritative(const figure *f);
void figure_trade_apply_host_result(int figure_id, int resource, int amount, int buying);
void figure_trade_emit_event_if_host(const figure *f, int event_type);
#endif

#endif // FIGURETYPE_TRADER_H
