#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
DIST_DIR="${ROOT_DIR}/dist"

"${ROOT_DIR}/scripts/extract_assets.sh"

mkdir -p "${BUILD_DIR}" "${DIST_DIR}"

pushd "${BUILD_DIR}" >/dev/null
qmake ../DesktopPet.pro
make -j"$(nproc)"
popd >/dev/null

cp "${BUILD_DIR}/QtCatPet" "${DIST_DIR}/QtCatPet"
chmod +x "${DIST_DIR}/QtCatPet"
