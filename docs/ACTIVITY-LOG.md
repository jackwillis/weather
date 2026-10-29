# Session Activity Log (#160)

The portable foundation is `src/app/activity.hpp`. The X11 app connects it to
selected lifecycle events and an independent CDE-style viewer. The OS 9 viewer
and core source-fallback notification are still pending. Existing diagnostic
sinks and on-disk log policy are unchanged.

`ActivityLog` owns 128 entries, retaining the newest entries in chronological
order. Each contains a UTC time with an explicit trust flag, severity, category
and bounded canonical message. Compile-time gates limit each entry to 256 bytes
and the entire history to 32 KiB. There is no allocation, mutable inline static
storage, callback, OS dependency or network dependency. A native UI should own
one instance with a lifetime spanning its session, outside a small stack frame.

`record` takes an event code and an optional `Status`, but uses only `Status::err`.
It never copies the diagnostic message or detail. Location names, coordinates,
request parameters, response bodies, seed/entropy material and keys cannot be
passed as activity text through this API. Unknown event/error codes use generic
text. Producers must pass a non-OK error with `RefreshFailed`; successful results
use `RefreshSucceeded`. Passing `Err::Ok` with a failure event is a producer
contract violation; the defensive mapping still records a generic warning rather
than fabricating a cause. Existing verbose diagnostic logging remains separate.

The owner is a single UI event loop. `at(0)` is the oldest entry; out-of-range
indexes return null. Entries are borrowed until the next `record` or `clear`.
Do not hold a pointer while pumping events. `clear` discards and clears the
session history. Future event producers should emit one coherent lifecycle event
per transition and suppress repeated status notifications there, rather than
filtering low-level diagnostic lines into this buffer.

`ActivityTime` defaults to untrusted. The platform must explicitly establish
clock trust before setting that flag. Plausible epoch seconds alone do not
establish trust. Invalid/untrusted values are normalized on insertion.
`activity_utc` formats trusted values as RFC 3339 UTC at second precision, or
`TIME-UNKNOWN`; it fails without partial text when the output buffer is too small.
It supports 1970 through 9999 and does not use platform `time_t` or printf.
Native viewers must format local time from the stored UTC in their platform
layer. Fetch durations must use a separate monotonic elapsed clock.

## X11 viewer

View > Activity Log (Ctrl+J) opens an ordinary window managed independently by
the window manager. Escape, Close and WM_DELETE_WINDOW close only the viewer;
Ctrl+Q from either window quits the app. Up/Down, Page keys, Home/End, the wheel
and visible buttons scroll. The newest events follow automatically until the
user scrolls away; reading position is preserved when the ring evicts an entry.

The viewer owns its GC, numeric font, backing pixmap and window, and borrows the
main theme's other fonts/colors. Drawing is cached; exposes copy the backing
image. Font metrics and viewport tests keep complete rows visible. Timestamp
and count columns use the X core fixed font after checking all digits against
capital-height and equal-width metrics. Times come from stored UTC via the
platform's local timezone; untrusted times retain TIME-UNKNOWN.

The X11 UI emits startup, saved-forecast restoration, refresh start/result,
transitions into stale data, applied preference changes and shutdown. It never
includes preference values, location names or diagnostic error text. Repeated
stale-state notifications are suppressed until state changes. The read-only
viewer remains responsive through the existing fetch yield, and the shared
pump processes at most 64 queued events per call. Viewer actions cannot invoke
a fetch. No diagnostic sink is redirected through this component.

Still open under #160: core source-fallback fan-out, the OS 9 viewer and its
local-time/event wiring, real ctwm/display behavior and PowerMac memory/runtime
acceptance. Linux/Xvfb evidence does not establish those hardware results.
