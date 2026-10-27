// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#include <cstdint>
#include <cstdio>
#include <cstring>

// Prove the executing ABI, not just the compiler's target macros.
int main()
{
    const std::uint32_t value = 0x01020304;
    unsigned char bytes[4];
    std::memcpy(bytes, &value, sizeof bytes);
    std::printf("runtime: bytes=%u,%u,%u,%u long=%u pointer=%u\n",
                bytes[0], bytes[1], bytes[2], bytes[3],
                unsigned(sizeof(long)), unsigned(sizeof(void *)));
    return bytes[0] == 1 && bytes[1] == 2 && bytes[2] == 3 && bytes[3] == 4 &&
           sizeof(long) == 4 && sizeof(void *) == 4 ? 0 : 1;
}
