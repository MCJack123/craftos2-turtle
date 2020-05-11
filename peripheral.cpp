#include "peripheral.hpp"

peripheral::~peripheral(){}

Computer *(*_startComputer)(int) = NULL;

const char * methods_keys[] = {
    
};

library_t turtle::methods = {"turtle", sizeof(methods_keys)/sizeof(const char*), methods_keys, NULL, nullptr, nullptr};