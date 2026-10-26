#!/bin/sh
# Repository rules from AGENTS.md that a machine can check. POSIX sh. Run from the repo root (CI does).
#   1. No TLS seed material or seed imports in the tree.
#   2. No Apple copyright headers or classic-Mac archive/binary formats (Apple SDK material is read for reference only,
#      outside the repo).
fail=0
bad() { echo "policy: $1"; fail=1; }

# 1. tracked files that look like seed material
if git ls-files | grep -E '(^|/)(weather\.seed|weather-seed\.import)$|\.seed$|\.import$' | grep -v '^$' >/dev/null; then
	git ls-files | grep -E '(^|/)(weather\.seed|weather-seed\.import)$|\.seed$|\.import$'
	bad "seed material must never be committed (see docs/OS9-SEEDS.md)"
fi

# 2. Apple headers and archives. Documentation may mention Apple by name; source and headers may not carry its copyright.
if git ls-files 'src/*' 'os9/*' 'spike/*' 'third_party/*' 'tools/*' 'test/*' | \
    xargs grep -lIE 'Copyright.*Apple (Computer|Inc)|Apple Computer, Inc\.' 2>/dev/null | grep -v '^tools/check-policy.sh$' | grep .; then
	bad "a file carries an Apple copyright notice; Apple material must not be copied into the tree"
fi
if git ls-files | grep -iE '\.(sit|hqx|smi|img|dsk|rsrc|bin)$' | grep -v '^$' | grep .; then
	bad "classic-Mac archive or disk-image file tracked; build artifacts are not committed"
fi

[ "$fail" = 0 ] && echo "policy: ok"
exit $fail
