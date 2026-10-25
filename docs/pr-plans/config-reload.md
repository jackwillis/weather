# Config reload and daemon mode

Status: conditional/deferred draft. Do not implement a daemon merely because this
PR exists. First demonstrate a real requirement not met by the window's event loop
and refresh model, and obtain the owner's scope decision.

Possible scope: POSIX SIGHUP reload, followed only if justified by a separate
decision by a foreground/background service mode. Do not assume OS 9 has POSIX
signals or share its Preferences storage blindly with Unix dotfiles.

Acceptance if activated:
- Define reload precedence, atomic validation, reporting and behavior during an
  active fetch. Invalid config retains the last working settings.
- Record signals safely and handle them in the normal event loop; no parsing,
  allocation or network operations in a signal handler.
- Test changed location/units, pending refresh/cancellation, malformed reload,
  repeated signals, shutdown and stale data. Preserve stdout/stderr and CLI exit
  contracts and ensure unattended commands do not start a daemon unexpectedly.
- Any service lifecycle has bounded work, explicit ownership and deterministic
  fixture tests; privileged installation remains the owner's step.

Dependencies: evidence from the completed window/settings work. This is not on the
critical path for the first OS 9 UI or initial release.
