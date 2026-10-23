# Requested feature status

| Request | Implementation |
|---|---|
| Singer groups | Shift-click multi-selection plus section/all buttons, group pan/level/mute/solo |
| Formation presets | Semicircle, cluster, wide rows; pan changes the audio |
| Humanize | Separate delay variation, pitch drift, vibrato |
| Conductor pad | Intensity/space parameters, audio-connected; equivalent sliders |
| Preset favourites + A/B | Custom XML presets, disk-backed favourites, two in-memory A/B snapshots |
| Effects rack | Low/high tone EQ, linked compressor, saturation, delay, algorithmic rooms |
| Output meters | L/R peak, clip latch, correlation, mono RMS and mono audition |
| Animation controls | Full/subtle/reduced motion |
| MIDI harmonies | Experimental monophonic pitch detection plus four MIDI note targets |
| Soprano/alto/tenor/bass parts | Four labeled pitch-shifted parts with separate level and interval; not sampled SATB voices |
| Distinct singer identities | Four synthetic tonal colors per layer; actual new human/AI identities are NOT implemented |
| AI choir conversion and words | NOT implemented; needs a separately licensed/trained engine and recordings |
| Mac DMG containing AU + VST3 installer | Build scripts and CI provided; actual Mac execution required |
| Signed/notarized public installer | Optional local script support; requires user's Apple certificates and profile |

No placeholder text-to-singing or AI-model controls are presented as functioning
features. These missing capabilities are substantial work, not installation steps.

Additional polish: parameter undo/redo, tooltips, smoothed FX and conductor,
soft MIDI gates, fractional delay reads, and active-voice CPU optimization.
See AUDIT.md for explicit executed coverage and remaining release gates.

Detailed vector singers include varied faces, skin tones, hairstyles, robes and audio-reactive mouths. These are visual identities, not trained voice identities. Reduced motion keeps mouths and glow static.

Reverb space page: six algorithmic settings, pre-delay, decay shape, width, damping, low/high send-filter toggles and frequencies, mono send, freeze, wet-only and bypass. Automated checks cover dry bypass, delayed wet onset, all 192 room/toggle combinations, and state recall.

Direct vocal import: local WAV/AIFF/FLAC, background decode, Play/Stop/Loop, file/DAW source switch, sample-rate conversion and project file references. The source performance supplies lyrics to all choir layers; there is no transcription, voice-cloning model or new-lyric generation. Playback is manually controlled and recordings are not embedded in presets.
