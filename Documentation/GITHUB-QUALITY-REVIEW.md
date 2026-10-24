# I.R.Choir - GitHub quality review

Research date: 23 October 2026. These are engineering recommendations, not newly integrated capabilities. The 0.3.1 work adds the place browser and 55 original parameter designs; it does not replace the pitch or reverb engines with any of these projects.

## Highest-value next work

| Priority | Improvement | Why it matters here | Candidate / reference | Evidence required before release |
|---|---|---|---|---|
| 1 | Formant-aware high-quality pitch shifting | Current two-grain shifts can change vocal character and smear words | Signalsmith Stretch; MIT | Compare real vocals at unison, +/-5, +/-7 and +/-12 semitones; measure latency and CPU on the target Mac |
| 1 | Preserve consonants and align latency | Current MIDI wet gating can remove unpitched consonants; the dry and shifted paths need a defined timing policy | Our processor plus Signalsmith latency API | Voiced/unvoiced crossfades, phrase transitions and dry/wet alignment tests; matched-loudness listening |
| 1 | DAW-synchronized import and offline export | Current player has manual transport independent of the host | JUCE AudioPlayHead | Starts, stops, seeks, looping, tempo changes, non-real-time bounces and project recall across hosts |
| 1 | Higher-quality imported-file resampling | The current player uses linear interpolation | libsamplerate; BSD-2-Clause | 44.1/48/96/192 kHz conversions, alias rejection, duration and sample-accurate endpoints |
| 2 | Separate early reflections from the late tail | All 55 new profiles use the current six algorithmic room settings; names do not establish measured venue acoustics | Dragonfly Reverb / Freeverb3; GPL-3.0 | Compare room/hall/plate behavior and stereo/mono response; benchmark CPU and transitions |
| 2 | Optional convolution with user-owned IR files | Provides a route to captured room acoustics when valid recordings are supplied | JUCE dsp::Convolution under our existing JUCE licensing | Background IR load, normalization policy, tail/latency reporting, missing files and large-IR stress tests |
| 2 | Vocal-friendly reverb ducking and de-essing | Clearer words during singing, with space returning in gaps | Original sidechain/envelope design; ChowDSP utilities as references | No pumping, stable automation and gain-reduction metering; matched-loudness listening |
| 2 | Optional clean output ceiling, loudness matching and LUFS/true-peak meters | Sample-peak clipping indication alone is not loudness metering or true-peak limiting | libebur128; MIT | Meter conformance and intersample test signals; separate limiter tests if a limiter is added |
| 3 | Better preset search, tags and state migration | A 55-place browser benefits from search/favourites, and future versions must recall old sessions | chowdsp_utils preset/state/search modules | Old state fixtures, renamed/missing presets, favourites persistence and accessible keyboard selection |
| 3 | Stronger real-time and host validation | Current successful validator level 5 is a baseline, not a top-tier quality certification | Tracktion pluginval | Level 10/fuzz runs, long sessions, full-processor sanitizers and real-time allocation/locking checks |

## Recommended architecture experiment

Prototype four high-quality pitch/formant buses, one per relative/MIDI part, then feed the lighter individual singer variation and stereo stage. Compare this with one high-quality processor per singer before choosing an architecture. Shared buses could reduce cost, but must be measured and listened to; they may also make singers too correlated. Keep a low-latency live mode and a separately measured high-quality rendering mode if the tradeoff is worthwhile.

Use a transient/unvoiced path to preserve consonants while harmonizing voiced content, with smooth confidence-based transitions. This is a proposed design, not a claim that the current engine already performs consonant separation.

Keep advanced settings on a second page. The main flow should remain: import vocal, choose choir character, adjust width/clarity, choose a space, set output. More graphics and preset names are not evidence of better sound.

## Primary repositories reviewed

- Signalsmith Stretch: https://github.com/Signalsmith-Audio/signalsmith-stretch - C++ pitch/time processing, formant controls and explicit input/output latency. Its own documentation notes that formant correction is less sharp than specialist monophonic methods. MIT; retain notices and review dependency versions.
- Dragonfly Reverb: https://github.com/michaelwillis/dragonfly-reverb - room, hall, plate and early-reflection examples using Freeverb3. GPL-3.0; study the DSP separately from its UI/platform stack. Do not assume its code is a drop-in JUCE module.
- ChowDSP utilities: https://github.com/Chowdhury-DSP/chowdsp_utils - parameter/state/preset/search utilities and DSP modules. Licenses vary by module: several common/state/preset modules are BSD, while numerous DSP/reverb modules are GPLv3. Check the selected modules and bundled dependencies individually.
- libsamplerate: https://github.com/libsndfile/libsamplerate - sample-rate conversion library; BSD-2-Clause. Evaluate a suitable quality mode and perform file conversion off the audio callback where possible.
- libebur128: https://github.com/jiixyj/libebur128 - momentary, short-term and integrated loudness, loudness range and true-peak scanning; MIT. A meter does not provide a limiter by itself.
- JUCE: https://github.com/juce-framework/JUCE - existing framework; AudioPlayHead for host position and dsp::Convolution for optional IR processing. Use the project's pinned version when prototyping, rather than assuming master APIs match 8.0.6.
- pluginval: https://github.com/Tracktion/pluginval - strictness levels 1-10; upstream describes 5 as a generally recognized minimum for host compatibility. Current project uses pinned 1.0.4. A newer release should be evaluated deliberately, not substituted without regression tests.

No third-party code from these projects was copied or linked as part of this research. No performance or listening result for a proposed integration is claimed. No downloaded impulse-response library, proprietary EastWest preset audio or venue photographs are included.

## Release acceptance plan

1. Build a permission-cleared vocal test set covering sustained vowels, sibilants, plosives, breath, silence, pitch jumps, low/high registers and different recording sample rates.
2. Compare the current engine with each candidate at matched perceived loudness; blind listening should judge lyric clarity, metallic artifacts, pitch stability and choir plausibility.
3. Measure per-mode latency and test fixed algorithmic dry/wet alignment independently of intentional humanize delays. Validate host compensation and offline export length/end tails.
4. Record average and worst-block render times for 2, 12 and 32 layers on the actual target Mac at normal host buffers; do not use this Linux benchmark as a Mac CPU promise.
5. Test stereo and mono, preset switching during singing, sample-rate/block-size changes, save/reopen and missing source/IR files. Run stronger validator and sanitizer jobs as separate gates.
6. Only adopt an integration after it improves the listening result at an acceptable measured cost. A larger dependency list is not a sound-quality target.

## Additional review: BillyDM / awesome-audio-dsp (24 October 2026)

Source: https://github.com/BillyDM/awesome-audio-dsp . The README identifies GitHub as a read-only mirror and Codeberg as the primary project. This is a curated index, not a choir engine or a dependency to install wholesale. Its CC0 list license does not license the linked projects.

Most useful additions to our engineering backlog:

| Resource | Proposed use for I.R.Choir | Acceptance gate |
|---|---|---|
| RealtimeSanitizer, https://github.com/realtime-sanitizer/rtsan | Detect audio-callback allocations, frees and blocking library calls. Complements ASan/UBSan; those do not prove realtime safety. | Separate instrumented test target with explicit realtime context; exercise playback, import publication, automation and MIDI. Clang 20+ recommended; standalone runtime requires deliberate integration. |
| ReaderWriterQueue, https://github.com/cameron314/readerwriterqueue | Candidate for preallocated one-producer/one-consumer command or ownership handoff. Do not replace existing atomics without a measured need. | Use nonallocating try_enqueue with bounded capacity and overflow policy; avoid allocating enqueue and blocking wait methods in the callback. Reclamation stays off the callback. |
| aubio, https://github.com/aubio/aubio | Offline/reference pitch, onset and transient analysis to compare our detector and explore consonant preservation. This does not replicate lyrics or model singers by itself. | Compare low/high registers, breath, sibilants and pitch jumps; evaluate GPLv3-or-later licensing and realtime costs before integrating. |
| JUCE DSP and open-source effect collections | Study smoothing, filters, effects and UI behavior using our existing C++ framework. | Adopt a specific measurable improvement rather than another framework or full effect suite. Inspect individual licenses. |

Recommendation: prioritize the already identified pitch/formant engine experiment, consonant preservation, resampling and host synchronization. Add RealtimeSanitizer as a subsequent engineering gate. Keep queues and analysis libraries conditional on demonstrated need; avoid a Rust/framework migration for the existing JUCE plugin. None of these additional resources has been integrated or claimed as tested in 0.3.2.
