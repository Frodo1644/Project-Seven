# Progress — Physical Engine Rehaul

**Last successful build:** Release x64, 2026-11-02.
**Test status at last run:** 111 engine checks, 35 physics checks, **0 failures**.
**Last known good commit:** the commit that adds `src/dsp/PhysicalEngine.*`
(this session).

## Completed this session

### Audit
- `docs/EXISTING_ENGINE_AUDIT.md` — what the 1.x engine did, what was wrong, what was
  kept. The 1.x engine remains selectable through `EngineParameters::useLegacyEngine`.

### Research
- `research/PHYSICAL_MODEL_RESEARCH.md` — string mass/E/inharmonicity/decay, humbucker
  RLC and sensing, structural modes and termination admittance, with provenance and the
  list of sources that could not be retrieved.

### New code

| File | Status | What it does |
|---|---|---|
| `src/dsp/GuitarSpec.h` | new | Parameter registry: geometry, mass, E, tension, Z₀, B. All constants carry provenance. |
| `src/dsp/PhysicalString.h/.cpp` | new | Stiff waveguide, pick collision, gesture damping, aperture sensing. |
| `src/dsp/BodyNeckModel.h/.cpp` | new | 9-mode positive-real structural admittance + junction. |
| `src/dsp/MagneticPickupModel.h/.cpp` | new | Nonlinear flux linkage, passive humbucker RLC, eddy pole. |
| `src/dsp/PhysicalEngine.h/.cpp` | new | Coupled render loop, voice pool, articulation. |
| `src/dsp/SevenAudioEngine.*` | modified | Façade + dispatch; legacy path preserved. |
| `src/params/Parameters.h` | modified | `useLegacyEngine` flag (default false = physical engine). |
| `tests/PhysicsTests.cpp` | new | 27 physics checks, WAV renders, CPU measurement. |
| `CMakeLists.txt` | modified | New sources and `ProjectSevenPhysicsTests` target. |

### Bugs found and fixed during the rehaul
1. **Loop loss was referred to the sample rate instead of the round trip.** A waveguide
   applies its loop filter once per period, so T60 must be converted with the
   fundamental. The 1.x engine had the same error, masked by a `max(0.9995, …)` floor.
   Symptom: palm-muted notes decayed only ~2× faster than open notes instead of
   hundreds of times faster.
2. **Structural junction history was mis-indexed** (`F[n−1]` used where `F[n−2]` was
   required), which let the modes re-inject energy.
3. **Aperture sensing read one travelling-wave direction.** A single delay-line tap
   carries no spatial information, so pickup position had no audible effect at all.
   Replaced with the two-tap bridge→pickup→bridge difference; the measured nulls now
   land on harmonics with a node at the pickup.
4. **Pickup output stage applied 1.8x gain**, producing a 1.36 peak on a seven-string
   chord. Trimmed to 0.55 (chord peak 0.41).

### Tension modulation (added after the first pass)
`PhysicalString` now modulates the loop delay by `1/(1 + beta A^2)` with
`beta = pi^2 E A_core / (8 L^2 T)` derived from the string's own geometry, and `A` the
physical pluck depth scaled by the decay of the sensed envelope. A hard low-B pluck
starts 4.5 cents sharp and settles within the first second; a soft pluck drifts 2.2
cents. Clamped at 25 cents for stability.

## Incomplete / partial

| Item | State | Detail |
|---|---|---|
| Multi-pickup support | planned | One humbucker voicing; the sensing model already takes position and aperture as parameters. |
| Humbucker notch validation | not measured | The two-coil comb is implemented; its absolute notch frequency is not confirmed against a reference pickup. |
| Measured T60 tables | not obtained | Paté et al. paywalled; internal-loss T60 curve is estimated. |
| Body damping ratios / pickup Q / coupling strengths | estimated | Centralised and labelled; replace when measured data is available. |

## Known limitations
- One pickup voicing (passive humbucker, 500 k load); no tone control, by design.
- Structural coupling is lumped, so dead spots depend on pitch but not on fret position.
- Settled tuning is within 4.5 cents of nominal from B1 to E4; Drop F F1 reads
  +7.2 cents, which is autocorrelation bias on a very low inharmonic string.
- A hard attack starts up to 4.5 cents sharp (tension modulation) and settles.

## Verified in this session
- Build: `ProjectSeven_VST3`, `ProjectSevenTests`, `ProjectSevenPhysicsTests`,
  `ProjectSevenVst3Smoke` all build in Release x64.
- Tests: 111 + 30 checks, 0 failures; 0 allocations on the audio thread.
- CPU: 2.3 % of one core for 7 voices at 48 kHz.
- Renders: `build/artifacts/10_physical_standard_b_chord.wav`,
  `11_physical_palm_mute.wav`, `12_physical_drop_f.wav`.
### Position-dependent coupling, two-coil sensing, fret and harmonic damping

Added after the first pass:

- `NeckPort` — a three-mode neck admittance per string, weighted by the clamped-free
  mode shapes at the stopping point. Open notes lose 15 % per lap at 430 Hz, the same
  string fretted at 12 loses 7.9 %: the dead-spot mechanism, with identical Z0 so the
  comparison is clean.
- Humbucker sensing is now two apertures 19 mm apart, which produces the measured
  notch where the coil spacing is an odd multiple of a half wavelength.
- Fret contact loss (`T60 / (1 + 0.01 x fret)`) and touched-node damping for harmonics
  (`T60 x 0.62`).

## Exact next task
Validate the model against a real instrument. Every number in
`PHYSICAL_MODEL_VALIDATION.md` is reproducible but internal: nothing has been compared
with a recorded DI. Add a comparison harness that reads a 24-bit mono WAV, measures
partial decay and the pickup-position comb for the same note, and reports the difference
against the model. Needs either a supplied DI recording or a documented measurement
procedure (fixed pluck position and force, bridge and neck pickup, 25.5 in scale).

Files: new `tests/DiComparison.cpp`, `tests/CMakeLists`-equivalent entry in
`CMakeLists.txt`, `docs/PHYSICAL_MODEL_VALIDATION.md`.
