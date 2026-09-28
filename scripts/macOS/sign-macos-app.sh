#!/usr/bin/env bash
# Ad-hoc sign a macOS app bundle and optional runtime Mach-O files.

set -euo pipefail

if [ $# -lt 1 ]; then
  echo "Usage: $0 APP_PATH [EXTRA_MACHO ...]" >&2
  exit 2
fi

APP_PATH="$1"
shift

if [ ! -d "${APP_PATH}/Contents" ]; then
  echo "ERROR: app bundle not found: ${APP_PATH}" >&2
  exit 1
fi

remove_python_bytecode_caches() {
  local cache_dir
  local removed_count=0

  while IFS= read -r -d '' cache_dir; do
    rm -rf "$cache_dir"
    removed_count=$((removed_count + 1))
  done < <(find "${APP_PATH}" -type d -name "__pycache__" -print0)

  if [ "$removed_count" -gt 0 ]; then
    echo "  Removed ${removed_count} Python bytecode cache(s)"
  fi
}

remove_broken_symlinks() {
  local link
  local removed_count=0

  while IFS= read -r -d '' link; do
    if [ ! -e "$link" ]; then
      rm -f "$link"
      removed_count=$((removed_count + 1))
    fi
  done < <(find "${APP_PATH}" -type l -print0)

  if [ "$removed_count" -gt 0 ]; then
    echo "  Removed ${removed_count} broken symlink(s)"
  fi
}

is_macho() {
  file -b "$1" 2>/dev/null | grep -q "Mach-O"
}

should_sign_file() {
  local binary="$1"

  case "$binary" in
    "${APP_PATH}/Contents/MacOS/"*)
      return 0
      ;;
  esac

  is_macho "$binary"
}

sign_code_file() {
  local binary="$1"
  local output

  [ -f "$binary" ] || return 0
  if should_sign_file "$binary"; then
    if ! output="$(codesign --force --sign - "$binary" 2>&1)"; then
      echo "ERROR: codesign failed for ${binary}" >&2
      printf '%s\n' "$output" >&2
      return 1
    fi
    SIGNED_COUNT=$((SIGNED_COUNT + 1))
  fi
}

resolve_main_executable() {
  local name=""

  if [ -f "${APP_PATH}/Contents/Info.plist" ] && command -v plutil >/dev/null 2>&1; then
    name="$(plutil -extract CFBundleExecutable raw -o - \
      "${APP_PATH}/Contents/Info.plist" 2>/dev/null || true)"
  fi
  if [ -z "$name" ]; then
    name="$(basename "${APP_PATH}")"
    name="${name%.app}"
  fi

  printf '%s\n' "${APP_PATH}/Contents/MacOS/${name}"
}

remove_python_bytecode_caches
remove_broken_symlinks

MAIN_EXECUTABLE="$(resolve_main_executable)"

SIGNED_COUNT=0
# Sign nested code first. codesign validates the nested Mach-O files a bundle's
# main executable references, so signing Contents/MacOS/<exe> while a dylib in
# Contents/Frameworks is still unsigned fails with "code object is not signed at
# all". find walks Contents/MacOS before Contents/Frameworks, so the main
# executable is deliberately skipped here and signed last.
while IFS= read -r -d '' candidate; do
  if [ "$candidate" = "${MAIN_EXECUTABLE}" ]; then
    continue
  fi
  sign_code_file "$candidate"
done < <(find "${APP_PATH}/Contents" -type f -print0)

for extra_binary in "$@"; do
  sign_code_file "$extra_binary"
done

sign_code_file "${MAIN_EXECUTABLE}"

echo "  Signed ${SIGNED_COUNT} code files"
if ! BUNDLE_SIGN_OUTPUT="$(codesign --force --deep --sign - "${APP_PATH}" 2>&1)"; then
  echo "ERROR: codesign failed for the app bundle: ${APP_PATH}" >&2
  printf '%s\n' "${BUNDLE_SIGN_OUTPUT}" >&2
  exit 1
fi
codesign --verify --deep --strict "${APP_PATH}"
