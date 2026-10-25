// Pure OS 9 compatibility helpers. No Toolbox calls or native-size arithmetic.
#pragma once
#include "core/text.hpp"
#include <cstdint>

namespace wx { namespace os9 {

constexpr long gmt_offset(uint32_t raw)
{
	// Subtract in a signed type after masking; do not rely on unsigned-to-signed conversion.
	long seconds = static_cast<long>(raw & 0x00ffffffU);
	return seconds >= 0x00800000L ? seconds - 0x01000000L : seconds;
}

constexpr long long mac_to_unix(uint32_t local_seconds, uint32_t raw_gmt_delta)
{
	// The signed low 24 bits include DST. The upper byte is informational.
	return static_cast<long long>(local_seconds) - 2082844800LL - gmt_offset(raw_gmt_delta);
}

inline bool decimal_unsigned(unsigned long long value, char *out, size_t cap, bool negative = false)
{
	if (!cap) return false;
	char digits[20]; size_t n = 0;
	do { digits[n++] = static_cast<char>('0' + value % 10); value /= 10; } while (value);
	if (n + (negative ? 1 : 0) >= cap) { out[0] = 0; return false; }
	size_t pos = 0;
	if (negative) out[pos++] = '-';
	while (n) out[pos++] = digits[--n];
	out[pos] = 0;
	return true;
}

inline bool decimal_signed(long long value, char *out, size_t cap)
{
	// Unsigned subtraction also handles LLONG_MIN without signed overflow.
	unsigned long long magnitude = static_cast<unsigned long long>(value);
	if (value < 0) magnitude = 0ULL - magnitude;
	return decimal_unsigned(magnitude, out, cap, value < 0);
}

// Adapt trusted printf format literals for Retro68. Only the size_t modifier is
// rewritten. In particular, %ll is never narrowed to %l. On truncation or an
// incomplete conversion, return an empty format so callers cannot consume a
// mismatched variadic argument list. This does not validate printf's full grammar.
inline bool rewrite_log_format(sv in, char *out, size_t cap)
{
	if (!cap) return false;
	size_t pos = 0; bool percent = false;
	for (char c : in) {
		if (!c || pos + 1 >= cap) { out[0] = 0; return false; }
		if (c == '%') percent = !percent;
		else if (percent && c == 'z') c = 'l';
		else if (percent && ((c >= 'a' && c <= 'z' && c != 'l' && c != 'h') || (c >= 'A' && c <= 'Z')))
			percent = false;
		out[pos++] = c;
	}
	if (percent) { out[0] = 0; return false; }
	out[pos] = 0; return true;
}

static_assert(mac_to_unix(2082844800U, 0) == 0, "Mac/Unix epoch");
static_assert(mac_to_unix(2082844800U, 0x80ffb9b0U) == 18000, "DST included once");
static_assert(mac_to_unix(UINT32_MAX, 0x00800000U) == 2220511103LL, "32-bit rollover and negative offset");

} } // namespace wx::os9
