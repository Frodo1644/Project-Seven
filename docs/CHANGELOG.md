# Changelog - Project Seven

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2026-11-02

### Added
- Coupled physical engine (`PhysicalEngine`) replacing the independent voice mixdown.
- `BodyNeckModel`: nine-mode positive-real structural admittance, measured body modes
  (55.3 / 160.1 / 189.4 / 300.5 / 369.7 Hz) plus a measured 430 Hz neck mode.
  Every string drives it and receives its response, so string-structure and
  string-string coupling are real rather than mixed.
- `MagneticPickupModel`: variable-reluctance flux linkage `λ = λ₀(1 + y/g + (y/g)²)`,
  EMF as `−dλ/dt`, passive humbucker RLC (L 8.73 H, C 381 pF derived, f_res 2.76 kHz)
  and an eddy-current pole.
- `PhysicalString`: stiff waveguide with physically derived inharmonicity, tension and
  wave impedance; pick/string collision with thickness and velocity dependence;
  aperture sensing over the 2.5 in humbucker footprint.
- `GuitarSpec.h`: single parameter registry with MEASURED / DERIVED / ESTIMATED
  provenance for every physical constant.
- `NeckPort`: per-string neck termination with clamped-free mode shapes at the
  stopping point, so the loss at a fret depends on its position along the neck
  (reflectance 0.849 open vs 0.921 at fret 12 at 430 Hz).
- Humbucker sensing now uses two apertures 19 mm apart, reproducing the coil-spacing
  notch, and fret contact loss plus touched-node harmonic damping.
- Tension modulation: `df/f = beta A^2` with `beta = pi^2 E A_core / (8 L^2 T)` derived
  from string geometry, so a hard pluck starts a few cents sharp and settles as the note
  decays. Drift scales with picking force (measured 4.5 cents hard vs 2.2 cents soft on
  the low B).
- `ProjectSevenPhysicsTests`: 27 physics checks plus WAV renders and a CPU measurement.
- Documentation: `EXISTING_ENGINE_AUDIT.md`, `PHYSICAL_ENGINE_ARCHITECTURE.md`,
  `PHYSICAL_COUPLING_SPECIFICATION.md`, `PHYSICAL_MODEL_VALIDATION.md`,
  `PHYSICS_ENGINE_REHAUL_ROADMAP.md`, `PROGRESS.md`,
  `../research/PHYSICAL_MODEL_RESEARCH.md`.

### Changed
- The output no longer passes through a fixed "body resonance" filter cascade. A
  magnetic pickup does not sense body motion; body character now comes from the
  structural coupling instead.
- Palm mute and note-off remove energy from the string rather than fading an output
  gain.
- Pickup position now produces a true position comb with nulls on harmonics that have a
  node at the pickup.

### Fixed
- Loop loss was converted from T60 using the sample rate instead of the round-trip
  rate, making sustain roughly `fs/f₀` times too long and masking the error behind a
  loop-gain floor.
- Structural junction history was mis-indexed (`F[n−1]` where `F[n−2]` was required),
  allowing the modes to re-inject energy.
- Aperture sensing read a single travelling-wave direction and therefore carried no
  positional information at all.
- Pickup output stage applied 1.8x gain; seven-string chords peaked at 1.36. Trimmed to
  0.55 (chord peak 0.41).

### Preserved
- The 1.x engine (`StringModel`, `PickupModel`) is unchanged and selectable through
  `EngineParameters::useLegacyEngine` for A/B comparison.

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
