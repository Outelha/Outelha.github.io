#!/usr/bin/env bash
# Renders the editor headless (main view, the eight Advanced panels, browser) at 2x and
# builds side-by-side sheets against the approved references in docs/ui-redesign/references.
#
#   tools/ui_snapshot.sh [build-dir] [out-dir]
#
# Needs a Linux build of shobeats_plugin_tests and Xvfb (xvfb-run).
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
build="${1:-$here/build}"
out="${2:-$here/build/ui-snapshots}"
mkdir -p "$out"
tests="$(find "$build" -type f -name shobeats_plugin_tests -perm -u+x | head -n 1)"
[ -n "$tests" ] || { echo "shobeats_plugin_tests not found in $build" >&2; exit 1; }
SHOBEATS_SNAPSHOT="$out/" xvfb-run -a -s "-screen 0 2560x1600x24" \
    "$tests" "Plugin: editor opens\\, resizes and closes" > "$out/test-log.txt" 2>&1 \
    || { cat "$out/test-log.txt"; exit 1; }
python3 -I "$here/tools/ui_compare.py" "$out" "$here/docs/ui-redesign/references"
echo "Snapshots and comparison sheets written to $out"
