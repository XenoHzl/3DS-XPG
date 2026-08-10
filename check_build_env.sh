#!/usr/bin/env bash
set -u

echo "=== XPerfect 3DS Downloader Build Check ==="

fail=0

check_file () {
  if [ -f "$1" ]; then
    echo "[OK] $1"
  else
    echo "[MISSING] $1"
    fail=1
  fi
}

if [ -z "${DEVKITPRO:-}" ]; then
  echo "[MISSING] DEVKITPRO environment variable"
  fail=1
else
  echo "[OK] DEVKITPRO=$DEVKITPRO"
  check_file "$DEVKITPRO/libnx/switch_rules"
fi

command -v aarch64-none-elf-g++ >/dev/null 2>&1 \
  && echo "[OK] aarch64-none-elf-g++" \
  || { echo "[MISSING] aarch64-none-elf-g++"; fail=1; }

command -v make >/dev/null 2>&1 \
  && echo "[OK] make" \
  || { echo "[MISSING] make"; fail=1; }

if [ -n "${DEVKITPRO:-}" ]; then
  check_file "$DEVKITPRO/portlibs/switch/include/curl/curl.h"
  # minizip header locations can vary slightly by package.
  if [ -f "$DEVKITPRO/portlibs/switch/include/minizip/unzip.h" ] || \
     [ -f "$DEVKITPRO/portlibs/switch/include/unzip.h" ]; then
    echo "[OK] minizip header"
  else
    echo "[MISSING] minizip header"
    fail=1
  fi
fi

if [ "$fail" -eq 0 ]; then
  echo
  echo "Environment looks ready. Run:"
  echo "  make clean"
  echo "  make -j\$(nproc)"
else
  echo
  echo "Environment is incomplete."
fi

exit "$fail"
