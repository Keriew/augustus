#ifndef WINDOW_BUILDING_INFO_H
#define WINDOW_BUILDING_INFO_H

void window_building_info_show(int grid_offset);

int window_building_info_get_building_type(void);

void window_building_info_show_storage_orders(void);

void window_building_info_show_roadblock_orders(void);

void window_building_info_show_storage_special_orders(void);

void window_building_info_show_storage_special_orders_on_top(int building_id);

void window_building_info_restore_previous_context(void);

void window_building_info_reset_previous_context(void);

void window_building_info_depot_select_source(void);

void window_building_info_depot_select_destination(void);

void window_building_info_depot_select_resource(void);

void window_building_info_depot_toggle_condition_type(void);

void window_building_info_depot_toggle_condition_threshold(void);

void window_building_info_depot_toggle_condition_threshold_reverse(void);

void window_building_info_depot_return_to_main_window(void);

#endif // WINDOW_BUILDING_INFO_H
