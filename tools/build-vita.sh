#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
IMAGE=${VITASDK_IMAGE:-vitasdk/vitasdk-softfp@sha256:99814094e0240b7ebe8700ba3742ee761bbcfb22bf8eedeac8db5c1dbe3e3d32}
PAF_ROOT=${LIVEAREA_SEARCH_PAF_ROOT:-"$ROOT/../VitaXMB/build/livearea-deps/vitasdk-paf-component/final"}
PSP2CXML=${PSP2CXML:-"$ROOT/../VitaXMB/build/livearea-deps/psp2cxml-tool/build-linux/psp2cxml-tool"}
BUILD_DIR=${LIVEAREA_SEARCH_BUILD_DIR:-build-vita}
JOBS=${LIVEAREA_SEARCH_JOBS:-4}

case "$BUILD_DIR" in
    /*|../*|*/../*|*/..)
        printf 'LIVEAREA_SEARCH_BUILD_DIR must stay inside the repository: %s\n' "$BUILD_DIR" >&2
        exit 2
        ;;
esac

case "$JOBS" in
    ''|*[!0-9]*)
        printf 'LIVEAREA_SEARCH_JOBS must be a positive integer: %s\n' "$JOBS" >&2
        exit 2
        ;;
esac

if [ "$JOBS" -eq 0 ]; then
    printf 'LIVEAREA_SEARCH_JOBS must be greater than zero\n' >&2
    exit 2
fi

if [ ! -f "$PAF_ROOT/include/paf.h" ] || [ ! -f "$PAF_ROOT/lib/libScePaf_stub_weak.a" ]; then
    printf 'Invalid LIVEAREA_SEARCH_PAF_ROOT: %s\n' "$PAF_ROOT" >&2
    exit 2
fi

if [ ! -x "$PSP2CXML" ]; then
    printf 'PSP2CXML must name an executable psp2cxml-tool: %s\n' "$PSP2CXML" >&2
    exit 2
fi

command -v docker >/dev/null 2>&1 || {
    printf 'docker is required\n' >&2
    exit 2
}

docker image inspect "$IMAGE" >/dev/null 2>&1 || {
    printf 'VitaSDK image is unavailable: %s\n' "$IMAGE" >&2
    exit 2
}

sync

docker run --rm \
    -v "$ROOT:/src" \
    -v "$PAF_ROOT:/deps/paf:ro" \
    -v "$PSP2CXML:/deps/psp2cxml-tool:ro" \
    -e LIVEAREA_BUILD_DIR="$BUILD_DIR" \
    -e LIVEAREA_BUILD_JOBS="$JOBS" \
    -w /src \
    "$IMAGE" \
    sh -lc '
        set -eu
        export VITASDK=/usr/local/vitasdk
        cmake -S . -B "$LIVEAREA_BUILD_DIR" \
            -DCMAKE_TOOLCHAIN_FILE=/usr/local/vitasdk/share/vita.toolchain.cmake \
            -DLIVEAREA_SEARCH_BUILD_TESTS=OFF \
            -DLIVEAREA_SEARCH_BUILD_VITA=ON \
            -DLIVEAREA_SEARCH_PAF_ROOT=/deps/paf \
            -DPSP2CXML=/deps/psp2cxml-tool
        cmake --build "$LIVEAREA_BUILD_DIR" --parallel "$LIVEAREA_BUILD_JOBS"
    '

printf 'SUPRX: %s/%s/dist/livearea_search.suprx\n' "$ROOT" "$BUILD_DIR"
printf 'RCO:   %s/%s/dist/ur0/data/livearea-search/livearea_search_plugin.rco\n' "$ROOT" "$BUILD_DIR"
