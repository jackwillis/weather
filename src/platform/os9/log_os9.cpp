// Logging for Mac OS 9: console window plus an appended "weather.log" next to the application.
#include "core/log.hpp"
#include <cstdarg>
#include <cstdio>

namespace wx {

static Level g_level = LogWarn;

void log_init(Level lvl, bool, const char *) { g_level = lvl; }
void log_set_level(Level lvl) { g_level = lvl; }
Level log_level() { return g_level; }

void logf(Level lvl, const char *fmt, ...)
{
	if (lvl > g_level) return;
	char line[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(line, sizeof line, fmt, ap);
	va_end(ap);
	static const char *const names[] = {"error", "warning", "info", "debug"};
	fprintf(stderr, "weather: %s: %s\n", names[lvl], line);
	if (FILE *f = fopen("weather.log", "a")) {
		fprintf(f, "%s: %s\n", names[lvl], line);
		fclose(f);
	}
}

} // namespace wx
