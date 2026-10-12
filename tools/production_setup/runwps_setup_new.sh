#!/usr/bin/env bash
# Independent download + WPS setup. Existing WRF/WPS runs are never touched.
set -euo pipefail
if [[ $# -lt 1 || $# -gt 3 ]]; then
  echo "Usage: $0 YYYYMMDDHH [new|control47] [NEW_RUN_ROOT]" >&2
  exit 2
fi
cycle="$1"
mode="${2:-new}"
root="${3:-/pool/wrf/WPS_new_runs/$cycle}"
[[ "$cycle" =~ ^[0-9]{10}$ ]] || { echo 'Expected YYYYMMDDHH' >&2; exit 2; }
[[ "$mode" == new || "$mode" == control47 ]] || { echo 'Expected new or control47' >&2; exit 2; }
if [[ "$(hostname -s)" == hpc00 ]]; then
  printf -v remote_command '%q ' /pool/wrf/runwps_setup_new.sh "$@"
  exec ssh -o BatchMode=yes hpc01 "$remote_command"
fi
[[ "$(hostname -s)" =~ ^hpc0[1-4]$ ]] || { echo 'Run this script from hpc00 or hpc01-04.' >&2; exit 2; }
unit="wps-setup-${mode}-${cycle}-$(date +%s)"
exec sudo -n systemd-run --unit="$unit" --uid="$(id -un)" --wait --pipe \
  -p CPUQuota=1600% -p MemoryMax=96G -p MemorySwapMax=0 -p LimitMEMLOCK=infinity \
  -p ReadOnlyPaths=/pool/wrf -p ReadWritePaths=/pool/wrf/WPS_new_runs \
  /usr/bin/taskset -c 32-47 /usr/bin/python3 /pool/wrf/WPS_new/tools/setup_run.py \
  "$cycle" --mode "$mode" --root "$root"
