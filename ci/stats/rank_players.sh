#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
root_dir="$(cd -- "${script_dir}/../.." && pwd)"
players_dir="${root_dir}/data/player"
rank_dir="${root_dir}/data/rank"

if [[ $# -eq 0 ]]; then
  echo "Usage: $0 <keyword>" >&2
  echo "Example: $0 average_step_seconds" >&2
  echo "         $0 stats.average_step_seconds" >&2
  exit 1
fi

keyword="$1"

if ! command -v jq >/dev/null 2>&1; then
  echo "jq is required but not installed" >&2
  exit 1
fi

if [[ ! -d "${players_dir}" ]]; then
  echo "Players directory not found: ${players_dir}" >&2
  exit 1
fi

mkdir -p "${rank_dir}"

safe_keyword="${keyword//\./_}"
output_file="${rank_dir}/${safe_keyword}.txt"

temp_file=$(mktemp)
trap 'rm -f "${temp_file}"' EXIT

find "${players_dir}" -name '*.json' -type f -print0 |
  xargs -0 -n1 sh -c '
    keyword="$1"
    file="$2"
    jq -r --arg key "$keyword" "
      getpath(\$key | split(\".\")) as \$val |
      select(\$val != null) |
      \"\(\$val)\t\(.name // \"\")\t\(input_filename)\"
    " "$file" 2>/dev/null || true
  ' _ "${keyword}" >"${temp_file}"

if [[ ! -s "${temp_file}" ]]; then
  echo "No players with keyword '${keyword}' found." >&2
  exit 1
fi

{
  echo "======================================"
  echo "Ranking by: ${keyword}"
  echo "Generated at: $(date '+%Y-%m-%d %H:%M:%S')"
  echo "Total players: $(wc -l < "${temp_file}")"
  echo "======================================"
  echo ""
  printf "%-6s\t%-12s\t%s\n" "Rank" "Value" "Player Name"
  echo "--------------------------------------"
  
  rank=1
  sort -n "${temp_file}" | while IFS=$'\t' read -r value name filepath; do
    printf "%-6d\t%-12s\t%s\n" "$rank" "$value" "$name"
    ((rank++))
  done
} > "${output_file}"

echo "Ranking saved to: ${output_file}"