#!/bin/bash
# Double-click me (or run in Terminal) after unzipping. Installs Roadhouse for your user account.
cd "$(dirname "$0")"
AU="$HOME/Library/Audio/Plug-Ins/Components"
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$AU" "$VST3"

if [ -d Roadhouse.component ]; then
  rm -rf "$AU/Roadhouse.component"; cp -R Roadhouse.component "$AU/"
  xattr -cr "$AU/Roadhouse.component"; codesign --force --deep --sign - "$AU/Roadhouse.component"
  echo "Installed Audio Unit  -> $AU"
fi
if [ -d Roadhouse.vst3 ]; then
  rm -rf "$VST3/Roadhouse.vst3"; cp -R Roadhouse.vst3 "$VST3/"
  xattr -cr "$VST3/Roadhouse.vst3"; codesign --force --deep --sign - "$VST3/Roadhouse.vst3"
  echo "Installed VST3        -> $VST3"
fi
killall -9 AudioComponentRegistrar 2>/dev/null
echo
echo "Done. Restart your DAW. In Logic Pro it appears under Audio FX > Audio Units > Michael > Roadhouse"
echo "(first launch runs Logic's plug-in validation, which can take a moment)."
