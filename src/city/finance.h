#ifndef CITY_FINANCE_H
#define CITY_FINANCE_H

#include "building/type.h"
#include "city/resource.h"
#include "core/buffer.h"
#include "game/save_version.h"

#define SMALL_TEMPLE_LEVY_MONTHLY 4
#define FORT_LEVY_MONTHLY 8
#define TOWER_LEVY_MONTHLY 2
#define GRAND_TEMPLE_LEVY_MONTHLY 44
#define PANTHEON_LEVY_MONTHLY 48
#define LIGHTHOUSE_LEVY_MONTHLY 8
#define COLOSSEUM_LEVY_MONTHLY 36
#define HIPPODROME_LEVY_MONTHLY 72
#define CARAVANSERAI_LEVY_MONTHLY 8
#define LARGE_TEMPLE_LEVY_MONTHLY 8
#define SMALL_MAUSOLEUM_LEVY_MONTHLY 2
#define HIGHWAY_LEVY_MONTHLY 1


typedef struct {
    int type;
    int amount;
} building_levy_for_type;

typedef struct {
    int type;
    int income_modifier;
    int coverage;
    int count;
} tourism_for_type;

typedef struct {
    int price;                      // final price per cart
    unsigned short empire_city_id;  // trader's origin city
    unsigned char storage_id;       // where trade took place
    unsigned char month;            // 1-12
    unsigned char resource_id;      // resource_type can be converted back and forth
    unsigned short trader_id;       // !! ACTUAL f->trader_id, not the f->id !!
    signed char quantity;           // amount traded at this price - negative for exports, positive for imports
} transaction_t; // 12 bytes babyyyyy

typedef struct {
    int year; // one trade ledger dataset per year, then archive and reset. 
    int transactions; // number of transactions for the year - used for transaction history
    // transactions count not yet wired - implement with history
    int stock[RESOURCE_MAX]; // in stock at the end of the year

    int imported[RESOURCE_MAX];  // cartloads, unsigned
    int exported[RESOURCE_MAX];  // cartloads, unsigned

    int produced[RESOURCE_MAX];  // cartloads, unsigned
    int consumed[RESOURCE_MAX];  // units(!), unsigned
    int balance[RESOURCE_MAX];   // in denarii, signed
} trade_ledger_data; //at the end of the year, archive this data. Saves should store up to 7 years

typedef struct {
    struct {
        int taxes;
        int exports;
        int donated;
        int total;
    } income;
    struct {
        int imports;
        int wages;
        int construction;
        int interest;
        int salary;
        int sundries;
        int tribute;
        int total;
        int levies;
    } expenses;
    int net_in_out;
    int balance;
} finance_overview;

int city_finance_treasury(void);

void city_finance_treasury_add(int amount);

void city_finance_treasury_add_miscellaneous(int amount);

int city_finance_out_of_money(void);

int city_finance_tax_percentage(void);

void city_finance_change_tax_percentage(int change);

void city_finance_set_tax_percentage(int new_rate);

int city_finance_percentage_taxed_people(void);

int city_finance_estimated_tax_income(void);

int city_finance_estimated_wages(void);

void city_finance_process_import(int price);

void city_finance_process_export(int price);

void city_finance_process_cheat(void);

void city_finance_process_console(int amount);

void city_finance_process_stolen(int stolen);

void city_finance_process_donation(int amount);

void city_finance_process_sundry(int cost);

void city_finance_process_construction(int cost);

void city_finance_update_interest(void);

void city_finance_update_salary(void);

void city_finance_calculate_totals(void);

void city_finance_estimate_wages(void);

void city_finance_estimate_taxes(void);

void city_finance_handle_month_change(void);

/** @brief Adds produced resources to the trade ledger.
* @param resource The resource type that was produced. */
void city_finance_trade_ledger_add_produced(resource_type resource);

/** @brief Adds consumed resources to the trade ledger. Also covers ceasar's requests.
* @param resource The resource type that was consumed.
* @param quantity The quantity of the resource that was consumed, measured in units, not cartloads!
*                 This is due to industry often consuming less than a full cartload. */
void city_finance_trade_ledger_add_consumed(resource_type resource, int quantity);

void city_finance_trade_ledger_add_imported(resource_type resource);

void city_finance_trade_ledger_add_exported(resource_type resource);

void city_finance_trade_ledger_add_balance(resource_type resource, int balance);

int city_finance_trade_ledger_get_produced(resource_type resource, int years_ago);

/** @brief Gets consumed resources from the trade ledger.
* @param resource The resource type that was consumed.
* @param years_ago How many years ago to get the data for.
* @return The quantity of the resource that was consumed, measured in cartloads. */
int city_finance_trade_ledger_get_consumed(resource_type resource, int years_ago);

int city_finance_trade_ledger_get_imported(resource_type resource, int years_ago);

int city_finance_trade_ledger_get_exported(resource_type resource, int years_ago);

int city_finance_trade_ledger_get_balance(resource_type resource, int years_ago);

int city_finance_trade_ledger_get_stock(resource_type resource, int years_ago);

void city_finance_handle_year_change(void);

void city_finance_record_trade_into_ledger(unsigned short trader_id, int price, unsigned short empire_city_id,
     unsigned char storage_id, unsigned char month, resource_type resource, unsigned char is_import);

int city_finance_tourism_income_last_month(void);

int city_finance_tourism_lowest_factor(void);

const finance_overview *city_finance_overview_last_year(void);

const finance_overview *city_finance_overview_this_year(void);

const finance_overview *city_finance_overview_for_year(int years_ago);

int city_finance_overview_years_stored(void);

void city_finance_ledger_init(void);

int city_finance_spawn_tourist(void);

void city_finance_ledger_save_state(buffer *buf);

void city_finance_ledger_load_state(buffer *buf, savegame_version_t version);

#endif // CITY_FINANCE_H
