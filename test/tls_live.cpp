// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Optional live compatibility probe: handshake only, with production trust and TLS policy.
#include "platform/posix/net_posix.hpp"
#include "core/log.hpp"
#include "tls/tls.hpp"
#include <cstdio>

int main(int argc, char **argv)
{
    if (argc != 2) { std::fprintf(stderr, "usage: tls-live HOST\n"); return 64; }
    wx::log_init(wx::LogDebug, false, "tls-live");
    wx::Net net; wx::posix_net_init(&net); wx::tls_net_init(&net);
    wx::Io raw, tls;
    wx::Status status = net.connect(net, argv[1], "443", 10, &raw);
    if (!status.ok()) { std::fprintf(stderr, "%s\n", status.msg); return 1; }
    status = net.tls_open(net, raw, argv[1], &tls);
    bool passed = status.ok() && tls.write(tls, "", 0) == 0; // flush drives the handshake, without HTTP bytes
    if (status.ok()) net.tls_close(net, tls);
    net.close(net, raw);
    std::printf("%s %s\n", passed ? "PASS" : "FAIL", argv[1]);
    return passed ? 0 : 1;
}
