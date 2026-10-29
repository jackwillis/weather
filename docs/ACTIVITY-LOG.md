# Session Activity Log (#160)

The portable foundation is `src/app/activity.hpp`. It is not yet connected to
production event producers or either native viewer. No menu, on-disk log or
existing diagnostic sink changes in this slice.

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
text. Existing verbose diagnostic logging remains separate.

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

Still open under #160: production event fan-out (including fallback and cache
restoration), local-time presentation, both independently closable native viewers,
scrolling/redraw/reentrancy checks, and real PowerMac memory/behavior acceptance.
The current foundation has no effect on forecast or network behavior.
