extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}
#include "peripheral.hpp"
#include <chrono>
#include <thread>
#include <unordered_map>
#include <json/json.h>

#define MAXIMUM_TURTLE_FUEL 20000

#define turtle_getdata() Computer * computer = get_comp(L);\
    if (computer->userdata.find(TURTLE_PLUGIN_USERDATA_KEY) == computer->userdata.end()) luaL_error(L, "Attempted to use turtle API on non-turtle");\
    turtle_userdata * data = (turtle_userdata*)computer->userdata[TURTLE_PLUGIN_USERDATA_KEY];
#define turtle_failure(msg) {lua_pushboolean(L, false); lua_pushstring(L, msg); return 2;}
#define turtle_success() lua_pushboolean(L, true); return 1;
#define check_upgrade(up, msg) if (data->leftUpgrade != turtle_userdata::upgrade::up && data->rightUpgrade != turtle_userdata::upgrade::up) turtle_failure(msg);
#define turtle_movement_wait() {std::this_thread::sleep_until(data->nextMovementTime); data->nextMovementTime = std::chrono::system_clock::now() + std::chrono::milliseconds(400);}
#define STUB turtle_failure("Not implemented")

std::unordered_map<int, std::unordered_map<uint8_t, std::unordered_map<int, block> > > world;

Json::Value itemDatabase;
Json::Value blockDatabase;
Json::Value recipeDatabase;

block getBlockAtPos(int x, uint8_t y, int z) {
    if (world.find(x) == world.end()) return block();
    std::unordered_map<uint8_t, std::unordered_map<int, block> >& yz = world[x];
    if (yz.find(y) == yz.end()) return block();
    std::unordered_map<int, block>& zz = yz[y];
    if (zz.find(z) == zz.end()) return block();
    else return zz[z];
}

void setBlockAtPos(int x, uint8_t y, int z, block b) {
    if (world.find(x) == world.end()) world.insert(std::make_pair(x, std::unordered_map<uint8_t, std::unordered_map<int, block> >()));
    std::unordered_map<uint8_t, std::unordered_map<int, block> >& yz = world[x];
    if (yz.find(y) == yz.end()) yz.insert(std::make_pair(y, std::unordered_map<int, block>()));
    std::unordered_map<int, block>& zz = yz[y];
    if (zz.find(z) == zz.end()) zz.insert(std::make_pair(z, b));
    else zz[z] = b;
}

void getCoordsForDirection(turtle_userdata * data, int& x, uint8_t& y, int& z) {
    x = data->x;
    y = data->y;
    z = data->z;
    switch (data->orientation) {
        case turtle_userdata::direction::north: z--; return;
        case turtle_userdata::direction::south: z++; return;
        case turtle_userdata::direction::east: x++; return;
        case turtle_userdata::direction::west: x--; return;
    }
}

int turtle_craft(lua_State *L) {
    turtle_getdata();
    check_upgrade(workbench, "No crafting table to craft with");
    int maxAmount = luaL_optinteger(L, 1, 64);
    if (maxAmount > 64) maxAmount = 64;
    else if (maxAmount <= 0) {
        lua_pushboolean(L, false);
        return 1;
    }
    // oh god this is going to be slow, is there any better way to do this???
    int inventoryIDs[4][4];
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
        if (data->inventory[y*4+x].id.empty()) inventoryIDs[3-y][x] = 0;
        else for (Json::ValueIterator it = itemDatabase.begin(); it != itemDatabase.end(); it++)
            if (data->inventory[y*4+x].id == "minecraft:" + (*it)["name"].asString()) {
                inventoryIDs[3-y][x] = (*it)["id"].asInt();
                break;
            }
    }
    for (Json::ValueIterator it = recipeDatabase.begin(); it != recipeDatabase.end(); it++) {
        for (Json::ValueIterator it2 = it->begin(); it2 != it->end(); it2++) {
            if (it2->isMember("inShape")) { // shaped recipe
                Json::Value &shape = (*it2)["inShape"];
                bool found = false;
                int starty, startx;
                for (starty = 0; starty < 5 - shape.size(); starty++) {
                    for (startx = 0; startx < 4; startx++) {
                        bool stop_b = false;
                        found = true;
                        for (int y = 0; y < shape.size(); y++) {
                            if (startx + shape[y].size() > 4) {
                                stop_b = true;
                                found = false;
                                break;
                            }
                            bool stop_a = false;
                            for (int x = 0; x < shape[y].size(); x++) {
                                if (shape[y][x].isInt() && shape[y][x].asInt() != inventoryIDs[starty+y][startx+x]) {
                                    stop_a = true;
                                    found = false;
                                    break;
                                }
                            }
                            if (stop_a) break;
                        }
                        if (stop_b || found) break;
                    }
                    if (found) break;
                }
                if (found) {
                    item targetItem;
                    for (Json::ValueIterator it3 = itemDatabase.begin(); it3 != itemDatabase.end(); it3++) {
                        if ((*it3)["id"].asInt() == (*it2)["result"]["id"].asInt()) {
                            targetItem.id = "minecraft:" + (*it3)["name"].asString();
                            break;
                        }
                    }
                    if (targetItem.id.empty()) turtle_failure("Could not find item name from ID");
                    int targetSlot = data->selectedItem-1;
                    if (data->inventory[targetSlot].count && data->inventory[targetSlot].id != targetItem.id) {
                        for (int i = 0; i < 16; i++) {
                            if (data->inventory[i].id.empty() || data->inventory[i].id == targetItem.id) {
                                targetSlot = i;
                                break;
                            }
                        }
                        if (targetSlot == data->selectedItem-1) turtle_failure("No room for result");
                    }
                    bool stop_d = false;
                    turtle_failure("Not implemented yet");
                    //for (targetItem.count = 0; targetItem.count < maxAmount; targetItem.count++) {

                    //}
                }
            } else { // shapeless recipe
                int itemSlots[16];
                int usedSlots = (*it2)["ingredients"].size();
                bool stop_c = false;
                for (int i = 0; i < usedSlots; i++) {
                    itemSlots[i] = -1;
                    for (int y = 0; y < 4; y++) {
                        bool stop_b = false;
                        for (int x = 0; x < 4; x++) {
                            bool stop_a = false;
                            for (int j = 0; j < i; j++) {
                                if (itemSlots[j] == y*4+x) {
                                    stop_a = true;
                                    break;
                                }
                            }
                            if (!stop_a && inventoryIDs[y][x] == (*it2)["ingredients"][y].asInt()) {
                                itemSlots[i] = y*4+x;
                                stop_b = true;
                                break;
                            }
                        }
                        if (stop_b) break;
                    }
                    if (itemSlots[i] == -1) {
                        stop_c = true;
                        break;
                    }
                }
                if (!stop_c) {
                    item targetItem;
                    for (Json::ValueIterator it3 = itemDatabase.begin(); it3 != itemDatabase.end(); it3++) {
                        if ((*it3)["id"].asInt() == (*it2)["result"]["id"].asInt()) {
                            targetItem.id = "minecraft:" + (*it3)["name"].asString();
                            break;
                        }
                    }
                    if (targetItem.id.empty()) turtle_failure("Could not find item name from ID");
                    int targetSlot = data->selectedItem-1;
                    if (data->inventory[targetSlot].count && data->inventory[targetSlot].id != targetItem.id) {
                        for (int i = 0; i < 16; i++) {
                            if (data->inventory[i].id.empty() || data->inventory[i].id == targetItem.id) {
                                targetSlot = i;
                                break;
                            }
                        }
                        if (targetSlot == data->selectedItem-1) turtle_failure("No room for result");
                    }
                    bool stop_d = false;
                    for (targetItem.count = 0; targetItem.count + (*it2)["result"]["count"].asInt() <= maxAmount && (data->inventory[targetSlot].id.empty() || targetItem.count + data->inventory[targetSlot].count + (*it2)["result"]["count"].asInt() <= 64) && !stop_d; targetItem.count += (*it2)["result"]["count"].asInt()) {
                        for (int i = 0; i < usedSlots; i++) {
                            if (--data->inventory[itemSlots[i]].count <= 0) {
                                data->inventory[itemSlots[i]].id.clear();
                                stop_d = true;
                            }
                        }
                    }
                    if (data->inventory[targetSlot].count) data->inventory[targetSlot].count += targetItem.count;
                    else data->inventory[targetSlot] = targetItem;
                    turtle_success();
                }
            }
        }
    }
    turtle_failure("No items to craft");
}

int turtle_forward(lua_State *L) {
    turtle_getdata();
    if (data->fuelLevel == 0) turtle_failure("Out of fuel");
    int x, z;
    uint8_t y;
    getCoordsForDirection(data, x, y, z);
    if (getBlockAtPos(x, y, z).id != "minecraft:air") turtle_failure("Movement obstructed");
    setBlockAtPos(data->x, data->y, data->z, block());
    data->x = x;
    data->z = z;
    setBlockAtPos(data->x, data->y, data->z, block("computercraft:turtle"));
    data->fuelLevel--;
    turtle_movement_wait();
    turtle_success();
}

int turtle_back(lua_State *L) {
    turtle_getdata();
    if (data->fuelLevel == 0) turtle_failure("Out of fuel");
    switch (data->orientation) {
        case turtle_userdata::direction::north:
            if (getBlockAtPos(data->x, data->y, data->z+1).id != "minecraft:air") turtle_failure("Movement obstructed");
            setBlockAtPos(data->x, data->y, data->z, block());
            data->z++; 
            break;
        case turtle_userdata::direction::south:
            if (getBlockAtPos(data->x, data->y, data->z-1).id != "minecraft:air") turtle_failure("Movement obstructed");
            setBlockAtPos(data->x, data->y, data->z, block());
            data->z--; 
            break;
        case turtle_userdata::direction::east:
            if (getBlockAtPos(data->x-1, data->y, data->z).id != "minecraft:air") turtle_failure("Movement obstructed");
            setBlockAtPos(data->x, data->y, data->z, block());
            data->x--; 
            break;
        case turtle_userdata::direction::west:
            if (getBlockAtPos(data->x+1, data->y, data->z).id != "minecraft:air") turtle_failure("Movement obstructed");
            setBlockAtPos(data->x, data->y, data->z, block());
            data->x++; 
            break;
    }
    setBlockAtPos(data->x, data->y, data->z, block("computercraft:turtle"));
    data->fuelLevel--;
    turtle_movement_wait();
    turtle_success();
}

int turtle_up(lua_State *L) {
    turtle_getdata();
    if (data->fuelLevel == 0) turtle_failure("Out of fuel");
    if (data->y == 255) turtle_failure("Too high to move");
    if (getBlockAtPos(data->x, data->y+1, data->z).id != "minecraft:air") turtle_failure("Movement obstructed");
    setBlockAtPos(data->x, data->y, data->z, block());
    data->y++;
    setBlockAtPos(data->x, data->y, data->z, block("computercraft:turtle"));
    data->fuelLevel--;
    turtle_movement_wait();
    turtle_success();
}

int turtle_down(lua_State *L) {
    turtle_getdata();
    if (data->fuelLevel == 0) turtle_failure("Out of fuel");
    if (data->y == 0) turtle_failure("Too low to move");
    if (getBlockAtPos(data->x, data->y-1, data->z).id != "minecraft:air") turtle_failure("Movement obstructed");
    setBlockAtPos(data->x, data->y, data->z, block());
    data->y--;
    setBlockAtPos(data->x, data->y, data->z, block("computercraft:turtle"));
    data->fuelLevel--;
    turtle_movement_wait();
    turtle_success();
}

int turtle_turnLeft(lua_State *L) {
    turtle_getdata();
    switch (data->orientation) {
        case turtle_userdata::direction::north: data->orientation = turtle_userdata::direction::west; break;
        case turtle_userdata::direction::south: data->orientation = turtle_userdata::direction::east; break;
        case turtle_userdata::direction::east: data->orientation = turtle_userdata::direction::north; break;
        case turtle_userdata::direction::west: data->orientation = turtle_userdata::direction::south; break;
    }
    turtle_movement_wait();
    turtle_success();
}

int turtle_turnRight(lua_State *L) {
    turtle_getdata();
    switch (data->orientation) {
        case turtle_userdata::direction::north: data->orientation = turtle_userdata::direction::east; break;
        case turtle_userdata::direction::south: data->orientation = turtle_userdata::direction::west; break;
        case turtle_userdata::direction::east: data->orientation = turtle_userdata::direction::south; break;
        case turtle_userdata::direction::west: data->orientation = turtle_userdata::direction::north; break;
    }
    turtle_movement_wait();
    turtle_success();
}

int turtle_select(lua_State *L) {
    turtle_getdata();
    int pos = luaL_checkinteger(L, 1);
    if (pos <= 0 || pos > 16) luaL_error(L, "Slot number %d out of range", pos);
    data->selectedItem = pos;
    turtle_success();
}

int turtle_getSelectedSlot(lua_State *L) {
    turtle_getdata();
    lua_pushinteger(L, data->selectedItem);
    return 1;
}

int turtle_getItemCount(lua_State *L) {
    turtle_getdata();
    int pos = luaL_optinteger(L, 1, data->selectedItem);
    if (pos <= 0 || pos > 16) luaL_error(L, "Slot number %d out of range", pos);
    lua_pushinteger(L, data->inventory[pos-1].count);
    return 1;
}

int turtle_getItemSpace(lua_State *L) {
    turtle_getdata();
    int pos = luaL_optinteger(L, 1, data->selectedItem);
    if (pos <= 0 || pos > 16) luaL_error(L, "Slot number %d out of range", pos);
    lua_pushinteger(L, 64 - data->inventory[pos-1].count);
    return 1;
}

int turtle_getItemDetail(lua_State *L) {
    turtle_getdata();
    int pos = luaL_optinteger(L, 1, data->selectedItem);
    if (pos <= 0 || pos > 16) luaL_error(L, "Slot number %d out of range", pos);
    if (data->inventory[pos-1].count == 0) lua_pushnil(L);
    else {
        lua_createtable(L, 0, 2);
        lua_pushlstring(L, data->inventory[pos-1].id.c_str(), data->inventory[pos-1].id.length());
        lua_setfield(L, -2, "name");
        lua_pushinteger(L, data->inventory[pos-1].count);
        lua_setfield(L, -2, "count");
    }
    return 1;
}

int turtle_equipLeft(lua_State *L) {
    turtle_getdata();
    turtle_userdata::upgrade newUpgrade;
    if (data->inventory[data->selectedItem-1].count == 0) newUpgrade = turtle_userdata::upgrade::none;
    else if (data->inventory[data->selectedItem-1].id == "computercraft:wireless_modem_normal") newUpgrade = turtle_userdata::upgrade::modem;
    else if (data->inventory[data->selectedItem-1].id == "computercraft:speaker") newUpgrade = turtle_userdata::upgrade::speaker;
    else if (data->inventory[data->selectedItem-1].id == "minecraft:diamond_pickaxe") newUpgrade = turtle_userdata::upgrade::pickaxe;
    else if (data->inventory[data->selectedItem-1].id == "minecraft:diamond_axe") newUpgrade = turtle_userdata::upgrade::axe;
    else if (data->inventory[data->selectedItem-1].id == "minecraft:diamond_sword") newUpgrade = turtle_userdata::upgrade::sword;
    else if (data->inventory[data->selectedItem-1].id == "minecraft:diamond_shovel") newUpgrade = turtle_userdata::upgrade::shovel;
    else if (data->inventory[data->selectedItem-1].id == "minecraft:diamond_hoe") newUpgrade = turtle_userdata::upgrade::hoe;
    else if (data->inventory[data->selectedItem-1].id == "minecraft:crafting_table") newUpgrade = turtle_userdata::upgrade::workbench;
    else turtle_failure("Not a valid upgrade");
    turtle_userdata::upgrade oldUpgrade = data->leftUpgrade;
    data->leftUpgrade = newUpgrade;
    data->inventory[data->selectedItem-1].count = 1;
    switch (oldUpgrade) {
        case turtle_userdata::upgrade::none: data->inventory[data->selectedItem-1].id.clear(); data->inventory[data->selectedItem-1].count = 0; break;
        case turtle_userdata::upgrade::modem: data->inventory[data->selectedItem-1].id = "computercraft:wireless_modem_normal"; break;
        case turtle_userdata::upgrade::speaker: data->inventory[data->selectedItem-1].id = "computercraft:speaker"; break;
        case turtle_userdata::upgrade::pickaxe: data->inventory[data->selectedItem-1].id = "minecraft:diamond_pickaxe"; break;
        case turtle_userdata::upgrade::axe: data->inventory[data->selectedItem-1].id = "minecraft:diamond_axe"; break;
        case turtle_userdata::upgrade::sword: data->inventory[data->selectedItem-1].id = "minecraft:diamond_sword"; break;
        case turtle_userdata::upgrade::shovel: data->inventory[data->selectedItem-1].id = "minecraft:diamond_shovel"; break;
        case turtle_userdata::upgrade::hoe: data->inventory[data->selectedItem-1].id = "minecraft:diamond_hoe"; break;
        case turtle_userdata::upgrade::workbench: data->inventory[data->selectedItem-1].id = "minecraft:crafting_table"; break;
    }
    turtle_success();
}

int turtle_equipRight(lua_State *L) {
    turtle_getdata();
    turtle_userdata::upgrade newUpgrade;
    if (data->inventory[data->selectedItem-1].count == 0) newUpgrade = turtle_userdata::upgrade::none;
    else if (data->inventory[data->selectedItem-1].id == "computercraft:wireless_modem_normal") newUpgrade = turtle_userdata::upgrade::modem;
    else if (data->inventory[data->selectedItem-1].id == "computercraft:speaker") newUpgrade = turtle_userdata::upgrade::speaker;
    else if (data->inventory[data->selectedItem-1].id == "minecraft:diamond_pickaxe") newUpgrade = turtle_userdata::upgrade::pickaxe;
    else if (data->inventory[data->selectedItem-1].id == "minecraft:diamond_axe") newUpgrade = turtle_userdata::upgrade::axe;
    else if (data->inventory[data->selectedItem-1].id == "minecraft:diamond_sword") newUpgrade = turtle_userdata::upgrade::sword;
    else if (data->inventory[data->selectedItem-1].id == "minecraft:diamond_shovel") newUpgrade = turtle_userdata::upgrade::shovel;
    else if (data->inventory[data->selectedItem-1].id == "minecraft:diamond_hoe") newUpgrade = turtle_userdata::upgrade::hoe;
    else if (data->inventory[data->selectedItem-1].id == "minecraft:crafting_table") newUpgrade = turtle_userdata::upgrade::workbench;
    else turtle_failure("Not a valid upgrade");
    turtle_userdata::upgrade oldUpgrade = data->rightUpgrade;
    data->rightUpgrade = newUpgrade;
    data->inventory[data->selectedItem-1].count = 1;
    switch (oldUpgrade) {
        case turtle_userdata::upgrade::none: data->inventory[data->selectedItem-1].id.clear(); data->inventory[data->selectedItem-1].count = 0; break;
        case turtle_userdata::upgrade::modem: data->inventory[data->selectedItem-1].id = "computercraft:wireless_modem_normal"; break;
        case turtle_userdata::upgrade::speaker: data->inventory[data->selectedItem-1].id = "computercraft:speaker"; break;
        case turtle_userdata::upgrade::pickaxe: data->inventory[data->selectedItem-1].id = "minecraft:diamond_pickaxe"; break;
        case turtle_userdata::upgrade::axe: data->inventory[data->selectedItem-1].id = "minecraft:diamond_axe"; break;
        case turtle_userdata::upgrade::sword: data->inventory[data->selectedItem-1].id = "minecraft:diamond_sword"; break;
        case turtle_userdata::upgrade::shovel: data->inventory[data->selectedItem-1].id = "minecraft:diamond_shovel"; break;
        case turtle_userdata::upgrade::hoe: data->inventory[data->selectedItem-1].id = "minecraft:diamond_hoe"; break;
        case turtle_userdata::upgrade::workbench: data->inventory[data->selectedItem-1].id = "minecraft:crafting_table"; break;
    }
    turtle_success();
}

int turtle_attack(lua_State *L) STUB
int turtle_attackUp(lua_State *L) STUB
int turtle_attackDown(lua_State *L) STUB
int turtle_dig(lua_State *L) STUB
int turtle_digUp(lua_State *L) STUB
int turtle_digDown(lua_State *L) STUB
int turtle_place(lua_State *L) STUB
int turtle_placeUp(lua_State *L) STUB
int turtle_placeDown(lua_State *L) STUB
int turtle_detect(lua_State *L) STUB
int turtle_detectUp(lua_State *L) STUB
int turtle_detectDown(lua_State *L) STUB

int turtle_inspect(lua_State *L) {
    turtle_getdata();
    int x, z;
    uint8_t y;
    getCoordsForDirection(data, x, y, z);
    block b = getBlockAtPos(x, y, z);
    if (b.id == "minecraft:air" || b.id.empty()) {
        lua_pushboolean(L, false);
        return 1;
    }
    lua_pushboolean(L, true);
    lua_createtable(L, 0, 2);
    lua_pushstring(L, b.id.c_str());
    lua_setfield(L, -2, "name");
    lua_newtable(L);
    for (auto s : b.state) {
        lua_pushstring(L, s.second.c_str());
        lua_setfield(L, -2, s.first.c_str());
    }
    lua_setfield(L, -2, "state");
    return 2;
}

int turtle_inspectUp(lua_State *L) {
    turtle_getdata();
    if (data->y == 255) {
        lua_pushboolean(L, false);
        return 1;
    }
    block b = getBlockAtPos(data->x, data->y+1, data->z);
    if (b.id == "minecraft:air" || b.id.empty()) {
        lua_pushboolean(L, false);
        return 1;
    }
    lua_pushboolean(L, true);
    lua_createtable(L, 0, 2);
    lua_pushstring(L, b.id.c_str());
    lua_setfield(L, -2, "name");
    lua_newtable(L);
    for (auto s : b.state) {
        lua_pushstring(L, s.second.c_str());
        lua_setfield(L, -2, s.first.c_str());
    }
    lua_setfield(L, -2, "state");
    return 2;
}

int turtle_inspectDown(lua_State *L) {
    turtle_getdata();
    if (data->y == 0) {
        lua_pushboolean(L, false);
        return 1;
    }
    block b = getBlockAtPos(data->x, data->y-1, data->z);
    if (b.id == "minecraft:air" || b.id.empty()) {
        lua_pushboolean(L, false);
        return 1;
    }
    lua_pushboolean(L, true);
    lua_createtable(L, 0, 2);
    lua_pushstring(L, b.id.c_str());
    lua_setfield(L, -2, "name");
    lua_newtable(L);
    for (auto s : b.state) {
        lua_pushstring(L, s.second.c_str());
        lua_setfield(L, -2, s.first.c_str());
    }
    lua_setfield(L, -2, "state");
    return 2;
}

int turtle_compare(lua_State *L) {
    turtle_getdata();
    int x, z;
    uint8_t y;
    getCoordsForDirection(data, x, y, z);
    block b = getBlockAtPos(x, y, z);
    if (b.id == "minecraft:air" || b.id.empty()) lua_pushboolean(L, data->inventory[data->selectedItem-1].id.empty());
    else lua_pushboolean(L, data->inventory[data->selectedItem-1].id == b.id);
    return 1;
}

int turtle_compareUp(lua_State *L) {
    turtle_getdata();
    if (data->y == 255) {
        lua_pushboolean(L, false);
        return 1;
    }
    block b = getBlockAtPos(data->x, data->y+1, data->z);
    if (b.id == "minecraft:air" || b.id.empty()) lua_pushboolean(L, data->inventory[data->selectedItem-1].id.empty());
    else lua_pushboolean(L, data->inventory[data->selectedItem-1].id == b.id);
    return 1;
}

int turtle_compareDown(lua_State *L) {
    turtle_getdata();
    if (data->y == 0) {
        lua_pushboolean(L, false);
        return 1;
    }
    block b = getBlockAtPos(data->x, data->y-1, data->z);
    if (b.id == "minecraft:air" || b.id.empty()) lua_pushboolean(L, data->inventory[data->selectedItem-1].id.empty());
    else lua_pushboolean(L, data->inventory[data->selectedItem-1].id == b.id);
    return 1;
}

int turtle_compareTo(lua_State *L) {
    turtle_getdata();
    int slot = luaL_checkinteger(L, 1);
    if (slot < 1 || slot > 16) luaL_error(L, "Slot number %d out of range", slot);
    lua_pushboolean(L, data->inventory[data->selectedItem-1].id == data->inventory[slot-1].id);
    return 1;
}

int turtle_drop(lua_State *L) STUB
int turtle_dropUp(lua_State *L) STUB
int turtle_dropDown(lua_State *L) STUB
int turtle_suck(lua_State *L) STUB
int turtle_suckUp(lua_State *L) STUB
int turtle_suckDown(lua_State *L) STUB

#define fuelMapForWoodTypes(type, value) {"minecraft:" type, value}, {"minecraft:oak_" type, value}, {"minecraft:spruce_" type, value}, {"minecraft:birch_" type, value}, {"minecraft:jungle_" type, value}, {"minecraft:acacia_" type, value}, {"minecraft:dark_oak_" type, value}
#define fuelMapForColorTypes(type, value) {"minecraft:" type, value}, \
    {"minecraft:white_" type, value}, {"minecraft:orange_" type, value}, {"minecraft:magenta_" type, value}, {"minecraft:light_blue_" type, value}, \
    {"minecraft:yellow_" type, value}, {"minecraft:lime_" type, value}, {"minecraft:pink_" type, value}, {"minecraft:gray_" type, value}, \
    {"minecraft:light_gray_" type, value}, {"minecraft:cyan_" type, value}, {"minecraft:purple_" type, value}, {"minecraft:blue_" type, value}, \
    {"minecraft:brown_" type, value}, {"minecraft:green_" type, value}, {"minecraft:red_" type, value}, {"minecraft:black_" type, value}

std::unordered_map<std::string, unsigned> fuelMap = {
    {"minecraft:bamboo", 2},
    fuelMapForColorTypes("carpet", 3),
    fuelMapForColorTypes("wool", 5),
    {"minecraft:stick", 5},
    fuelMapForWoodTypes("sapling", 5),
    {"minecraft:bowl", 5},
    fuelMapForWoodTypes("door", 10),
    fuelMapForWoodTypes("sign", 10),
    {"minecraft:wooden_sword", 10},
    {"minecraft:wooden_axe", 10},
    {"minecraft:wooden_hoe", 10},
    {"minecraft:wooden_shovel", 10},
    {"minecraft:wooden_pickaxe", 10},
    fuelMapForWoodTypes("button", 5),
    {"minecraft:ladder", 15},
    {"minecraft:fishing_rod", 15},
    {"minecraft:bow", 15},
    fuelMapForWoodTypes("slab", 7),
    fuelMapForColorTypes("banner", 15),
    {"minecraft:red_mushroom_block", 15}, {"minecraft:brown_mushroom_block", 15}, {"minecraft:mushroom_stem", 15},
    {"minecraft:note_block", 15},
    {"minecraft:jukebox", 15},
    {"minecraft:daylight_detector", 15},
    {"minecraft:barrel", 15},
    {"minecraft:trapped_chest", 15},
    {"minecraft:chest", 15},
    {"minecraft:composter", 15},
    {"minecraft:lectern", 15},
    {"minecraft:bookshelf", 15},
    {"minecraft:loom", 15},
    {"minecraft:smithing_table", 15},
    {"minecraft:fletching_table", 15},
    {"minecraft:cartography_table", 15},
    {"minecraft:crafting_table", 15},
    fuelMapForWoodTypes("trapdoor", 15),
    fuelMapForWoodTypes("stairs", 15),
    fuelMapForWoodTypes("fence_gate", 15),
    fuelMapForWoodTypes("fence", 15),
    fuelMapForWoodTypes("pressure_plate", 15),
    fuelMapForWoodTypes("planks", 15),
    fuelMapForWoodTypes("log", 15),
    {"minecraft:scaffolding", 20},
    fuelMapForWoodTypes("boat", 10),
    {"minecraft:charcoal", 80},
    {"minecraft:coal", 80},
    {"minecraft:blaze_rod", 120},
    {"minecraft:dried_kelp_block", 200},
    {"minecraft:coal_block", 800},
    {"minecraft:lava_bucket", 1000},
};

int turtle_refuel(lua_State *L) {
    turtle_getdata();
    if (data->inventory[data->selectedItem-1].count == 0) turtle_failure("No items to combust");
    if (fuelMap.find(data->inventory[data->selectedItem-1].id) == fuelMap.end()) turtle_failure("Items not combustible");
    for (int count = luaL_optinteger(L, 1, data->inventory[data->selectedItem-1].count); count > 0 && data->inventory[data->selectedItem-1].count > 0 && data->fuelLevel < 20000; count--, data->inventory[data->selectedItem-1].count--) {
        unsigned newValue = data->fuelLevel + fuelMap[data->inventory[data->selectedItem-1].id];
        data->fuelLevel = newValue > 20000 ? 20000 : newValue;
    }
    if (data->inventory[data->selectedItem-1].count == 0) data->inventory[data->selectedItem-1].id.clear();
    turtle_success();
}

int turtle_getFuelLevel(lua_State *L) {
    turtle_getdata();
    lua_pushinteger(L, data->fuelLevel);
    return 1;
}

int turtle_getFuelLimit(lua_State *L) {
    lua_pushinteger(L, MAXIMUM_TURTLE_FUEL);
    return 1;
}

int turtle_transferTo(lua_State *L) {
    turtle_getdata();
    int dest = luaL_checkinteger(L, 1);
    if (dest <= 0 || dest > 16) turtle_failure("Slot out of range");
    if (data->inventory[dest-1].count > 0) turtle_failure("No space for items");
    int count = luaL_optinteger(L, 1, data->inventory[data->selectedItem-1].count);
    if (count > data->inventory[data->selectedItem-1].count) count = data->inventory[data->selectedItem-1].count;
    data->inventory[dest-1].id = data->inventory[data->selectedItem-1].id;
    data->inventory[dest-1].count += count;
    data->inventory[data->selectedItem-1].count -= count;
    if (data->inventory[data->selectedItem-1].count == 0) data->inventory[data->selectedItem-1].id.clear();
    turtle_success();
}

luaL_reg turtle_lib[] = {
    {"craft", turtle_craft},
    {"forward", turtle_forward},
    {"back", turtle_back},
    {"up", turtle_up},
    {"down", turtle_down},
    {"turnLeft", turtle_turnLeft},
    {"turnRight", turtle_turnRight},
    {"select", turtle_select},
    {"getSelectedSlot", turtle_getSelectedSlot},
    {"getItemCount", turtle_getItemCount},
    {"getItemSpace", turtle_getItemSpace},
    {"getItemDetail", turtle_getItemDetail},
    {"equipLeft", turtle_equipLeft},
    {"equipRight", turtle_equipRight},
    {"attack", turtle_attack},
    {"attackUp", turtle_attackUp},
    {"attackDown", turtle_attackDown},
    {"dig", turtle_dig},
    {"digUp", turtle_digUp},
    {"digDown", turtle_digDown},
    {"place", turtle_place},
    {"placeUp", turtle_placeUp},
    {"placeDown", turtle_placeDown},
    {"detect", turtle_detect},
    {"detectUp", turtle_detectUp},
    {"detectDown", turtle_detectDown},
    {"inspect", turtle_inspect},
    {"inspectUp", turtle_inspectUp},
    {"inspectDown", turtle_inspectDown},
    {"compare", turtle_compare},
    {"compareUp", turtle_compareUp},
    {"compareDown", turtle_compareDown},
    {"compareTo", turtle_compareTo},
    {"drop", turtle_drop},
    {"dropUp", turtle_dropUp},
    {"dropDown", turtle_dropDown},
    {"suck", turtle_suck},
    {"suckUp", turtle_suckUp},
    {"suckDown", turtle_suckDown},
    {"refuel", turtle_refuel},
    {"getFuelLevel", turtle_getFuelLevel},
    {"getFuelLimit", turtle_getFuelLimit},
    {"transferTo", turtle_transferTo},
    {NULL, NULL}
};