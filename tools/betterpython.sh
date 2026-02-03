#!/usr/bin/env sh
set -eu

in="$1"
out="${2:-}"

if [ -z "${out}" ]; then
    out="$(mktemp -t betterpython.XXXXXX).bpc"
fi

here="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
"$here/bin/bpcc" "$in" -o "$out"
"$here/bin/bpvm" "$out"
