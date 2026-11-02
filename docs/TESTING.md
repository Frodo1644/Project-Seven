# Testing - Project Seven

## Test Overview

Project Seven has comprehensive automated tests covering audio engine behaviour, MIDI handling, parameter management, and VST3 integration.

## Engine Tests

The engine test suite runs 111 checks with 0 failures at 48 kHz. A further 35 checks
verify the physical model directly (`ProjectSevenPhysicsTests`).

### Physics probe (`ProjectSevenPhysicsTests`)

`tests/PhysicsTests.cpp` drives `PhysicalEngine` headlessly and reports measured values:

- decay envelope per 0.25 s for open and palm-muted notes;
- pitch accuracy in cents for B1-E4 and Drop F F1;
- partial-by-partial decay of E2 (higher partials must die first);
- pickup position comb (bridge vs neck, partial amplitudes);
- structural coupling (`getStructureVelocity()`, string `Z0`);
- flux-linkage nonlinearity and the derived humbucker capacitance/resonance;
- output peaks for a single note and a seven-string chord;
- tension modulation: attack drift, settled pitch and the force-scaling law;
- neck termination: coupling weight, reflectance and passivity versus fret;
- pickup position comb and two-coil aperture;
- CPU for 7 voices over 10 s;
- WAV renders written to the working directory's `artifacts/` folder.

Run it with:

```
cmake --build build --config Release --target ProjectSevenPhysicsTests
./build/ProjectSevenPhysicsTests_artefacts/Release/ProjectSevenPhysicsTests.exe
```

Measured results are recorded in `PHYSICAL_MODEL_VALIDATION.md`.

### Categories

#### Frequency Accuracy
- String frequency matches expected pitch for standard tunings
- Drop tunings accurately modelled
- Custom tunings resolve correctly
- Pitch bend shifts frequencies as expected

#### Excitation
- Note-on produces expected attack transient
- Velocity affects excitation amplitude
- Pick position affects timbre
- Pick force affects brightness

#### Decay
- T60 (decay time) matches parameter settings
- Pitch-dependent T60 adjustment works
- Note-off decay behaves correctly (strings ring naturally after release)
- Retrigger fade prevents clicks
- Sustain mode: strings continue vibrating after note-off with natural decay
- **Extended sustain**: strings can ring for 3-15 seconds depending on pitch
- **Loop gain**: min 0.9995 for very long sustain

#### Damping
- String damping parameter affects decay
- Palm mute increases damping (djenty character)
- Palm mute shortens effective string length
- Palm mute produces strong attack for djent chugs
- Damping values within valid range
- **Palm mute threshold**: 0.75 for less choked sound

#### Sustain
- Strings ring after note-off (natural sustain)
- Sustain pedal (CC64) extends decay when held
- Sustain mode enables longer decay (loopGainSustain)
- Release time: 0.2s (palm mute), 0.4s (open)
- **Realistic electric guitar sustain**: low strings ring longer than high strings

#### Body Resonance
- Body resonance affects overall tone
- Low frequencies enhanced by body response
- Sustain nodes boost mid frequencies
- Air resonance adds warmth
- Body parameters within valid range

#### Pickup Interaction
- Pickup damping affects string sustain
- Pickup position affects harmonic content
- Electromagnetic damping at string ends
- Stronger pickup = more damping

#### Harmonics
- Natural harmonics at expected fret positions
- Pinch harmonics produce expected timbre
- Flageolet excitation for harmonic notes
- Harmonic content matches physical model

#### Tuning
- Standard B, Drop A, Drop G, Drop F presets correct
- Note names match standard guitar conventions
- Frequencies match 12-TET tuning (±0.1 Hz tolerance)

#### MIDI
- Note on/off handled correctly
- Pitch bend applied to all active voices
- Sustain pedal (CC64) holds/releases notes
- All notes off (CC120/121/123) clears voices
- Keyswitches (C0-C#0-D0-D#0) change articulation
- CC20 latches articulation
- Voice stealing under polyphony limit

#### Velocity
- Velocity affects excitation amplitude
- 0 velocity produces no sound
- Maximum velocity produces full excitation

#### Polyphony
- Maximum polyphony respected (default 12)
- Voice stealing allocates oldest non-sustaining voice
- Retrigger replaces existing voice on same string

#### Parameter Persistence
- State save/load preserves all parameters
- Tuning presets saved and loaded correctly
- String parameters preserve per-string values

#### Audio Stability
- No clicks or glitches at common block sizes (64-1024 samples)
- No clicks or glitches at common sample rates (44.1/48/88.2/96/192 kHz)
- No audio-thread allocations during note-on/render
- Oversized host blocks processed without allocation

## VST3 Smoke Test

The smoke test loads the VST3 bundle in a JUCE VST3 host and verifies:

- Bundle is discovered and loaded
- Plugin identifies as instrument (no inputs, stereo output)
- MIDI note-on produces audio output (peak measured)
- 2-second audio render is finite (not NaN/inf)
- State save/load round-trips correctly
- Hosted editor can be created (1000×650)

## Visual Verification

### Editor Snapshot
The editor snapshot tool renders the plugin editor to PNG:

```bash
ProjectSevenEditorSnapshot.exe editor_preview.png 1000 650 <preset_index>
```

Presets:
- 0: Standard B
- 1: Drop A
- 2: Drop G
- 3: Drop F

Two screenshots verified:
- `editor_preview.png` (1000×650) - default view
- `editor_small.png` (900×585, Drop F preset, note E2)

### Visual Checks
- Dark industrial palette (#101113 bg, #202328 panels, #343840 borders)
- Accent colour #B52132, highlight #E03B4B
- Seven string columns with note name, frequency, damping slider
- Articulation buttons (Open, Palm, Nat. Harm, Pinch)
- Plectrum & Pickup knobs (Pick position, Pick force, Pickup position)
- Voice controls (Polyphony, Bend range, Output)
- Footer status (active voices, last string, last fret)
- Tuning preset selector with save/load

## Audio Artifacts

WAV files for manual listening in `artifacts/`:

| File | Description |
|------|-------------|
| `01_standard_b_chord.wav` | Standard B tuning chord |
| `02_palm_mute_chugs.wav` | Palm-muted chugging pattern |
| `03_natural_harmonics.wav` | Natural harmonic sweep |
| `04_drop_f_low.wav` | Drop F tuning low notes |

## Manual Testing Checklist

### Basic Playback
- [ ] Open plugin in Cubase 15 (or other VST3 host)
- [ ] Play a single note, verify clean electric guitar tone
- [ ] Play a chord, verify polyphony
- [ ] Play with varying velocity, verify dynamic response

### Articulations
- [ ] Open articulation sounds clean/un-dampened
- [ ] Palm mute produces dampened, percussive tone
- [ ] Natural harmonic at 12th fret (and other positions)
- [ ] Pinch harmonic produces squeal
- [ ] Keyswitches (C0-C#0-D0-D#0) trigger articulations
- [ ] CC20 changes articulation

### Tunings
- [ ] Standard B tuning sounds correct
- [ ] Drop A tuning sounds correct (low A)
- [ ] Drop G tuning sounds correct
- [ ] Drop F tuning sounds correct (very low F)
- [ ] Custom tuning can be saved and loaded

### Effects
- [ ] Pitch bend shifts pitch smoothly
- [ ] Sustain pedal holds notes
- [ ] All notes off clears held notes

### CPU and Stability
- [ ] Plugin runs without audio glitches at 44.1 kHz
- [ ] Plugin runs without audio glitches at 48 kHz
- [ ] CPU usage reasonable with 7-string chords
- [ ] No clicks during note retrigger

### Known Limitations
- Manual listening not yet performed (rely on WAV artifacts)
- Cubase 15 validation not yet performed (rely on JUCE host smoke test)
- Some edge cases may not be covered

## Running Tests

### Engine Tests
```bash
build/ProjectSevenTests_artefacts/Release/ProjectSevenTests.exe
```

### VST3 Smoke Test
```bash
build/ProjectSevenVst3Smoke_artefacts/Release/ProjectSevenVst3Smoke.exe
```

### CTest
```bash
cd build
ctest -C Release --output-on-failure
```
