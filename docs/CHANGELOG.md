# Changelog - Project Seven

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Initial implementation of physically modelled seven-string electric guitar VST3
- Extended Karplus-Strong / digital waveguide string synthesis
- Magnetic pickup sensing with position and harmonic cancellation
- Seven independent string configurations
- Tuning presets: Standard B, Drop A, Drop G, Drop F
- Custom tuning save/load (.ps7t XML format)
- MIDI support: note on/off, velocity, pitch bend, sustain, all-notes-off
- Keyswitches: C0 (Open), C#0 (Palm), D0 (Nat. Harm), D#0 (Pinch)
- CC20 articulation control, CC64 sustain
- Natural and pinch/artificial harmonics
- Palm muting with physical damping
- Clean DI output only (no built-in effects)
- Custom dark industrial editor GUI
- Editor snapshot tool for visual verification
- VST3 host smoke test
- Comprehensive engine test suite (111 checks)
- WAV artifacts for manual listening
- **Physics engine enhancements (v1.1)**:
  - Natural sustain: strings ring out after note release
  - Djenty palm mute: strong percussive attack with controlled decay
  - Improved loop filter: gentler damping for brighter, longer sustain
  - Realistic T60 decay times for electric guitar character
  - Sustain mode for extended ringing after note-off
  - Enhanced pick attack for palm-muted djent chugs

### Fixed
- Static MSVC runtime linking (no VCRUNTIME140/MSVCP140/UCRT dependencies)
- Zero audio-thread allocations during note-on/render
- Oversized host block processing with preallocated buffer
- Retrigger same note/string click prevention (6ms fade)
- Clicks/glitches stability at common sample rates and block sizes
- Note-off behavior: strings now ring naturally instead of stopping immediately
- Palm mute sound: improved djenty character with strong attack

### Known Issues
- Cubase 15 / clean-machine validation not performed (only JUCE host smoke test)
- Manual listening not performed (WAV artifacts available)
- Some edge cases may not be fully covered

## [1.2.0] - 2026-11-01

### Added
- **Complete physics engine overhaul for realistic electric guitar sound**
- Strings now sustain for many seconds like a real electric guitar
- Guitar body resonance model (60-120 Hz fundamental, 200-400 Hz sustain)
- Pickup impedance interaction (electromagnetic damping)
- Improved pickup position sensing with physical modeling
- Full polyphony support (24 voices, no longer limited to 2)
- Voice stealing bug fixed - can now play multiple notes simultaneously

### Changed
- **String sustain**: Loop gain min 0.9995 for long sustain, T60 3-15 seconds
- **Loop filters**: fc1Base 5200-12000 Hz (was 5200-10000), fc1Mute 1800 Hz
- **High shelf**: fc2Base 22000 Hz (was 19000), fc2Mute 1200 Hz (was 900)
- **Release times**: 0.2s palm mute, 0.4s open (was 0.15s/0.3s)
- **Palm mute threshold**: 0.75 (was 0.88) for less choked sound
- **Voice pool**: Now uses full pool size for polyphony, not user parameter
- **Body resonance**: Added 60-120 Hz fundamental, 200-400 Hz sustain, 800-1200 Hz air
- **Pickup model**: Added 8kΩ coil resistance, electromagnetic damping

### Fixed
- Note-off no longer kills string immediately; strings ring for many seconds
- Palm mute now produces djenty chug with strong attack
- Polyphony bug fixed - can now play 3+ notes simultaneously
- Body resonance now modeled
- Pickup-string interaction now modeled
- All tests pass with improved physics

## [1.0.0] - 2026-10-31

### Added
- Initial public release
- All features as listed above
