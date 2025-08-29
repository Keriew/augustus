#ifndef GAME_SAVE_VERSION_H
#define GAME_SAVE_VERSION_H

/************************************************SAVEGAME GUIDE*******************************************************

1.  Savegame versions are used to determine the save and load functions order of reading and writing data.
    A new data added to the savegame can be completely compatible with order versions of the code, but because the
    writing and loading instructions change, the savegame version must be updated to prevent bricking the saves.
2.  Versions are hexadecimal numbers, so the first version is 0x00, the second is 0x01, etc. If you areunsure what is
    going to be the next version, you can check the next hex online. When increasing the save version, you need to
    increase the SAVE_GAME_CURRENT_VERSION by 1, and add a version that best describes the save before your change, e.g.:
       If you have added a new building, a version could be 'SAVE_GAME_LAST_NO_NEW_BUILDING', or
       If you changed a behaviour, a version could be 'SAVE_GAME_LAST_OLD_BEHAVIOUR'.
3.  New properties of structures are not automatically included in the savegame process, so you need to add them deliberately.
    If you are unsure, it's best to add them after all the other properties, unless you know a reason why it would be better
    to include them earlier.
    Properties need to be added in two places: Loading and Saving functions. These are usually located in the same file
    as the .c file of structure, e.g. building state functions are in src/building/state.c; figure functions are in src/figure/figure.c.
4.  When adding new properties to these functions, you may need to update the buffer size used in the function, which is
    defined on top of the file, e.g. FIGURE_ORIGINAL_BUFFER_SIZE or FIGURE_CURRENT_BUFFER_SIZE.
    *_ORIGINAL_BUFFER_SIZE is used to maintain backward compatibility with old savegames, so it shouldn't be changed.
    Safest approach is to increase the size of the CURRENT buffer by the size of the new property e.g.
    if you are adding a uint32_t (32 bits = 4 bytes) property, you should increase the buffer size by 4.
    some structures, like building/state.h even define helper macros for each version of the buffer. It's the standard
    we should strive to follow where possible!
5.  Remember that the order and size of reading and writing properties in the savegame must be the same, so if you are
    adding a buffer_write_u8(buf, b->new_property), you must also add a buffer_read_u8(buf) in the loading function,
    in the exact same order.
6.  In the loading functions, you need to check if the savegame version is high enough to include the new property,
    usually done by if(version > SAVE_GAME_LAST_OLD_PROPERTY) { buffer_read_u8(buf); } else { ... };
    { ... } needs to include logic of handling the case when the property is not present,
    e.g. setting it to 0 or some default value. If a function can set this to a correct value during the loading,
    use that. If the previous value is not compatible, a migration function needs to written and used.
7.  If you are updating the data type of an already existing property, make sure that the new data type is large enough
    to hold the data, e.g. uint16_t can hold values from 0 to 65535, while uint8_t can only hold values from 0 to 255.
8.  It's not always necessary to update the savegame version when adding new properties. Check the structure that you
    are modifying to see if there are unusued properties that can be used instead, or if there are properties that are
    not used for certain types of structure. e.g. non-combat figures don't use 'wait_ticks_next_target', so to avoid
    adding a new property, this property was used to store number of ticks to wait before changing destination for cartpushers.
    If are using a property in non-original way, please make sure to leave a comment explaining the change.

If you are unsure about anything regarding the savegame versioning, please ask on github or discord.
**********************************************************************************************************************/

typedef enum {

    SAVE_GAME_CURRENT_VERSION = 0xa8,

    SAVE_GAME_LAST_ORIGINAL_LIMITS_VERSION = 0x66,
    SAVE_GAME_LAST_SMALLER_IMAGE_ID_VERSION = 0x76,
    SAVE_GAME_LAST_NO_DELIVERIES_VERSION = 0x77,
    SAVE_GAME_LAST_STATIC_VERSION = 0x78,
    SAVE_GAME_LAST_JOINED_IMPORT_EXPORT_VERSION = 0x79,
    SAVE_GAME_LAST_STATIC_BUILDING_COUNT_VERSION = 0x80,
    SAVE_GAME_LAST_STATIC_MONUMENT_DELIVERIES_VERSION = 0x81,
    SAVE_GAME_LAST_STORED_IMAGE_IDS = 0x83,
    // SAVE_GAME_INCREASE_GRANARY_CAPACITY shall be updated if we decide to change granary capacity again.
    SAVE_GAME_INCREASE_GRANARY_CAPACITY = 0x85,
    SAVE_GAME_ROADBLOCK_DATA_MOVED_FROM_SUBTYPE = 0x86,
    SAVE_GAME_LAST_ORIGINAL_TERRAIN_DATA_SIZE_VERSION = 0x86,
    SAVE_GAME_LAST_CARAVANSERAI_WRONG_OFFSET = 0x87,
    SAVE_GAME_LAST_ZIP_COMPRESSION = 0x88,
    SAVE_GAME_LAST_ENEMY_ARMIES_BUFFER_BUG = 0x89,
    SAVE_GAME_LAST_BARRACKS_TOWER_SENTRY_REQUEST = 0x8a,
    // SAVE_GAME_LAST_WITHOUT_HIGHWAYS = 0x8b, no actual changes to how games are saved. Crudelios just wants this here 
    SAVE_GAME_LAST_UNVERSIONED_SCENARIOS = 0x8c,
    SAVE_GAME_LAST_EMPIRE_RESOURCES_ALWAYS_WRITE = 0x8d,
    // the difference between this version and UNVERSIONED_SCENARIOS above is this one actually saves the scenario version
    // in the data, whereas the previous one did a lookup based on the save version
    SAVE_GAME_LAST_NO_SCENARIO_VERSION = 0x8e,
    SAVE_GAME_LAST_UNKNOWN_UNUSED_CITY_DATA = 0x8f,
    SAVE_GAME_LAST_STATIC_RESOURCES = 0x90,
    SAVE_GAME_LAST_GLOBAL_BUILDING_INFO = 0x91,
    SAVE_GAME_LAST_NO_GOLD_AND_MINTING = 0x92,
    SAVE_GAME_LAST_STATIC_SCENARIO_OBJECTS = 0x93,
    SAVE_GAME_LAST_NO_EXTENDED_REQUESTS = 0x94,
    SAVE_GAME_LAST_NO_EVENTS = 0x95,
    SAVE_GAME_LAST_NO_CUSTOM_MESSAGES = 0x96,
    SAVE_GAME_LAST_NO_CART_DEPOT = 0x97,
    SAVE_GAME_LAST_NO_NEW_MONUMENT_RESOURCES = 0x98,
    SAVE_GAME_LAST_MONUMENT_TYPE_DATA = 0x99,
    SAVE_GAME_LAST_NO_CUSTOM_VARIABLES = 0x9a,
    SAVE_GAME_LAST_WRONG_SCENARIO_END_OFFSET = 0x9b,
    SAVE_GAME_LAST_NO_CUSTOM_EMPIRE_MAP_IMAGE = 0x9c,
    SAVE_GAME_LAST_NO_CUSTOM_CAMPAIGNS = 0x9d,
    SAVE_GAME_LAST_STATIC_SCENARIO_ORIGINAL_DATA = 0x9e,
    SAVE_GAME_LAST_NO_LATRINES = 0x9f,
    SAVE_GAME_LAST_SPRITE_BRIDGES = 0xa0,
    SAVE_GAME_LAST_SPRITE_BRIDGES_MIGRATION_FIX = 0xa1,
    SAVE_GAME_LAST_NO_ALT_NATIVE_HUTS = 0xa2,
    SAVE_GAME_LAST_NO_EXTRA_NATIVE_BUILDINGS = 0xa3,
    SAVE_GAME_LAST_STORAGE_STATE_AND_QUANTITY_TOGETHER = 0xa4,
    SAVE_GAME_LAST_10_LEGIONS_MAX = 0xa5,
    SAVE_GAME_LAST_GRANARY_WAREHOUSE_NON_ROADBLOCKS = 0xa6,
    SAVE_GAME_LAST_NO_VISIBLE_CUSTOM_VARIABLES = 0xa7
} savegame_version_t;

typedef enum {
    SCENARIO_CURRENT_VERSION = 19,

    SCENARIO_VERSION_NONE = 0,
    SCENARIO_LAST_UNVERSIONED = 1,
    SCENARIO_LAST_NO_WAGE_LIMITS = 2,
    SCENARIO_LAST_EMPIRE_OBJECT_BUFFERS = 3,
    SCENARIO_LAST_EMPIRE_RESOURCES_U8 = 4,
    SCENARIO_LAST_EMPIRE_RESOURCES_ALWAYS_WRITE = 5,
    SCENARIO_LAST_NO_SAVE_VERSION_WRITE = 6,
    SCENARIO_LAST_NO_STATIC_RESOURCES = 7,
    SCENARIO_LAST_NO_DYNAMIC_OBJECTS = 8,
    SCENARIO_LAST_NO_EXTENDED_REQUESTS = 9,
    SCENARIO_LAST_NO_EVENTS = 10,
    SCENARIO_LAST_NO_CUSTOM_MESSAGES = 11,
    SCENARIO_LAST_NO_CUSTOM_VARIABLES = 12,
    SCENARIO_LAST_WRONG_END_OFFSET = 13,
    SCENARIO_LAST_NO_CUSTOM_EMPIRE_MAP_IMAGE = 14,
    SCENARIO_LAST_STATIC_ORIGINAL_DATA = 15,
    SCENARIO_LAST_NO_ALT_NATIVE_HUTS = 16,
    SCENARIO_LAST_NO_EXTRA_NATIVE_BUILDINGS = 17,
    SCENARIO_LAST_NO_VISIBLE_CUSTOM_VARIABLES = 18
} scenario_version_t;

typedef enum {
    SAVEGAME_STATUS_NEWER_VERSION = -1,
    SAVEGAME_STATUS_INVALID = 0,
    SAVEGAME_STATUS_OK = 1
} savegame_load_status;

#endif // GAME_SAVE_VERSION_H
