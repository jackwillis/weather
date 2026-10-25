// Logging for Mac OS 9: console window plus an appended "weather.log" next to the application.
#include "core/log.hpp"
#include <cstdarg>
#include <cstdio>

namespace wx {

// Retro68's printf does not know the C99 'z' length modifier. size_t is the same size as long here, so rewrite %zu -> %lu.
static_assert(sizeof(size_t) == sizeof(unsigned long), "size_t must match unsigned long for the %z rewrite");
static void fix_fmt(const char *in, char *out, size_t cap)
{
	size_t o = 0;
	for (bool pct = false; *in && o + 1 < cap; in++) {
		if (*in == '%') pct = !pct;
		else if (pct && (*in == 'z')) { out[o++] = 'l'; pct = false; continue; }
		else if (pct && (*in >= 'a' && *in <= 'y' && *in != 'l' && *in != 'h')) pct = false;
		else if (pct && (*in >= 'A' && *in <= 'Z')) pct = false;
		out[o++] = *in;
	}
	out[o] = 0;
}

static Level g_level = LogWarn;

void log_init(Level lvl, bool, const char *) { g_level = lvl; }
void log_set_level(Level lvl) { g_level = lvl; }
Level log_level() { return g_level; }

void logf(Level lvl, const char *fmt, ...)
{
	if (lvl > g_level) return;
	char line[512], f2[256];
	fix_fmt(fmt, f2, sizeof f2);
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(line, sizeof line, f2, ap);
	va_end(ap);
	static const char *const names[] = {"error", "warning", "info", "debug"};
	fprintf(stderr, "weather: %s: %s\n", names[lvl], line);
	if (FILE *f = fopen("weather.log", "a")) {
		fprintf(f, "%s: %s\n", names[lvl], line);
		fclose(f);
	}
}

} // namespace wx
