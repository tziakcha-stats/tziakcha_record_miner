#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
root_dir="$(cd -- "${script_dir}/../.." && pwd)"
build_dir="${root_dir}/build"
data_dir="${root_dir}/data"
fetcher_cli="${build_dir}/src/fetcher/fetcher_cli"

show_usage() {
  cat <<EOF
Usage: $0 [OPTIONS]

Fetch game records for a date range in three steps:
  1. Fetch history data
  2. Extract sessions
  3. Fetch individual records

OPTIONS:
  -s, --start-date DATE    Start date in YYYYMMDD format (required)
  -e, --end-date DATE      End date in YYYYMMDD format (default: same as start date)
  -d, --data-dir PATH      Data directory (default: ${data_dir})
  -h, --help               Show this help message

EXAMPLES:
  # Fetch data for a single day
  $0 --start-date 20260101

  # Fetch data for a date range
  $0 --start-date 20260101 --end-date 20260103

  # Use custom data directory
  $0 -s 20260101 -d /path/to/data
EOF
}

log_info() {
  echo "[INFO] $*"
}

log_error() {
  echo "[ERROR] $*" >&2
}

start_date=""
end_date=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    -s|--start-date)
      start_date="$2"
      shift 2
      ;;
    -e|--end-date)
      end_date="$2"
      shift 2
      ;;
    -d|--data-dir)
      data_dir="$2"
      shift 2
      ;;
    -h|--help)
      show_usage
      exit 0
      ;;
    *)
      log_error "Unknown option: $1"
      show_usage
      exit 1
      ;;
  esac
done

if [[ -z "${start_date}" ]]; then
  log_error "Start date is required"
  show_usage
  exit 1
fi

if [[ ! "${start_date}" =~ ^[0-9]{8}$ ]]; then
  log_error "Start date must be in YYYYMMDD format"
  exit 1
fi

if [[ -z "${end_date}" ]]; then
  end_date="${start_date}"
fi

if [[ ! "${end_date}" =~ ^[0-9]{8}$ ]]; then
  log_error "End date must be in YYYYMMDD format"
  exit 1
fi

if [[ ! -x "${fetcher_cli}" ]]; then
  log_error "fetcher_cli not found or not executable: ${fetcher_cli}"
  log_error "Please build the project first: cd ${root_dir} && cmake --build build"
  exit 1
fi

if [[ ! -d "${data_dir}" ]]; then
  log_info "Creating data directory: ${data_dir}"
  mkdir -p "${data_dir}"
fi

if [[ "${start_date}" == "${end_date}" ]]; then
  date_range_key="${start_date}"
else
  date_range_key="${start_date}_${end_date}"
fi

history_file="history/history_${date_range_key}.json"
session_file="session/history_${date_range_key}.json"
records_dir="records/history_${date_range_key}"

log_info "==================================="
log_info "Fetching data for date range: ${start_date} to ${end_date}"
log_info "Data directory: ${data_dir}"
log_info "==================================="

log_info ""
log_info "Step 1/3: Fetching history data..."
if "${fetcher_cli}" history --start-date "${start_date}" --end-date "${end_date}" --data-dir "${data_dir}"; then
  log_info "✓ History data saved to: ${data_dir}/${history_file}"
else
  log_error "✗ Failed to fetch history data"
  exit 1
fi

log_info ""
log_info "Step 2/3: Extracting sessions..."
if [[ ! -f "${data_dir}/${history_file}" ]]; then
  log_error "History file not found: ${data_dir}/${history_file}"
  exit 1
fi

if "${fetcher_cli}" sessions -i "${history_file}" -o "session/history_${date_range_key}" --data-dir "${data_dir}"; then
  log_info "✓ Sessions saved to: ${data_dir}/${session_file}"
else
  log_error "✗ Failed to extract sessions"
  exit 1
fi

log_info ""
log_info "Step 3/3: Fetching individual records..."
if "${fetcher_cli}" records -i "${session_file}" -o "${records_dir}" --data-dir "${data_dir}"; then
  log_info "✓ Records saved to: ${data_dir}/${records_dir}/"
else
  log_error "✗ Failed to fetch records"
  exit 1
fi

log_info ""
log_info "==================================="
log_info "✓ All steps completed successfully"
log_info "==================================="
log_info "Output files:"
log_info "  History:  ${data_dir}/${history_file}"
log_info "  Sessions: ${data_dir}/${session_file}"
log_info "  Records:  ${data_dir}/${records_dir}/"
