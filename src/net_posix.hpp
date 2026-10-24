#pragma once
#include "net.hpp"
namespace wx {
// BSD sockets implementation of the Net seam (connect/close/sleep/now).
void posix_net_init(Net *net);
}
