# Architecture - Project Seven

## Overview

Project Seven is a physically modelled seven-string electric guitar VST3 instrument. It uses digital waveguide synthesis (extended Karplus-Strong) to model steel strings, with magnetic pickup sensing and electrical response modelling.

## High-level Architecture

```
MIDI Input → Voice Manager → String Models (×7) → Pickup Model → Output
```

### Components

1. **PluginEntry**: VST3 entry point, registers the plugin
2. **PluginProcessor**: Audio processor, parameter management, MIDI handling, audio rendering
3. **PluginEditor**: Custom GUI with seven string columns, articulation controls, pickup controls
4. **SevenAudioEngine**: Voice pool management, note allocation, pitch bend, sustain
5. **StringModel**: Per-string waveguide synthesis
6. **PickupModel**: Shared pickup electrical response

## String Model

Each string is modelled as a digital waveguide with:

- **Fractional delay line** for pitch accuracy
- **Two one-pole loop lowpass filters** for body resonance and frequency-dependent damping
- **Four allpass dispersion sections** for inharmonicity (stiff string physics)
- **Triangular excitation with pick smoothing and noise**
- **Pickup comb filter** (two-tap) for pickup position effects
- **Pitch-dependent T60** decay time (longer for low strings)
- **Palm-mute damping** (increased damping, shortened effective length, djenty character)
- **Flageolet (natural harmonic) excitation**
- **Sustain model**: strings ring naturally after note release, simulating real electric guitar behavior

### Physics Improvements (v1.1)

The string model has been enhanced for more realistic electric guitar behavior:

**Loop Filter**:
- Base cutoff: 5200-10000 Hz (gentler than before, preserves brightness)
- Palm mute cutoff: 1800 Hz (tighter but not too dark)
- High-shelf: 22000 Hz base, 1200 Hz mute

**Decay Times**:
- Base T60: 7.5 * pow(82.407/freq, 0.35) → 3.0-12.0 seconds
- Mute T60: 0.35 * pow(82.407/freq, 0.20) → 0.15-0.60 seconds
- Harmonic: 1.20x multiplier
- Pinch: 0.85x multiplier

**Sustain Model**:
- Strings continue vibrating after note-off (natural ring)
- Separate output gain release for controlled fade
- Sustain mode enables longer decay (loopGainSustain = 0.9999)
- Release time: 0.15s (palm mute), 0.3s (open)

**Palm Mute Djenty Character**:
- Stronger transient attack (1.4x boost for palm mute)
- Mute threshold: 0.75 minimum (less choked than before)
- Tighter high-frequency damping while keeping fundamental strong

### Parameters per string
- Open tuning frequency (note)
- Damping (T60)
- Pick position
- Pick force
- Pickup position

### Tuning
- Seven strings, lowest to highest
- Standard B, Drop A, Drop G, Drop F presets
- Custom tunings with save/load to `.ps7t` files (XML format)

## Pickup Model

Shared across all strings:

- **28 Hz high-pass filter** (removes DC, very low rumble)
- **~4300 Hz resonance** (Q ~1.28)
- **~9500 Hz rolloff** (gentle high-frequency attenuation)
- **DC blocker**

The pickup position affects the comb filter in the string model, creating the characteristic harmonic cancellation of magnetic pickups at different positions along the string.

## Voice Management

- **Voice pool** with configurable maximum polyphony (default 12)
- **Deterministic string/fret mapping**: MIDI note → string + fret
- **Pitch bend**: All active voices shifted by bend amount
- **Sustain**: CC64 holds notes; when released, strings ring naturally with extended decay
- **Voice stealing**: Oldest non-sustaining voice when polyphony exceeded
- **Retrigger**: Same note/string retrigger kills old voice with 6ms fade to prevent clicks

### MIDI mapping
| CC/Note | Function |
|---------|----------|
| C0 | Open articulation (momentary) |
| C#0 | Palm mute articulation (momentary) |
| D0 | Natural harmonic articulation (momentary) |
| D#0 | Pinch harmonic articulation (momentary) |
| CC20 | Articulation (latched) |
| CC64 | Sustain pedal (strings ring after release) |
| CC120 | All notes off |
| CC121 | All sounds off |
| CC123 | All controllers off |

## Plugin Structure

### PluginProcessor
- `AudioProcessorValueTreeState` (APVTS) for parameter management
- Parameter listeners for presets and string synchronisation
- MIDI input handling in `processBlock`
- State save/load via `getStateInformation`/`setStateInformation`
- Untimed parameter change notifications

### PluginEditor
- Custom `SevenLookAnd Feel` for dark industrial styling
- Seven string columns (String 1 to String 7 from high to low)
- Articulation buttons (Open, Palm, Nat. Harm, Pinch)
- Plectrum & Pickup knobs (Pick position, Pick force, Pickup position)
- Voice controls (Polyphony, Bend range, Output)
- Tuning preset selector with save/load
- Footer status display (active voices, last string, last fret)
- Timer-based status updates (300ms default, faster in plugin UI thread)

## Memory and CPU

- Preallocated buffers for note-on voices (no audio-thread allocation)
- `monoBuffer` of `max(8192, samplesPerBlock)` for oversized block processing
- Chunked rendering for very large host blocks
- No dynamic allocation during audio rendering
- Low CPU: ~0.74% real-time for 7 voices at 48 kHz on test machine

## Thread Safety

- Audio thread: `processBlock` reads parameters atomically, writes audio
- UI thread: Editor updates parameters via APVTS
- Status fields are `std::atomic<int>` for safe cross-thread reads
- `momentaryArticulation` is `std::atomic<int>` for UI reads

## Testing

See `TESTING.md` for test coverage details.

Tests cover:
- Frequency accuracy
- Excitation response
- Decay characteristics
- Damping behaviour
- Harmonic production
- Tuning accuracy
- MIDI handling
- Velocity response
- Pitch bend
- Polyphony and voice stealing
- Parameter persistence
- Audio stability

## Visual Design

Dark industrial palette:
- Background: #101113
- Panels: #202328
- Borders: #343840
- Text: #E5E7EA
- Secondary text: #9298A2
- Accent: #B52132
- Highlight: #E03B4B
- Optional accent: #C58B4A

## Distribution

The VST3 bundle includes:
- `Project Seven.vst3` (DLL + VST3 metadata)
- Editor resources (icons, fonts)
- No external dependencies (static runtime)

Install to VST3 scan path (e.g., `C:/Program Files/Common Files/VST3/`).
