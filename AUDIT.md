# I.R.Choir 0.3.2 - audit scope

The current Release and Test Report PDF records the exact build commit, workflow
run and completed results. Installer publication is gated on all workflow steps
succeeding. This source file describes coverage; it is not an independent PASS
certificate for an arbitrary local rebuild.

## Automated release gates

- Universal AU and VST3 build: arm64 and x86_64 bundles.
- DSP and native processor/editor tests: all pages, groups, formations, presets,
  favourites, A/B, undo/redo, state validation, meters and reverb combinations.
- All 55 original place designs, reverb-only edits, browser and state recall.
- WAV/AIFF/FLAC import, stereo preservation, corrupt/unsupported files, looping,
  stop/end, source switching, asynchronous recall, missing/empty paths and relink.
- NaN/Inf/extreme-input recovery; rejected malformed parameter state and presets.
- DMG checksum/structure, mount, actual package install, signatures and architectures.
- Installed VST3 pluginval 1.0.4 strictness 10, seed 12345; installed AU auval.
- ASan/UBSan DSP and native processor/editor tests, with leak detection disabled.
- Refreshed documents and DMG, then final package reinstall and both validators.

A sanitizer finding in JUCE 8.0.6 bundled libFLAC encoder test setup was recorded.
The plugin imports but does not export FLAC. Tests now use an original fixed
synthetic FLAC fixture; its decoding remains fully instrumented. No sanitizer
suppression was added. WAV/AIFF generation remains exercised in test setup.

## Limits and remaining acceptance work

Runtime gates execute on Apple Silicon; actual Intel execution is unverified.
No completed result establishes every DAW, realtime-safety certification, thread
race freedom, leak freedom, or subjective radio-ready sound quality. Real DAW
scanning, routing, automation, reopening and long sessions remain to be checked.
Real-vocal listening must evaluate consonants, pitch transitions, mono and matched
loudness. Developer ID signing and notarization are not included in this unsigned
release. Historical evidence is identified by its older version/run.

## Sound and import scope

The engine layers a supplied recording. It does not include trained singers,
commercial choir samples, a word builder, formant preservation or new lyrics.
Visual identities are decorative. Import accepts local WAV/AIFF/FLAC mono/stereo,
8-192 kHz, up to five minutes and 128 MiB decoded. Playback is manual, independent
of host transport. Audio is referenced by path, not embedded. Decode/allocation
run on a worker; audio reads preloaded memory through a nonblocking try-lock.
Import tests use synthetic audio; human listening is separate. All 55 places
are original parameter designs using six base profiles of the existing JUCE
algorithmic reverb, not measured venues or third-party impulse responses.

Research recommendations in Documentation/GITHUB-QUALITY-REVIEW.md are not
implemented integrations. See that review for the proposed next quality gates.

Pro Tools: the AU/VST3 installer does not load directly in Pro Tools. See
Documentation/PROTOOLS-AAX.txt for the separate unsigned AAX developer target,
required Avid/PACE signing and uncompleted host testing.
