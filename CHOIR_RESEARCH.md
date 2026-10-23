Historical v0.2 research notes. FEATURE_STATUS.md and AUDIT.md describe the current v0.3 implementation.

# Choir research and I.R.Choir v0.2 changes
Reviewed 23 October 2026, using manufacturer pages.

| Reference | Relevant behavior | I.R.Choir decision |
|---|---|---|
| Antares Choir | Up to 32 unison voices, pitch/timing/vibrato variation, stereo spread | Expanded DSP layering to 32; retained modulated delay and stereo spread. This is not independent pitch/vibrato modeling. |
| Antares Harmony Engine | Four harmony voices plus additional unison layering | Per-layer level, pan, mute and solo implemented. Musical interval generation remains future work. |
| Spitfire Eric Whitacre Choir | Expression/dynamics controls, evolving textures and section legatos | Simple macros and cloud-like DSP presets implemented. No sampled evolutions or legato added. |
| Native Instruments / Strezov Choir: Omnia | Independent SATB sections, syllable sequences, editable presets, mic mix and ambience | Stage rows, native preset menu and algorithmic room ambience implemented. Rows are visual organization, not SATB voices or mic positions. |
| EastWest Hollywood Choirs | WordBuilder, phoneme timing/balance and phrase presets | Word/phoneme editing remains future work requiring an appropriate engine and licensed recordings. No dummy lyric controls added. |

## Native stage interactions
32 per-layer meters feed singer glows from the audio thread via atomics. Click a
singer to select. Drag left/right to pan and up/down to adjust level; double-click
to toggle mute. The inspector exposes conventional sliders and mute/solo buttons.
Only the wet ensemble layers are muted/soloed: use 100% Choir Mix to hear a
selected singer alone. The dry input remains audible when the mix is below 100%.
Stage pan is scaled by global Stereo Width; panning has no effect in mono.
All parameters are host-automatable and saved through the parameter state.

## Included preset bank
| Preset | Layers | Character |
|---|---:|---|
| Intimate | 4 | Narrow, restrained motion, little ambience |
| Wide | 12 | Broad unison backing |
| Cloud | 24 | More movement and ambience |
| Massive | 32 | Dense spread |
| Tight Doubles | 2 | Subtle doubling, no room |
| Wide Backing | 12 | Wide background-vocal effect |
| Dream Haze | 24 | Strong motion and ambience |
| Cathedral | 16 | Maximum room blend; algorithmic reverb, not a cathedral impulse response |
| Crowd Wash | 32 | Strong motion, mostly wet |

These presets adjust existing DSP parameters. Their names describe intended
character, not measured acoustic authenticity. They need listening validation.
Selecting a preset resets all singer mutes, solos, positions and levels.

## Boundaries
No sampled singer identities, AI models, word generation, SATB synthesis,
convolution hall, pitch-correct harmonies or microphone recordings are included.
The current effect requires incoming vocal audio. Neither the code nor the GUI
has been compiled or tested in a Mac DAW. The workflow builds an unsigned pkg
only after a successful Mac runner build. No installer binary is in this zip.

## Primary references
- https://www.antarestech.com/products/creative-vocal-effects/choir
- https://www.antarestech.com/blog/tutorial-introduction-to-harmony-engine
- https://support.spitfireaudio.com/en/articles/11815820-eric-whitacre-choir-faq
- https://www.native-instruments.com/products/choir-omnia
- https://www.soundsonline.com/vocals/hollywood-choirs
