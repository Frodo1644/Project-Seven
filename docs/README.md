# Project Seven

A seven-string electric guitar instrument for Windows x64, built as a VST3 plugin. It
produces its sound in real time from a coupled physical model of a solid-body guitar.
There are no samples anywhere in the project.

## What it is

MIDI goes in, a clean electric-guitar DI signal comes out. No amplifier, cabinet, EQ,
drive, gate, compression or modulation — the intended chain is

```
MIDI -> Project Seven -> amp sim / IR -> your mix
```

## The physical model

The engine is not a synthesiser with a guitar-shaped interface. Six subsystems are
modelled and mechanically coupled:

1. **Strings** — stiff digital waveguides. Mass, tension, wave impedance and
   inharmonicity are derived per string from gauge, material and the sounding pitch:
   `T = 4L²f²μ`, `Z₀ = √(Tμ)`, `B = π³Ed⁴/(64TL²)`. Fretting shortens the vibrating
   length and raises B by `1/L²` automatically. Stretching also modulates tension, so
   hard attacks start a few cents sharp and settle. The stopped end is loaded by the
   neck's own modes, so a note loses more energy to the neck at the nut than when fretted
   high — the measured dead-spot mechanism, and it is partial-selective.
2. **Picking** — a pick/string collision: the pick's thickness sets the contact width,
   velocity sets the contact time, so playing harder changes the attack spectrum and
   not just the level.
3. **Structure** — nine measured/reduced modes of body, neck and bridge, expressed as a
   positive-real mechanical admittance. Every string drives it and every string is
   driven back by it, so decay, dead spots and inter-string interaction are emergent.
4. **Pickup** — variable-reluctance sensing over the humbucker's 2.5 in aperture
   (two-tap difference, so nulls fall on harmonics with a node at the pickup), a
   nonlinear flux-linkage map, then the passive coil/cable network (L 8.73 H,
   f_res 2.76 kHz, eddy pole).
5. **Articulation** — palm muting and note-off remove energy at the contact; they are
   not output envelopes.
6. **Harmonics** — natural and pinch harmonics as modal excitation of the string.

Details: `PHYSICAL_ENGINE_ARCHITECTURE.md`, `PHYSICAL_COUPLING_SPECIFICATION.md`.
Measured numbers and sources: `../research/PHYSICAL_MODEL_RESEARCH.md`.
Validation results: `PHYSICAL_MODEL_VALIDATION.md`.

## Controls

| Control | Function |
|---|---|
| Tuning preset | Standard B, Drop A, Drop G, Drop F, Custom |
| String 1…7 | Independent pitch per string (MIDI note) |
| Articulation | Open, Palm Muted, Natural Harmonic, Pinch Harmonic |
| Pick Position | Distance from the bridge, % of scale length |
| Pick Intensity | Pick stiffness/force, affects attack brightness |
| Pickup Position | Distance from the bridge, % of scale length |
| Palm Mute | Amount of palm contact |
| Polyphony | 1…24 voices |
| Pitch Bend Range | 0…12 semitones |
| Output Level | −24…+6 dB |

Keyswitches: C0 open, C#0 palm mute, D0 natural harmonic, D#0 pinch harmonic.
CC20 selects articulation latched; CC64 sustain; CC120/121/123 all-notes-off.

## Requirements

- Windows x64, a VST3 host (developed against Cubase Pro 15).
- No Visual C++ redistributable and no development tools needed: the plugin links the
  static MSVC runtime.
- No sample library.

## Building from source

See `BUILD.md`. Short version:

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target ProjectSeven_VST3
```

## Typical performance and tuning

Standard B (B1 E2 A2 D3 G3 B3 E4), Drop A (A1 E2 A2 D3 G3 B3 E4),
Drop G (G1 D2 G2 C3 F3 A3 D4), Drop F (F1 C2 F2 Bb2 Eb3 G3 C4).

## Known limitations

- One pickup voicing (passive humbucker, 500 k load); no tone control, by design.
- Structural coupling uses one effective point, so dead spots depend on pitch but not
  on fret position.
- No tension modulation, so there is no amplitude-dependent pitch drift.
- Body damping, pickup Q and structural coupling strengths are estimated, not measured;
  they are centralised and labelled in `../src/dsp/GuitarSpec.h` and
  `../research/PHYSICAL_MODEL_RESEARCH.md`.
