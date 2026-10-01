#!/bin/bash
# Runs a release_1.0 OpenRGB with this plugin in an isolated config folder.
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
config="$HOME/.config/OpenRGB-plasma-dev"
mkdir -p "$config/plugins"
cp "$repo/build/src/libOpenRGBPlasmaPlugin.so" "$config/plugins/"
exec "${OPENRGB_BIN:-$HOME/git/OpenRGB-1.0/build/openrgb}" --config "$config" --startminimized --server --server-host 127.0.0.1 --server-port 6743 -v --loglevel 4
