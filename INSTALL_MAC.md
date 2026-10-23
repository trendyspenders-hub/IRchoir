# Installing I.R.Choir on Mac

No Mac binary is present in the source archive. First run the supplied Mac build
locally or in GitHub Actions and download its successful DMG artifact.

1. Quit your DAW. Open I.R.Choir-0.3.0-unsigned.dmg.
2. Read the included audit. Open the .pkg and follow Apple's Installer prompts.
3. The package installs these compiled plugin bundles:
   /Library/Audio/Plug-Ins/Components/IRChoir.component
   /Library/Audio/Plug-Ins/VST3/IRChoir.vst3
4. Restart your DAW and rescan its plugin manager. Use AU in Logic or VST3 in a
   compatible host. The AU is a MIDI-controlled effect, not a virtual instrument.
5. Feed the audio input a single vocal. For MIDI harmonies, route note events
   through the host's supported MIDI/audio effect routing. Input audio is still
   required; pressing a MIDI key alone does not synthesize a voice.

Target: macOS 12+, Intel and Apple Silicon. Actual Mac compatibility and DAW scans
must be confirmed after a successful Mac runner build. VST2 and AAX are absent.
Unsigned downloads may be blocked by macOS. Use only builds whose origin and
source you trust; do not disable Gatekeeper globally.

## Signed distribution
On a Mac with certificates already installed in your keychain, set:

    export IRCHOIR_APP_IDENTITY='Developer ID Application: YOUR NAME (TEAMID)'
    export IRCHOIR_INSTALLER_IDENTITY='Developer ID Installer: YOUR NAME (TEAMID)'
    export IRCHOIR_NOTARY_PROFILE='YOUR_EXISTING_KEYCHAIN_PROFILE'
    bash Scripts/build-mac.sh

Both identities are required together. The optional existing notary profile
allows submission, waiting for Apple's result, and stapling the resulting DMG.
No certificates, Apple account access or notarization credentials are included.
The default GitHub workflow produces an ad-hoc-signed plugin inside an unsigned
installer/DMG and does not import signing secrets. Signed is not the same as
notarized; inspect the script output and Apple's acceptance result.

## Presets and removal
Custom presets and favourites are saved under your user application-data location
in IRChoir. They are not inside either plugin bundle. To uninstall, quit the DAW,
remove only the two bundles listed above, and rescan. Your custom presets remain.

## Release validation still required
Mac native/Intel builds; auval; VST3 validation; a fresh machine install/uninstall;
Logic/Ableton/REAPER routing and project recall; long playback and automation;
CPU and dropout measurements; listening tests, especially with real vocals,
unvoiced consonants, noisy input, rapid pitch changes and stereo cancellation.
