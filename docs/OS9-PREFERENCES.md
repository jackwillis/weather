# Location and Preferences on Mac OS 9

Options > Location (Cmd-L) opens the application's native Location dialog when
no update is running. Latitude, longitude and name use native TextEdit controls;
LocationForm/TextEdit provide portable validation. Tab and Shift-Tab move among
the three text fields. Return applies; Escape or Cmd-. cancels. Metric units and
automatic updates have checkboxes. File > Quit / Cmd-Q remains available.

The dialog suspends automatic updates while open, and cannot open during a fetch.
Apply validates every field, then saves, then changes the current configuration.
A changed coordinate or units clears the old data and requests exactly one new
fetch. Renaming alone keeps the forecast. Cancel leaves configuration, forecast
and scheduling unchanged, including a pre-existing startup configuration error.
Menu unit/automatic-update changes also save before applying. A failure stays
visible and does not falsely commit new settings in memory.

## Configuration precedence and migration

Both Weather and WeatherConsole use these rules:

1. A nonempty `Weather Preferences` file in the classic System Folder's
   Preferences folder is the complete configuration source.
2. If Preferences is missing or empty, use legacy `weather.conf` next to the app.
3. If both are missing/empty, use the existing defaults.

An unreadable or invalid selected file reports an error and applies nothing.
Invalid Preferences never silently falls back to weather.conf. The first successful
save copies legacy settings/comments into Preferences while replacing latitude,
longitude, name, units, autorefresh and cache. The Options menu exposes the cache
setting as “Save forecasts on this computer”, enabled by default. Switching it
off first removes the saved forecast; a deletion failure leaves the setting on
and displays an error. MET Norway is unavailable while saving is off. The legacy file is never modified.
Subsequent loads/saves use Preferences. To edit providers, contact or other settings
after migration, edit Weather Preferences and restart; changing weather.conf no
longer changes the active configuration once nonempty Preferences exists.

Only five UI-owned keys change; other lines, duplicate owned keys, inline comments
and unrelated settings are retained by the existing settings editor. Before each
save the selected file is read again, preserving unrelated external text edits.
Such edits are not live-reloaded into the running configuration during ordinary
saves. After repairing a failed startup configuration load, the entire saved
configuration is reloaded before fetching, so retained provider settings are honored. The parser and
settings editor retain their existing line-ending behavior; PR #38 independently
addresses classic CR-only text. UI saves generate normal LF-separated text.

Files are bounded to 4095 bytes. The File Manager adapter creates an exclusive
process/tick-named temporary in the same folder, checks write count and close,
flushes, then exchanges with an existing file or renames into a missing path.
Before exchanging an existing file, a native write-open and checked close ensure that its lock/write permissions are respected; Classic file exchange alone did not reject a locked target in hardware testing. Replacement, cleanup and subsequent flush results are checked. A failure after
replacement may leave a complete new file on disk despite the reported error;
the current in-memory settings still stay unchanged. Physical power-loss durability
is unverified, and simultaneous GUI writers are not serialized. Settings use their
own filename/temp prefix and never read or modify managed TLS seed records.

## Diagnostics and current verification limits

Host policy tests cover precedence, migration/comments, invalid fields, read/save
failure, cancellation, exactly one location refresh, name-only edits and reloading.
Mutation tests exercise the same config/text path and check save readback using
its exact byte count, including embedded NULs in comments.

With diagnostics enabled, build these separate executables:

- `PreferencesProbe`: seed-free native File Manager boundary, corruption,
  locked-file and fault-injection cases in a newly owned directory. It tests
  partial writes, close/flush/exchange/rename/delete failures and old-or-complete-new
  contents. It deletes only its own files/directory and writes preferences-probe.log.
- `WeatherPrefsUITest`: isolated fake forecasts and a fresh private store. It
  prepares screenshots of invalid fields, save failure, persisted values and
  name-only changes; exercises native Tab/Shift-Tab/Return/Escape, Apply/Cancel,
  and Quit during a simulated fetch. It reads back with a fresh adapter instance.
- `WeatherUITest`: the earlier window scenario now uses an explicit memory-only
  settings store, so menu tests cannot write production Preferences.

Deliver each diagnostic normally with tools/deliver-os9.sh while Tiger is booted.
Run `python3 tools/check-os9-preferences.py /tmp/weather-prefs-shots` for the UI
protocol; read the store probe's own log after launching PreferencesProbe.
The driver opens only the diagnostic app and exchanges its explicit stage markers;
it never clicks or answers a system dialog. Keep screenshots local for inspection.

These targets cross-build, but none of the new native cases have run yet. Dialog
layout, real typing, locked/read-only volume behavior, actual process-restart
persistence, and production delivery remain acceptance work for a Tiger boot.
A fresh-adapter read is not an actual application restart. Host/sanitizer and
NetBSD portable-policy results are recorded separately in the PR plan.
