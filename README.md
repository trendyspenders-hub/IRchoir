# I.R.Choir 0.3.0 — native vocal ensemble prototype

This archive contains source, tests and a Mac DMG build pipeline. It is not a
Mac installer. See AUDIT.md for what actually ran and the remaining release gates.

## Added features
- Native animated choir stage with 2–32 audio-reactive singer icons.
- Click to select; Shift-click for multi-selection; drag for pan and level;
  double-click to mute. Section buttons and group level/pan/mute/solo controls.
- Semicircle, tight cluster and wide-row formations, linked to stereo pan.
- Conductor pad controlling intensity and space; equivalent accessible sliders.
- Separate timing looseness, pitch drift and vibrato controls.
- Per-layer warm/airy/dark/bright synthetic tone colors with a global blend.
- Four independently mixed parts with editable relative pitch intervals.
- MIDI note targets from a monophonic vocal guide, sustain, channel-aware note
  release and all-notes-off handling. Targets are limited to +/- one octave.
- Master low/high EQ, linked compression, saturation, feedback delay and six
  algorithmic room settings, with pre-delay, damping, width, send filters,
  mono send, freeze, wet-only audition and reverb bypass. Output gain is applied after the effects.
- Nine factory presets, named custom preset saving/loading, favourites, and A/B.
- Left/right peak meters, persistent clip indication, correlation, mono RMS and
  mono audition. The clip indicator reports overload; it is not a limiter.
- Full, subtle or reduced-motion display modes. A smaller 840px initial height.

## What the voices are
These are processed layers of the incoming vocal, not distinct recorded people.
The four parts use pitch shifting, not sampled soprano/alto/tenor/bass voices.
Synthetic tone colors do not replace licensed or consent-trained singer models.
There is no AI voice-conversion model, word builder or text-to-singing engine.
The effect needs incoming audio. Its sound has not been judged against I AM
CHOIR, and no claim of equivalent quality or functionality is made.

## How to use
Put a clear single-voice vocal into the audio input. Start with Intimate or Wide.
The stage controls only wet layers: for a true singer solo, set Choir Mix to 100%.
Choose a lower panel from the control-page menu to access humanize, EQ, delay or
part controls. Select a formation again after changing ensemble size to re-space
all singers. Pan is scaled by global width; it has no audible effect in mono.

The conductor pad's vertical axis changes layer gain and modulation intensity;
its horizontal axis changes ambience. Four-part mode uses the four interval
knobs relative to the input. MIDI mode requires routed MIDI plus a stable vocal
pitch and gates the wet layers when no notes are held. Only the four lowest
unique held notes are used; fewer notes repeat the highest available note.
Pitch detection is approximate (65–1000 Hz), and grain shifting is experimental,
without formant preservation. Use offline listening tests before real sessions.

## Presets and A/B
Factory: Intimate, Wide, Cloud, Massive, Tight Doubles, Wide Backing, Dream Haze,
Cathedral and Crowd Wash. These names describe DSP settings, not sampled choirs.
Select a factory preset to reset all sonic parameters, including singer edits;
its selected animation preference is preserved. Name a custom preset and click
Save. Saves create a new file rather than silently replacing an older preset.
Star toggles the current preset's favourite flag; Favourites filters the menu.

A/B stores the current side before loading the other side. Copy copies the
current sound to the other side. A/B snapshots persist while the plugin instance
exists, including editor closure; only the current sound persists in the DAW
project. The preset browser label is not a complete dirty-state indicator.

## Mac build and packaging
Run `bash Scripts/build-mac.sh` on a Mac with CMake and Xcode command line tools.
The script builds universal arm64/x86_64 AU and VST3, runs CTest, and produces
`dist/I.R.Choir-0.3.0-unsigned.dmg`. The DMG contains a package installer which
installs the compiled bundles in /Library/Audio/Plug-Ins. It does not compile
plugins on the recipient's Mac. No VST2 or AAX bundle is included.

Without your own Mac, upload the CONTENTS of this folder (including .github)
to the root of a dedicated GitHub repository. A push to main/master, or running
Build and audit Mac DMG in Actions, starts the Mac build. Successful completion
offers the DMG and test evidence as downloadable artifacts. The workflow must
actually run before a Mac installer exists.

The AU is a MIDI-controlled audio effect (MusicEffect). Host MIDI routing differs;
in Logic, use its MIDI-controlled effect workflow with a vocal audio source.
See INSTALL_MAC.md for install details and optional signing configuration.

## Tests
`g++ -std=c++17 -O2 Tests/dsp_tests.cpp -o /tmp/irchoir-dsp && /tmp/irchoir-dsp`
executes the dependency-free pitch and meter tests. CMake/CTest additionally
builds and runs the actual JUCE processor/editor smoke suite. On headless Linux,
run the native smoke under an X server. It uses an isolated temporary preset
folder and does not write to your real preset collection.

## Licensing
Original source: AGPL-3.0-or-later; full text in LICENSE.txt. JUCE 8.0.6 is fetched
from its official repository and has separate AGPL/commercial licensing. Review
all applicable terms before commercial distribution. No third-party voice
models, training data, commercial library samples or copied commercial artwork
are included. Name/trademark clearance has not been performed.

## Usability and performance polish
Parameter edits have undo/redo and descriptive tooltips. Undo history is local
to the plugin instance and excludes whole preset-file loads / A/B replacement.
FX gains, the conductor, MIDI gates and delay time are smoothed. Unused voice
slots stop processing after their fade-out; pitch modulation uses an interpolated
16-sample control rate. See AUDIT.md for measured before/after render costs.

## Branded documents and installer resources
Start with START-HERE.txt. Documentation contains a two-page Quick Start,
seven-page User Guide and four-page Release and Test Report. InstallerResources
contains the original replacement logo and branded Apple Installer welcome,
readme and license screens. The Mac packaging script uses productbuild for that
flow, and includes the PDF guides in the generated DMG. The Mac flow is prepared
and statically checked, but has not been executed on macOS here.
