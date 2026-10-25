#pragma once
#include "core/net.hpp"
namespace wx {
// Open Transport (Mac OS 8/9) implementation of the Net seam: connect/close/sleep/now/entropy.
void os9_net_init(Net *net);
// Terminal shutdown: closes OT and wipes/releases the TLS seed pool. Registered
// with atexit; safe to call twice. Reinitialization after shutdown is unsupported.
void os9_net_shutdown();
}
