#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST_DIR="${ROOT_DIR}/assets/dragon_attenuation"
BASE_URL="https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models/DragonAttenuation/glTF"

TMP_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

mkdir -p "${TMP_DIR}"
curl -L --fail --output "${TMP_DIR}/DragonAttenuation.gltf" "${BASE_URL}/DragonAttenuation.gltf"
curl -L --fail --output "${TMP_DIR}/DragonAttenuation.bin" "${BASE_URL}/DragonAttenuation.bin"
curl -L --fail --output "${TMP_DIR}/Dragon_ThicknessMap.jpg" "${BASE_URL}/Dragon_ThicknessMap.jpg"
curl -L --fail --output "${TMP_DIR}/checkerboard.png" "${BASE_URL}/checkerboard.png"

rm -rf "${DEST_DIR}"
mkdir -p "${DEST_DIR}"
cp "${TMP_DIR}/DragonAttenuation.gltf" "${DEST_DIR}/"
cp "${TMP_DIR}/DragonAttenuation.bin" "${DEST_DIR}/"
cp "${TMP_DIR}/Dragon_ThicknessMap.jpg" "${DEST_DIR}/"
cp "${TMP_DIR}/checkerboard.png" "${DEST_DIR}/"

echo "Fetched DragonAttenuation into ${DEST_DIR}"
