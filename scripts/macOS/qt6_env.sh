#!/usr/bin/env bash
# Locate the Qt6 host tool directory on macOS without depending on the caller's
# PATH, and prepend it. Meant to be sourced by the macOS build/package scripts.
#
# Why this exists: qtpaths6 and macdeployqt only sit on PATH by accident of how
# Qt was installed. Homebrew links them into /opt/homebrew/bin (or
# /usr/local/bin on Intel), which is normally on PATH. The official Qt online
# installer and aqtinstall drop them in ~/Qt/<version>/macos/bin and never touch
# shell profiles, so an interactive shell there has no Qt at all. Scripts that
# just called `command -v qtpaths6` worked on one machine and failed on another
# for reasons that had nothing to do with the project.

# resolve_qt6_bin_dir [cmake_cache_dir]
#
# Prints the Qt6 bin directory on success, returns non-zero when nothing is
# found. cmake_cache_dir is optional and names a directory holding a
# CMakeCache.txt to reuse the Qt the tree was already configured against.
resolve_qt6_bin_dir() {
    local cmake_cache_dir="${1:-}"
    local candidate qt6_dir prefix qmake cache_file

    # 1. Explicit override for unusual layouts.
    if [ -n "${QT6_BIN_DIR:-}" ]; then
        printf '%s\n' "${QT6_BIN_DIR}"
        return 0
    fi

    # 2. Already on PATH (Homebrew installs, or a profile that sets it up).
    candidate="$(command -v qtpaths6 2>/dev/null || true)"
    if [ -n "${candidate}" ]; then
        printf '%s\n' "$(dirname "${candidate}")"
        return 0
    fi

    # 3. Reuse the Qt recorded in an existing CMake cache, so packaging uses the
    #    same Qt the binary was linked against.
    if [ -n "${cmake_cache_dir}" ] && [ -f "${cmake_cache_dir}/CMakeCache.txt" ]; then
        cache_file="${cmake_cache_dir}/CMakeCache.txt"
        qt6_dir="$(sed -n 's/^Qt6_DIR:[A-Za-z]*=//p' "${cache_file}" | head -n 1)"
        if [ -n "${qt6_dir}" ]; then
            prefix="${qt6_dir%/lib/cmake/Qt6}"
            if [ -x "${prefix}/bin/qtpaths6" ]; then
                printf '%s\n' "${prefix}/bin"
                return 0
            fi
        fi
    fi

    # 4. Ask qmake where the host tools live.
    for qmake in qmake6 qmake; do
        candidate="$(command -v "${qmake}" 2>/dev/null || true)"
        [ -n "${candidate}" ] || continue
        prefix="$("${candidate}" -query QT_HOST_BINS 2>/dev/null || true)"
        if [ -n "${prefix}" ] && [ -x "${prefix}/qtpaths6" ]; then
            printf '%s\n' "${prefix}"
            return 0
        fi
    done

    # 5. Well-known install roots, newest official installer tree first, then
    #    Homebrew on Apple Silicon and Intel.
    while IFS= read -r candidate; do
        [ -n "${candidate}" ] || continue
        if [ -x "${candidate}/qtpaths6" ]; then
            printf '%s\n' "${candidate}"
            return 0
        fi
    done < <(
        ls -d "${HOME}"/Qt/6.*/macos/bin 2>/dev/null | sort -Vr
        printf '%s\n' \
            /opt/homebrew/opt/qt/bin \
            /opt/homebrew/opt/qt@6/bin \
            /opt/homebrew/bin \
            /usr/local/opt/qt/bin \
            /usr/local/opt/qt@6/bin \
            /usr/local/bin
    )

    return 1
}

# require_qt6_tools_on_path [cmake_cache_dir]
#
# Prepends the resolved Qt6 bin directory to PATH, or fails with an actionable
# message. Safe to call more than once.
require_qt6_tools_on_path() {
    local qt6_bin_dir
    if ! qt6_bin_dir="$(resolve_qt6_bin_dir "${1:-}")" || [ -z "${qt6_bin_dir}" ]; then
        echo "ERROR: qtpaths6 was not found."
        echo "       Add the Qt6 bin directory to PATH, or set QT6_BIN_DIR, e.g.:"
        echo "         export QT6_BIN_DIR=\"\${HOME}/Qt/6.5.3/macos/bin\""
        return 1
    fi
    case ":${PATH}:" in
        *":${qt6_bin_dir}:"*) ;;
        *) PATH="${qt6_bin_dir}:${PATH}" ;;
    esac
    export PATH
    echo "Qt6 tools: ${qt6_bin_dir}"
}
