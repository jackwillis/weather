#include "core/log.hpp"
#include <cstdarg>
#include <cstdio>
#include <syslog.h>

namespace wx {

static Level g_level = LogWarn;
static bool g_syslog = false;

void log_init(Level lvl, bool use_syslog, const char *ident)
{
	g_level = lvl;
	g_syslog = use_syslog;
	if (use_syslog) openlog(ident, LOG_PID, LOG_USER);
}

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
	static const int prio[] = {LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG};
	if (g_syslog) syslog(prio[lvl], "%s", line);
	else fprintf(stderr, "weather: %s: %s\n", names[lvl], line);
}

} // namespace wx
