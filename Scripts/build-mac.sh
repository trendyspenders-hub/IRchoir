#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "$(uname -s)" != Darwin ]]; then
  echo "This script requires macOS with Xcode command line tools and CMake." >&2
  exit 1
fi
for command in cmake xcrun pkgbuild productbuild hdiutil codesign; do
  command -v "$command" >/dev/null || { echo "Missing required tool: $command" >&2; exit 1; }
done
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DIRCHOIR_BUILD_TESTS=ON
cmake --build build --config Release --parallel 3
ctest --test-dir build -C Release --output-on-failure
if [[ -n "${IRCHOIR_DOC_PYTHON:-}" ]]; then
  cp build/IRChoir-editor-smoke.png TestEvidence/IRChoir-Native-Interface.png
  cp build/IRChoir-reverb-smoke.png TestEvidence/IRChoir-Reverb-Interface.png
  "$IRCHOIR_DOC_PYTHON" Scripts/make-documents.py
fi
bash Scripts/package-mac.sh
