#!/usr/bin/env bash
# Builds Roadhouse on your Mac and installs it for your user account.
# Needs: Xcode Command Line Tools (xcode-select --install) and CMake (brew install cmake)
set -euo pipefail
cd "$(dirname "$0")/.."

command -v cmake >/dev/null || { echo "CMake missing. Run: brew install cmake"; exit 1; }
xcode-select -p >/dev/null 2>&1 || { echo "Xcode tools missing. Run: xcode-select --install"; exit 1; }

cmake -B build -DCMAKE_BUILD_TYPE=Release -DROADHOUSE_COPY_AFTER_BUILD=ON
cmake --build build --config Release -j "$(sysctl -n hw.ncpu)"

for p in "$HOME/Library/Audio/Plug-Ins/Components/Roadhouse.component" \
         "$HOME/Library/Audio/Plug-Ins/VST3/Roadhouse.vst3"; do
  [ -d "$p" ] && codesign --force --deep --sign - "$p" && echo "Installed: $p"
done
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Done. Restart your DAW."
