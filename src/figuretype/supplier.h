#ifndef FIGURETYPE_SUPPLIER_H
#define FIGURETYPE_SUPPLIER_H

#include "figure/figure.h"

#define MAX_FOOD_STOCKED_MARKET 800
#define MAX_FOOD_STOCKED_MESS_HALL 1600 
#define MAX_FOOD_STOCKED_CARAVANSERAI 1600

#ifdef __cplusplus
extern "C" {
#endif

int figure_supplier_max_stocked_mess_hall_adjusted(void);

int figure_supplier_create_delivery_boy(int leader_id, int first_figure_id, int type);

void figure_supplier_action(figure *f);

void figure_delivery_boy_action(figure *f);

void figure_fort_supplier_action(figure *f);

#ifdef __cplusplus
}
#endif

#endif // FIGURETYPE_SUPPLIER_H
