#!/usr/bin/env bash
# Build a distributable macOS .app bundle + .dmg for PXTOOL.
#
# Usage:
#   bash scripts/macOS/package-macos.sh [--skip-build] [--no-dmg]
#
# Output:
#   build.macOS/PXTOOL.app   - standalone app bundle
#   build.macOS/PXTOOL.dmg   - DMG installer (unless --no-dmg)

set -euo pipefail

# Config
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
INSTALL_PREFIX="${ROOT}/package-root"
BUILD_APP="${ROOT}/build.macOS/PXTOOL.app"
PKG_ROOT="${INSTALL_PREFIX}/PXTOOL.app"
DIST_DIR="${ROOT}/build.macOS"
DIST_APP="${DIST_DIR}/PXTOOL.app"
FRAMEWORKS_DIR="${DIST_APP}/Contents/Frameworks"
DMG_OUT="${DIST_DIR}/PXTOOL.dmg"
SIGN_APP_SCRIPT="${ROOT}/scripts/macOS/sign-macos-app.sh"
DMG_STAGE_DIR=""

# Qt tool discovery is shared with build_and_run.sh. Resolve it up front so that
# qtpaths6 and macdeployqt are reachable regardless of how Qt was installed:
# Homebrew puts them on PATH, the official installer and aqtinstall do not.
# shellcheck source=scripts/macOS/qt6_env.sh
. "${SCRIPT_DIR}/qt6_env.sh"
require_qt6_tools_on_path "${ROOT}"

cleanup_dmg_stage() {
  if [ -n "$DMG_STAGE_DIR" ] && [ -d "$DMG_STAGE_DIR" ]; then
    rm -rf "$DMG_STAGE_DIR"
  fi
}

trap cleanup_dmg_stage EXIT

SKIP_BUILD=0
NO_DMG=0
for arg in "$@"; do
  case "$arg" in
    --skip-build) SKIP_BUILD=1 ;;
    --no-dmg)     NO_DMG=1 ;;
  esac
done

detach_mounted_dmg() {
  local image_path="$1"
  local device

  command -v hdiutil >/dev/null 2>&1 || return 0

  device=$(hdiutil info 2>/dev/null | awk -v image_path="$image_path" '
    /^[[:space:]]*image-path[[:space:]]*:/ {
      current = $0
      sub(/^[^:]*:[[:space:]]*/, "", current)
    }
    current == image_path && /^[[:space:]]*dev-entry[[:space:]]*:/ {
      device = $0
      sub(/^[^:]*:[[:space:]]*/, "", device)
      print device
      exit
    }
  ')

  if [ -n "$device" ]; then
    hdiutil detach "$device" >/dev/null 2>&1 || \
      hdiutil detach "$device" -force >/dev/null 2>&1 || true
  fi
}

remove_dmg_artifact() {
  local artifact="$1"

  if [ -e "$artifact" ]; then
    detach_mounted_dmg "$artifact"
    rm -f "$artifact"
  fi
}

restore_local_qt_framework_imports() {
  local executable="$1"
  local qtpaths qt_libs dependencies dependency framework_suffix local_dependency install_name

  qtpaths="$(command -v qtpaths6 || true)"
  if [ -z "$qtpaths" ]; then
    echo "ERROR: qtpaths6 was not found on PATH."
    return 1
  fi
  if ! qt_libs="$("$qtpaths" --query QT_INSTALL_LIBS 2>&1)"; then
    echo "ERROR: qtpaths6 could not report QT_INSTALL_LIBS."
    printf '%s\n' "$qt_libs"
    return 1
  fi
  if ! dependencies="$(otool -L "$executable" 2>&1)"; then
    echo "ERROR: otool could not inspect $executable."
    printf '%s\n' "$dependencies"
    return 1
  fi

  while IFS= read -r dependency; do
    case "$dependency" in
      @executable_path/../Frameworks/Qt*.framework/*)
        framework_suffix="${dependency#@executable_path/../Frameworks/}"
        ;;
      @rpath/Qt*.framework/*)
        framework_suffix="${dependency#@rpath/}"
        ;;
      "$qt_libs"/Qt*.framework/*)
        framework_suffix="${dependency#"$qt_libs"/}"
        ;;
      *)
        continue
        ;;
    esac

    local_dependency="$qt_libs/$framework_suffix"
    if [ ! -f "$local_dependency" ]; then
      echo "ERROR: local Qt framework dependency is missing: $local_dependency"
      return 1
    fi
    if ! install_name="$(otool -D "$local_dependency" 2>&1 | awk 'NR == 2 { print; exit }')" \
        || [ -z "$install_name" ]; then
      echo "ERROR: could not read Qt framework install name: $local_dependency"
      return 1
    fi
    install_name_tool -change "$dependency" "$install_name" "$executable"
  done < <(printf '%s\n' "$dependencies" | awk 'NR > 1 { print $1 }')
}

restore_bundled_python_imports() {
  local app="$1"
  local python_framework="$app/Contents/Frameworks/Python.framework"
  local version_dir python_library candidate replacement dependencies dependency

  for version_dir in "$python_framework"/Versions/[0-9]*; do
    [ -d "$version_dir" ] || continue
    python_library="$version_dir/Python"
    if [ -f "$python_library" ]; then
      install_name_tool -id \
        "@executable_path/../Frameworks/Python.framework/Versions/${version_dir##*/}/Python" \
        "$python_library"
    fi
    for candidate in \
      "$version_dir/bin/python${version_dir##*/}" \
      "$version_dir/Resources/Python.app/Contents/MacOS/Python"; do
      [ -f "$candidate" ] || continue
      case "$candidate" in
        */bin/*) replacement="@loader_path/../Python" ;;
        *) replacement="@loader_path/../../../../Python" ;;
      esac

      if ! dependencies="$(otool -L "$candidate" 2>&1)"; then
        echo "ERROR: otool could not inspect bundled Python helper: $candidate"
        printf '%s\n' "$dependencies"
        return 1
      fi
      while IFS= read -r dependency; do
        case "$dependency" in
          /opt/homebrew/*/Python.framework/Versions/*/Python|/usr/local/*/Python.framework/Versions/*/Python)
            install_name_tool -change "$dependency" "$replacement" "$candidate"
            ;;
        esac
      done < <(printf '%s\n' "$dependencies" | awk 'NR > 1 { print $1 }')
    done
  done
}

# Official Qt builds (online installer / aqtinstall) give their frameworks
# @rpath install names, so macdeployqt leaves plugin imports as @rpath/... and
# the plugin keeps its Qt-install rpath (@loader_path/../../lib), which does not
# exist inside the bundle. Homebrew's Qt uses absolute install names, so its
# plugins get rewritten to @executable_path/../Frameworks instead and never hit
# this. Point every bundled plugin at Contents/Frameworks so the bundle resolves
# on its own instead of relying on the main executable's run-path list.
# macdeployqt rewrites Homebrew dependencies for Mach-O files it can reach by
# walking load commands from the main executable. The C decoders under
# Resources/share/libsigrokdecode/decoders/c_decoders are dlopen'd at runtime,
# so it never sees them and they keep their absolute /usr/local/opt/glib/...
# dependency. Two consequences, both bad:
#
#   * On a machine without Homebrew glib the decoders fail to dlopen at all, so
#     every "(C)" protocol silently disappears.
#   * On a machine that has it, the process ends up with TWO copies of glib --
#     the bundled one for the app, the Homebrew one for the decoders. Each copy
#     owns a private GVariantTypeInfo table, so an option GVariant created
#     inside a decoder (srd_c_decoder_entry -> g_variant_new_string) is
#     unreadable by the app: g_variant_is_of_type() from the other copy fails
#     g_variant_type_info_check() and aborts. That is the crash seen when
#     clicking a C decoder in the Decode dock (DecoderOptions -> bind_enum ->
#     print_gvariant).
#
# Point these modules at the bundled copy instead, using @loader_path so the
# depth of the module inside the bundle does not have to be hardcoded.
rebind_dlopened_module_deps() {
  local app="$1"
  local frameworks="$app/Contents/Frameworks"
  local module_dir="$app/Contents/Resources/share/libsigrokdecode/decoders/c_decoders"
  local module file_description relative up depth dep base rebound=0 scanned=0

  if [ ! -d "$module_dir" ]; then
    echo "  No dlopen'd C decoder modules to rebind"
    return 0
  fi

  while IFS= read -r -d '' module; do
    if ! file_description="$(file -b "$module" 2>&1)"; then
      echo "ERROR: file could not inspect C decoder module: $module"
      printf '%s\n' "$file_description"
      return 1
    fi
    [[ "$file_description" == *Mach-O* ]] || continue
    scanned=$((scanned + 1))

    # Number of directory levels between Contents/ and this module.
    relative="${module#"$app/Contents/"}"
    depth="$(printf '%s' "${relative%/*}" | awk -F/ '{print NF}')"
    up=""
    while [ "$depth" -gt 0 ]; do
      up="../$up"
      depth=$((depth - 1))
    done

    while IFS= read -r dep; do
      case "$dep" in
        /usr/local/*|/opt/homebrew/*) ;;
        *) continue ;;
      esac
      base="${dep##*/}"
      # Only redirect what is actually bundled; anything else must stay visible
      # as a missing dependency rather than be silently pointed at nothing.
      [ -f "$frameworks/$base" ] || continue
      if ! install_name_tool -change "$dep" "@loader_path/${up}Frameworks/$base" \
        "$module" 2>/dev/null; then
        echo "ERROR: could not rebind $base in $module"
        return 1
      fi
      rebound=$((rebound + 1))
    done < <(otool -L "$module" 2>/dev/null | awk 'NR > 1 { print $1 }')
  done < <(find "$module_dir" -type f -print0)

  echo "  Rebound $rebound dependencies across $scanned dlopen'd C decoder modules"
}

# macdeployqt rewrites a dependency only when it can resolve the path the
# dependency is written as. libbrotlidec names its companion
# @rpath/libbrotlicommon.1.dylib rather than by an absolute Homebrew path, so
# macdeployqt copied libbrotlicommon into Contents/Frameworks but never rewrote
# its LC_ID_DYLIB -- the one stale identity among 35 bundled dylibs, still
# reading /opt/homebrew/opt/brotli/lib/libbrotlicommon.1.dylib.
#
# This did not break the app on its own: libbrotlidec had already been rewritten
# to @executable_path/../Frameworks/libbrotlicommon.1.dylib, so the bundled copy
# is what dyld actually maps (confirmed with vmmap). An absolute identity is
# still wrong to ship, because a linker records a library's install name in
# whatever links against it -- so anything built later against the bundled
# library would reference /opt/homebrew, and a Homebrew machine would end up
# with two copies of it in one process. That is the failure mode the dlopen'd
# decoders above already demonstrated for glib.
#
# Give every bundled Mach-O a bundle-relative identity. Also pin @rpath
# references that name a plain dylib we ship: macdeployqt does not always rewrite
# those (older output left libbrotlidec and the libwebp family pointing at
# @rpath/...), and an @rpath reference is resolved against the run-path list of
# the whole loading chain, where the main executable still lists /opt/homebrew/lib
# ahead of @executable_path/../Frameworks. Pinning removes that ambiguity.
# @executable_path is used rather than @loader_path because it does not depend on
# how deep the file sits in the bundle.
#
# @rpath references naming a framework (@rpath/QtCore.framework/Versions/A/
# QtCore) are deliberately left alone: those are macdeployqt's own convention
# and resolve through the rpaths it and ensure_bundled_plugin_rpaths set up.
normalize_bundled_macho_identities() {
  local app="$1"
  local frameworks="$app/Contents/Frameworks"
  local macho file_description install_name base dep ids=0 pinned=0

  [ -d "$frameworks" ] || return 0

  while IFS= read -r -d '' macho; do
    if ! file_description="$(file -b "$macho" 2>&1)"; then
      echo "ERROR: file could not inspect bundled Mach-O: $macho"
      printf '%s\n' "$file_description"
      return 1
    fi
    [[ "$file_description" == *Mach-O* ]] || continue

    # LC_ID_DYLIB. Empty for executables and bundle/module files, which have no
    # identity to fix.
    install_name="$(otool -D "$macho" 2>/dev/null | awk 'NR == 2 { print; exit }')"
    case "$install_name" in
      /opt/homebrew/*|/usr/local/*|/Users/*)
        # Derive the identity from where the file actually sits, not from the
        # basename of the stale name: a Mach-O inside a framework has to keep
        # its full QtFoo.framework/Versions/A/QtFoo suffix, otherwise dyld
        # looks for a bare QtFoo next to the other dylibs and finds nothing.
        install_name="@executable_path/../${macho#"$app/Contents/"}"
        if ! install_name_tool -id "$install_name" "$macho" 2>/dev/null; then
          echo "ERROR: could not set a bundle-relative id on $macho"
          return 1
        fi
        echo "  Bundle-relative id: ${macho#"$app/"} -> $install_name"
        ids=$((ids + 1))
        ;;
    esac

    while IFS= read -r dep; do
      # otool -L lists LC_ID_DYLIB alongside the dependencies; skip it so the
      # identity is not mistaken for a link against itself.
      [ -n "$install_name" ] && [ "$dep" = "$install_name" ] && continue
      case "$dep" in
        @rpath/*) ;;
        *) continue ;;
      esac
      base="${dep#@rpath/}"
      case "$base" in
        */*) continue ;;  # framework import, not a plain dylib
      esac
      [ -f "$frameworks/$base" ] || continue
      if ! install_name_tool -change "$dep" \
        "@executable_path/../Frameworks/$base" "$macho" 2>/dev/null; then
        echo "ERROR: could not pin $dep in $macho"
        return 1
      fi
      echo "  Pinned ${macho#"$app/"}: $dep -> @executable_path/../Frameworks/$base"
      pinned=$((pinned + 1))
    done < <(otool -L "$macho" 2>/dev/null | awk 'NR > 1 { print $1 }')
  done < <(find "$app" -type f -print0)

  echo "  Normalized $ids bundled dylib identities, pinned $pinned @rpath references"
}

ensure_bundled_plugin_rpaths() {
  local app="$1"
  local plugins_dir="$app/Contents/PlugIns"
  local candidate file_description relative depth up rpath existing

  [ -d "$plugins_dir" ] || return 0

  while IFS= read -r -d '' candidate; do
    if ! file_description="$(file -b "$candidate" 2>&1)"; then
      echo "ERROR: file could not inspect plugin candidate: $candidate"
      printf '%s\n' "$file_description"
      return 1
    fi
    [[ "$file_description" == *Mach-O* ]] || continue

    relative="${candidate#"$app/Contents/"}"
    depth="$(printf '%s' "$relative" | tr -cd '/' | wc -c | tr -d ' ')"
    up=""
    while [ "$depth" -gt 0 ]; do
      up="../$up"
      depth=$((depth - 1))
    done
    rpath="@loader_path/${up}Frameworks"

    existing="$(otool -l "$candidate" 2>/dev/null | awk '
      $1 == "cmd" && $2 == "LC_RPATH" { found = 1; next }
      found && $1 == "path" { print $2; found = 0 }
    ')"
    if printf '%s\n' "$existing" | grep -qx -- "$rpath"; then
      continue
    fi
    if ! install_name_tool -add_rpath "$rpath" "$candidate" 2>/dev/null; then
      echo "ERROR: could not add bundle rpath to plugin: $candidate"
      return 1
    fi
    echo "  Added bundle rpath to $relative: $rpath"
  done < <(find "$plugins_dir" -type f \( -perm -111 -o -name '*.dylib' \) -print0)
}

cleanup_dmg_artifacts() {
  local dmg_out="$1"
  local dmg_dir
  local dmg_name
  local stale_rw

  dmg_dir="$(dirname "$dmg_out")"
  dmg_name="$(basename "$dmg_out")"

  remove_dmg_artifact "$dmg_out"
  for stale_rw in "$dmg_dir"/rw.*."$dmg_name"; do
    [ -e "$stale_rw" ] || continue
    remove_dmg_artifact "$stale_rw"
  done
}

is_expected_macho_candidate() {
  local candidate="$1"
  local app="$2"
  local relative framework_root framework_name

  if [ "$candidate" = "$app/Contents/MacOS/PXTOOL" ]; then
    return 0
  fi

  case "$candidate" in
    "$app/Contents/MacOS/"*)
      relative="${candidate#"$app/Contents/MacOS/"}"
      [[ "$relative" != */* ]]
      return
      ;;
    "$app/Contents/Frameworks/"*|"$app/Contents/PlugIns/"*)
      case "$candidate" in
        *.dylib|*.bundle|*.so)
          return 0
          ;;
      esac
      if [[ "$candidate" == *.framework/* ]]; then
        framework_root="${candidate%%.framework/*}.framework"
        framework_name="${framework_root##*/}"
        framework_name="${framework_name%.framework}"
        [ "${candidate##*/}" = "$framework_name" ]
        return
      fi
      ;;
  esac

  return 1
}

find_framework_info_plist() {
  local framework="$1"
  local plist

  for plist in \
    "$framework/Resources/Info.plist" \
    "$framework/Versions/Current/Resources/Info.plist"; do
    if [ -f "$plist" ]; then
      printf '%s\n' "$plist"
      return 0
    fi
  done

  plist="$(find -L "$framework" -type f -path '*/Resources/Info.plist' -print -quit 2>/dev/null || true)"
  if [ -n "$plist" ]; then
    printf '%s\n' "$plist"
    return 0
  fi
  return 1
}

read_plist_value() {
  local plist="$1"
  local value

  if command -v plutil >/dev/null 2>&1; then
    if value="$(plutil -extract CFBundleShortVersionString raw -o - "$plist" 2>/dev/null)"; then
      printf '%s\n' "$value"
      return 0
    fi
    if value="$(plutil -extract CFBundleVersion raw -o - "$plist" 2>/dev/null)"; then
      printf '%s\n' "$value"
      return 0
    fi
  fi

  if [ -x /usr/libexec/PlistBuddy ]; then
    if value="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$plist" 2>/dev/null)"; then
      printf '%s\n' "$value"
      return 0
    fi
    if value="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$plist" 2>/dev/null)"; then
      printf '%s\n' "$value"
      return 0
    fi
  fi

  return 1
}

verify_qt_framework() {
  local framework_dir="$1"
  local framework_name framework_lower plist version

  framework_name="${framework_dir##*/}"

  framework_lower="$(printf '%s' "$framework_name" | tr '[:upper:]' '[:lower:]')"

  if [[ "$framework_lower" =~ ^qt[0-9]+ ]] \
      && [[ ! "$framework_lower" =~ ^qt6([^0-9]|$) ]]; then
    echo "ERROR: non-Qt6 framework imported or bundled: $framework_name"
    return 1
  fi

  if ! plist="$(find_framework_info_plist "$framework_dir")"; then
    echo "ERROR: Qt framework has no readable Info.plist: $framework_dir"
    return 1
  fi
  if ! version="$(read_plist_value "$plist")"; then
    echo "ERROR: could not read Qt framework version: $framework_dir"
    return 1
  fi
  if [[ ! "$version" =~ ^6[.][0-9]+([.][0-9]+)?$ ]]; then
    echo "ERROR: Qt framework is not version 6.x: $framework_dir ($version)"
    return 1
  fi
}

resolve_macho_framework_import() {
  local candidate="$1"
  local dependency="$2"
  local candidate_dir executable_dir rpath rpaths resolved

  candidate_dir="$(cd "$(dirname "$candidate")" && pwd -P)"
  executable_dir="$(cd "$FRAMEWORKS_DIR/../MacOS" && pwd -P)"

  case "$dependency" in
    @rpath/*)
      rpaths="$(otool -l "$candidate" 2>/dev/null | awk '
        $1 == "cmd" && $2 == "LC_RPATH" { found = 1; next }
        found && $1 == "path" { print $2; found = 0 }
      ')"
      while IFS= read -r rpath; do
        [ -n "$rpath" ] || continue
        case "$rpath" in
          @loader_path/*) rpath="$candidate_dir/${rpath#@loader_path/}" ;;
          @executable_path/*) rpath="$executable_dir/${rpath#@executable_path/}" ;;
          /*) ;;
          *) continue ;;
        esac
        resolved="$rpath/${dependency#@rpath/}"
        if [ -e "$resolved" ]; then
          printf '%s/%s\n' "$(cd "$(dirname "$resolved")" && pwd -P)" "$(basename "$resolved")"
          return 0
        fi
      done <<<"$rpaths"
      ;;
    @loader_path/*)
      resolved="$candidate_dir/${dependency#@loader_path/}"
      ;;
    @executable_path/*)
      resolved="$executable_dir/${dependency#@executable_path/}"
      ;;
    /*)
      resolved="$dependency"
      ;;
    *)
      return 1
      ;;
  esac

  if [ -e "$resolved" ]; then
    printf '%s/%s\n' "$(cd "$(dirname "$resolved")" && pwd -P)" "$(basename "$resolved")"
    return 0
  fi
  return 1
}

verify_framework_import() {
  local candidate="$1"
  local dependency="$2"
  local framework_path="$3"
  local resolved_dependency framework_dir frameworks_dir framework_name framework_lower

  case "$dependency" in
    /System/Library/Frameworks/*)
      return 0
      ;;
  esac

  if ! resolved_dependency="$(resolve_macho_framework_import "$candidate" "$dependency")"; then
    echo "ERROR: external or unresolved framework import in Mach-O candidate: $candidate ($dependency)"
    return 1
  fi

  framework_dir="${resolved_dependency%%.framework/*}.framework"
  frameworks_dir="$(cd "$FRAMEWORKS_DIR" && pwd -P)"
  case "$framework_dir" in
    "$frameworks_dir"/*) ;;
    *)
      echo "ERROR: external framework import in Mach-O candidate: $candidate ($dependency)"
      return 1
      ;;
  esac

  framework_name="${framework_path##*/}"
  framework_lower="$(printf '%s' "$framework_name" | tr '[:upper:]' '[:lower:]')"
  if [[ "$framework_lower" == qt*.framework ]]; then
    verify_qt_framework "$framework_dir"
  fi
}

verify_macho_file() {
  local candidate="$1"
  local require_qt="${2:-0}"
  local macho_dependencies dependency dependency_lower framework_path framework_name qt_major
  local qt_import_found=0
  local dependency_list

  if [ ! -r "$candidate" ]; then
    echo "ERROR: Mach-O candidate is not readable: $candidate"
    return 1
  fi
  if ! macho_dependencies="$(otool -L "$candidate" 2>&1)"; then
    echo "ERROR: otool could not inspect Mach-O candidate: $candidate"
    printf '%s\n' "$macho_dependencies"
    return 1
  fi

  dependency_list="$(printf '%s\n' "$macho_dependencies" | awk 'NR > 1 { print $1 }')"
  while IFS= read -r dependency; do
    [ -n "$dependency" ] || continue
    dependency_lower="$(printf '%s' "$dependency" | tr '[:upper:]' '[:lower:]')"

    if [[ "$dependency_lower" =~ qt[@_-]?([0-9]+) ]]; then
      qt_major="${BASH_REMATCH[1]}"
      if [ "$qt_major" != 6 ]; then
        echo "ERROR: non-Qt6 import in Mach-O candidate: $candidate ($dependency)"
        return 1
      fi
    fi

    if [[ "$dependency" =~ (.*[.]framework)(/|$) ]]; then
      framework_path="${BASH_REMATCH[1]}"
      framework_name="${framework_path##*/}"
      if ! verify_framework_import "$candidate" "$dependency" "$framework_path"; then
        return 1
      fi
      if [[ "$(printf '%s' "$framework_name" | tr '[:upper:]' '[:lower:]')" == qt*.framework ]]; then
        qt_import_found=1
      fi
    elif [[ "$dependency_lower" =~ (^|/)(lib)?qt[0-9]+ ]]; then
      if [[ ! "$dependency_lower" =~ (^|/)(lib)?qt6([^0-9]|$) ]]; then
        echo "ERROR: non-Qt6 import in Mach-O candidate: $candidate ($dependency)"
        return 1
      fi
      qt_import_found=1
    elif [[ "$dependency_lower" =~ (^|/)(lib)?qt[^/]*[.]dylib$ ]]; then
      echo "ERROR: unversioned Qt dylib cannot be verified as Qt6: $candidate ($dependency)"
      return 1
    fi
  done <<<"$dependency_list"

  if [ "$require_qt" -eq 1 ] && [ "$qt_import_found" -eq 0 ]; then
    echo "ERROR: main PXTOOL executable does not import Qt."
    return 1
  fi
}

verify_macos_qt_bundle() {
  local app="$1"
  local main_executable="$app/Contents/MacOS/PXTOOL"
  local framework_dir framework_name framework_lower candidate file_description require_qt
  local qt_framework_count=0

  if [ ! -r "$main_executable" ]; then
    echo "ERROR: main PXTOOL executable is missing or unreadable: $main_executable"
    return 1
  fi
  if ! command -v file >/dev/null 2>&1 || ! command -v otool >/dev/null 2>&1; then
    echo "ERROR: file and otool are required to validate the macOS bundle."
    return 1
  fi
  if ! command -v plutil >/dev/null 2>&1 && [ ! -x /usr/libexec/PlistBuddy ]; then
    echo "ERROR: plutil or PlistBuddy is required to validate Qt framework versions."
    return 1
  fi
  if [ ! -d "$FRAMEWORKS_DIR" ]; then
    echo "ERROR: Qt framework directory is missing: $FRAMEWORKS_DIR"
    return 1
  fi

  while IFS= read -r -d '' framework_dir; do
    framework_name="${framework_dir##*/}"
    framework_lower="$(printf '%s' "$framework_name" | tr '[:upper:]' '[:lower:]')"
    if [[ "$framework_lower" == qt*.framework ]]; then
      if ! verify_qt_framework "$framework_dir"; then
        return 1
      fi
      qt_framework_count=$((qt_framework_count + 1))
    fi
  done < <(find -L "$FRAMEWORKS_DIR" -type d -name '*.framework' -print0)

  if [ "$qt_framework_count" -eq 0 ]; then
    echo "ERROR: no Qt6 framework was found in the app bundle."
    return 1
  fi

  if ! find "$app" -type f \( \
    -perm -111 -o -name '*.dylib' -o -name '*.so' -o -name '*.bundle' \
  \) -print0 | while IFS= read -r -d '' candidate; do
    if [ ! -r "$candidate" ]; then
      echo "ERROR: bundle file is not readable: $candidate"
      exit 1
    fi
    if ! file_description="$(file -b "$candidate" 2>&1)"; then
      echo "ERROR: file could not inspect bundle candidate: $candidate"
      printf '%s\n' "$file_description"
      exit 1
    fi
    if [[ "$file_description" == *Mach-O* ]]; then
      require_qt=0
      if [ "$candidate" = "$main_executable" ]; then
        require_qt=1
      fi
      if ! verify_macho_file "$candidate" "$require_qt"; then
        exit 1
      fi
    elif is_expected_macho_candidate "$candidate" "$app"; then
      echo "ERROR: expected Mach-O candidate is invalid: $candidate"
      printf '%s\n' "$file_description"
      exit 1
    fi
  done; then
    return 1
  fi

  local legacy_qt_artifact legacy_qt_scan_status
  if legacy_qt_artifact="$(find -L "$app" -type f \( \
    -iname '*qt[0-9]*' -o -ipath '*qt[0-9]*' \
  \) ! -ipath '*qt6*' -print -quit 2>&1)"; then
    :
  else
    legacy_qt_scan_status=$?
    echo "ERROR: failed to scan app bundle for legacy Qt artifacts (status $legacy_qt_scan_status)."
    printf '%s\n' "$legacy_qt_artifact"
    return "$legacy_qt_scan_status"
  fi
  if [ -n "$legacy_qt_artifact" ]; then
    echo "ERROR: non-Qt6 Qt artifact remains in app bundle: $legacy_qt_artifact"
    return 1
  fi
}

restore_macos_system_framework_imports() {
  local app="$1"
  local candidate file_description macho_dependencies dependency scan_root
  local framework_name framework_suffix system_framework system_dependency
  local -a scan_roots=()

  for scan_root in \
    "$app/Contents/MacOS" \
    "$app/Contents/Frameworks" \
    "$app/Contents/PlugIns"; do
    [ -e "$scan_root" ] && scan_roots+=("$scan_root")
  done

  while IFS= read -r -d '' candidate; do
    if ! file_description="$(file -b "$candidate" 2>&1)"; then
      echo "ERROR: file could not inspect bundle candidate: $candidate"
      printf '%s\n' "$file_description"
      return 1
    fi
    [[ "$file_description" == *Mach-O* ]] || continue

    if ! macho_dependencies="$(otool -L "$candidate" 2>&1)"; then
      echo "ERROR: otool could not inspect Mach-O candidate: $candidate"
      printf '%s\n' "$macho_dependencies"
      return 1
    fi

    while IFS= read -r dependency; do
      [ -n "$dependency" ] || continue
      case "$dependency" in
        @rpath/*.framework/*)
          framework_name="${dependency#@rpath/}"
          framework_name="${framework_name%%/*}"
          framework_suffix="${dependency#@rpath/$framework_name/}"
          ;;
        *)
          continue
          ;;
      esac

      if [ -d "$app/Contents/Frameworks/$framework_name" ]; then
        continue
      fi

      system_framework="/System/Library/Frameworks/$framework_name"
      [ -d "$system_framework" ] || continue
      system_dependency="$system_framework/$framework_suffix"

      if ! install_name_tool -change "$dependency" "$system_dependency" "$candidate"; then
        echo "ERROR: could not restore system framework import in: $candidate ($dependency)"
        return 1
      fi
      echo "  Restored system framework: $dependency -> $system_dependency"
    done < <(printf '%s\n' "$macho_dependencies" | awk 'NR > 1 { print $1 }')
  done < <(find "${scan_roots[@]}" -type f \( \
    -perm -111 -o -name '*.dylib' -o -name '*.so' -o -name '*.bundle' \
  \) -print0)
}

# Step 1: Build
if [ $SKIP_BUILD -eq 0 ]; then
  echo "[1/6] Building PXTOOL..."
  cd "$ROOT"
  cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" .
  make -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 8)"
  cmake --build "$ROOT" --target stage_webui --parallel 1
  cmake --install .
else
  echo "[1/6] Skipping build/install (--skip-build)"
  if [ ! -f "$ROOT/web/dist/index.html" ]; then
    echo "ERROR: --skip-build was used but web/dist/index.html is missing."
    echo "       Run without --skip-build, or run: cmake --build $ROOT --target stage_webui"
    exit 1
  fi
fi

# Step 2: Assemble bundle
echo "[2/6] Assembling app bundle..."
mkdir -p "$DIST_DIR"

if [ "$BUILD_APP" != "$DIST_APP" ]; then
  rm -rf "$DIST_APP"
  cp -R "$BUILD_APP" "$DIST_APP"
elif [ ! -d "$DIST_APP" ]; then
  echo "ERROR: built app not found at $DIST_APP"
  exit 1
else
  # Keep non-Qt frameworks such as Python, but force macdeployqt to rebuild
  # the Qt frameworks and plugin tree instead of reusing stale deployment data.
  restore_local_qt_framework_imports "$DIST_APP/Contents/MacOS/PXTOOL"
  rm -rf "$DIST_APP/Contents/PlugIns"
  if [ -d "$FRAMEWORKS_DIR" ]; then
    find "$FRAMEWORKS_DIR" -type d -name 'Qt*.framework' -prune -exec rm -rf {} +
    find "$FRAMEWORKS_DIR" -type f -iname '*qt*.dylib' -exec rm -f {} +
  fi
fi

# The build tree may contain symlinks back into package-root (e.g. share ->).
# Remove them before overlaying the real resources so cp doesn't see identical inodes.
find "$DIST_APP/Contents/Resources" -type l -delete

# Copy data resources from package-root (res/, decoders, lang, demo, etc.)
cp -R "$PKG_ROOT/Contents/Resources/" "$DIST_APP/Contents/Resources/"

if [ ! -f "$DIST_APP/Contents/MacOS/webui/index.html" ]; then
  echo "ERROR: MCP browser Web Console missing from app bundle at Contents/MacOS/webui/index.html"
  exit 1
fi

# Bundle Python before macdeployqt scans dependencies. Homebrew's Python
# framework exposes compatibility paths that macdeployqt may not resolve, so
# make the app point at the in-bundle framework first.
echo "[3/6] Bundling Python.framework and running macdeployqt..."
mkdir -p "$FRAMEWORKS_DIR"
install_name_tool -add_rpath "@executable_path/../Frameworks" \
  "$DIST_APP/Contents/MacOS/PXTOOL" 2>/dev/null || true

# Match both Homebrew prefixes: /opt/homebrew (Apple Silicon) and /usr/local
# (Intel). restore_bundled_python_imports() below already handles both.
PY_HOMEBREW_LIB=$(otool -L "$DIST_APP/Contents/MacOS/PXTOOL" 2>/dev/null \
  | grep -E "(/opt/homebrew|/usr/local).*Python.framework.*/Python" | awk '{print $1}' || true)

if [ -n "$PY_HOMEBREW_LIB" ]; then
  PY_VERSION=$(echo "$PY_HOMEBREW_LIB" | grep -oE "Versions/[0-9.]+" | head -1 | cut -d/ -f2)
  PY_FRAMEWORK_SRC=$(echo "$PY_HOMEBREW_LIB" | sed 's|/Versions/.*||')
  PY_DEST="$FRAMEWORKS_DIR/Python.framework"

  echo "  Found Python ${PY_VERSION} at: $PY_FRAMEWORK_SRC"
  rm -rf "$PY_DEST"
  mkdir -p "$PY_DEST/Versions/${PY_VERSION}"
  cp -R "$PY_FRAMEWORK_SRC/Versions/${PY_VERSION}/." "$PY_DEST/Versions/${PY_VERSION}/"
  ln -sf "${PY_VERSION}" "$PY_DEST/Versions/Current"
  ln -sf "Versions/Current/Python" "$PY_DEST/Python"
  ln -sf "Versions/Current/Resources" "$PY_DEST/Resources"
  chmod +w "$PY_DEST/Versions/${PY_VERSION}/Python"

  install_name_tool -id \
    "@rpath/Python.framework/Versions/${PY_VERSION}/Python" \
    "$PY_DEST/Versions/${PY_VERSION}/Python"
  install_name_tool -change \
    "$PY_HOMEBREW_LIB" \
    "@rpath/Python.framework/Versions/${PY_VERSION}/Python" \
    "$DIST_APP/Contents/MacOS/PXTOOL"
else
  echo "  No Homebrew Python.framework reference found."
fi

restore_bundled_python_imports "$DIST_APP"

# Step 3: macdeployqt - bundle Qt frameworks
MACDEPLOYQT="$(command -v macdeployqt || true)"
if [ -z "$MACDEPLOYQT" ]; then
  echo "ERROR: macdeployqt was not found on PATH."
  exit 1
fi
QTPATHS6="$(dirname "$MACDEPLOYQT")/qtpaths6"
if [ ! -x "$QTPATHS6" ]; then
  echo "ERROR: qtpaths6 was not found next to macdeployqt: $QTPATHS6"
  exit 1
fi
if MACDEPLOYQT_VERSION="$("$QTPATHS6" --qt-version 2>&1)"; then
  :
else
  MACDEPLOYQT_VERSION_STATUS=$?
  echo "ERROR: qtpaths6 --qt-version failed (status $MACDEPLOYQT_VERSION_STATUS)."
  printf '%s\n' "$MACDEPLOYQT_VERSION"
  exit "$MACDEPLOYQT_VERSION_STATUS"
fi
if ! printf '%s\n' "$MACDEPLOYQT_VERSION" \
    | grep -Eq '^[[:space:]]*6([.][0-9]+){1,2}[[:space:]]*$'; then
  echo "ERROR: macdeployqt 6 is required."
  printf '%s\n' "$MACDEPLOYQT_VERSION"
  exit 1
fi
MACDEPLOYQT_LOG="$(mktemp)"
MACDEPLOYQT_ARGS=("$DIST_APP" -verbose=1)
# macdeployqt only learned -no-codesign in Qt 6.9. Older tools (the newest Qt
# available on macOS 12 is 6.7) never sign the bundle themselves, so the flag is
# a no-op there and must be omitted to avoid "Unknown argument".
MACDEPLOYQT_HELP="$("$MACDEPLOYQT" 2>&1 || true)"
if printf '%s\n' "$MACDEPLOYQT_HELP" | grep -q -- '-no-codesign'; then
  MACDEPLOYQT_ARGS+=(-no-codesign)
fi
for libpath in /opt/homebrew/lib /opt/homebrew/Frameworks /usr/local/lib /usr/local/Frameworks; do
  if [ -d "$libpath" ]; then
    MACDEPLOYQT_ARGS+=("-libpath=$libpath")
  fi
done
if ! "$MACDEPLOYQT" "${MACDEPLOYQT_ARGS[@]}" >"$MACDEPLOYQT_LOG" 2>&1; then
  cat "$MACDEPLOYQT_LOG"
  rm -f "$MACDEPLOYQT_LOG"
  exit 1
fi
awk '
  /QtPdf\.framework|QtVirtualKeyboard(Qml)?\.framework/ { skip_next = 1; next }
  skip_next && /using QList/ { skip_next = 0; next }
  { skip_next = 0; print }
' "$MACDEPLOYQT_LOG"
rm -f "$MACDEPLOYQT_LOG"

# macdeployqt deploys broad plugin sets. PXTOOL does not use these optional
# plugins, and they can drag in optional Homebrew Qt frameworks.
for plugin in \
  "$DIST_APP/Contents/PlugIns/imageformats/libqpdf.dylib" \
  "$DIST_APP/Contents/PlugIns/platforminputcontexts/libqtvirtualkeyboardplugin.dylib"; do
  if [ -f "$plugin" ]; then
    rm -f "$plugin"
    echo "  Removed optional plugin: ${plugin#$DIST_APP/Contents/PlugIns/}"
  fi
done

for framework in QtQml QtQmlMeta QtQmlModels QtQmlWorkerScript QtQuick; do
  if [ -d "$FRAMEWORKS_DIR/$framework.framework" ]; then
    rm -rf "$FRAMEWORKS_DIR/$framework.framework"
    echo "  Removed optional framework: $framework.framework"
  fi
done

restore_macos_system_framework_imports "$DIST_APP"
ensure_bundled_plugin_rpaths "$DIST_APP"
rebind_dlopened_module_deps "$DIST_APP"
normalize_bundled_macho_identities "$DIST_APP"

# Step 4: Ensure rpath is set (macdeployqt handles Qt + most dylibs)
echo "[4/6] Verifying rpath and macdeployqt-bundled dylibs..."

# Ensure @executable_path/../Frameworks is in rpath for non-Qt libs
install_name_tool -add_rpath "@executable_path/../Frameworks" \
  "$DIST_APP/Contents/MacOS/PXTOOL" 2>/dev/null || true

echo "  Verifying all Mach-O files and Qt frameworks..."
verify_macos_qt_bundle "$DIST_APP"

# Confirm the key libs were bundled by macdeployqt
for lib in libglib-2.0.0.dylib libusb-1.0.0.dylib libfftw3.3.dylib; do
  if [ -f "$FRAMEWORKS_DIR/$lib" ]; then
    echo "  OK: $lib"
  else
    echo "  WARNING: $lib not found in bundle - macdeployqt may have missed it"
  fi
done

# share/PXTOOL/cdecoders is the CDecoderRegistry plugin directory. It ships
# empty -- no example plugin is bundled, so there is nothing to verify here.
# Built-in C decoders live under share/libsigrokdecode/decoders/c_decoders,
# checked below.
SRD_CDECODERS_DIR="$DIST_APP/Contents/Resources/share/libsigrokdecode/decoders/c_decoders"
if [ -d "$SRD_CDECODERS_DIR" ]; then
  SRD_CDECODER_COUNT=$(find "$SRD_CDECODERS_DIR" -type f -name "*.dylib" -o -name "*.so" | wc -l | tr -d ' ')
  echo "  OK: libsigrokdecode C decoders ($SRD_CDECODER_COUNT modules)"
else
  echo "  WARNING: libsigrokdecode C decoders missing at $SRD_CDECODERS_DIR"
fi

# Step 5: Verify dependencies and sign
echo "[5/6] Verifying dependencies and signing..."

if [ -d "$FRAMEWORKS_DIR/Python.framework" ]; then
  echo "  OK: Python.framework"
fi

echo "  Re-signing app bundle..."
"$SIGN_APP_SCRIPT" "$DIST_APP"

# Final check for any remaining external dependencies.
#
# This used to inspect only Contents/MacOS/PXTOOL, which is why 215 dlopen'd C
# decoders could keep an absolute /usr/local/opt/glib dependency while this step
# still reported "All external libs resolved." Walk every Mach-O in the bundle:
# one missed file is enough to break the app on a machine without Homebrew, or
# to pull in a second copy of a library that has to be process-wide unique.
echo "  Auditing every Mach-O in the bundle for external dependencies..."
BUNDLE_MACHO_COUNT=0
BROKEN=""
while IFS= read -r -d '' MACHO_FILE; do
  MACHO_DESC=""
  if ! MACHO_DESC="$(file -b "$MACHO_FILE" 2>&1)"; then
    echo "ERROR: unable to inspect $MACHO_FILE"
    printf '%s\n' "$MACHO_DESC"
    exit 1
  fi
  case "$MACHO_DESC" in
    *Mach-O*) ;;
    *) continue ;;
  esac
  BUNDLE_MACHO_COUNT=$((BUNDLE_MACHO_COUNT + 1))

  MACHO_DEPENDENCIES=""
  if ! MACHO_DEPENDENCIES="$(otool -L "$MACHO_FILE" 2>&1)"; then
    echo "ERROR: unable to inspect Mach-O dependencies: $MACHO_FILE"
    printf '%s\n' "$MACHO_DEPENDENCIES"
    exit 1
  fi

  # NR > 1 also covers the LC_ID_DYLIB line that otool -L prints for a dylib, so
  # a library whose own identity still points outside the bundle is caught too
  # -- that is how the stale libbrotlicommon.1.dylib id surfaced.
  MACHO_EXTERNAL="$(printf '%s\n' "$MACHO_DEPENDENCIES" \
    | awk 'NR > 1 && ($1 ~ /^\/opt\/homebrew\// || $1 ~ /^\/usr\/local\// || $1 ~ /^\/Users\//) { print $1 }')"
  if [ -n "$MACHO_EXTERNAL" ]; then
    while IFS= read -r MACHO_DEP; do
      BROKEN="${BROKEN}${MACHO_FILE#"$DIST_APP/"} -> ${MACHO_DEP}"$'\n'
    done <<<"$MACHO_EXTERNAL"
  fi
done < <(find "$DIST_APP" -type f -print0)

if [ -n "$BROKEN" ]; then
  echo "ERROR: these bundled Mach-O files still reference or declare paths outside the app:"
  printf '%s' "$BROKEN" | sed 's/^/    /'
  echo "  The app would not run on a machine that lacks those paths."
  exit 1
fi
echo "  All external libs resolved ($BUNDLE_MACHO_COUNT Mach-O files checked)."

# Step 6: Create DMG
if [ $NO_DMG -eq 0 ]; then
  echo "[6/6] Creating DMG..."
  # Get version from Info.plist
  VERSION=$(defaults read "$DIST_APP/Contents/Info.plist" CFBundleShortVersionString 2>/dev/null || echo "1.0")

  # Name the DMG after the architecture actually produced. This was hardcoded to
  # arm64, which happens to be right on Apple Silicon but silently mislabelled
  # x86_64 builds on Intel Macs as arm64 packages.
  DMG_ARCH="$(file -b "$DIST_APP/Contents/MacOS/PXTOOL" 2>/dev/null || true)"
  case "$DMG_ARCH" in
    *universal*) DMG_ARCH="universal" ;;
    *arm64*)     DMG_ARCH="arm64" ;;
    *x86_64*)    DMG_ARCH="x86_64" ;;
    *)           DMG_ARCH="$(uname -m)" ;;
  esac

  DMG_OUT="${DIST_DIR}/PXTOOL-${VERSION}-${DMG_ARCH}-macOS.dmg"
  cleanup_dmg_artifacts "$DMG_OUT"

  DMG_STAGE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pxtool-dmg.XXXXXX")"
  cp -R "$DIST_APP" "$DMG_STAGE_DIR/PXTOOL.app"

  create-dmg \
    --volname "PXTOOL ${VERSION}" \
    --volicon "$DIST_APP/Contents/Resources/PXTOOL.icns" \
    --window-pos 200 120 \
    --window-size 600 400 \
    --icon-size 100 \
    --icon "PXTOOL.app" 150 180 \
    --hide-extension "PXTOOL.app" \
    --app-drop-link 450 180 \
    "$DMG_OUT" \
    "$DMG_STAGE_DIR" \
    2>&1 | tail -5

  echo ""
  echo "  DMG created: $DMG_OUT"
else
  echo "[6/6] Skipping DMG (--no-dmg)"
fi

echo ""
echo "Done! Distributable bundle:"
echo "  App: $DIST_APP"
[ $NO_DMG -eq 0 ] && echo "  DMG: $DMG_OUT"
du -sh "$DIST_APP"
