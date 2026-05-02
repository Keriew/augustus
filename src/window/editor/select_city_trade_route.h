#ifndef WINDOW_EDITOR_SELECT_CITY_TRADE_ROUTE_H
#define WINDOW_EDITOR_SELECT_CITY_TRADE_ROUTE_H

#include <stdint.h>

void window_editor_select_city_trade_route_show(void (*callback)(int));
void window_editor_select_city_resources_for_route_show(void (*callback)(int), int route_id);

// Encode/decode helpers for composite route+resource parameter
int window_editor_select_city_trade_route_encode_route_resource(int trade_route_id, int resource_id);
int window_editor_select_city_trade_route_decode_route_id(int encoded_value);
int window_editor_select_city_trade_route_decode_resource_id(int encoded_value);

// Get display name from encoded value
const uint8_t *window_editor_select_city_trade_route_show_get_selected_name(int encoded_value);

#endif // WINDOW_EDITOR_SELECT_CITY_TRADE_ROUTE_H
