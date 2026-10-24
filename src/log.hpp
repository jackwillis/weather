#pragma once
namespace wx {

enum Level { LogError = 0, LogWarn = 1, LogInfo = 2, LogDebug = 3 };

void log_init(Level lvl, bool use_syslog, const char *ident);
void log_set_level(Level lvl);
Level log_level();
void logf(Level lvl, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

} // namespace wx
