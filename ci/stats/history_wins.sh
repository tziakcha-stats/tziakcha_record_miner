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

output_file="${rank_dir}/history_win.csv"

temp_file=$(mktemp)
trap 'rm -f "${temp_file}"' EXIT

# Extract all wins from all player files
find "${players_dir}" -name '*.json' -type f -print0 |
  xargs -0 -n1 bash -c '
    file="$1"
    
    jq -r '\''
      (.name // .player_id // "Unknown") as $pname | 
      (.player_id // "") as $pid |
      .wins[]? | 
      [.total_fan, .hand_raw, (.starting_hand_raw // ""), (.max_fans | map(.name) | join("、")), .win_type, $pname, $pid, .record_id] | @csv
    '\'' "$file" 2>/dev/null || true
  ' _ >"${temp_file}"

if [[ ! -s "${temp_file}" ]]; then
  echo "No wins found." >&2
  exit 1
fi

# Sort by total_fan (descending) and take top k
{
  echo "Rank,Fan,Hand,Starting Hand,Fan Types,Win Type,Player,Player ID,Record"
  
  rank=1
  # Sort by numeric value of the first column (Fan), descending.
  # Since it's CSV, we can use -t, -k1nr to sort by the first comma-separated field numerically reverse.
  # But the jq output puts quotes around strings, and @csv might quote numbers too depending on jq version, though usually not.
  # Let's inspect jq @csv output: 88,"hand","start","types","ron","name","id"
  # sort -t, -k1nr should work if the first field is a number.
  # Adjust sort command to handle CSV. Or rely on raw sort if numbers are simpler.
  # Actually, let's keep it simple: sort -t, -k1,1nr
  
  sort -t, -k1,1nr "${temp_file}" 2>/dev/null | head -n "${top_k}" 2>/dev/null | while read -r line; do
    echo "${rank},${line}"
    ((rank++))
  done || true
} > "${output_file}"

echo "Top wins ranking saved to: ${output_file}"
echo "Total wins processed: $(wc -l < "${temp_file}")"
exit 0
