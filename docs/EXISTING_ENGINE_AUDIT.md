# Existing Engine Audit — Project Seven 1.2.0

This audit records the state of the engine that Project Seven shipped with, before the
physical-modelling rehaul. It is the baseline the new engine is measured against.

## 1. What existed

| Component | File | Role |
|---|---|---|
| `StringModel` | `src/dsp/StringModel.cpp` | Per-string Karplus-Strong style waveguide |
| `PickupModel` | `src/dsp/PickupModel.cpp` | Filter cascade applied to the summed voice signal |
| `SevenAudioEngine` | `src/dsp/SevenAudioEngine.cpp` | Voice pool, string assignment, articulation, sustain |
| `TuningManager` | `src/params/TuningManager.cpp` | MIDI note → string/fret, harmonic lookup |
| `PluginProcessor` | `src/core/PluginProcessor.cpp` | Parameters, MIDI dispatch, block splitting |

Signal path: `MIDI → voice pool → per-voice StringModel → sum → PickupModel → output`.

## 2. String generation (what it actually did)

- One delay line per voice, fractional read with 4-point Lagrange interpolation.
- Loop delay derived from the MIDI pitch: `delay = fs / f`, minus the measured phase
  delay of the loop filters.
- Two one-pole low-passes inside the loop for "frequency-dependent damping".
- Four first-order all-pass sections for dispersion. The dispersion coefficient was
  found by bisection to hit a target delay reduction computed from a *hand-entered*
  inharmonicity number, not from string geometry.
- Loop gain from a target T60 of the form `7.5 * (82.407/f)^0.35`, clamped, then
  floored at 0.9995 "to keep strings ringing".
- Excitation: a triangular displacement shaped by pick position, smoothed by a
  one-pole whose corner frequency depended on velocity, plus a short noise burst.
- Palm mute: blend of a second loop gain (`0.35 * (82.4/f)^0.2`) and a 1.8 kHz
  low-pass, plus a 1.4× transient boost.
- Harmonics: sinusoidal initial shape at the requested partial.

## 3. Pickup (what it actually did)

`PickupModel::processSample` ran, in series, over the *summed* voice output:

1. 28 Hz high-pass
2. 4.3 kHz low-pass, Q 1.28
3. 9.5 kHz low-pass, Q 0.62
4. band-pass at 80 Hz, 300 Hz and 1 kHz, summed and scaled — commented as
   "guitar body resonance"
5. DC blocker, output trim 0.85

Pickup *position* was implemented inside `StringModel` as a two-tap difference
`s - 0.38 * tap`, with the tap offset being a fraction of the loop delay.

## 4. Findings

### Correct and worth keeping
- Voice allocation, string/fret assignment, harmonic lookup, sustain pedal, keyswitch
  and CC20 handling, state serialisation, block splitting, no-allocation rendering.
- The two-tap difference is the right *idea* for magnetic position sensing.
- Fractional delay + phase-delay compensation gives genuinely accurate pitch.

### Physically wrong
1. **The body was modelled as an EQ on the output.** Stages 4–5 above are a static
   filter cascade. A magnetic pickup does not sense body motion at all, and a
   solid-body guitar's body is not a soundboard. This is exactly the "arbitrary EQ
   curve disguised as physics" the specification forbids.
2. **No coupling existed.** Every string decayed according to its own hard-coded
   T60 curve; the "body" could not absorb string energy, no string could excite
   another, and nothing the structure did ever returned to a string.
3. **Decay was an envelope, not a loss mechanism.** `noteOff` multiplied an output
   gain and enabled a "sustain mode" with `sustainGain = 0.99995` per sample. The
   `sustainGain` path was also the cause of the historical no-sound bug: it decayed
   but was never *not* applied, so a note could be silenced while still "active".
4. **Inharmonicity was a guess.** `baseInharmonicity` came from a fitted power law on
   the open-string frequency (`5.5e-5 * (82.407/f)^1.25`), with no link to gauge,
   tension, Young's modulus or vibrating length. Fretting scaled it by the square of
   the frequency ratio, which is *directionally* right (`B ∝ 1/L²`) but not derived.
5. **Loop gain was referred to the wrong rate.** T60 was converted with a per-sample
   gain, but the loop only applies its filter once per round trip, so the effective
   sustain was ~`fs/f0` times too long; the `max(0.9995, …)` floor then hid the error.
   This was found and fixed during the rehaul (see `docs/PROGRESS.md`).
6. **Palm mute did not change the termination.** It changed a gain and a low-pass,
   i.e. it shortened the envelope rather than damping the string.
7. **The pickup was linear.** Flux was never modelled, so the even-harmonic
   nonlinearity of a real variable-reluctance pickup was absent, and there was no
   aperture averaging (no humbucker width comb).
8. **Velocity changed a filter corner and an amplitude, not the collision.**

### Limitations that remain acceptable for now
- Single effective structural coupling point (documented in the coupling spec).
- No time-varying tension (no pitch drift / intermodulation).
- One fixed pickup voicing (passive humbucker, 500 k load).

## 5. Decision

The original engine is **preserved unmodified** in `src/dsp/StringModel.*` and
`src/dsp/PickupModel.*` and remains selectable through
`ps::EngineParameters::useLegacyEngine`. A/B comparison is therefore possible from the
same MIDI performance. The new coupled engine (`PhysicalEngine`) is the default.

Replacement criteria that had to be met before switching the default:

| Criterion | Result |
|---|---|
| Pitch accuracy ≤ 8 cents, B1–E4 and Drop F | met (≤ 5.6 cents) |
| Palm mute reduces stored string energy | met (tail ratio < 0.001) |
| Higher partials decay faster | met (see validation) |
| Pickup position produces a real comb | met (null at n=4, bridge pickup) |
| Structure takes energy from the string | met (`structure velocity > 0`) |
| No allocation on the audio thread | met (0 allocations) |
| CPU below 5 % of one core at 7 voices | met (≈ 2 %) |
