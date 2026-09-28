#!/usr/bin/env bash
# Tests for scripts/macOS/qt6_env.sh Qt6 tool discovery.
#
# The point of these cases is that Qt tool locations differ by how Qt was
# installed, not by CPU: Homebrew links qtpaths6 into /opt/homebrew/bin (Apple
# Silicon) or /usr/local/bin (Intel), which is normally on PATH, while the
# official online installer and aqtinstall drop it in ~/Qt/<version>/macos/bin
# and never touch shell profiles. Both layouts must resolve, on either CPU.

set -uo pipefail

HELPER="$(cd "$(dirname "$0")" && pwd)/qt6_env.sh"
NOQT_PATH=/usr/bin:/bin:/usr/sbin:/sbin
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

PASS=0
FAIL=0

check() { # check <name> <expected> <actual>
    if [ "$2" = "$3" ]; then
        echo "  ok    $1"
        PASS=$((PASS + 1))
    else
        echo "  FAIL  $1: expected [$2] got [$3]"
        FAIL=$((FAIL + 1))
    fi
}

make_qt_bin() { # make_qt_bin <dir>
    mkdir -p "$1"
    : >"$1/qtpaths6"
    chmod +x "$1/qtpaths6"
}

# 1. QT6_BIN_DIR wins over everything else.
make_qt_bin "$WORK/override/bin"
check "QT6_BIN_DIR override" "$WORK/override/bin" \
    "$(env PATH="$NOQT_PATH" QT6_BIN_DIR="$WORK/override/bin" \
        bash -c ". $HELPER; resolve_qt6_bin_dir")"

# 2. qtpaths6 already on PATH (the Homebrew layout).
make_qt_bin "$WORK/brewbin"
check "PATH hit (Homebrew layout)" "$WORK/brewbin" \
    "$(env PATH="$WORK/brewbin:$NOQT_PATH" \
        bash -c ". $HELPER; resolve_qt6_bin_dir")"

# 3. Nothing on PATH: reuse the Qt the build tree was configured against.
mkdir -p "$WORK/cache" "$WORK/qtroot/lib/cmake/Qt6"
make_qt_bin "$WORK/qtroot/bin"
printf 'Qt6_DIR:PATH=%s/qtroot/lib/cmake/Qt6\n' "$WORK" >"$WORK/cache/CMakeCache.txt"
check "CMakeCache fallback" "$WORK/qtroot/bin" \
    "$(env PATH="$NOQT_PATH" \
        bash -c ". $HELPER; resolve_qt6_bin_dir '$WORK/cache'")"

# 4. Fall back to qmake -query QT_HOST_BINS.
mkdir -p "$WORK/qmakebin"
make_qt_bin "$WORK/hostbins"
cat >"$WORK/qmakebin/qmake6" <<EOF
#!/usr/bin/env bash
[ "\$1" = "-query" ] && [ "\$2" = "QT_HOST_BINS" ] && echo "$WORK/hostbins" && exit 0
exit 1
EOF
chmod +x "$WORK/qmakebin/qmake6"
check "qmake QT_HOST_BINS fallback" "$WORK/hostbins" \
    "$(env PATH="$WORK/qmakebin:$NOQT_PATH" \
        bash -c ". $HELPER; resolve_qt6_bin_dir")"

# 5. Fall back to ~/Qt/6.*/macos/bin, newest version first.
make_qt_bin "$WORK/home/Qt/6.5.3/macos/bin"
make_qt_bin "$WORK/home/Qt/6.10.0/macos/bin"
check "~/Qt fallback picks newest" "$WORK/home/Qt/6.10.0/macos/bin" \
    "$(env PATH="$NOQT_PATH" HOME="$WORK/home" \
        bash -c ". $HELPER; resolve_qt6_bin_dir")"

# 6. A CMakeCache carried over from another machine points at a prefix that does
#    not exist here (e.g. /opt/homebrew on an Intel Mac). Skip it, keep looking.
mkdir -p "$WORK/cache_stale"
make_qt_bin "$WORK/home2/Qt/6.7.1/macos/bin"
printf 'Qt6_DIR:PATH=/nonexistent/lib/cmake/Qt6\n' >"$WORK/cache_stale/CMakeCache.txt"
check "stale CMakeCache keeps searching" "$WORK/home2/Qt/6.7.1/macos/bin" \
    "$(env PATH="$NOQT_PATH" HOME="$WORK/home2" \
        bash -c ". $HELPER; resolve_qt6_bin_dir '$WORK/cache_stale'")"

# 7. Nothing found: non-zero exit and an actionable message.
OUT="$(env PATH=/usr/bin:/bin HOME="$WORK/empty" \
    bash -c ". $HELPER; require_qt6_tools_on_path" 2>&1)"
check "missing Qt exits non-zero" "1" "$?"
case "$OUT" in
    *QT6_BIN_DIR*) check "missing Qt message is actionable" "yes" "yes" ;;
    *)             check "missing Qt message is actionable" "yes" "no" ;;
esac

# 8. require_qt6_tools_on_path is idempotent: no duplicate PATH entries.
check "PATH entry not duplicated" "1" \
    "$(env PATH="$WORK/brewbin:$NOQT_PATH" bash -c \
        ". $HELPER; require_qt6_tools_on_path >/dev/null; \
         require_qt6_tools_on_path >/dev/null; \
         awk -v p=\"\$PATH\" 'BEGIN{n=split(p,a,\":\");c=0;
             for(i=1;i<=n;i++) if (a[i]==\"$WORK/brewbin\") c++; print c}'")"

echo
if [ "$FAIL" -ne 0 ]; then
    echo "qt6_env tests FAILED: $FAIL of $((PASS + FAIL))"
    exit 1
fi
echo "qt6_env tests passed ($PASS cases)"
