#ifndef EMPIRE_TYPE_H
#define EMPIRE_TYPE_H

typedef enum {
    EMPIRE_OBJECT_ORNAMENT = 0,
    EMPIRE_OBJECT_CITY = 1,
    EMPIRE_OBJECT_BATTLE_ICON = 3,
    EMPIRE_OBJECT_LAND_TRADE_ROUTE = 4,
    EMPIRE_OBJECT_SEA_TRADE_ROUTE = 5,
    EMPIRE_OBJECT_ROMAN_ARMY = 6,
    EMPIRE_OBJECT_ENEMY_ARMY = 7,
    EMPIRE_OBJECT_TRADE_WAYPOINT = 8,
    EMPIRE_OBJECT_BORDER = 9,
    EMPIRE_OBJECT_BORDER_EDGE = 10
} empire_object_type;

typedef enum {
    EMPIRE_CITY_DISTANT_ROMAN = 0,
    EMPIRE_CITY_OURS = 1,
    EMPIRE_CITY_TRADE = 2,
    EMPIRE_CITY_FUTURE_TRADE = 3,
    EMPIRE_CITY_DISTANT_FOREIGN = 4,
    EMPIRE_CITY_VULNERABLE_ROMAN = 5,
    EMPIRE_CITY_FUTURE_ROMAN = 6,
} empire_city_type;

typedef enum {
    EMPIRE_CITY_ICON_DEFAULT, // not specified
    EMPIRE_CITY_ICON_CONSTRUCTION, // construction   Empire_Icon_Construction_01.png
    EMPIRE_CITY_ICON_DISTANT_TOWN, // dis_town       Empire_Icon_Distant_01.png
    EMPIRE_CITY_ICON_DISTANT_VILLAGE, // dis_village Empire_Icon_Distant_02.png
    EMPIRE_CITY_ICON_RESOURCE_FOOD, // res_food      Empire_Icon_Resource_01.png
    EMPIRE_CITY_ICON_RESOURCE_GOODS, // res_goods    Empire_Icon_Resource_02.png
    EMPIRE_CITY_ICON_RESOURCE_SEA, // res_sea        Empire_Icon_Resource_03.png
    EMPIRE_CITY_ICON_TRADE_TOWN, // tr_town          Empire_Icon_Roman_01.png
    EMPIRE_CITY_ICON_ROMAN_TOWN, // ro_town          Empire_Icon_Roman_02.png
    EMPIRE_CITY_ICON_TRADE_VILLAGE, // tr_village    Empire_Icon_Roman_03.png
    EMPIRE_CITY_ICON_ROMAN_VILLAGE, // ro_village    Empire_Icon_Roman_04.png
    EMPIRE_CITY_ICON_ROMAN_CAPITAL, // ro_capital    Empire_Icon_Roman_05.png
    EMPIRE_CITY_ICON_TRADE_SEA, // tr_sea            Empire_Icon_Trade_01.png
    EMPIRE_CITY_ICON_TRADE_LAND, // tr_land          Empire_Icon_Trade_02.png
    EMPIRE_CITY_ICON_OUR_CITY, // our_city
    EMPIRE_CITY_ICON_TRADE_CITY, // tr_city
    EMPIRE_CITY_ICON_ROMAN_CITY, // ro_city
    EMPIRE_CITY_ICON_DISTANT_CITY, // dis_city
    EMPIRE_CITY_ICON_TOWER, // tower                Empire_Icon_Flag_01.png - Empire_Icon_Flag_06.png
} empire_city_icon_type;

#endif // EMPIRE_TYPE_H
