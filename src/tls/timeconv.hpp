// BearSSL wants certificate-check time as (days since 0000-01-01, seconds into the day).
// The Unix epoch (1970-01-01) is day 719528 on that scale.
#pragma once

namespace wx {

constexpr long long kUnixEpochBrDays = 719528;

struct BrTime {
	unsigned long days;
	unsigned long secs;
};

constexpr BrTime unix_to_br_time(long long unix_s)
{
	long long d = unix_s / 86400, s = unix_s % 86400;
	if (s < 0) { s += 86400; d -= 1; }
	return BrTime{static_cast<unsigned long>(d + kUnixEpochBrDays), static_cast<unsigned long>(s)};
}

static_assert(unix_to_br_time(0).days == 719528 && unix_to_br_time(0).secs == 0, "epoch");
static_assert(unix_to_br_time(86399).days == 719528 && unix_to_br_time(86399).secs == 86399, "end of first day");
static_assert(unix_to_br_time(1735689600LL).days == 719528 + 20089 && unix_to_br_time(1735689600LL).secs == 0,
    "2025-01-01 is 20089 days after the epoch");
static_assert(unix_to_br_time(-1).days == 719527 && unix_to_br_time(-1).secs == 86399, "before the epoch");
static_assert(unix_to_br_time(4102444800LL).days == 719528 + 47482, "2100-01-01");

} // namespace wx
