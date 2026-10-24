// Keeps libstdc++.so out of the binary. <string_view> bounds checks reference
// this function; with -fno-exceptions it can only mean a bug in our own code
// (an out-of-range substr), so say so loudly and stop.
#include <cstdio>
#include <cstdlib>

namespace std {
void __throw_out_of_range_fmt(const char *fmt, ...) __attribute__((__noreturn__));
void __throw_out_of_range_fmt(const char *, ...)
{
	fputs("weather: internal error: string_view out of range (please report this)\n", stderr);
	abort();
}
}
