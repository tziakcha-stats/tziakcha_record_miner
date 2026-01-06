#!/usr/bin/env bash
set -eo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
root_dir="$(cd -- "${script_dir}/../.." && pwd)"
players_dir="${root_dir}/data/player"
rank_dir="${root_dir}/data/rank"

top_k=100

while getopts "p:r:k:h" opt; do
  case "$opt" in
    p) players_dir="$OPTARG" ;;
    r) rank_dir="$OPTARG" ;;
    k) top_k="$OPTARG" ;;
    h)
      echo "Usage: $0 [-p <players_dir>] [-r <rank_dir>] [-k <top_k>]"
      echo "Options:"
      echo "  -p <dir>   Players data directory (default: \${root_dir}/data/player)"
      echo "  -r <dir>   Rank output directory (default: \${root_dir}/data/rank)"
      echo "  -k <num>   Top K wins to display (default: 100)"
      echo "  -h         Show this help message"
      echo ""
      echo "Examples:"
      echo "  $0"
      echo "  $0 -k 50"
      echo "  $0 -p ./data/player -r ./rank -k 200"
      exit 0
      ;;
    *)
      echo "Invalid option: -$OPTARG" >&2
      exit 1
      ;;
  esac
done

if ! command -v jq >/dev/null 2>&1; then
  echo "jq is required but not installed" >&2
  exit 1
fi

if [[ ! -d "${players_dir}" ]]; then
  echo "Players directory not found: ${players_dir}" >&2
  exit 1
fi

mkdir -p "${rank_dir}"

output_file="${rank_dir}/history_win.txt"

temp_file=$(mktemp)
trap 'rm -f "${temp_file}"' EXIT

# Extract all wins from all player files
find "${players_dir}" -name '*.json' -type f -print0 |
  xargs -0 -n1 bash -c '
    file="$1"
    player_id=$(jq -r ".player_id // empty" "$file")
    if [[ -z "$player_id" ]]; then
      exit 0
    fi
    
    jq -r --arg pid "$player_id" ".wins[]? | [.total_fan, .hand_raw, (.max_fans | map(.name) | join(\"、\")), .win_type, \$pid, .record_id] | @tsv" "$file" 2>/dev/null || true
  ' _ >"${temp_file}"

if [[ ! -s "${temp_file}" ]]; then
  echo "No wins found." >&2
  exit 1
fi

# Sort by total_fan (descending) and take top k
{
  echo "======================================"
  echo "Top ${top_k} Wins by Fan Count"
  echo "Generated at: $(date '+%Y-%m-%d %H:%M:%S')"
  echo "======================================"
  echo ""
  printf "%-5s\t%-6s\t%-30s\t%-25s\t%-8s\t%-8s\t%-10s\n" "Rank" "Fan" "Hand" "Fan Types" "Win Type" "Player" "Record"
  echo "--------------------------------------"
  
  rank=1
  sort -rn "${temp_file}" 2>/dev/null | head -n "${top_k}" 2>/dev/null | while IFS=$'\t' read -r fan_count hand_raw fan_types win_type player_id record_id; do
    # Truncate long fields for display
    hand_display="${hand_raw:0:30}"
    fan_display="${fan_types:0:25}"
    
    printf "%-5d\t%-6s\t%-30s\t%-25s\t%-8s\t%-8s\t%-10s\n" "$rank" "$fan_count" "$hand_display" "$fan_display" "$win_type" "$player_id" "$record_id"
    ((rank++))
  done || true
} > "${output_file}"

echo "Top wins ranking saved to: ${output_file}"
echo "Total wins processed: $(wc -l < "${temp_file}")"
exit 0
