#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
assets_root="${repo_root}/assets"
destination="${assets_root}/sponza_optimized"
archive_url="https://codeload.github.com/toji/sponza-optimized/zip/refs/heads/main"

usage() {
    cat <<'EOF'
usage:
  ./scripts/fetch_sponza_optimized.sh

downloads the toji/sponza-optimized glTF asset pack and installs it to:
  assets/sponza_optimized/

the destination is replaced on each run.
EOF
}

main() {
    if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
        usage
        exit 0
    fi

    mkdir -p "${assets_root}"

    local temp_dir
    temp_dir="$(mktemp -d)"
    trap "rm -rf '${temp_dir}'" EXIT

    echo "downloading ${archive_url}"
    curl -fL "${archive_url}" -o "${temp_dir}/sponza_optimized.zip"
    unzip -q "${temp_dir}/sponza_optimized.zip" -d "${temp_dir}/extract"

    local extracted_root="${temp_dir}/extract/sponza-optimized-main"
    if [[ ! -d "${extracted_root}" ]]; then
        echo "unexpected archive layout: missing ${extracted_root}" >&2
        exit 1
    fi

    rm -rf "${destination}"
    mkdir -p "${destination}"
    cp -a "${extracted_root}/." "${destination}/"
    echo "installed to ${destination}"
}

main "$@"
