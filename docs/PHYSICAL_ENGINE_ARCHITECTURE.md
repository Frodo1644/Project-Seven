# Physical Engine Architecture

## Signal path

```
MIDI ──► PhysicalEngine ──► 36 × PhysicalString (stiff digital waveguide)
              │                    │  incident wave f⁺
              │                    ▼
              ├────────► BodyNeckModel (9 modes, shared) ──► f⁻
              │                    │  shared termination velocity v
              │                    ▼
              │             back into each string
              │
              └──► per-voice flux linkage λ(y) ──► EMF e = −dλ/dt
                                  │
                                  ▼
                        MagneticPickupModel (RLC + eddy pole)
                                  │
                                  ▼
                              DI output
```

One `PhysicalEngine` owns the instrument: the strings, the shared structure and one
pickup. Nothing is mixed after the fact — every voice contributes a force to the same
junction, and every voice receives the response.

## Files

| File | Responsibility |
|---|---|
| `src/dsp/GuitarSpec.h` | Central parameter registry. String geometry, mass, E, B derivation, tension, Z₀. Every constant has a provenance comment. |
| `src/dsp/PhysicalString.h/.cpp` | Stiff-string waveguide, pick/string collision, gesture damping, aperture sensing. |
| `src/dsp/BodyNeckModel.h/.cpp` | 9-mode structural admittance and the junction solver. |
| `src/dsp/NeckPort.h/.cpp` | Per-string neck termination: the stopped end's mobility, weighted by the neck's mode shapes at the fret. |
| `src/dsp/MagneticPickupModel.h/.cpp` | Flux linkage law and the passive humbucker's electrical network. |
| `src/dsp/PhysicalEngine.h/.cpp` | Voice pool, string assignment, articulation, coupled render loop. |
| `src/dsp/SevenAudioEngine.*` | Façade: dispatches to `PhysicalEngine`, or to the preserved 1.x engine when `useLegacyEngine` is set. |

## String model

**Type**: stiff digital waveguide (extended Karplus-Strong), chosen over finite
difference or pure modal synthesis because it gives sample-exact pitch at any
frequency, has bounded cost independent of pitch, is unconditionally stable with a
passive termination, and supports the two-phase junction the coupling requires.

Loop: `delay(P) → one-pole loss → one-pole HF loss → 4 × all-pass dispersion → junction`.

- **Pitch**: `P = fs/f`, corrected by the *measured* group delay of the loss filters and
  the dispersion chain at the loop frequency, so tuning does not drift with brightness.
- **Inharmonicity**: `B = π³ E d⁴/(64 T L²)` from `GuitarSpec.h`, realised by bisection on
  the all-pass coefficient so the 8th partial lands on `n f₀√(1 + Bn²)`. Fretting raises
  B by 1/L² automatically.
- **Loss**: one filter pass per round trip, so the T60 gain is referred to the
  fundamental (`g = 10^(−3/(T60·f₀))`), not to the sample rate.
- **Fret-dependent neck termination**: the stopped end is loaded by a three-mode
  neck admittance whose weights are the clamped-free mode shapes at the stopping
  point, `phi_m = sin((m+1/2) pi x/L)` with `x/L = 2^(-fret/12)`. The nut is the free
  end (maximum mobility) and the body joint is stiff, so a note loses more energy to
  the neck when it is open and less as it is fretted higher. Applied inside the loop,
  so the loss is partial-selective rather than broadband.
- **Fretting and harmonic contact**: `T60` is divided by `1 + 0.01 x fret` for fret
  contact loss, and multiplied by 0.62 for a touched harmonic where the fingertip damps
  the node.
- **Tension modulation**: a displaced string is longer, so its tension and pitch rise.
  The loop delay is scaled by `1/(1 + beta A^2)` with `beta = pi^2 E A_core/(8 L^2 T)`
  and `A` the physical pluck depth scaled by the decay of the sensed envelope, clamped
  to 25 cents. Hard attacks therefore start slightly sharp and settle.
- **Excitation**: the pick imposes a triangular displacement over its contact width,
  box-averaged by pick thickness, then released through a one-pole whose corner comes
  from the contact time (`≈ 0.24 ms + 0.9 ms·(1 − velocity)`). Velocity therefore changes
  the *spectrum* of the collision, not only the level. A 5 ms filtered noise burst is
  injected into the string state as pick scrape.
- **Sensing**: `y = s[n] − s[n − qP]`, averaged over 5 taps across the 2.5 in humbucker
  aperture. Nulls land on harmonics with a node at the pickup.

## Structure model

Nine second-order admittances in parallel, bilinear-discretised:

```
Y(z) = Σ bᵢ(1 − z⁻²) / (1 + a₁ᵢz⁻¹ + a₂ᵢz⁻²)
```

with `bᵢ = (k/mᵢ)/a₀`, `a₁ᵢ = (−2k² + 2ωᵢ²)/a₀`, `a₂ᵢ = (k² − 2ζᵢωᵢk + ωᵢ²)/a₀`,
`k = 2fs`, and `mᵢ = cᵢ/(2ζᵢωᵢ)` so the peak admittance is exactly `1/cᵢ`.

This form is positive-real, so the junction is passive: the coupled system cannot
generate energy, which is why no limiter or safety net is needed in the loop.

## Pickup model

1. `λ(y) = λ₀(1 + y/g + (y/g)²)`, y clamped to ±0.6 g, g = 2 mm.
2. `e[n] = (λ[n] − λ[n−1])·fs`, summed over voices.
3. Second-order resonant low-pass at 2760 Hz, Q 2.4 (C derived as 381 pF from the
   measured L = 8.73 H).
4. First-order eddy pole at 6.5 kHz for the measured ~18 dB/oct top end.
5. 5 Hz DC block (below any playable fundamental — a passive pickup has no LF roll-off
   in the audio band).
6. Fixed output trim 0.55.

No body-resonance filter is applied to the output, because a magnetic pickup does not
sense the body. This is a deliberate removal of the previous engine's largest
unphysical element.

## Real-time discipline

- All buffers allocated in `prepare()`; `render()`, `noteOn()` and `noteOff()` allocate
  nothing (verified by the allocation counter in the engine tests: 0 allocations).
- No locks, no `new`, no file or string handling on the audio thread.
- Renders in 64-sample internal chunks so the per-sample junction solve stays in cache.
- Measured cost: 7 voices, 48 kHz, 10 s of audio in 0.20 s → **2.0 % of one core**.
- Voices: 36-slot pool, user polyphony limit with deterministic oldest-voice stealing.
- Stable at 44.1/48/88.2/96/192 kHz and 64–1024 sample blocks (covered by tests).

## Parameter flow

`EngineParameters` is a plain snapshot read once per block. String geometry and
per-string physics live in `GuitarSpec.h`, not in the parameter tree, because they are
instrument construction data rather than performance controls. Tuning still changes the
physics: it changes the sounding frequency, hence tension, Z₀ and B for that note.
