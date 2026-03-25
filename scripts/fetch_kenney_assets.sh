#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
assets_root="${repo_root}/assets/kenney"

usage() {
    cat <<'EOF'
usage:
  ./scripts/fetch_kenney_assets.sh <kenney-slug> [more-slugs...]

examples:
  ./scripts/fetch_kenney_assets.sh animated-characters-1
  ./scripts/fetch_kenney_assets.sh city-kit-roads food-kit cube-pets

downloads each asset from its official Kenney asset page:
  https://kenney.nl/assets/<slug>

and extracts it to:
  assets/kenney/<slug>/
EOF
}

extract_zip_url() {
    local page_html="$1"

    grep -oE 'https://kenney\.nl/media/pages/assets/[^"]+\.zip' <<<"${page_html}" | head -n 1
}

extract_to_destination() {
    local extracted_root="$1"
    local destination="$2"

    mkdir -p "${destination}"

    mapfile -t entries < <(find "${extracted_root}" -mindepth 1 -maxdepth 1 | sort)
    if [[ ${#entries[@]} -eq 1 && -d "${entries[0]}" ]]; then
        cp -a "${entries[0]}/." "${destination}/"
        return
    fi

    cp -a "${extracted_root}/." "${destination}/"
}

download_slug() {
    local slug="$1"
    local page_url="https://kenney.nl/assets/${slug}"
    local destination="${assets_root}/${slug}"
    local temp_dir
    local page_html
    local zip_url

    temp_dir="$(mktemp -d)"
    trap 'rm -rf "${temp_dir}"' RETURN

    echo "fetching ${page_url}"
    page_html="$(curl -fsSL "${page_url}")"
    zip_url="$(extract_zip_url "${page_html}")"

    if [[ -z "${zip_url}" ]]; then
        echo "failed to find downloadable zip for ${slug}" >&2
        return 1
    fi

    echo "downloading ${zip_url}"
    curl -fL "${zip_url}" -o "${temp_dir}/${slug}.zip"
    unzip -q "${temp_dir}/${slug}.zip" -d "${temp_dir}/extract"

    rm -rf "${destination}"
    extract_to_destination "${temp_dir}/extract" "${destination}"

    echo "installed to ${destination}"
    trap - RETURN
    rm -rf "${temp_dir}"
}

main() {
    if [[ $# -lt 1 ]]; then
        usage >&2
        exit 1
    fi

    mkdir -p "${assets_root}"

    local slug
    for slug in "$@"; do
        download_slug "${slug}"
    done
}

main "$@"
