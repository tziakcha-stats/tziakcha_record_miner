#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
root_dir="$(cd -- "${script_dir}/../.." && pwd)"
players_dir="${root_dir}/data/player"
rank_dir="${root_dir}/data/rank"

rate_type=""

usage() {
  echo "Usage: $0 [-p <players_dir>] [-r <rank_dir>] <rate_type>"
  echo "Rate Types:"
  echo "  win_rate            Win count / Total rounds (和牌率)"
  echo "  tsumo_rate          Tsumo win count / Win count (自摸率)"
  echo "  deal_in_rate        Deal-in count / Total rounds (放铳率)"
  echo "  tsumo_against_rate  Tsumo-against count / Total rounds (被摸率)"
  echo ""
  echo "Options:"
  echo "  -p <dir>   Players data directory (default: \${root_dir}/data/player)"
  echo "  -r <dir>   Rank output directory (default: \${root_dir}/data/rank)"
  echo "  -h         Show this help message"
  exit 1
}

while getopts "p:r:h" opt; do
  case "$opt" in
    p) players_dir="$OPTARG" ;;
    r) rank_dir="$OPTARG" ;;
    h) usage ;;
    *) usage ;;
  esac
done

shift $((OPTIND - 1))

if [[ $# -eq 0 ]]; then
  echo "Error: rate_type is required" >&2
  usage
fi

rate_type="$1"

if ! command -v jq >/dev/null 2>&1; then
  echo "jq is required but not installed" >&2
  exit 1
fi

if [[ ! -d "${players_dir}" ]]; then
  echo "Players directory not found: ${players_dir}" >&2
  exit 1
fi

mkdir -p "${rank_dir}"
output_file="${rank_dir}/rate_${rate_type}.txt"

temp_file=$(mktemp)
trap 'rm -f "${temp_file}"' EXIT

# Process files using a while loop for safer variable handling
find "${players_dir}" -name '*.json' -type f | while read -r file; do
    jq -r --arg type "${rate_type}" '
      .stats as $s |
      (if $type == "win_rate" then
         (if $s.total_rounds > 0 then $s.win_count / $s.total_rounds else 0 end)
       elif $type == "tsumo_rate" then
         (if $s.win_count > 0 then $s.tsumo_win_count / $s.win_count else 0 end)
       elif $type == "deal_in_rate" then
         (if $s.total_rounds > 0 then $s.deal_in_count / $s.total_rounds else 0 end)
       elif $type == "tsumo_against_rate" then
         (if $s.total_rounds > 0 then $s.tsumo_against_count / $s.total_rounds else 0 end)
       else
         null
       end) as $val |
      if $val != null then
        "\($val)\t\(.name // "")\t\($s.total_rounds // 0)"
      else
        empty
      end
    ' "$file" 2>/dev/null || true
done >"${temp_file}"

if [[ ! -s "${temp_file}" ]]; then
  echo "Error: Invalid rate_type '${rate_type}' or no data found." >&2
  usage
fi

{
  case "${rate_type}" in
    win_rate)           title="Win Rate (和牌率) = win_count / total_rounds" ;;
    tsumo_rate)         title="Tsumo Rate (自摸率) = tsumo_win_count / win_count" ;;
    deal_in_rate)       title="Deal-in Rate (放铳率) = deal_in_count / total_rounds" ;;
    tsumo_against_rate) title="Tsumo-against Rate (被摸率) = tsumo_against_count / total_rounds" ;;
    *)                  title="Rate Ranking: ${rate_type}" ;;
  esac

  echo "======================================"
  echo "${title}"
  echo "Generated at: $(date '+%Y-%m-%d %H:%M:%S')"
  echo "Total players: $(wc -l < "${temp_file}")"
  echo "======================================"
  echo ""
  printf "%-6s\t%-12s\t%-25s\t%s\n" "Rank" "Rate" "Player Name" "Rounds"
  echo "------------------------------------------------------------"
  
  rank=1
  # Sort numerically descending
  sort -rn "${temp_file}" | while IFS=$'\t' read -r value name rounds filepath; do
    # Convert to percentage for display
    percentage=$(awk "BEGIN {printf \"%.2f%%\", $value * 100}")
    printf "%-6d\t%-12s\t%-25s\t%s\n" "$rank" "$percentage" "$name" "$rounds"
    ((rank++))
  done
} > "${output_file}"

echo "Ranking saved to: ${output_file}"
