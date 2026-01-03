#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
root_dir="$(cd -- "${script_dir}/../.." && pwd)"
players_dir="${root_dir}/data/player"

if ! command -v jq >/dev/null 2>&1; then
  echo "jq is required but not installed" >&2
  exit 1
fi

if [[ ! -d "${players_dir}" ]]; then
  echo "Players directory not found: ${players_dir}" >&2
  exit 1
fi

temp_file=$(mktemp)
trap 'rm -f "${temp_file}"' EXIT

find "${players_dir}" -name '*.json' -type f -print0 |
  xargs -0 -n1 jq -r '(.stats.average_step_seconds // empty) as $avg | select($avg != null) | "\($avg)\t\(.name // "")"' >"${temp_file}"

if [[ -s "${temp_file}" ]]; then
  printf "%-6s\t%s\n" "avg_s" "name"
  echo "=== Top 10 Fastest ==="
  sort -n "${temp_file}" | head -n 10 | awk -F'\t' '{printf "%.3gs\t%s\n", $1, $2}'
  echo "=== Top 10 Slowest ==="
  sort -n "${temp_file}" | tail -n 10 | awk -F'\t' '{printf "%.3gs\t%s\n", $1, $2}'
else
  echo "No players with stats.average_step_seconds found." >&2
fi
