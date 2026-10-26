// Test-owned host stand-ins; no SDK content. Not an ABI/layout verification.
#pragma once
#include <cstdint>
using UInt8 = uint8_t;
using UInt16 = uint16_t;
using UInt32 = uint32_t;
using SInt32 = int32_t;
using OSStatus = int32_t;
#define pascal
constexpr OSStatus noErr = 0;
