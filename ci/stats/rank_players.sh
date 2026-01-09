#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
root_dir="$(cd -- "${script_dir}/../.." && pwd)"
players_dir="${root_dir}/data/player"
rank_dir="${root_dir}/data/rank"

keyword=""

debug_mode=false

while getopts "p:r:hd" opt; do
  case "$opt" in
    p) players_dir="$OPTARG" ;;
    r) rank_dir="$OPTARG" ;;
    d) debug_mode=true ;;
    h)
      echo "Usage: $0 [-p <players_dir>] [-r <rank_dir>] [-d] <keyword>"
      echo "Options:"
      echo "  -p <dir>   Players data directory (default: \${root_dir}/data/player)"
      echo "  -r <dir>   Rank output directory (default: \${root_dir}/data/rank)"
      echo "  -d         Debug mode: print all fields in the first found JSON file"
      echo "  -h         Show this help message"
      echo ""
      echo "Examples:"
      echo "  $0 average_step_seconds"
      echo "  $0 -p ./data/player -r ./rank stats.average_step_seconds"
      exit 0
      ;;
    *)
      echo "Invalid option: -$OPTARG" >&2
      exit 1
      ;;
  esac
done

shift $((OPTIND - 1))

if [[ "${debug_mode}" == "true" ]]; then
  if [[ ! -d "${players_dir}" ]]; then
    echo "Players directory not found: ${players_dir}" >&2
    exit 1
  fi
  
  first_file=$(find "${players_dir}" -name '*.json' -type f -print -quit)
  if [[ -z "${first_file}" ]]; then
    echo "No JSON files found in ${players_dir}" >&2
    exit 1
  fi

  echo "Debug: Listing all fields in ${first_file}"
  # Show all paths (keys flattened)
  jq -r '[paths | map(tostring) | join(".")] | .[]' "${first_file}"
fi

if [[ $# -eq 0 ]]; then
  echo "Error: keyword is required" >&2
  echo "Usage: $0 [-p <players_dir>] [-r <rank_dir>] [-d] <keyword>" >&2
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
output_file="${rank_dir}/${safe_keyword}.csv"

temp_file=$(mktemp)
trap 'rm -f "${temp_file}"' EXIT

find "${players_dir}" -name '*.json' -type f -print0 |
  xargs -0 -n1 sh -c '
    keyword="$1"
    file="$2"
    jq -r --arg key "$keyword" "
      getpath(\$key | split(\".\")) as \$val |
      select(\$val != null) |
      [\$val, (.name // \"\"), (.stats.total_rounds // 0), (.player_id // \"\")] | @csv
    " "$file" || true
  ' _ "${keyword}" >"${temp_file}"

if [[ ! -s "${temp_file}" ]]; then
  echo "No players with keyword '${keyword}' found." >&2
  exit 1
fi

{
  echo "Rank,Value,Player Name,Rounds,Player ID"
  
  rank=1
  # Sort numerically descending for most stats
  sort -t, -k1,1nr "${temp_file}" | while read -r line; do
    echo "${rank},${line}"
    ((rank++))
  done
} > "${output_file}"

echo "Ranking saved to: ${output_file}"