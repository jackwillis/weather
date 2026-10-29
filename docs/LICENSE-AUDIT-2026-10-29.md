# v0.1 licence review proposal (#65, #111)

Status: proposal for Jack's sign-off, 2026-10-29. This does not change any licence,
reopen the accepted v0.0.1 decision, or authorize extraction into another project.
Reviewed repository baseline: main `4a5d70c`.

## Existing distribution

| Component | Evidence reviewed | Proposed conclusion / remaining check |
| --- | --- | --- |
| Weather original code | `LICENSE`, SPDX headers, repository policy; owner decision #129 | Keep GPL-3.0-or-later. Publish the corresponding source and build scripts with each binary release. Provider service terms do not relicense the application. |
| Vendored BearSSL | `third_party/bearssl/LICENSE.txt`, its README and copyright headers | MIT permission permits inclusion in the GPL application; preserve its permission/warranty notice and per-file copyrights. The source contains Thomas Pornin notices from 2016, 2017 and 2018; the top-level upstream licence/THIRD_PARTY notice says 2016. Ship the complete source with those headers intact. No relicensing of BearSSL is needed. |
| Retro68 runtime | Existing `THIRD_PARTY.md`; upstream [README](https://github.com/autc04/Retro68), [runtime exception](https://github.com/autc04/Retro68/blob/master/COPYING.RUNTIME) | Keep the previously accepted runtime notice. GPL runtime linkage is compatible with GPL Weather. The exception matters particularly to future non-GPL consumers; the existing caveat about per-file exception coverage is unresolved. |
| Multiversal Interfaces | Existing MIT notice; upstream [COPYING](https://github.com/autc04/multiversal/blob/master/COPYING) | Preserve the copyright and full MIT notice in OS 9 distributions. |
| Open Transport import libraries | Existing notice and accepted option A in closed #65 | The upstream author's belief about symbol-name import libraries remains a residual uncertainty, not an affirmative licence grant. Jack accepted this position for v0.0.1; propose explicitly retaining it for v0.1. No Apple SDK headers/binaries may enter the repository. |
| Forecast / geocoding data | `docs/PROVIDER-TERMS.md`, #73 | Treat data attribution and service access conditions separately. NWS public-domain data must not be presented as CC BY; geocoding needs its own Open-Meteo/GeoNames credit. #73 owns corrections and provider acceptance. |
| Trust roots | `src/tls/roots.pem`, `THIRD_PARTY.md` | Public certificate data, not private keys. Preserve provenance; this review does not validate future trust-store redistribution policies. |

`make install` copies LICENSE and THIRD_PARTY.md to the documentation directory;
`tools/stage-dsk.sh` copies both into the release image. `tools/mkpkg-netbsd.sh`
sets the combined package's licence to `gnu-gpl-v3`. These are source inspections,
not a fresh package/image installation or proof that a future tag contains them.
The release candidate still needs archive/package inspection under #67/#204.

## MPL-2.0 intention in #111

Mozilla's [MPL 2.0 text, especially §§1.5, 1.12 and 3.3](https://www.mozilla.org/en-US/MPL/2.0/)
and [FAQ Q14](https://www.mozilla.org/en-US/MPL/2.0/FAQ/) permit combination with
GPLv3 through the secondary-licence mechanism when the MPL files are eligible.
That requires checking the actual files for Exhibit B / secondary-licence
incompatibility, preserving MPL obligations, and additionally distributing the
covered source under the relevant GPL terms for the combined work. Compatibility
is conditional, not permission to relabel existing GPL files as MPL.

Propose MPL-2.0 only for independently authored future subsystem files whose
copyright holders authorize that licence. Before extracting Weather code,
identify every contributor and copied dependency and obtain any necessary
relicensing permission. GPL Weather stays GPL. BearSSL stays MIT. A future MIT
example/helper must be independently licensed by its rights holders; copying
GPL implementation into an example does not remove GPL obligations.

No subsystem is linked into v0.1 by this proposal. No new repository, extraction,
runtime dependency, trademark decision or security claim is authorized.

## Jack's review

- [ ] Accept the existing GPL-3.0-or-later / MIT distribution approach for v0.1.
- [ ] Confirm carrying forward the accepted import-library uncertainty.
- [ ] Accept the conditional MPL/GPL combination proposal and ownership gate.
- [ ] Confirm release notices and corresponding source in the final artifacts.

Unverified: exact pinned-container runtime source provenance and exception
coverage for every linked object; all contributor relicensing rights; future
MPL-covered inputs; legal/trademark clearance; actual v0.1 artifact installation.
No hardware was used for this review.
