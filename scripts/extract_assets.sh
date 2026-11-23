#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IDLE_VIDEO="${ROOT_DIR}/media/静止1.mp4"
ROTATE_VIDEO="${ROOT_DIR}/media/旋转1.mp4"
IDLE_DIR="${ROOT_DIR}/resources/assets/idle"
ROTATE_DIR="${ROOT_DIR}/resources/assets/rotate"
MATTE_SCRIPT="${ROOT_DIR}/scripts/remove_black_background.py"
TMP_DIR="$(mktemp -d)"
TMP_IDLE_DIR="${TMP_DIR}/idle"
TMP_ROTATE_DIR="${TMP_DIR}/rotate"

trap 'rm -rf "${TMP_DIR}"' EXIT

mkdir -p "${IDLE_DIR}" "${ROTATE_DIR}"
mkdir -p "${TMP_IDLE_DIR}" "${TMP_ROTATE_DIR}"
rm -f "${IDLE_DIR}"/*.png "${ROTATE_DIR}"/*.png

ffmpeg -y -i "${IDLE_VIDEO}" \
  -vf "crop=850:760:480:170" \
  -update 1 \
  -frames:v 1 \
  "${TMP_IDLE_DIR}/idle_raw.png"

python3 "${MATTE_SCRIPT}" "${TMP_IDLE_DIR}/idle_raw.png" "${IDLE_DIR}/idle.png" --size 256

ffmpeg -y -i "${ROTATE_VIDEO}" \
  -vf "fps=15,crop=960:600:480:140" \
  "${TMP_ROTATE_DIR}/frame_%02d.png"

shopt -s nullglob
for frame in "${TMP_ROTATE_DIR}"/frame_*.png; do
  output_name="$(basename "${frame}")"
  python3 "${MATTE_SCRIPT}" "${frame}" "${ROTATE_DIR}/${output_name}" --size 256
done
