#!/usr/bin/env bash
# Read native ccache counters from the ACTUAL run. A restored vcpkg/XML
# dependency cache alone must never masquerade as compiler-cache acceptance.
set -euo pipefail
profile="${1:?usage: report-ccache.sh profile}"
command -v ccache >/dev/null
ccache --show-stats
result="$RUNNER_TEMP/stun-ccache-stats-$profile.tsv"
ccache --print-stats > "$result"
# Stable ccache 4.x machine-readable, tab-separated counter names.
read -r hits misses < <(
  awk -F '\t' '
    $1=="direct_cache_hit" || $1=="preprocessed_cache_hit" { hits+=$2 }
    $1=="cache_miss" { misses+=$2 }
    END { printf "%.0f %.0f\n", hits+0, misses+0 }
  ' "$result"
)
total=$((hits + misses))
if ((total < 1)); then
  echo "::error::ccache did not record any cacheable C/C++ compilation"
  exit 1
fi
rate="$(awk -v h="$hits" -v t="$total" 'BEGIN {printf "%.2f",100*h/t}')"
echo "ccache compiler calls: $total; hits: $hits; misses: $misses; hit rate: $rate%"
if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
  {
    echo "### ccache compiler-output evidence ($profile)"
    echo "Native compiler invocations: **$total** · hits: **$hits** · misses: **$misses** · hit rate: **$rate%**"
    echo "First cold run may have zero hits; this gate requires real compiler invocations, not cache hits."
  } >> "$GITHUB_STEP_SUMMARY"
fi
