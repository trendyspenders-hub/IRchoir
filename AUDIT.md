# I.R.Choir 0.3.0 build audit

Status: native prototype with a successful Linux build and automated checks.
NOT a verified Mac release. This report records the authoring environment's
results; a later Mac runner's logs must establish its separate results.

## Executed checks
| Check | Result | Scope / limits |
|---|---|---|
| Full JUCE 8.0.6 Release build | PASS | Linux x86_64 VST3, native processor and native editor; not Mac AU |
| CTest DSP suite | PASS | Pitch detection at 44.1/48/96 kHz, silence clearing, +/- octave test-tone spectrum, correlation and mono cancellation |
| CTest native processor/editor smoke | PASS | 44.1/48/96 kHz; block sizes 1/64/512; three harmony modes; finite output; note channels/sustain/panic; state round trip; FX extremes; mono layout/audition; editor creation, resizing and factory controls |
| Preset / interaction smoke | PASS | Temporary-folder preset save/load and favourite persistence, A/B, group actions, conductor drag, singer drag/mute, undo/redo |
| Native editor rendering | PASS | Actual JUCE editor snapshot inspected; singer overlap and meter-label collision fixed at 1180x840 |
| pluginval 1.0.4, strictness 5 | PASS | Linux VST3, deterministic seed 12345; GUI tests skipped. Native GUI has separate offscreen smoke/render coverage |
| ASan + UBSan DSP helper suite | PASS | LeakSanitizer disabled because process inspection is restricted here. This does not establish leak freedom; full processor was not run under sanitizers |
| Strict syntax check | PASS at pre-polish checkpoint | -Wall -Wextra -Wpedantic -Werror against JUCE headers. Final source additionally compiled and ran in the full build |
| Shell scripts and workflow | PASS static checks | bash -n and YAML parsing; does not validate Apple packaging execution |
| Mac arm64 / x86_64, AU, auval | NOT RUN | No macOS environment or project repository runner available in this session |
| Actual DMG creation / installation | NOT RUN | No .dmg or Mac plugin binary has been created here |
| Apple signing / notarization | NOT RUN | No user certificates or keychain profile supplied |
| Real vocal listening / target-DAW tests | NOT RUN | Test tones are not a substitute for musical evaluation |

See TestEvidence for the raw executed-test logs, CPU measurements and native
interface image. The source archive contains no Mac binaries or Linux binaries.

## Fixes made during this audit
- Fixed invalid JUCE slider color identifiers in the earlier uncompiled GUI.
- Fixed the VST3 manifest loader failure caused by dotted native bundle names.
  Native files are now IRChoir.component / IRChoir.vst3; display name I.R.Choir.
- Avoided destructive formation changes during parameter-state recall.
- Preserved mono behavior when panning wet layers; moved output gain after FX.
- Added soft MIDI gating and bounded dual-grain shift buffers.
- Fixed stage overlap, group-selection clearing and meter label collisions.
- Fixed snapshot output appending, which had displayed a stale image.
- Isolated automated preset tests from the user's real preset folder.
- Added parameter undo/redo, tooltips, FX/conductor smoothing, and fractional,
  smoothed delay-time reads. A delay-time change can still produce tape-like
  pitch movement; it is not a crossfade between two independent delay lines.
- Stopped processing unused voice slots after their fade-out. Pitch targets are
  calculated every 16 samples and interpolated, rather than recalculated on
  every sample for every layer.

## CPU benchmark: observed wall-clock render cost
48 kHz, 256-sample blocks, 2 seconds of a 220 Hz stereo guide, Release build,
this Linux environment. Percent is elapsed/render duration, not measured OS CPU.

| Layers | Before: seconds | After: seconds | Before budget | After budget |
|---:|---:|---:|---:|---:|
| 2 | 0.3386 | 0.0277 | 16.93% | 1.39% |
| 12 | 0.3330 | 0.0842 | 16.65% | 4.21% |
| 32 | 0.3430 | 0.1840 | 17.15% | 9.20% |

These observations are not Mac performance guarantees or maximum-load tests.

## Remaining findings and release gates
1. **Product gap:** no trained choir conversion engine, human singer identities,
   licensed choir samples, text-to-singing or phoneme/word builder. Synthetic
   tone colors and shifted parts must not be advertised as those capabilities.
2. **Sound quality:** the monophonic guide detector is approximate and can lose
   unvoiced/noisy notes. Pitch shifting has no formant preservation and may smear
   consonants or produce grain artifacts. MIDI targets are the lowest four unique
   held notes, repeated if necessary and limited to +/- 12 semitones from input.
3. **Host release gate:** Mac universal build, AU validation, VST3 validation on
   Mac, actual installer install/remove and target DAW project recall remain.
4. **Automation / DSP:** smoothing was added, but rapid automation, mode changes,
   discontinuous input pitch and extreme FX still need long-form listening and
   dropout testing. Clip indication does not prevent clipping.
5. **UI / history:** A/B alternate snapshots are instance-local, not saved across
   a DAW restart. Undo covers parameter edits; loading complete preset files and
   A/B state replacement are separate operations. Preset labels do not indicate
   every unsaved edit. More keyboard/screen-reader work remains beyond sliders.
6. **Warnings:** JUCE's aggressive recommended conversion warnings remain at
   integer/array-index boundaries. Bounds are constrained, but explicit cleanup
   is still appropriate before a production release.
7. **Distribution:** the default DMG pipeline is unsigned/unnotarized. Optional
   certificate/profile support requires actual Apple credentials. Check JUCE
   licensing and any future model/data licenses before distribution.

## Mac pipeline prepared, not executed
Scripts/build-mac.sh -> universal AU + VST3 -> CTest -> package-mac.sh -> .pkg
inside .dmg with corresponding source and license copies. The GitHub workflow
also invokes pinned pluginval and auval before offering a download. Packaging
fails if either native plugin bundle is missing or lacks either architecture.
A dedicated repository link is needed to connect this project to a Mac runner;
no unrelated repository was modified.

## Detailed choir and reverb update
Native Release build and both CTest targets pass after the visual/reverb changes. pluginval 1.0.4 strictness 5 passes (GUI tests skipped; native editor separately exercised). Reverb checks cover exact initial bypass, wet-only pre-delay, a nonzero tail, all six rooms across 32 toggle combinations, and state recall. Updated native performance/reverb screenshots are included. Visual singer identities are decorative; the audio still uses synthetic voice colors. Mac execution remains pending.
