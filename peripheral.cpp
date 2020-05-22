#include "peripheral.hpp"
#include <terminal/SDLTerminal.hpp>
#include <SDL2/SDL.h>

peripheral::~peripheral(){}

Computer *(*_startComputer)(int) = NULL;
void* (*_queueTask)(std::function<void*(void*)>, void*, bool) = NULL;
Computer *(*getComputerById)(int) = NULL;
int selectedRenderer;
extern std::unordered_map<int, std::unordered_map<uint8_t, std::unordered_map<int, block> > > world;

#include <regex>
#include <unordered_set>
#include <cstring>

std::unordered_set<Computer*> freedComputers;
std::list<turtle_userdata*> allocated_userdata;

int turtle::turnOn(lua_State *L) {return 0;}

int turtle::isOn(lua_State *L) {
    lua_pushboolean(L, freedComputers.find(comp) == freedComputers.end());
    return 1;
}

int turtle::getID(lua_State *L) {
    if (freedComputers.find(comp) != freedComputers.end()) return 0;
    lua_pushinteger(L, comp->id);
    return 1;
}

int turtle::shutdown(lua_State *L) {
    if (freedComputers.find(comp) != freedComputers.end()) return 0;
    comp->running = 0;
    return 0;
}

int turtle::reboot(lua_State *L) {
    if (freedComputers.find(comp) != freedComputers.end()) return 0;
    comp->running = 2;
    return 0;
}

int turtle::getLabel(lua_State *L) {
    if (freedComputers.find(comp) != freedComputers.end()) return 0;
    lua_pushlstring(L, comp->config.label.c_str(), comp->config.label.length());
    return 1;
}

int turtle::setBlock(lua_State *L) {
    int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2), z = luaL_checkinteger(L, 3);
    std::string type = luaL_checkstring(L, 4);
    if (!lua_isnoneornil(L, 5) && !lua_istable(L, 5)) luaL_error(L, "bad argument #5 (expected string or nil, got %s)", lua_typename(L, lua_type(L, 5)));
    if (type.empty()) type = "minecraft:air";
    else if (type.find(":") == std::string::npos) type = "minecraft:" + type;
    if (y < 0 || y > 255) luaL_error(L, "bad argument #2 (y out of range)");
    if (world.find(x) == world.end()) world.insert(std::make_pair(x, std::unordered_map<uint8_t, std::unordered_map<int, block> >()));
    std::unordered_map<uint8_t, std::unordered_map<int, block> >& yz = world[x];
    if (yz.find(y) == yz.end()) yz.insert(std::make_pair(y, std::unordered_map<int, block>()));
    std::unordered_map<int, block>& zz = yz[y];
    if (zz.find(z) == zz.end()) zz.insert(std::make_pair(z, block()));
    zz[z].id = type;
    if (lua_istable(L, 5)) {
        lua_pushvalue(L, 5);
        lua_pushnil(L);
        for (int i = 0; lua_next(L, -2); i++) {
            lua_pushvalue(L, -2);
            if (lua_isnumber(L, -2)) zz[z].state[lua_tostring(L, -1)] = std::to_string(lua_tointeger(L, -2));
            else zz[z].state[lua_tostring(L, -1)] = lua_tostring(L, -2);
            lua_pop(L, 2);
        }
        lua_pop(L, 1);
    }
    return 0;
}

int turtle::setSlotContents(lua_State *L) {
    turtle_userdata * data = (turtle_userdata*)comp->userdata[TURTLE_PLUGIN_USERDATA_KEY];
    int slot = luaL_checkinteger(L, 1), count = luaL_checkinteger(L, 3);
    std::string item = luaL_checkstring(L, 2);
    if (slot < 1 || slot > 16) luaL_error(L, "bad argument #1 (slot out of range)");
    else if (count < 0 || count > 64) luaL_error(L, "bad argument #3 (item count out of range)");
    if (count == 0) item.clear();
    else if (item.empty()) count = 0;
    if (item.find(":") == std::string::npos) item = "minecraft:" + item;
    data->inventory[slot-1].id = item;
    data->inventory[slot-1].count = count;
    return 0;
}

int turtle::teleport(lua_State *L) {
    turtle_userdata * data = (turtle_userdata*)comp->userdata[TURTLE_PLUGIN_USERDATA_KEY];
    int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2), z = luaL_checkinteger(L, 3);
    if (y < 0 || y > 255) luaL_error(L, "bad argument #2 (y out of range)");
    data->x = x;
    data->y = y;
    data->z = z;
    return 0;
}

int turtle::getLocation(lua_State *L) {
    turtle_userdata * data = (turtle_userdata*)comp->userdata[TURTLE_PLUGIN_USERDATA_KEY];
    lua_pushinteger(L, data->x);
    lua_pushinteger(L, data->y);
    lua_pushinteger(L, data->z);
    return 3;
}

turtle::turtle(lua_State *L, const char * side) {
    if (strlen(side) < 8 || std::string(side).substr(0, 7) != "turtle_" || (strlen(side) > 7 && !std::all_of(side + 7, side + strlen(side), ::isdigit))) 
        throw std::invalid_argument("\"side\" parameter must be in the form of turtle_[0-9]+");
    int id = atoi(&side[7]);
    comp = getComputerById(id);
    if (comp == NULL) comp = (Computer*)_queueTask([ ](void* arg)->void*{return _startComputer(*(int*)arg);}, &id, false);
    if (comp == NULL) throw std::runtime_error("Failed to open computer");
    turtle_userdata * data = new turtle_userdata;
    comp->userdata.insert(std::make_pair(TURTLE_PLUGIN_USERDATA_KEY, data));
    comp->userdata_destructors.insert(std::make_pair(TURTLE_PLUGIN_USERDATA_KEY, [](Computer*comp, int key, void* value){delete (turtle_userdata*)value;}));
    if (selectedRenderer == 0) ((SDLTerminal*)comp->term)->resizeWholeWindow(39, 13);
    thiscomp = get_comp(L);
    comp->referencers.push_back(thiscomp);
}

turtle::~turtle() {
    if (thiscomp->peripherals_mutex.try_lock()) thiscomp->peripherals_mutex.unlock();
    else return;
    for (auto it = comp->referencers.begin(); it != comp->referencers.end(); it++) {
        if (*it == thiscomp) {
            it = comp->referencers.erase(it);
            if (it == comp->referencers.end()) break;
        }
    }
}

int turtle::call(lua_State *L, const char * method) {
    std::string m(method);
    if (m == "turnOn") return turnOn(L);
    else if (m == "shutdown") return shutdown(L);
    else if (m == "reboot") return reboot(L);
    else if (m == "getID") return getID(L);
    else if (m == "isOn") return isOn(L);
    else if (m == "getLabel") return getLabel(L);
    else if (m == "setBlock") return setBlock(L);
    else if (m == "setSlotContents") return setSlotContents(L);
    else if (m == "teleport" || m == "setPosition") return teleport(L);
    else if (m == "getLocation") return getLocation(L);
    else return 0;
}

const char * computer_keys[11] = {
    "turnOn",
    "shutdown",
    "reboot",
    "getID",
    "isOn",
    "getLabel",
    "setBlock",
    "setSlotContents",
    "setPosition",
    "teleport",
    "getLocation"
};

library_t turtle::methods = {"computer", 11, computer_keys, NULL, nullptr, nullptr};