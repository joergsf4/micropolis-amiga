#!/bin/bash
# Clones the Micropolis sources (GPL-3, EA assets) that src/core was derived from and the graphics are converted from.
set -euo pipefail
cd "$(dirname "$0")/.."
[ -d upstream ] || git clone --depth 1 https://github.com/osgcc/simcity upstream
