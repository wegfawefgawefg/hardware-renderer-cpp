#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
assets_root="${repo_root}/assets/kenney"
readonly project_slugs=(
    animated-characters-1
    city-kit-commercial
    city-kit-industrial
    city-kit-roads
    city-kit-suburban
    car-kit
)

usage() {
    cat <<'EOF'
usage:
  ./scripts/fetch_kenney_assets.sh
  ./scripts/fetch_kenney_assets.sh <kenney-slug> [more-slugs...]

examples:
  ./scripts/fetch_kenney_assets.sh
  ./scripts/fetch_kenney_assets.sh \
    city-kit-commercial \
    city-kit-industrial \
    city-kit-roads \
    city-kit-suburban \
    car-kit
  ./scripts/fetch_kenney_assets.sh food-kit cube-pets

with no args, downloads the Kenney packs currently referenced by this repo.

with args, downloads exactly those Kenney page slugs:
  https://kenney.nl/assets/<slug>

and extracts it under:
    assets/kenney/

preserving Kenney's archive folder name, for example:
  assets/kenney/kenney_city-kit-commercial_2.1/
EOF
}

destination_dir_name() {
    local slug="$1"
    local archive_stem="$2"

    case "${slug}" in
        animated-characters-1) echo "animated-characters-1" ;;
        city-kit-commercial) echo "kenney_city-kit-commercial_2.1" ;;
        city-kit-industrial) echo "kenney_city-kit-industrial_1.0" ;;
        city-kit-roads) echo "kenney_city-kit-roads" ;;
        city-kit-suburban) echo "kenney_city-kit-suburban_20" ;;
        car-kit) echo "kenney_car-kit" ;;
        *) echo "${archive_stem}" ;;
    esac
}

extract_zip_url() {
    local page_html="$1"

    grep -oE 'https://kenney\.nl/media/pages/assets/[^"]+\.zip' <<<"${page_html}" | head -n 1
}

extract_to_assets_root() {
    local extracted_root="$1"
    local assets_root="$2"
    local destination_name="$3"

    mapfile -t entries < <(find "${extracted_root}" -mindepth 1 -maxdepth 1 | sort)
    if [[ ${#entries[@]} -eq 1 && -d "${entries[0]}" ]]; then
        local destination="${assets_root}/${destination_name}"
        rm -rf "${destination}"
        mkdir -p "${destination}"
        cp -a "${entries[0]}/." "${destination}/"
        echo "${destination}"
        return
    fi

    local destination="${assets_root}/${destination_name}"
    rm -rf "${destination}"
    mkdir -p "${destination}"
    cp -a "${extracted_root}/." "${destination}/"
    echo "${destination}"
}

download_slug() {
    local slug="$1"
    local page_url="https://kenney.nl/assets/${slug}"
    local destination="${assets_root}/${slug}"
    local temp_dir
    local page_html
    local zip_url
    local destination
    local archive_stem
    local destination_name

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
    archive_stem="$(basename "${zip_url}" .zip)"
    destination_name="$(destination_dir_name "${slug}" "${archive_stem}")"

    destination="$(extract_to_assets_root "${temp_dir}/extract" "${assets_root}" "${destination_name}")"
    echo "installed to ${destination}"
    trap - RETURN
    rm -rf "${temp_dir}"
}

main() {
    mkdir -p "${assets_root}"

    local -a slugs=()
    if [[ $# -eq 0 ]]; then
        slugs=("${project_slugs[@]}")
    else
        if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
            usage
            exit 0
        fi
        slugs=("$@")
    fi

    local slug
    for slug in "${slugs[@]}"; do
        download_slug "${slug}"
    done
}

main "$@"
