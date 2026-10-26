#pragma once
#include "core/settings.hpp"
#include <Files.h>
namespace wx {
struct Os9SettingsFileOps {
    OSErr (*read)(short,long *,void *);
    OSErr (*write)(short,long *,const void *);
    OSErr (*close)(short);
    OSErr (*flush)(short);
    OSErr (*exchange)(FSSpec *,FSSpec *);
    OSErr (*rename)(FSSpec *,const unsigned char *);
    OSErr (*remove)(FSSpec *);
};
const Os9SettingsFileOps &os9_settings_file_ops();
struct Os9Settings {
    SettingsStore base;
    FSSpec file = {};
    const Os9SettingsFileOps *ops = nullptr;
    bool ready = false;
};
// Keep the object at a stable address after init. Diagnostics alone supply an
// explicit disposable path/ops table. Default: Preferences:Weather Preferences.
Status os9_settings_init(Os9Settings *,const char *diagnostic_path=nullptr,const Os9SettingsFileOps *ops=nullptr);
}
