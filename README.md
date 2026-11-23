# Roadhouse — 70s Rock & Blues Amp Suite

Audio Unit (Logic Pro, GarageBand, etc.) + VST3 + Standalone for macOS. Built with JUCE.

## Amps
| # | Amp | Character |
|---|-----|-----------|
| 1 | Plexi Clean (Marshall-style) | Bright, mid-forward, lots of headroom, crunches when pushed |
| 2 | Blackface Drive (Fender-style) | Glassy, scooped mids, sparkly top, turns to clean-ish breakup |
| 3 | Tweed Overdrive (Fender-style) | Loose, saggy, compressed bark |
| 4 | Steel String Clean (Dumble-style) | Warm, round, huge headroom, "Deep" switch for low-end bloom |
| 5 | Overdrive Special (Dumble-style) | Smooth, mid-hump, singing sustain, creamy compression |

Controls: Input, Gain, Bass, Mid, Treble, Presence, Master, Bright, Deep, Cabinet (off / 1x12 / 2x12 / 4x12 closed / 4x12 vintage),
Reverb + Size, Noise Gate, Output. 9 factory presets. 4x oversampled to keep the distortion smooth.
Set Cabinet to "Off" if you want to load your own impulse responses after it.

## Option A — Build on your Mac (about 10 minutes)
1. `xcode-select --install`  and  `brew install cmake`
2. In Terminal:  `cd` into this folder, then  `./scripts/build_mac.sh`
3. Restart your DAW. Logic: Audio FX > Audio Units > Michael > Roadhouse

## Option B — Let GitHub build it for you (no developer tools needed)
1. Create a free GitHub repo and upload this folder.
2. Actions tab > "Build macOS plugin" > Run workflow.
3. Download the `Roadhouse-macOS` artifact, unzip, double-click `Install.command`
   (right-click > Open if macOS warns). Restart your DAW.

## Notes
- The plugin is ad-hoc signed, not notarized — fine for your own machines. To distribute it publicly you'd need an Apple Developer ID.
- JUCE is AGPLv3 / commercial. Personal use is fine; selling Roadhouse would require a JUCE commercial licence.
- Amp/cab names are "-style" descriptions only; these are original DSP models, not affiliated with any amp maker.
