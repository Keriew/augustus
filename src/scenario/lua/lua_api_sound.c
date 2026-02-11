#include "lua/lua.h"
#include "lua/lauxlib.h"

#include "sound/effect.h"
#include "sound/music.h"

// sound.play_effect(id)
static int l_sound_play_effect(lua_State *L)
{
    sound_effect_type effect = (sound_effect_type) luaL_checkinteger(L, 1);
    if (effect >= 0 && effect < SOUND_EFFECT_MAX) {
        sound_effect_play(effect);
    }
    return 0;
}

// sound.play_music(track)
static int l_sound_play_music(lua_State *L)
{
    int track = (int) luaL_checkinteger(L, 1);
    sound_music_play_track_by_id(track);
    return 0;
}

static const luaL_Reg sound_funcs[] = {
    {"play_effect", l_sound_play_effect},
    {"play_music", l_sound_play_music},
    {NULL, NULL}
};

void lua_api_sound_register(lua_State *L)
{
    luaL_newlib(L, sound_funcs);

    // SOUND_EFFECT constants
    lua_newtable(L);
    lua_pushinteger(L, SOUND_EFFECT_EXPLOSION);
    lua_setfield(L, -2, "EXPLOSION");
    lua_pushinteger(L, SOUND_EFFECT_FANFARE);
    lua_setfield(L, -2, "FANFARE");
    lua_pushinteger(L, SOUND_EFFECT_FANFARE_URGENT);
    lua_setfield(L, -2, "FANFARE_URGENT");
    lua_pushinteger(L, SOUND_EFFECT_ARROW);
    lua_setfield(L, -2, "ARROW");
    lua_pushinteger(L, SOUND_EFFECT_SWORD);
    lua_setfield(L, -2, "SWORD");
    lua_pushinteger(L, SOUND_EFFECT_CLUB);
    lua_setfield(L, -2, "CLUB");
    lua_pushinteger(L, SOUND_EFFECT_MARCHING);
    lua_setfield(L, -2, "MARCHING");
    lua_pushinteger(L, SOUND_EFFECT_WOLF_HOWL);
    lua_setfield(L, -2, "WOLF_HOWL");
    lua_pushinteger(L, SOUND_EFFECT_FIRE_SPLASH);
    lua_setfield(L, -2, "FIRE_SPLASH");
    lua_pushinteger(L, SOUND_EFFECT_FORMATION_SHIELD);
    lua_setfield(L, -2, "FORMATION_SHIELD");
    lua_pushinteger(L, SOUND_EFFECT_ELEPHANT);
    lua_setfield(L, -2, "ELEPHANT");
    lua_pushinteger(L, SOUND_EFFECT_LION_ATTACK);
    lua_setfield(L, -2, "LION_ATTACK");
    lua_pushinteger(L, SOUND_EFFECT_BALLISTA_SHOOT);
    lua_setfield(L, -2, "BALLISTA_SHOOT");
    lua_setfield(L, -2, "EFFECT");

    // MUSIC_TRACK constants
    lua_newtable(L);
    lua_pushinteger(L, 1);
    lua_setfield(L, -2, "CITY_1");
    lua_pushinteger(L, 2);
    lua_setfield(L, -2, "CITY_2");
    lua_pushinteger(L, 3);
    lua_setfield(L, -2, "CITY_3");
    lua_pushinteger(L, 4);
    lua_setfield(L, -2, "CITY_4");
    lua_pushinteger(L, 5);
    lua_setfield(L, -2, "CITY_5");
    lua_pushinteger(L, 6);
    lua_setfield(L, -2, "COMBAT_SHORT");
    lua_pushinteger(L, 7);
    lua_setfield(L, -2, "COMBAT_LONG");
    lua_setfield(L, -2, "TRACK");

    lua_setglobal(L, "sound");
}
