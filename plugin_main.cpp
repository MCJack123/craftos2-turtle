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
#include <functional>
#include <json/json.h>
#include <cstdio>
#include "peripheral.hpp"

#define PLUGIN_VERSION 2

void bad_argument(lua_State *L, const char * type, int pos) {
    luaL_error(L, "bad argument #%d (expected %s, got %s)", pos, type, lua_typename(L, lua_type(L, pos)));
}

extern luaL_reg turtle_lib[];
extern Computer *(*_startComputer)(int);
extern void* (*_queueTask)(std::function<void*(void*)>, void*, bool);
extern Computer *(*getComputerById)(int);
extern int selectedRenderer;
extern Json::Value itemDatabase;
extern Json::Value blockDatabase;
extern Json::Value recipeDatabase;
extern const char itemDatabaseData[];
extern const unsigned int itemDatabaseData_len;
extern const char blockDatabaseData[];
extern const unsigned int blockDatabaseData_len;
extern const char recipeDatabaseData[];
extern const unsigned int recipeDatabaseData_len;

void justParseTheJson(const char * str, unsigned int len, Json::Value &root) {
    Json::CharReaderBuilder builder;
    JSONCPP_STRING err;
    const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (!reader->parse(str, str + len, &root, &err))
        printf("Error parsing JSON: %s\n", err.c_str());
}

int registerCapability(lua_State *L) {
    std::string m(lua_tostring(L, 2));
    if (m == "registerPeripheral") ((void(*)(std::string, peripheral_init))lua_touserdata(L, 1))("turtle", &turtle::init);
    else if (m == "startComputer") _startComputer = (Computer*(*)(int))lua_touserdata(L, 1);
    else if (m == "queueTask") _queueTask = (void*(*)(std::function<void*(void*)>, void*, bool))lua_touserdata(L, 1);
    else if (m == "getComputerById") getComputerById = (Computer*(*)(int))lua_touserdata(L, 1);
    else if (m == "selectedRenderer") selectedRenderer = lua_tointeger(L, 1);
    return 0;
}

extern "C" {
#ifdef _WIN32
_declspec(dllexport)
#endif
int luaopen_turtle(lua_State *L) {
    if (!itemDatabase) justParseTheJson(itemDatabaseData, itemDatabaseData_len, itemDatabase);
    if (!blockDatabase) justParseTheJson(blockDatabaseData, blockDatabaseData_len, blockDatabase);
    if (!recipeDatabase) justParseTheJson(recipeDatabaseData, recipeDatabaseData_len, recipeDatabase);
    Computer * comp = get_comp(L);
    if (comp->userdata.find(TURTLE_PLUGIN_USERDATA_KEY) != comp->userdata.end()) luaL_register(L, "turtle", turtle_lib);
    else lua_pushnil(L);
    return 1;
}

#ifdef _WIN32
_declspec(dllexport)
#endif
int plugin_info(lua_State *L) {
    if (!lua_isstring(L, 1)) return 0; // check for CraftOS-PC v2.3.2 or later, which adds a version string
    lua_newtable(L);
    lua_pushinteger(L, PLUGIN_VERSION);
    lua_setfield(L, -2, "version");
    lua_pushcfunction(L, registerCapability);
    lua_setfield(L, -2, "register_registerPeripheral");
    lua_pushcfunction(L, registerCapability);
    lua_setfield(L, -2, "register_startComputer");
    lua_pushcfunction(L, registerCapability);
    lua_setfield(L, -2, "register_queueTask");
    lua_pushcfunction(L, registerCapability);
    lua_setfield(L, -2, "register_getComputerById");
    lua_pushcfunction(L, registerCapability);
    lua_setfield(L, -2, "get_selectedRenderer");
    return 1;
}
}