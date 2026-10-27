// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// One active-time clock shared by DNS and socket/TLS deadlines. TickCount and
// credited modal intervals are modulo 2^32; sample at least once per full wrap.
#pragma once
#include <cstdint>

namespace wx { namespace os9 {
class ModalClock {
	uint32_t paused_ = 0, last_ = 0;
	uint64_t elapsed_ = 0;
	bool initialized_ = false;
public:
	void credit(uint32_t begin, uint32_t end) { paused_ += static_cast<uint32_t>(end - begin); }
	uint32_t ticks(uint32_t raw) const { return static_cast<uint32_t>(raw - paused_); }
	long long milliseconds(uint32_t raw)
	{
		uint32_t current = ticks(raw);
		if (!initialized_) { last_ = current; initialized_ = true; }
		// Saturation is far beyond TickCount's wrap period, but prevents an
		// eventual multiplication/conversion overflow in a process-long clock.
		constexpr uint64_t limit = (9223372036854775807ULL / 1000) * 60;
		uint32_t delta = static_cast<uint32_t>(current - last_);
		elapsed_ = elapsed_ > limit - delta ? limit : elapsed_ + delta;
		last_ = current;
		return static_cast<long long>(elapsed_ / 60) * 1000 + static_cast<long long>(elapsed_ % 60) * 1000 / 60;
	}
};
} }
