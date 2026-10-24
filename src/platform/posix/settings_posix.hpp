#pragma once
#include "core/settings.hpp"
namespace wx {
// Dotfile store. path == nullptr means $HOME/.weatherrc. must_exist makes a missing file an
// error (used when the user named the file with -c).
struct PosixStore {
	SettingsStore base;
	char path[512];
	bool must_exist;
};
Status posix_settings_init(PosixStore *s, const char *path, bool must_exist);
}
