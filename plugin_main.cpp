/*
 * peripheral_base.cpp
 * CraftOS-PC 2
 * 
 * This file can be used as a template for new peripheral plugins.
 */

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include <Computer.hpp>
#include "peripheral.hpp"

#define PLUGIN_VERSION 2

void bad_argument(lua_State *L, const char * type, int pos) {
    luaL_error(L, "bad argument #%d (expected %s, got %s)", pos, type, lua_typename(L, lua_type(L, pos)));
}

extern luaL_reg turtle_lib[];
extern Computer *(*_startComputer)(int);

extern "C" {
#ifdef _WIN32
_declspec(dllexport)
#endif
int luaopen_turtle(lua_State *L) {
    Computer * comp = get_comp(L);
    if (comp->userdata.find(TURTLE_PLUGIN_USERDATA_KEY) != comp->userdata.end()) luaL_register(L, "turtle", turtle_lib);
    else lua_pushnil(L);
    return 1;
}

int register_registerPeripheral(lua_State *L) {
    ((void(*)(std::string, peripheral_init))lua_touserdata(L, 1))("turtle", &turtle::init); 
    return 0;
}

int register_startComputer(lua_State *L) {
    _startComputer = (Computer*(*)(int))lua_touserdata(L, 1);
    return 0;
}

#ifdef _WIN32
_declspec(dllexport)
#endif
int plugin_info(lua_State *L) {
    lua_newtable(L);
    lua_pushinteger(L, PLUGIN_VERSION);
    lua_setfield(L, -2, "version");
    lua_pushcfunction(L, register_registerPeripheral);
    lua_setfield(L, -2, "register_registerPeripheral");
    lua_pushcfunction(L, register_startComputer);
    lua_setfield(L, -2, "register_startComputer");
    return 1;
}
}