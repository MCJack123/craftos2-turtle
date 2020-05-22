#include <peripheral/peripheral.hpp>
#include <unordered_set>

#define TURTLE_PLUGIN_USERDATA_KEY 0x536bf08e // userdata key for holding plugin data

struct item {
    std::string id;
    int8_t count = 0;
};

struct block {
    std::string id = "minecraft:air";
    std::unordered_map<std::string, std::string> state;
    item droppedItem;
    block(){}
    block(std::string i): id(i) {}
};

struct turtle_userdata {
    enum class upgrade {
        none,
        modem,
        speaker,
        pickaxe,
        axe,
        sword,
        shovel,
        hoe,
        workbench
    };
    enum class direction {
        north, // -z
        south, // +z
        east,  // +x
        west   // -x
    };
    bool isTurtle = false;
    upgrade leftUpgrade = upgrade::none;
    upgrade rightUpgrade = upgrade::none;
    int x = 0;
    uint8_t y = 64;
    int z = 0;
    direction orientation = direction::north;
    item inventory[16];
    int selectedItem = 1;
    unsigned fuelLevel = 0;
    std::chrono::system_clock::time_point nextMovementTime = std::chrono::system_clock::now();
};

class turtle: public peripheral {
    Computer * comp;
    Computer * thiscomp;
    int turnOn(lua_State *L);
    int shutdown(lua_State *L);
    int reboot(lua_State *L);
    int getID(lua_State *L);
    int isOn(lua_State *L);
    int getLabel(lua_State *L);
    int setBlock(lua_State *L);
    int setSlotContents(lua_State *L);
    int teleport(lua_State *L);
    int getLocation(lua_State *L);
public:
    static library_t methods;
    turtle(lua_State *L, const char * side);
    ~turtle();
    static peripheral * init(lua_State *L, const char * side) {return new turtle(L, side);}
    static void deinit(peripheral * p) {delete (turtle*)p;}
    destructor getDestructor() {return deinit;}
    int call(lua_State *L, const char * method);
    library_t getMethods() {return methods;}
};