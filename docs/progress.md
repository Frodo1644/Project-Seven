# Progress — Physical Engine Rehaul

**Last successful build:** Release x64, 2026-11-02.
**Test status at last run:** 111 engine checks, 27 physics checks, **0 failures**.
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
4. **Pickup output stage applied 1.8× gain**, producing a 1.36 peak on a seven-string
   chord. Trimmed to 0.55 (chord peak 0.41).

## Incomplete / partial

| Item | State | Detail |
|---|---|---|
| Tension modulation | not implemented | No pitch drift or amplitude-dependent intermodulation. |
| Position-dependent mobility | approximation | All strings drive one structural point. |
| Humbucker two-point antiphase sensing | not implemented | The measured 3–4 kHz low-string notch is absent. |
| Natural harmonic node damping | partial | Modal excitation on the open string; the touching finger's extra damping is not modelled. |
| Measured T60 tables | not obtained | Paté et al. paywalled; internal-loss T60 curve is estimated. |
| Body damping ratios / pickup Q / coupling strengths | estimated | Centralised and labelled; replace when measured data is available. |

## Known limitations
- One pickup voicing (passive humbucker, 500 k load); no tone control, by design.
- Structural coupling is lumped, so dead spots depend on pitch but not on fret position.
- The pitch error is up to 6 cents at the extremes (dispersion phase-delay estimate).

## Verified in this session
- Build: `ProjectSeven_VST3`, `ProjectSevenTests`, `ProjectSevenPhysicsTests`,
  `ProjectSevenVst3Smoke` all build in Release x64.
- Tests: 111 + 27 checks, 0 failures; 0 allocations on the audio thread.
- CPU: 2.0 % of one core for 7 voices at 48 kHz.
- Renders: `build/artifacts/10_physical_standard_b_chord.wav`,
  `11_physical_palm_mute.wav`, `12_physical_drop_f.wav`.

## Exact next task
Implement tension modulation in `PhysicalString::pullIncident`: modulate the loop delay
by `1 + β·|y|²` with `β = E·A/(T·L²)` clamped, and verify with a new probe check that
the fundamental drifts up by 2–6 cents immediately after a hard attack and settles,
without breaking the ≤ 8-cent tuning bound.

Files: `src/dsp/PhysicalString.cpp` (loop delay), `src/dsp/PhysicalEngine.cpp`
(pass β per voice), `tests/PhysicsTests.cpp` (new check).
