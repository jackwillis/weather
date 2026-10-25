#pragma once
#include "core/net.hpp"
namespace wx {
// Open Transport (Mac OS 8/9) implementation of the Net seam: connect/close/sleep/now/entropy.
void os9_net_init(Net *net);
// Shuts Open Transport down (also registered with atexit). Safe to call twice.
void os9_net_shutdown();
}
