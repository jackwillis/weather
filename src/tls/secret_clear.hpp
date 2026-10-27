// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#pragma once
#include <cstddef>
namespace wx {
inline void secret_clear(void *ptr, size_t len)
{
	volatile unsigned char *p = static_cast<volatile unsigned char *>(ptr);
	while (len--) *p++ = 0;
}
}
