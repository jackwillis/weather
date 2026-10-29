# Entropy assessment readiness, 2026-10-28 (#121)

Initial hardware captures exist for Classic and native OS 9. This record checks
their provenance and the separate assessment tool; it establishes no source
min-entropy bound, production credit, collection duration or seed threshold.
Production provisioning and the analyzer’s unconditional zero credit are unchanged.

## Capture inventory

Re-ran the existing bounded analyzer on the seven complete CSVs retained outside
the repository in `~/scratch/tiger-2026-10-28/`. Environment/run labels follow the
owner’s notes and [issue #121](https://github.com/jackwillis/weather/issues/121).
Every capture passed the existing format/footer validation. No captures were
pooled, edited, expanded or uploaded. SHA-256 identifies the exact input bytes.

| Capture | Rows | SHA-256 |
|---|---:|---|
| `entropy-classic-idle1.csv` | 2693 | `c9feefdff3bd2bbe77f53fab03c0b021cd598f8acd2009feca739b1cfe85904a` |
| `entropy-classic-idle3.csv` | 904 | `905020087afbe7b3cf5ac12369a2332c842f531a9f9751558a92e86c8f0798e2` |
| `entropy-classic-manual1.csv` | 3093 | `0c94f81e81dc16f12d22ccc0bac08768c984c05c6737e0a844a08e1133bb96ae` |
| `entropy-classic-manual2.csv` | 1734 | `a602abcc9d3055c5f3370f793cdcbc433aeee3228f8285dac8cbc501763093ac` |
| `entropy-native-idle1.csv` | 2697 | `248cd843b8e2c76db23c17bb22371f68dfc25a8ad39de353189d60922ed03a9b` |
| `entropy-native-idle2.csv` | 2695 | `6fe9d9268d273e162d3848b03630810964988c184a0b2b259477bb514230639e` |
| `entropy-native-manual1.csv` | 3117 | `03abfb56c394b8319bafe6e7c7abdd6dc18d1f1ac3438ede95dadbfb2c2f771c` |

Phase names are probe labels: the second phase of a fully idle control remains
an idle control even though its CSV phase is called interaction. The first
phase of Classic manual1 is contaminated by owner input beginning around 24 s.
Classic idle3 and manual2 have about 10 samples/s in their first phase, compared
with about 30/s in the other runs; the cause is unresolved. Native trials were
separate launches in one native boot, not a noise-source restart experiment.

For a descriptive cross-check, native idle1/idle2 first-phase Delay(1) readings
have 84/89 distinct values and most-common shares 15.4%/17.7%; Classic idle1
has 291 and 3.8%. Native manual1’s second phase has 650 and 7.4%. These are
empirical distribution summaries, not conservative entropy estimates. Dependence,
adversarial predictability, workload and observer effects remain unassessed.

## Separate NIST tool build

The National Institute of Standards and Technology supplies the
[assessment implementation](https://github.com/usnistgov/SP800-90B_EntropyAssessment/tree/87c104d0ed4cbc96103e7b8b38d6f2c7e0a6b289).
Used exact revision `87c104d0ed4cbc96103e7b8b38d6f2c7e0a6b289` (noniid 1.1.8),
unchanged, outside Weather. Its full upstream notice remains intact in that
checkout; no NIST source, binaries or capture data are vendored or linked here.

Built all tools on Linux with GCC 12.2.0, OpenMP and the upstream Makefile in an
unprivileged container. Dependencies: libbz2 1.0.8-5+b1, libdivsufsort 2.0.1-5,
libjsoncpp 1.9.5-4, libmpfr 4.2.0-1, libssl 3.0.22-1~deb12u2. The isolated runtime
image ID is `738e1a1fc942f9992009d40d951f6aa62a7276241b40ca0a7e65803bc7c3039e`.
No host packages or Mac files were changed.

Ran the upstream self-test with `OMP_NUM_THREADS=2`. Then independently required
each of its 11 generated/reference comparisons to return success, since the
upstream shell wrapper alone does not stop on every earlier failure. All passed;
maximum reported comparison delta was 1.00808250635964e-13 (the comparator’s
actual tolerance is 1e-10). This validates tool reproduction, not our source.

Binary SHA-256:

- `ea_non_iid`: `7168b9bc7fcc8ee3feafb7e4bc98f07a2f6b7621fc52830be43a0e039ea13053`
- `ea_restart`: `c9c740cd123b3c1c5ac8daf08a75553cbce4097cbb9aff9dfe1ab4c6d831c796`

Reproduction from a separate, dependency-equipped checkout:

```sh
git checkout 87c104d0ed4cbc96103e7b8b38d6f2c7e0a6b289
cd cpp
make -j2
cd selftest
OMP_NUM_THREADS=2 ./selftest
set -eu
count=0
for file in ../../bin/*.bin; do
    name=${file##*/}
    ./compareresults.pl "${name%.bin}.res" "refdata/${name%.bin}.res"
    count=$((count + 1))
done
test "$count" -eq 11
```

The initial host build failed for missing `json/json.h`; the first dependency
container lacked native `g++`. Both setup failures preceded tool execution and
were resolved inside the isolated runtime. Final logs and generated results:
`/tmp/weather-nist-{selftest,reference-validation,provenance}.log`,
`/tmp/weather-nist-90b/cpp/selftest/`, and
`/tmp/weather-entropy-assessment-2026-10-28/`. Temporary paths are not durable
archives; preserve the original captures and their owner notes separately.

## Assessment still required

[SP 800-90B, section 3.1.1](https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-90B.pdf)
requires at least one million raw sequential samples for validation and a
separate 1000-by-1000 restart dataset. These short captures supply neither.
Section 6.4 addresses alphabets above 256 symbols; raw multi-byte timings need
a declared reduction before byte-symbol tools can assess them. Selecting the
best-looking low bits after inspecting results would not establish that model.
The [publication page](https://csrc.nist.gov/pubs/sp/800/90/b/final) also links
potential errata; include them in the eventual method review.

Next: declare the physical source, restart meaning, fixed symbol mapping,
controls and observer model; design a diagnostic capture protocol large enough
for that assessment; then collect repeated native trials and restart data with
the owner. The present 8192-row diagnostic buffer cannot hold a full sequential
dataset. Any longer/chunked capture design must account for its I/O effects.
Do not concatenate environments, duplicate data or count correlated fields as
independent sources to fill the gap.

No short-capture NIST output is presented as an estimate here. No numbers from
this record authorize #120 to credit samples or initialize a production seed.
Cold boots, other G3/G4 models, long-term drift, scripted controls and conservative
source/health-test review remain outstanding. No new Mac access was performed.
