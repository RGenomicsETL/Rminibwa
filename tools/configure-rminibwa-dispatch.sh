#!/bin/sh
# Stage Rminibwa KSW SIMD backend objects for runtime dispatch.
# This follows the RsimdDispatch package pattern: configure compiles ISA-specific
# objects into src/rmb-ksw and Makevars links those staged objects via PKG_LIBS.

set -eu

ROOT=${RMINIBWA_PACKAGE_ROOT:-$(pwd)}
MAKEVARS_IN=${RMINIBWA_MAKEVARS_IN:-src/Makevars.in}
MAKEVARS_OUT=${RMINIBWA_MAKEVARS_OUT:-src/Makevars}
CONFIG_OUT=${RMINIBWA_CONFIG_OUT:-src/rminibwa_simd_config.h}
VENDOR=${RMINIBWA_VENDOR_DIR:-src/vendor/minibwa}
OBJ_MAKE_DIR=${RMINIBWA_KSW_OBJECT_MAKE_DIR:-rmb-ksw}
OBJ_DIR=${RMINIBWA_KSW_OBJECT_DIR:-src/$OBJ_MAKE_DIR}

MAKEVARS_IN_PATH="$ROOT/$MAKEVARS_IN"
MAKEVARS_OUT_PATH="$ROOT/$MAKEVARS_OUT"
CONFIG_OUT_PATH="$ROOT/$CONFIG_OUT"
VENDOR_PATH="$ROOT/$VENDOR"
OBJ_DIR_PATH="$ROOT/$OBJ_DIR"

if [ ! -f "$MAKEVARS_IN_PATH" ]; then
    echo "ERROR: cannot find Makevars template: $MAKEVARS_IN_PATH" >&2
    exit 1
fi
if [ ! -d "$VENDOR_PATH" ]; then
    echo "ERROR: cannot find vendored minibwa source: $VENDOR_PATH" >&2
    exit 1
fi

R_CC_CMD=${CC:-}
if [ -z "$R_CC_CMD" ]; then
    if [ -n "${R_HOME:-}" ] && [ -x "${R_HOME}/bin/R" ]; then
        R_CC_CMD=$("${R_HOME}/bin/R" CMD config CC)
    else
        R_CC_CMD=cc
    fi
fi
CC_BIN=$(printf '%s\n' "$R_CC_CMD" | awk '{print $1}')
CC_REST=$(printf '%s\n' "$R_CC_CMD" | sed 's/^[^[:space:]]*[[:space:]]*//')
CC_EXTRA=${RMINIBWA_CC_EXTRA:-$CC_REST}

TARGET_WASM=0
case "$(basename "$CC_BIN") ${RMINIBWA_CONFIGURE_ARGS:-}" in
    *emcc*|*wasm*|*emscripten*) TARGET_WASM=1 ;;
esac

if [ -z "${CFLAGS:-}" ] && [ -n "${R_HOME:-}" ] && [ -x "${R_HOME}/bin/R" ]; then
    CFLAGS=$("${R_HOME}/bin/R" CMD config CFLAGS)
fi
if [ -z "${CPPFLAGS:-}" ] && [ -n "${R_HOME:-}" ] && [ -x "${R_HOME}/bin/R" ]; then
    CPPFLAGS=$("${R_HOME}/bin/R" CMD config CPPFLAGS 2>/dev/null || true)
fi
if [ -z "${CPICFLAGS:-}" ] && [ -n "${R_HOME:-}" ] && [ -x "${R_HOME}/bin/R" ]; then
    CPICFLAGS=$("${R_HOME}/bin/R" CMD config CPICFLAGS 2>/dev/null || true)
fi

RSCRIPT_BIN=${RMINIBWA_RSCRIPT:-}
if [ -z "$RSCRIPT_BIN" ] && [ -n "${R_HOME:-}" ] && [ -x "${R_HOME}/bin/Rscript" ]; then
    RSCRIPT_BIN="${R_HOME}/bin/Rscript"
fi
if [ -z "$RSCRIPT_BIN" ]; then
    RSCRIPT_BIN=$(command -v Rscript || true)
fi
if [ -z "$RSCRIPT_BIN" ] || [ ! -x "$RSCRIPT_BIN" ]; then
    echo "ERROR: Rscript is required to resolve Rminibwa build dependencies" >&2
    exit 1
fi

SIMDE_INCLUDE_DIR=${RMINIBWA_SIMDE_INCLUDE_DIR:-}
if [ -z "$SIMDE_INCLUDE_DIR" ]; then
    SIMDE_INCLUDE_DIR=$("$RSCRIPT_BIN" -e 'cat(system.file("include", package = "RsimdDispatch"))' 2>/dev/null || true)
fi
if [ -z "$SIMDE_INCLUDE_DIR" ] || [ ! -d "$SIMDE_INCLUDE_DIR/simde" ]; then
    echo "ERROR: could not find SIMDe headers from RsimdDispatch" >&2
    exit 1
fi
RMINIBWA_SIMDE_CPPFLAGS="-I\"$SIMDE_INCLUDE_DIR\""

TMPDIR=${TMPDIR:-/tmp}
CONFDIR=$(mktemp -d "$TMPDIR/rminibwa-conf-XXXXXX")
trap 'rm -rf "$CONFDIR"' EXIT INT HUP TERM
mkdir -p "$OBJ_DIR_PATH" "$(dirname "$CONFIG_OUT_PATH")"
rm -f "$OBJ_DIR_PATH"/*.o

HTSLIB_CONTRACT_TOOL="$ROOT/tools/rduckhts-htslib-contract.R"
HTSLIB_CONTRACT_OUT="$CONFDIR/rduckhts-htslib-contract"
if [ ! -f "$HTSLIB_CONTRACT_TOOL" ]; then
    echo "ERROR: cannot find Rduckhts contract resolver: $HTSLIB_CONTRACT_TOOL" >&2
    exit 1
fi
HTSLIB_CONTRACT_MODE=native
RMINIBWA_HTSLIB_DEFS=-DRMINIBWA_WITH_HTSLIB=1
if [ "$TARGET_WASM" -eq 1 ]; then
    HTSLIB_CONTRACT_MODE=wasm-no-link
    RMINIBWA_HTSLIB_DEFS=-DRMINIBWA_WITH_HTSLIB=0
fi
"$RSCRIPT_BIN" "$HTSLIB_CONTRACT_TOOL" "$HTSLIB_CONTRACT_OUT" "$HTSLIB_CONTRACT_MODE"
if [ "$(wc -l < "$HTSLIB_CONTRACT_OUT" | tr -d '[:space:]')" -ne 6 ]; then
    echo "ERROR: Rduckhts htslib contract resolver returned an invalid receipt" >&2
    exit 1
fi
RMINIBWA_HTSLIB_CPPFLAGS=$(sed -n '1p' "$HTSLIB_CONTRACT_OUT")
RMINIBWA_HTSLIB_LDFLAGS=$(sed -n '2p' "$HTSLIB_CONTRACT_OUT")
RMINIBWA_HTSLIB_VERSION=$(sed -n '3p' "$HTSLIB_CONTRACT_OUT")
RMINIBWA_HTSLIB_SOURCE_ID=$(sed -n '4p' "$HTSLIB_CONTRACT_OUT")
RMINIBWA_HTSLIB_BUILD_ID=$(sed -n '5p' "$HTSLIB_CONTRACT_OUT")
RMINIBWA_HTSLIB_LINK=$(sed -n '6p' "$HTSLIB_CONTRACT_OUT")

sed_escape() {
    printf '%s' "$1" | sed 's/[\\&|]/\\&/g'
}

check_cflag() {
    flags=$1
    header=$2
    macro=$3
    {
        printf '#include <%s>\n' "$header"
        if [ -n "$macro" ]; then
            printf '#ifndef %s\n#error "%s not defined"\n#endif\n' "$macro" "$macro"
        fi
        printf 'int main(void) { return 0; }\n'
    } > "$CONFDIR/conftest.c"
    # shellcheck disable=SC2086
    ${CC_BIN} ${CC_EXTRA:-} ${CPPFLAGS:-} ${CFLAGS:-} -I"$SIMDE_INCLUDE_DIR" ${flags} -c "$CONFDIR/conftest.c" -o "$CONFDIR/conftest.o" >/dev/null 2>&1
}

STAGED_OBJECTS=""
append_object() {
    if [ -z "$STAGED_OBJECTS" ]; then
        STAGED_OBJECTS="$OBJ_MAKE_DIR/$1"
    else
        STAGED_OBJECTS="$STAGED_OBJECTS $OBJ_MAKE_DIR/$1"
    fi
}

compile_obj() {
    src=$1
    obj=$2
    flags=$3
    defs=$4
    log="$CONFDIR/$obj.log"
    # shellcheck disable=SC2086
    ${CC_BIN} ${CC_EXTRA:-} ${CPPFLAGS:-} ${CFLAGS:-} ${CPICFLAGS:-} \
        -I"$VENDOR_PATH" -I"$SIMDE_INCLUDE_DIR" \
        -DHAVE_KALLOC -DUSE_GPL ${defs} ${flags} \
        -c "$VENDOR_PATH/$src" -o "$OBJ_DIR_PATH/$obj" >"$log" 2>&1 || {
        echo "ERROR: failed to compile staged KSW object $obj" >&2
        sed 's/^/  /' "$log" >&2 || true
        exit 1
    }
    append_object "$obj"
}

COMMON_EXTZ_RENAME='-Dksw_extz2_sse=rmb_ksw_extz2_BACKEND'
COMMON_EXTD_RENAME='-Dksw_extd2_sse=rmb_ksw_extd2_BACKEND'
COMMON_LL_RENAME='-Dksw_ll_qinit=rmb_ksw_ll_qinit_BACKEND -Dksw_ll_u8_core=rmb_ksw_ll_u8_core_BACKEND -Dksw_ll_i16_core=rmb_ksw_ll_i16_core_BACKEND -Dksw_ll_i16=rmb_ksw_ll_i16_BACKEND'

rename_backend() {
    printf '%s' "$1" | sed "s/BACKEND/$2/g"
}

# Scalar: portable SIMDe fallback with native intrinsics disabled and no ISA
# flags. SIMDe may still use compiler vector extensions on some compilers; this
# is the portable baseline for dispatch, not a promise that no vector instruction
# appears in generated machine code.
SCALAR_SIMDE_DEFS='-DRMINIBWA_USE_SIMDE -DSIMDE_NO_NATIVE'
compile_obj ksw2_extz2_sse.c ksw2_extz2_scalar.o "" "$SCALAR_SIMDE_DEFS $(rename_backend "$COMMON_EXTZ_RENAME" scalar)"
compile_obj ksw2_extd2_sse.c ksw2_extd2_scalar.o "" "$SCALAR_SIMDE_DEFS $(rename_backend "$COMMON_EXTD_RENAME" scalar)"
compile_obj ksw2_ll_sse.c    ksw2_ll_scalar.o    "" "$SCALAR_SIMDE_DEFS $(rename_backend "$COMMON_LL_RENAME" scalar)"

HAVE_SSE4=0
HAVE_AVX2=0

if check_cflag "-msse4.1" "simde/x86/sse4.1.h" "SIMDE_X86_SSE4_1_NATIVE"; then
    HAVE_SSE4=1
    compile_obj ksw2_extz2_sse.c ksw2_extz2_sse4.o "-msse4.1" "-DRMINIBWA_USE_SIMDE $(rename_backend "$COMMON_EXTZ_RENAME" sse4)"
    compile_obj ksw2_extd2_sse.c ksw2_extd2_sse4.o "-msse4.1" "-DRMINIBWA_USE_SIMDE $(rename_backend "$COMMON_EXTD_RENAME" sse4)"
    compile_obj ksw2_ll_sse.c    ksw2_ll_sse4.o    "-msse4.1" "-DRMINIBWA_USE_SIMDE $(rename_backend "$COMMON_LL_RENAME" sse4)"
else
    echo "Rminibwa configure: sse4 backend disabled; compiler probe failed" >&2
fi

if check_cflag "-mavx2" "immintrin.h" "__AVX2__"; then
    HAVE_AVX2=1
    compile_obj ksw2_extz2_sse.c ksw2_extz2_avx2.o "-mavx2" "-DRMINIBWA_USE_SIMDE $(rename_backend "$COMMON_EXTZ_RENAME" avx2)"
    compile_obj ksw2_ll_sse.c    ksw2_ll_avx2.o    "-mavx2" "-DRMINIBWA_USE_SIMDE $(rename_backend "$COMMON_LL_RENAME" avx2)"
    compile_obj ksw2_extd2_sse.c ksw2_extd2_avx2_ref.o "-mavx2" "-DRMINIBWA_USE_SIMDE -Dksw_extd2_sse=rmb_ksw_extd2_avx2_ref"
    compile_obj ksw2_extd2_wide.c ksw2_extd2_avx2.o "-mavx2" "-Dksw_extd2_sse=rmb_ksw_extd2_avx2 -Dextd2_ref_impl=rmb_ksw_extd2_avx2_ref"
else
    echo "Rminibwa configure: avx2 backend disabled; compiler probe failed" >&2
fi

cat > "$CONFIG_OUT_PATH" <<EOF
#ifndef RMINIBWA_SIMD_CONFIG_H
#define RMINIBWA_SIMD_CONFIG_H

#define RMB_HAVE_SSE4 ${HAVE_SSE4}
#define RMB_HAVE_AVX2 ${HAVE_AVX2}

#endif
EOF

STAGED_OBJECTS_ESC=$(sed_escape "$STAGED_OBJECTS")
SIMDE_CPPFLAGS_ESC=$(sed_escape "$RMINIBWA_SIMDE_CPPFLAGS")
HTSLIB_CPPFLAGS_ESC=$(sed_escape "$RMINIBWA_HTSLIB_CPPFLAGS")
HTSLIB_LDFLAGS_ESC=$(sed_escape "$RMINIBWA_HTSLIB_LDFLAGS")
HTSLIB_DEFS_ESC=$(sed_escape "$RMINIBWA_HTSLIB_DEFS")
sed \
    -e "s|@RMINIBWA_STAGED_OBJECTS@|${STAGED_OBJECTS_ESC}|g" \
    -e "s|@RMINIBWA_SIMDE_CPPFLAGS@|${SIMDE_CPPFLAGS_ESC}|g" \
    -e "s|@RMINIBWA_HTSLIB_CPPFLAGS@|${HTSLIB_CPPFLAGS_ESC}|g" \
    -e "s|@RMINIBWA_HTSLIB_LDFLAGS@|${HTSLIB_LDFLAGS_ESC}|g" \
    -e "s|@RMINIBWA_HTSLIB_DEFS@|${HTSLIB_DEFS_ESC}|g" \
    "$MAKEVARS_IN_PATH" > "$MAKEVARS_OUT_PATH"

echo "Rminibwa configure: staged objects='$STAGED_OBJECTS'"
echo "Rminibwa configure: htslib='$RMINIBWA_HTSLIB_VERSION' source='$RMINIBWA_HTSLIB_SOURCE_ID' build='$RMINIBWA_HTSLIB_BUILD_ID' link='$RMINIBWA_HTSLIB_LINK'"
echo "Rminibwa configure: wrote $CONFIG_OUT and $MAKEVARS_OUT"
