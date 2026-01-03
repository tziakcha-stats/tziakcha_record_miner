#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
root_dir="$(cd -- "${script_dir}/../.." && pwd)"
players_dir="${root_dir}/data/player.竹花杯"

if ! command -v jq >/dev/null 2>&1; then
  echo "jq is required but not installed" >&2
  exit 1
fi

if [[ ! -d "${players_dir}" ]]; then
  echo "Players directory not found: ${players_dir}" >&2
  exit 1
fi

k="${1:-10}"

temp_all=$(mktemp)
temp_current=$(mktemp)
temp_peak=$(mktemp)
temp_lowest=$(mktemp)
trap 'rm -f "${temp_all}" "${temp_current}" "${temp_peak}" "${temp_lowest}"' EXIT

find "${players_dir}" -name '*.json' -type f -print0 |
  xargs -0 -n1 jq -r '
    .player_id as $id |
    .name as $name |
    .current_elo as $current |
    (.elo_history // [] | map(.elo) | max) as $peak |
    (.elo_history // [] | map(.elo) | min) as $lowest |
    "\($current // "")\t\($peak // "")\t\($lowest // "")\t\($name // "")\t\($id // "")"
  ' >"${temp_all}"

awk -F'\t' '$1 != "" {print $1 "\t" $4 "\t" $5}' "${temp_all}" >"${temp_current}"
awk -F'\t' '$2 != "" {print $2 "\t" $4 "\t" $5}' "${temp_all}" >"${temp_peak}"
awk -F'\t' '$3 != "" {print $3 "\t" $4 "\t" $5}' "${temp_all}" >"${temp_lowest}"

echo "=== Top ${k} by Current Elo ==="
printf "%-8s\t%-20s\t%s\n" "Elo" "Name" "ID"
if [[ -s "${temp_current}" ]]; then
  sort -rn "${temp_current}" | head -n "${k}" | awk -F'\t' '{printf "%-8.1f\t%-20s\t%s\n", $1, $2, $3}'
else
  echo "No players with current_elo found." >&2
fi

echo ""
echo "=== Top ${k} by Peak Elo (Historical) ==="
printf "%-8s\t%-20s\t%s\n" "Peak" "Name" "ID"
if [[ -s "${temp_peak}" ]]; then
  sort -rn "${temp_peak}" | head -n "${k}" | awk -F'\t' '{printf "%-8.1f\t%-20s\t%s\n", $1, $2, $3}'
else
  echo "No players with elo_history found." >&2
fi
