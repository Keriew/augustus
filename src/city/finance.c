#include "finance.h"

#include "building/building.h"
#include "building/count.h"
#include "building/highway_station.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/buildings.h"
#include "city/data_private.h"
#include "city/culture.h"
#include "city/festival.h"
#include "city/resource.h"
#include "core/array.h"
#include "core/buffer.h"
#include "core/calc.h"
#include "core/log.h"
#include "core/random.h"
#include "empire/trade_prices.h"
#include "game/difficulty.h"
#include "game/time.h"
#include "figuretype/entertainer.h"
#include "map/data.h"
#include "map/terrain.h"

#define MAX_HOUSE_LEVELS 20
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static building_levy_for_type building_levies[] = {
    {BUILDING_FORT_ARCHERS, FORT_LEVY_MONTHLY},
    {BUILDING_FORT_LEGIONARIES, FORT_LEVY_MONTHLY},
    {BUILDING_FORT_JAVELIN, FORT_LEVY_MONTHLY},
    {BUILDING_FORT_MOUNTED, FORT_LEVY_MONTHLY},
    {BUILDING_FORT_AUXILIA_INFANTRY, FORT_LEVY_MONTHLY},
    {BUILDING_SMALL_TEMPLE_CERES, SMALL_TEMPLE_LEVY_MONTHLY },
    {BUILDING_SMALL_TEMPLE_NEPTUNE, SMALL_TEMPLE_LEVY_MONTHLY },
    {BUILDING_SMALL_TEMPLE_MERCURY, SMALL_TEMPLE_LEVY_MONTHLY },
    {BUILDING_SMALL_TEMPLE_MARS, SMALL_TEMPLE_LEVY_MONTHLY },
    {BUILDING_SMALL_TEMPLE_VENUS, SMALL_TEMPLE_LEVY_MONTHLY },//10
    {BUILDING_LARGE_TEMPLE_CERES, LARGE_TEMPLE_LEVY_MONTHLY },
    {BUILDING_LARGE_TEMPLE_NEPTUNE, LARGE_TEMPLE_LEVY_MONTHLY },
    {BUILDING_LARGE_TEMPLE_MERCURY, LARGE_TEMPLE_LEVY_MONTHLY },
    {BUILDING_LARGE_TEMPLE_MARS, LARGE_TEMPLE_LEVY_MONTHLY },
    {BUILDING_LARGE_TEMPLE_VENUS, LARGE_TEMPLE_LEVY_MONTHLY },
    {BUILDING_ORACLE, SMALL_TEMPLE_LEVY_MONTHLY },
    {BUILDING_TOWER, TOWER_LEVY_MONTHLY },
    {BUILDING_LIGHTHOUSE, LIGHTHOUSE_LEVY_MONTHLY },
    {BUILDING_GRAND_TEMPLE_CERES, GRAND_TEMPLE_LEVY_MONTHLY},
    {BUILDING_GRAND_TEMPLE_NEPTUNE, GRAND_TEMPLE_LEVY_MONTHLY},//20
    {BUILDING_GRAND_TEMPLE_MERCURY, GRAND_TEMPLE_LEVY_MONTHLY},
    {BUILDING_GRAND_TEMPLE_MARS, GRAND_TEMPLE_LEVY_MONTHLY},
    {BUILDING_GRAND_TEMPLE_VENUS, GRAND_TEMPLE_LEVY_MONTHLY},
    {BUILDING_PANTHEON, PANTHEON_LEVY_MONTHLY},
    {BUILDING_COLOSSEUM, COLOSSEUM_LEVY_MONTHLY},
    {BUILDING_HIPPODROME, HIPPODROME_LEVY_MONTHLY},
    {BUILDING_SMALL_MAUSOLEUM, SMALL_MAUSOLEUM_LEVY_MONTHLY},
    {BUILDING_LARGE_MAUSOLEUM, SMALL_MAUSOLEUM_LEVY_MONTHLY},
    {BUILDING_NYMPHAEUM, SMALL_TEMPLE_LEVY_MONTHLY},
    {BUILDING_CARAVANSERAI, CARAVANSERAI_LEVY_MONTHLY },//30
};

static tourism_for_type tourism_modifiers[] = {
    {BUILDING_TAVERN, 2, TAVERN_COVERAGE, 0},
    {BUILDING_THEATER, 1, THEATER_COVERAGE, 0},
    {BUILDING_AMPHITHEATER, 1, AMPHITHEATER_COVERAGE, 0},
    {BUILDING_ARENA, 2, ARENA_COVERAGE, 0},
    {BUILDING_COLOSSEUM, 4, 0, 0},
    {BUILDING_HIPPODROME, 5, 0, 0},
    {BUILDING_GRAND_TEMPLE_CERES, 3, 0, 0},
    {BUILDING_GRAND_TEMPLE_NEPTUNE, 3, 0, 0},
    {BUILDING_GRAND_TEMPLE_MERCURY, 3, 0, 0},
    {BUILDING_GRAND_TEMPLE_MARS, 3, 0, 0},
    {BUILDING_GRAND_TEMPLE_VENUS, 3, 0, 0},
    {BUILDING_PANTHEON, 3, 0, 0}
};

#define TRANSACTION_STEP_SIZE 100
#define FINANCE_OVERVIEW_HISTORY_YEARS 6

static trade_ledger_data trade_ledgers[8]; // 7 years of data + current year
static short trade_ledgers_count;
static finance_overview finance_overviews[FINANCE_OVERVIEW_HISTORY_YEARS];
static unsigned char finance_overview_years_stored;
static array(transaction_t) current_year_transactions; // array of transaction structs for the current year
static array(transaction_t) last_year_transactions; // array of transaction structs for the last year
// transaction histories are only stored for current and last year - throw in a joke to explain 'why' to the players
// we could store more but i dont want crudelios to blame me for the savegame bloat

static void trade_ledger_year_change(void);
static void archive_last_year_finance_overview(void);


int city_finance_treasury(void)
{
    return city_data.finance.treasury;
}

void city_finance_treasury_add(int amount)
{
    city_data.finance.treasury += amount;
}

void city_finance_treasury_add_miscellaneous(int amount)
{
    city_finance_treasury_add(amount);
    city_data.finance.misc_this_year += amount;
}

int city_finance_out_of_money(void)
{
    return city_data.finance.treasury <= -5000;
}

int city_finance_tax_percentage(void)
{
    return city_data.finance.tax_percentage;
}

void city_finance_change_tax_percentage(int change)
{
    city_finance_set_tax_percentage(city_data.finance.tax_percentage + change);
}

void city_finance_set_tax_percentage(int new_rate)
{
    city_data.finance.tax_percentage = calc_bound(new_rate, 0, 25);
}

int city_finance_percentage_taxed_people(void)
{
    return city_data.taxes.percentage_taxed_people;
}

int city_finance_estimated_tax_income(void)
{
    return city_data.finance.estimated_tax_income;
}

int city_finance_estimated_wages(void)
{
    return city_data.finance.estimated_wages;
}

void city_finance_process_import(int price)
{
    city_data.finance.treasury -= price;
    city_data.finance.this_year.expenses.imports += price;
}

void city_finance_process_export(int price)
{
    city_data.finance.treasury += price;
    city_data.finance.this_year.income.exports += price;
    if (city_data.religion.neptune_trade_bonus_active) {
        city_data.finance.treasury += price / 2;
        city_data.finance.this_year.income.exports += price / 2;
    }
}

void city_finance_process_cheat(void)
{
    if (city_data.finance.treasury < 5000) {
        city_data.finance.treasury += 1000;
        city_data.finance.cheated_money += 1000;
    }
}

void city_finance_process_console(int amount)
{
    city_data.finance.treasury += amount;
    city_data.finance.cheated_money += amount;
}

void city_finance_process_stolen(int stolen)
{
    city_data.finance.stolen_this_year += stolen;
    city_finance_process_sundry(stolen);
}

void city_finance_process_donation(int amount)
{
    city_data.finance.treasury += amount;
    city_data.finance.this_year.income.donated += amount;
}

void city_finance_process_sundry(int cost)
{
    city_data.finance.treasury -= cost;
    city_data.finance.this_year.expenses.sundries += cost;
}

void city_finance_process_construction(int cost)
{
    city_data.finance.treasury -= cost;
    city_data.finance.this_year.expenses.construction += cost;
}

void city_finance_update_interest(void)
{
    city_data.finance.this_year.expenses.interest = city_data.finance.interest_so_far;
}

void city_finance_update_salary(void)
{
    city_data.finance.this_year.expenses.salary = city_data.finance.salary_so_far;
}

void city_finance_calculate_totals(void)
{
    finance_overview *this_year = &city_data.finance.this_year;
    this_year->income.total =
        this_year->income.donated +
        this_year->income.taxes +
        this_year->income.exports +
        city_data.finance.misc_this_year;

    this_year->expenses.total =
        this_year->expenses.sundries +
        this_year->expenses.salary +
        this_year->expenses.interest +
        this_year->expenses.construction +
        this_year->expenses.wages +
        this_year->expenses.levies +
        this_year->expenses.imports;

    finance_overview *last_year = &city_data.finance.last_year;
    last_year->net_in_out = last_year->income.total - last_year->expenses.total;
    this_year->net_in_out = this_year->income.total - this_year->expenses.total;
    this_year->balance = last_year->balance + this_year->net_in_out;

    this_year->expenses.tribute = 0;
}

void city_finance_estimate_wages(void)
{
    int monthly_wages = city_data.labor.wages * city_data.labor.workers_employed / 10 / 12;
    city_data.finance.this_year.expenses.wages = city_data.finance.wages_so_far;
    city_data.finance.estimated_wages = (12 - game_time_month()) * monthly_wages + city_data.finance.wages_so_far;
}

void city_finance_estimate_taxes(void)
{
    city_data.taxes.monthly.collected_plebs = 0;
    city_data.taxes.monthly.collected_patricians = 0;
    for (building_type type = BUILDING_HOUSE_SMALL_TENT; type <= BUILDING_HOUSE_LUXURY_PALACE; type++) {
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state == BUILDING_STATE_IN_USE && b->house_size && b->house_tax_coverage) {
                int is_patrician = b->subtype.house_level >= HOUSE_SMALL_VILLA;
                int trm = difficulty_adjust_money(model_get_house(b->subtype.house_level)->tax_multiplier);
                if (is_patrician) {
                    city_data.taxes.monthly.collected_patricians += b->house_population * trm;
                } else {
                    city_data.taxes.monthly.collected_plebs += b->house_population * trm;
                }
            }
        }
    }
    int monthly_patricians = calc_adjust_with_percentage(
        city_data.taxes.monthly.collected_patricians / 2,
        city_data.finance.tax_percentage);
    int monthly_plebs = calc_adjust_with_percentage(
        city_data.taxes.monthly.collected_plebs / 2,
        city_data.finance.tax_percentage);
    int estimated_rest_of_year = (12 - game_time_month()) * (monthly_patricians + monthly_plebs);

    city_data.finance.this_year.income.taxes =
        city_data.taxes.yearly.collected_plebs + city_data.taxes.yearly.collected_patricians;
    city_data.finance.estimated_tax_income = city_data.finance.this_year.income.taxes + estimated_rest_of_year;
}

static void collect_monthly_taxes(void)
{
    city_data.taxes.taxed_plebs = 0;
    city_data.taxes.taxed_patricians = 0;
    city_data.taxes.untaxed_plebs = 0;
    city_data.taxes.untaxed_patricians = 0;
    city_data.taxes.monthly.uncollected_plebs = 0;
    city_data.taxes.monthly.collected_plebs = 0;
    city_data.taxes.monthly.uncollected_patricians = 0;
    city_data.taxes.monthly.collected_patricians = 0;

    for (int i = 0; i < MAX_HOUSE_LEVELS; i++) {
        city_data.population.at_level[i] = 0;
    }
    for (building_type type = BUILDING_HOUSE_SMALL_TENT; type <= BUILDING_HOUSE_LUXURY_PALACE; type++) {
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE || !b->house_size) {
                continue;
            }

            int is_patrician = b->subtype.house_level >= HOUSE_SMALL_VILLA;
            int population = b->house_population;
            int trm = difficulty_adjust_money(model_get_house(b->subtype.house_level)->tax_multiplier);
            city_data.population.at_level[b->subtype.house_level] += population;

            int tax = population * trm;
            if (b->house_tax_coverage) {
                if (is_patrician) {
                    city_data.taxes.taxed_patricians += population;
                    city_data.taxes.monthly.collected_patricians += tax;
                } else {
                    city_data.taxes.taxed_plebs += population;
                    city_data.taxes.monthly.collected_plebs += tax;
                }
                b->tax_income_or_storage += tax;
            } else {
                if (is_patrician) {
                    city_data.taxes.untaxed_patricians += population;
                    city_data.taxes.monthly.uncollected_patricians += tax;
                } else {
                    city_data.taxes.untaxed_plebs += population;
                    city_data.taxes.monthly.uncollected_plebs += tax;
                }
            }
        }
    }

    int collected_patricians = calc_adjust_with_percentage(
        city_data.taxes.monthly.collected_patricians / 2,
        city_data.finance.tax_percentage);
    int collected_plebs = calc_adjust_with_percentage(
        city_data.taxes.monthly.collected_plebs / 2,
        city_data.finance.tax_percentage);
    int collected_total = collected_patricians + collected_plebs;

    city_data.taxes.yearly.collected_patricians += collected_patricians;
    city_data.taxes.yearly.collected_plebs += collected_plebs;
    city_data.taxes.yearly.uncollected_patricians += calc_adjust_with_percentage(
        city_data.taxes.monthly.uncollected_patricians / 2,
        city_data.finance.tax_percentage);
    city_data.taxes.yearly.uncollected_plebs += calc_adjust_with_percentage(
        city_data.taxes.monthly.uncollected_plebs / 2,
        city_data.finance.tax_percentage);

    city_data.finance.treasury += collected_total;

    int total_patricians = city_data.taxes.taxed_patricians + city_data.taxes.untaxed_patricians;
    int total_plebs = city_data.taxes.taxed_plebs + city_data.taxes.untaxed_plebs;
    city_data.taxes.percentage_taxed_patricians = calc_percentage(city_data.taxes.taxed_patricians, total_patricians);
    city_data.taxes.percentage_taxed_plebs = calc_percentage(city_data.taxes.taxed_plebs, total_plebs);
    city_data.taxes.percentage_taxed_people = calc_percentage(
        city_data.taxes.taxed_patricians + city_data.taxes.taxed_plebs,
        total_patricians + total_plebs);
}

static void pay_monthly_wages(void)
{
    int wages = city_data.labor.wages * city_data.labor.workers_employed / 10 / 12;
    city_data.finance.treasury -= wages;
    city_data.finance.wages_so_far += wages;
    city_data.finance.wage_rate_paid_this_year += city_data.labor.wages;
}

static void pay_monthly_interest(void)
{
    if (city_data.finance.treasury < 0) {
        int interest = calc_adjust_with_percentage(-city_data.finance.treasury, 10) / 12;
        city_data.finance.treasury -= interest;
        city_data.finance.interest_so_far += interest;
    }
}

static void pay_monthly_salary(void)
{
    if (!city_finance_out_of_money()) {
        city_data.finance.salary_so_far += city_data.emperor.salary_amount;
        city_data.emperor.personal_savings += city_data.emperor.salary_amount;
        city_data.finance.treasury -= city_data.emperor.salary_amount;
    }
}

static void pay_monthly_building_levies(void)
{
    int levies = 0;
    for (int i = 0; i < (int) ARRAY_SIZE(building_levies); i++) {
        building_type type = building_levies[i].type;
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            b->monthly_levy = building_levies[i].amount;
            int levy = building_get_levy(b);
            levies += levy;
        }
    }

    int num_highway_tiles = 0;
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
                num_highway_tiles++;
            }
        }
    }
    int highway_tax = num_highway_tiles / 4 * HIGHWAY_LEVY_MONTHLY;
    if (city_buildings_has_working_highway_station()) {
        highway_tax /= 2;
    }
    levies += highway_tax;

    city_data.finance.treasury -= levies;
    city_data.finance.this_year.expenses.levies += levies;
}

static void activate_monthly_tourism(void)
{
    for (int i = 0; i < (int) ARRAY_SIZE(tourism_modifiers); i++) {
        building_type type = tourism_modifiers[i].type;
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE || !b->num_workers) {
                continue;
            }
            b->is_tourism_venue = 1;
            if (game_time_month() == 0) {
                b->tourism_income_this_year = 0;
            }
            tourism_modifiers[i].count++;
            // disable redundant venues for tourism
            if ((tourism_modifiers[i].count * tourism_modifiers[i].coverage) > city_data.population.population) {
                b->tourism_disabled = 1;
                b->tourism_income = 0;
            } else {
                b->tourism_disabled = 0;
                b->tourism_income = tourism_modifiers[i].income_modifier;
            }
        }
    }
}

void city_finance_handle_month_change(void)
{
    collect_monthly_taxes();
    activate_monthly_tourism();
    pay_monthly_wages();
    pay_monthly_interest();
    pay_monthly_salary();
    pay_monthly_building_levies();
    building_highway_station_consume_monthly();
}

static void reset_taxes(void)
{
    city_data.finance.last_year.income.taxes =
        city_data.taxes.yearly.collected_plebs + city_data.taxes.yearly.collected_patricians;
    city_data.taxes.yearly.collected_plebs = 0;
    city_data.taxes.yearly.collected_patricians = 0;
    city_data.taxes.yearly.uncollected_plebs = 0;
    city_data.taxes.yearly.uncollected_patricians = 0;

    // reset tax income in building list
    for (building_type type = BUILDING_HOUSE_SMALL_TENT; type <= BUILDING_HOUSE_LUXURY_PALACE; type++) {
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state == BUILDING_STATE_IN_USE && b->house_size) {
                b->tax_income_or_storage = 0;
            }
        }
    }
}

static void copy_amounts_to_last_year(void)
{
    finance_overview *last_year = &city_data.finance.last_year;
    finance_overview *this_year = &city_data.finance.this_year;

    // wages
    last_year->expenses.wages = city_data.finance.wages_so_far;
    city_data.finance.wages_so_far = 0;
    city_data.finance.wage_rate_paid_last_year = city_data.finance.wage_rate_paid_this_year;
    city_data.finance.wage_rate_paid_this_year = 0;

    //levies
    last_year->expenses.levies = this_year->expenses.levies;
    this_year->expenses.levies = 0;

    // import/export
    last_year->income.exports = this_year->income.exports;
    this_year->income.exports = 0;
    last_year->expenses.imports = this_year->expenses.imports;
    this_year->expenses.imports = 0;

    // construction
    last_year->expenses.construction = this_year->expenses.construction;
    this_year->expenses.construction = 0;

    // interest
    last_year->expenses.interest = city_data.finance.interest_so_far;
    city_data.finance.interest_so_far = 0;

    // salary
    city_data.finance.last_year.expenses.salary = city_data.finance.salary_so_far;
    city_data.finance.salary_so_far = 0;

    // sundries
    last_year->expenses.sundries = this_year->expenses.sundries;
    this_year->expenses.sundries = 0;
    city_data.finance.stolen_last_year = city_data.finance.stolen_this_year;
    city_data.finance.stolen_this_year = 0;

    // donations
    last_year->income.donated = this_year->income.donated;
    this_year->income.donated = 0;

    //tourism
    city_data.finance.misc_last_year = city_data.finance.misc_this_year;
    city_data.finance.misc_this_year = 0;
}

static void pay_tribute(void)
{
    finance_overview *last_year = &city_data.finance.last_year;
    int income =
        last_year->income.donated +
        last_year->income.taxes +
        last_year->income.exports +
        city_data.finance.misc_last_year;

    int expenses =
        last_year->expenses.sundries +
        last_year->expenses.salary +
        last_year->expenses.interest +
        last_year->expenses.construction +
        last_year->expenses.wages +
        last_year->expenses.levies +
        last_year->expenses.imports;

    city_data.finance.tribute_not_paid_last_year = 0;
    if (city_data.finance.treasury <= 0) {
        // city is in debt
        city_data.finance.tribute_not_paid_last_year = 1;
        city_data.finance.tribute_not_paid_total_years++;
        last_year->expenses.tribute = 0;
    } else if (income <= expenses) {
        // city made a loss: fixed tribute based on population
        city_data.finance.tribute_not_paid_total_years = 0;
        if (city_data.population.population > 2000) {
            last_year->expenses.tribute = 200;
        } else if (city_data.population.population > 1000) {
            last_year->expenses.tribute = 100;
        } else {
            last_year->expenses.tribute = 0;
        }
    } else {
        // city made a profit: tribute is max of: 25% of profit, fixed tribute based on population
        city_data.finance.tribute_not_paid_total_years = 0;
        if (city_data.population.population > 5000) {
            last_year->expenses.tribute = 500;
        } else if (city_data.population.population > 3000) {
            last_year->expenses.tribute = 400;
        } else if (city_data.population.population > 2000) {
            last_year->expenses.tribute = 300;
        } else if (city_data.population.population > 1000) {
            last_year->expenses.tribute = 225;
        } else if (city_data.population.population > 500) {
            last_year->expenses.tribute = 150;
        } else {
            last_year->expenses.tribute = 50;
        }
        int pct_profit = calc_adjust_with_percentage(income - expenses, 25);
        if (pct_profit > last_year->expenses.tribute) {
            last_year->expenses.tribute = pct_profit;
        }
    }

    city_data.finance.treasury -= last_year->expenses.tribute;
    city_data.finance.this_year.expenses.tribute = 0;

    last_year->balance = city_data.finance.treasury;
    last_year->income.total = income;
    last_year->expenses.total = last_year->expenses.tribute + expenses;
}

void city_finance_trade_ledger_add_produced(resource_type resource)
{
    trade_ledgers[0].produced[resource]++;
    return;
}

void city_finance_trade_ledger_add_consumed(resource_type resource, int quantity)
{
    trade_ledgers[0].consumed[resource] += quantity;
    return;
}

void city_finance_trade_ledger_add_imported(resource_type resource)
{
    trade_ledgers[0].imported[resource]++;
    return;
}

void city_finance_trade_ledger_add_exported(resource_type resource)
{
    trade_ledgers[0].exported[resource]++;
    return;
}

void city_finance_trade_ledger_add_balance(resource_type resource, int balance)
{
    trade_ledgers[0].balance[resource] += balance;
    return;
}

int city_finance_trade_ledger_get_produced(resource_type resource, int years_ago)
{
    if (years_ago < 0 || years_ago > trade_ledgers_count) {
        return 0;
    }
    return trade_ledgers[years_ago].produced[resource];
}

int city_finance_trade_ledger_get_consumed(resource_type resource, int years_ago)
{
    if (years_ago < 0 || years_ago > trade_ledgers_count) {
        return 0;
    }
    int consumed = trade_ledgers[years_ago].consumed[resource];
    consumed = consumed < RESOURCE_ONE_LOAD ? consumed : consumed / RESOURCE_ONE_LOAD;
    return consumed; // cartloads
}

int city_finance_trade_ledger_get_imported(resource_type resource, int years_ago)
{
    if (years_ago < 0 || years_ago > trade_ledgers_count) {
        return 0;
    }
    return trade_ledgers[years_ago].imported[resource];
}

int city_finance_trade_ledger_get_exported(resource_type resource, int years_ago)
{
    if (years_ago < 0 || years_ago > trade_ledgers_count) {
        return 0;
    }
    return trade_ledgers[years_ago].exported[resource];
}

int city_finance_trade_ledger_get_balance(resource_type resource, int years_ago)
{
    if (years_ago < 0 || years_ago > trade_ledgers_count) {
        return 0;
    }
    return trade_ledgers[years_ago].balance[resource];
}

int city_finance_trade_ledger_get_stock(resource_type resource, int years_ago)
{
    if (years_ago < 0 || years_ago > trade_ledgers_count) {
        return 0;
    }
    // update from city resource - no point in duplicating the efforts
    if (years_ago == 0) {
        trade_ledgers[0].stock[resource] = city_resource_get_total_amount(resource, 0);
    }
    return trade_ledgers[years_ago].stock[resource];
}

void city_finance_handle_year_change(void)
{
    archive_last_year_finance_overview();
    reset_taxes();
    trade_ledger_year_change();
    copy_amounts_to_last_year();
    pay_tribute();

}

static int get_transaction_index(unsigned short trader_id, int price, unsigned short empire_city_id,
     unsigned char storage_id, unsigned char month, resource_type resource, unsigned char is_import)
{
    // O(n) instead of O(1) like array_item, but it shouldn't be called often enough to matter at all.
    // If game lags during transactions, then the answer is storing the id in the transaction_t.
    // small increase in structure size, but still completely manageable

    unsigned char resource_id = (unsigned char) resource;
    // find the transaction in the current year transactions array that matches the given parameters
    for (unsigned int i = 0; i < current_year_transactions.size; i++) {
        transaction_t *tx = array_item(current_year_transactions, i);
        unsigned char tx_is_import = tx->quantity < 0 ? 0 : 1;
        if (tx->trader_id == trader_id &&
            tx->month == month &&
            tx->resource_id == resource_id &&
            tx->price == price &&
            tx->storage_id == storage_id &&
            tx->empire_city_id == empire_city_id &&
            tx_is_import == is_import) {
            return i;
        }
    }
    return -1;
}

void city_finance_record_trade_into_ledger(unsigned short trader_id, int price, unsigned short empire_city_id,
     unsigned char storage_id, unsigned char month, resource_type resource, unsigned char is_import)
{
    int transaction_index = get_transaction_index(trader_id, price, empire_city_id, storage_id, month, resource, is_import);
    if (transaction_index >= 0) {
        // there's already a transaction that matches the parameters this year
        transaction_t *tx = array_item(current_year_transactions, transaction_index);
        tx->quantity += is_import ? 1 : -1;
    } else {
        // new transaction - add to the array
        transaction_t new_transaction = {
            .trader_id = trader_id,
            .price = price,
            .empire_city_id = empire_city_id,
            .storage_id = storage_id,
            .month = month,
            .resource_id = (unsigned char) resource,
            .quantity = is_import ? -1 : 1
        };
        transaction_t *tx;
        array_new_item(current_year_transactions, tx);
        *tx = new_transaction;
    }
}

static int transfer_transactions_to_last_year(void) // AI solution for array transfer
{
    // 1) Clear destination first (your required sequence)
    array_clear(last_year_transactions);

    // 2) Initialize destination container
    if (!array_init(last_year_transactions, TRANSACTION_STEP_SIZE,
        current_year_transactions.constructor, current_year_transactions.in_use)) {
        return 0;
    }

    // 3) Copy payload
    if (current_year_transactions.size > 0) {
        if (!array_expand(last_year_transactions, current_year_transactions.size)) {
            array_clear(last_year_transactions);
            return 0;
        }

        for (unsigned int i = 0; i < current_year_transactions.size; i++) {
            memcpy(array_item(last_year_transactions, i),
                   array_item(current_year_transactions, i),
                   sizeof(*array_item(current_year_transactions, i)));
        }
        last_year_transactions.size = current_year_transactions.size;
    }

    // 4) Clear source only after successful copy
    array_clear(current_year_transactions);
    return 1;
}

static void trade_ledger_year_change(void)
{
    // EOD 22/06/2026 notes
    // next time - connect the production, consumption, import and export - DONE
    // import and export - decide if directly tied to transactions array or not - DONE, not directly, separate counts
    // add save/load for transactions array and ledger asap for testing -todo
    // pull the data from the ledger to the display and check if behaves as expected -todo
    // for display use lang_text_draw_month_year_max_width() - draws month year

    for (int i = 0; i < RESOURCE_MAX; i++) {
        trade_ledgers[0].stock[i] = city_resource_get_total_amount((resource_type) i, 0);
    }

    if (trade_ledgers_count < 7) {
        trade_ledgers_count++;
    }

    for (int i = trade_ledgers_count; i > 0; i--) {
        trade_ledgers[i] = trade_ledgers[i - 1];
    }

    memset(&trade_ledgers[0], 0, sizeof(trade_ledgers[0]));
    trade_ledgers[0].year = game_time_year();

    if (!transfer_transactions_to_last_year()) {
        log_error("Failed to transfer transactions to last year. Game will probably crash.", 0, 0);
        return;
    }

    if (!array_init(current_year_transactions, TRANSACTION_STEP_SIZE, 0, 0)) {
        log_error("Failed to allocate memory for current year transactions. Game will probably crash.", 0, 0);
        return;
    }
}

static void archive_last_year_finance_overview(void)
{
    if (finance_overview_years_stored >= FINANCE_OVERVIEW_HISTORY_YEARS) {
        finance_overview_years_stored = FINANCE_OVERVIEW_HISTORY_YEARS;
    } else if (trade_ledgers_count > 0 || finance_overview_years_stored > 0) {
        finance_overview_years_stored++;
    } else {
        return;
    }

    for (int i = finance_overview_years_stored - 1; i > 0; i--) {
        finance_overviews[i] = finance_overviews[i - 1];
    }

    finance_overviews[0] = city_data.finance.last_year;
}

int city_finance_tourism_income_last_month(void)
{
    return city_data.finance.tourism_last_month;
}

int city_finance_tourism_lowest_factor(void)
{
    return city_data.finance.tourism_lowest_factor;
}

const finance_overview *city_finance_overview_last_year(void)
{
    return &city_data.finance.last_year;
}

const finance_overview *city_finance_overview_this_year(void)
{
    return &city_data.finance.this_year;
}

const finance_overview *city_finance_overview_for_year(int years_ago)
{
    if (years_ago <= 0) {
        return &city_data.finance.this_year;
    }
    if (years_ago == 1) {
        return &city_data.finance.last_year;
    }
    if (years_ago - 2 < finance_overview_years_stored) {
        return &finance_overviews[years_ago - 2];
    }
    return 0;
}

int city_finance_overview_years_stored(void)
{
    return finance_overview_years_stored;
}

void city_finance_ledger_init(void)
{
    trade_ledgers_count = 0;
    memset(trade_ledgers, 0, sizeof(trade_ledgers));
    finance_overview_years_stored = 0;
    memset(finance_overviews, 0, sizeof(finance_overviews));
    array_init(current_year_transactions, TRANSACTION_STEP_SIZE, 0, 0);
    array_init(last_year_transactions, TRANSACTION_STEP_SIZE, 0, 0);
}

static size_t finance_overview_saved_size(void)
{
    return sizeof(int32_t) * 15;
}

static void write_finance_overview(buffer *buf, const finance_overview *overview)
{
    buffer_write_i32(buf, overview->income.taxes);
    buffer_write_i32(buf, overview->income.exports);
    buffer_write_i32(buf, overview->income.donated);
    buffer_write_i32(buf, overview->income.total);
    buffer_write_i32(buf, overview->expenses.imports);
    buffer_write_i32(buf, overview->expenses.wages);
    buffer_write_i32(buf, overview->expenses.construction);
    buffer_write_i32(buf, overview->expenses.interest);
    buffer_write_i32(buf, overview->expenses.salary);
    buffer_write_i32(buf, overview->expenses.sundries);
    buffer_write_i32(buf, overview->expenses.tribute);
    buffer_write_i32(buf, overview->expenses.total);
    buffer_write_i32(buf, overview->expenses.levies);
    buffer_write_i32(buf, overview->net_in_out);
    buffer_write_i32(buf, overview->balance);
}

static void read_finance_overview(buffer *buf, finance_overview *overview)
{
    overview->income.taxes = buffer_read_i32(buf);
    overview->income.exports = buffer_read_i32(buf);
    overview->income.donated = buffer_read_i32(buf);
    overview->income.total = buffer_read_i32(buf);
    overview->expenses.imports = buffer_read_i32(buf);
    overview->expenses.wages = buffer_read_i32(buf);
    overview->expenses.construction = buffer_read_i32(buf);
    overview->expenses.interest = buffer_read_i32(buf);
    overview->expenses.salary = buffer_read_i32(buf);
    overview->expenses.sundries = buffer_read_i32(buf);
    overview->expenses.tribute = buffer_read_i32(buf);
    overview->expenses.total = buffer_read_i32(buf);
    overview->expenses.levies = buffer_read_i32(buf);
    overview->net_in_out = buffer_read_i32(buf);
    overview->balance = buffer_read_i32(buf);
}

void city_finance_ledger_save_state(buffer *buf)
{
    size_t current_count = current_year_transactions.size;
    size_t last_count = last_year_transactions.size;
    size_t tx_size = 4 + 2 + 1 + 1 + 1 + 2 + 1; // 12 bytes per transaction
    size_t total_size = sizeof(int16_t) // trade_ledgers_count
        + 8 * (sizeof(int32_t) * 2 + sizeof(int32_t) * RESOURCE_MAX * 6) // 8 ledger entries
        + sizeof(int32_t) + current_count * tx_size // current year transactions
        + sizeof(int32_t) + last_count * tx_size    // last year transactions
        + sizeof(uint8_t) + FINANCE_OVERVIEW_HISTORY_YEARS * finance_overview_saved_size();

    buffer_init_dynamic(buf, total_size);

    buffer_write_i16(buf, trade_ledgers_count);
    for (int i = 0; i < 8; i++) {
        buffer_write_i32(buf, trade_ledgers[i].year);
        buffer_write_i32(buf, trade_ledgers[i].transactions);
        for (int r = 0; r < RESOURCE_MAX; r++) { buffer_write_i32(buf, trade_ledgers[i].stock[r]); }
        for (int r = 0; r < RESOURCE_MAX; r++) { buffer_write_i32(buf, trade_ledgers[i].imported[r]); }
        for (int r = 0; r < RESOURCE_MAX; r++) { buffer_write_i32(buf, trade_ledgers[i].exported[r]); }
        for (int r = 0; r < RESOURCE_MAX; r++) { buffer_write_i32(buf, trade_ledgers[i].produced[r]); }
        for (int r = 0; r < RESOURCE_MAX; r++) { buffer_write_i32(buf, trade_ledgers[i].consumed[r]); }
        for (int r = 0; r < RESOURCE_MAX; r++) { buffer_write_i32(buf, trade_ledgers[i].balance[r]); }
    }

    buffer_write_i32(buf, (int32_t) current_count);
    transaction_t *tx;
    array_foreach(current_year_transactions, tx)
    {
        buffer_write_i32(buf, tx->price);
        buffer_write_u16(buf, tx->empire_city_id);
        buffer_write_u8(buf, tx->storage_id);
        buffer_write_u8(buf, tx->month);
        buffer_write_u8(buf, tx->resource_id);
        buffer_write_u16(buf, tx->trader_id);
        buffer_write_i8(buf, tx->quantity);
    }

    buffer_write_i32(buf, (int32_t) last_count);
    array_foreach(last_year_transactions, tx)
    {
        buffer_write_i32(buf, tx->price);
        buffer_write_u16(buf, tx->empire_city_id);
        buffer_write_u8(buf, tx->storage_id);
        buffer_write_u8(buf, tx->month);
        buffer_write_u8(buf, tx->resource_id);
        buffer_write_u16(buf, tx->trader_id);
        buffer_write_i8(buf, tx->quantity);
    }

    buffer_write_u8(buf, finance_overview_years_stored);
    for (int i = 0; i < FINANCE_OVERVIEW_HISTORY_YEARS; i++) {
        write_finance_overview(buf, &finance_overviews[i]);
    }
}

void city_finance_ledger_load_state(buffer *buf, savegame_version_t version)
{
    buffer_load_dynamic(buf);

    trade_ledgers_count = buffer_read_i16(buf);
    for (int i = 0; i < 8; i++) {
        trade_ledgers[i].year = buffer_read_i32(buf);
        trade_ledgers[i].transactions = buffer_read_i32(buf);
        for (int r = 0; r < RESOURCE_MAX; r++) { trade_ledgers[i].stock[r] = buffer_read_i32(buf); }
        for (int r = 0; r < RESOURCE_MAX; r++) { trade_ledgers[i].imported[r] = buffer_read_i32(buf); }
        for (int r = 0; r < RESOURCE_MAX; r++) { trade_ledgers[i].exported[r] = buffer_read_i32(buf); }
        for (int r = 0; r < RESOURCE_MAX; r++) { trade_ledgers[i].produced[r] = buffer_read_i32(buf); }
        for (int r = 0; r < RESOURCE_MAX; r++) { trade_ledgers[i].consumed[r] = buffer_read_i32(buf); }
        for (int r = 0; r < RESOURCE_MAX; r++) { trade_ledgers[i].balance[r] = buffer_read_i32(buf); }
    }

    array_clear(current_year_transactions);
    int count = buffer_read_i32(buf);
    if (!array_init(current_year_transactions, TRANSACTION_STEP_SIZE, 0, 0) ||
        !array_expand(current_year_transactions, count)) {
        log_error("Failed to allocate memory for current year transactions during load.", 0, 0);
        return;
    }
    for (int i = 0; i < count; i++) {
        transaction_t *tx = array_next(current_year_transactions);
        tx->price = buffer_read_i32(buf);
        tx->empire_city_id = buffer_read_u16(buf);
        tx->storage_id = buffer_read_u8(buf);
        tx->month = buffer_read_u8(buf);
        tx->resource_id = buffer_read_u8(buf);
        tx->trader_id = buffer_read_u16(buf);
        tx->quantity = buffer_read_i8(buf);
    }

    array_clear(last_year_transactions);
    count = buffer_read_i32(buf);
    if (!array_init(last_year_transactions, TRANSACTION_STEP_SIZE, 0, 0) ||
        !array_expand(last_year_transactions, count)) {
        log_error("Failed to allocate memory for last year transactions during load.", 0, 0);
        return;
    }
    for (int i = 0; i < count; i++) {
        transaction_t *tx = array_next(last_year_transactions);
        tx->price = buffer_read_i32(buf);
        tx->empire_city_id = buffer_read_u16(buf);
        tx->storage_id = buffer_read_u8(buf);
        tx->month = buffer_read_u8(buf);
        tx->resource_id = buffer_read_u8(buf);
        tx->trader_id = buffer_read_u16(buf);
        tx->quantity = buffer_read_i8(buf);
    }

    finance_overview_years_stored = 0;
    memset(finance_overviews, 0, sizeof(finance_overviews));
    if (version <= SAVE_GAME_LAST_NO_FINANCE_OVERVIEW_HISTORY) {
        return;
    }

    finance_overview_years_stored = buffer_read_u8(buf);
    if (finance_overview_years_stored > FINANCE_OVERVIEW_HISTORY_YEARS) {
        finance_overview_years_stored = FINANCE_OVERVIEW_HISTORY_YEARS;
    }
    for (int i = 0; i < FINANCE_OVERVIEW_HISTORY_YEARS; i++) {
        read_finance_overview(buf, &finance_overviews[i]);
    }
}

int city_finance_spawn_tourist(void)
{
    if (!city_festival_games_active()) {
        return 0;
    }
    int tick_increase = random_byte() % city_data.ratings.culture;
    city_data.finance.tourist_spawn_delay += tick_increase;
    if (city_data.finance.tourist_spawn_delay > 500) {
        figure_spawn_tourist();
        city_data.finance.tourist_spawn_delay = 0;
    }

    return 1;
}
