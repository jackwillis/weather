# MET Locationforecast symbol coverage

`test/met_symbols.inc` vendors the sorted filename stems of all 83 files under
`weather/svg/` in [MET's weathericons repository](https://github.com/metno/weathericons/tree/89e3173756248b4696b9b10677b66c4ef435db53/weather/svg),
commit `89e3173756248b4696b9b10677b66c4ef435db53`, retrieved 2026-10-26.
Only the factual symbol names are included; no icon images or SVG code are copied.
Tests need no network. This list is independent of Weather's mapping table.

[Locationforecast documentation](https://api.met.no/weatherapi/locationforecast/2.0/documentation)
says filenames match `symbol_code` and documents the retained extra `s` in
`lightssleetshowersandthunder` and `lightssnowshowersandthunder`.
Weather accepts these published names and its previously accepted corrected
spellings. All suffix variants pass through the same suffix parser.

To refresh the test list, query the pinned repository tree (GitHub's recursive
Git tree API), select `weather/svg/*.svg`, strip that prefix and extension, sort
in ASCII order, and emit each stem as a C++ string literal. Review the revision,
list diff, new mappings, and parser/mutation tests together before replacing the
snapshot. The current count assertion intentionally flags a changed snapshot.

The unit suite requires every published name to resolve and parse, checks day/
night/polar-twilight flags, and requires thunder symbols to produce thunderstorm
code95 even with zero cloud cover. Two full JSON fixtures exercise the documented
spellings in mutation tests. Corrected spelling aliases remain covered too.

This addresses the missing-symbol portion of #50. Sleet currently still maps to
WMO freezing-rain codes66/67; choosing a more faithful display label remains open.
Unknown or missing symbols still use the existing cloud-cover fallback; this
change does not claim that fallback can represent precipitation reliably.
