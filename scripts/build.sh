#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
DIST_DIR="${ROOT_DIR}/dist"

# 设置 SKIP_ASSETS=1 可跳过素材提取，直接使用 resources/assets 中已有的 PNG
if [[ "${SKIP_ASSETS:-0}" != "1" ]]; then
  "${ROOT_DIR}/scripts/extract_assets.sh"
fi

mkdir -p "${BUILD_DIR}" "${DIST_DIR}"

pushd "${BUILD_DIR}" >/dev/null
qmake ../DesktopPet.pro
make -j"$(nproc)"
popd >/dev/null

cp "${BUILD_DIR}/QtCatPet" "${DIST_DIR}/QtCatPet"
chmod +x "${DIST_DIR}/QtCatPet"
