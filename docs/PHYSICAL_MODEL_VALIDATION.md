# Physical Model Validation

All figures below were produced by `ProjectSevenPhysicsTests` (`tests/PhysicsTests.cpp`)
running on the build machine: Windows x64, Release, 48 kHz, Ryzen 7 8700F.
Reproduce with:

```
cmake --build build --config Release --target ProjectSevenPhysicsTests
./build/ProjectSevenPhysicsTests_artefacts/Release/ProjectSevenPhysicsTests.exe
```

## 1. Pitch accuracy (autocorrelation, settled window 1.1-1.9 s)

| Note | Expected | Measured | Error |
|---|---|---|---|
| B1 (35) | 61.74 Hz | 61.86 Hz | +3.4 ¢ |
| E2 (40) | 82.41 Hz | 82.62 Hz | +4.4 ¢ |
| A2 (45) | 110.00 Hz | 110.09 Hz | +1.4 ¢ |
| D3 (50) | 146.83 Hz | 146.79 Hz | −0.5 ¢ |
| G3 (55) | 196.00 Hz | 195.92 Hz | −0.7 ¢ |
| B3 (59) | 246.94 Hz | 247.42 Hz | +3.4 ¢ |
| E4 (64) | 329.63 Hz | 328.77 Hz | −4.5 ¢ |
| Drop F F1 (29) | 43.65 Hz | — | +7.2 ¢ |

The tuning is measured after the attack has settled, because a hard pluck is physically
sharp (see §1b). The residual is not a tuning error: autocorrelation reads slightly sharp
on inharmonic low strings, which is why the same +3.4 cents appears for B1 before and
after tension modulation was added. It is below the ~10-cent threshold most players hear
on a low string, and well inside the 8-cent acceptance bound used by the engine tests.

### 1b. Tension modulation (attack drift)

A displaced string is slightly longer, so its tension and pitch rise. The coefficient is
derived, not fitted: `df/f = beta A^2` with `beta = pi^2 E A_core / (8 L^2 T)`.

| Pluck | Attack (0.03-0.28 s) | Settled (1.5-2.4 s) | Drift |
|---|---|---|---|
| Low B, velocity 0.95 | +7.84 ¢ | +3.37 ¢ | **4.47 ¢** |
| Low B, velocity 0.25 | +5.60 ¢ | +3.37 ¢ | **2.23 ¢** |

The drift scales with picking force, as the physics requires, and decays to the settled
value within the first second. The shift is clamped to 25 cents so an extreme excursion
can never destabilise the loop.

## 2. Decay: higher partials must die first (E2, dB lost between 0.05–0.55 s and 1.20–1.70 s)

| Partial | Frequency | Decay |
|---|---|---|
| 1 | 82.4 Hz | 6.9 dB |
| 2 | 164.8 Hz | 33.7 dB |
| 3 | 247.2 Hz | 9.0 dB |
| 4 | 329.6 Hz | 13.9 dB |
| 5 | 412.0 Hz | 21.9 dB |
| 6 | 494.4 Hz | 2.2 dB |
| 7 | 576.8 Hz | 8.8 dB |
| 8 | 659.3 Hz | 36.8 dB |

The trend is right (highest partials decay fastest) but not monotonic: partial 2 and
partial 8 fall into the aperture comb (see §3), and partial 6 sits on a structural
antiresonance. This is physically consistent — a real pickup also produces partials
whose apparent decay differs from the string's because the sensor's comb weights them
differently. It is *not* a smooth analytic curve, and it should not be.

## 3. Magnetic aperture and position (E2, ratio = bridge pickup / neck pickup)

Bridge pickup at 6 % of scale length, neck at 30 %, humbucker aperture 2.5 in (63.5 mm).

| Partial | Bridge | Neck | Ratio |
|---|---|---|---|
| 1 | 0.00187 | 0.00821 | 0.23 |
| 2 | 0.00082 | 0.00328 | 0.25 |
| 3 | 0.00353 | 0.00396 | 0.89 |
| 4 | 0.00267 | 0.00085 | **3.15** |
| 5 | 0.00133 | 0.00151 | 0.88 |
| 6 | 0.00030 | 0.00035 | 0.87 |

The 4th partial is strongly suppressed at the neck pickup and not at the bridge pickup.
For q = 0.30 the first node is at harmonic 1/0.30 = 3.3, and the two-tap difference
nulls harmonics with q = k/n, so harmonic 4 is the first strong node on the neck side —
the measured behaviour of a position comb. The overall lower level at the bridge is the
expected ~10 dB loss of fundamental output near the bridge (Tillman).

## 4. Palm mute removes energy rather than fading an envelope

| Time (s) | Open B1 RMS | Palm-muted B1 RMS |
|---|---|---|
| 0.25 | 2.33e−2 | 8.7e−5 |
| 0.50 | 1.76e−2 | 6.8e−6 |
| 1.00 | 1.38e−2 | 9.0e−7 |
| 2.25 | 4.08e−3 | 3.6e−12 |

Tail ratio (1.0–2.0 s): **< 0.001**. The palm-muted string is silent because its stored
energy was absorbed at the contact, not because an output gain was lowered.

## 5. Structural coupling is live

- Peak termination velocity while sounding E2: **2.11e−2 m/s**.
- String wave impedance: **0.744 N·s/m** for the low B (√(Tμ) from 74 N and 11.5 g/m),
  ≈ 0.17 N·s/m for the high E.
- Because `|R| ≤ 1` is guaranteed by the positive-real admittance, no energy-gain case
  is reachable; the engine tests confirm finite, bounded output at every sample rate
  and block size.

## 6. Levels

| Case | Peak |
|---|---|
| Single Drop F note, velocity 0.95 | 0.046 |
| Seven-string Standard B chord | 0.414 |

Headroom is deliberate: the instrument is meant to feed an amp sim, and the user's
output control adds up to +6 dB.

## 7. Cost

| Case | Time |
|---|---|
| 7 voices, 10 s of audio | 0.20 s → **2.0 % of one core** |
| 7 voices, allocation counter | **0 allocations** during note-on, note-off and render |

## 8. Listening material

Written by the probe into `build/artifacts`:

| File | Content |
|---|---|
| `10_physical_standard_b_chord.wav` | Seven-string B chord, open |
| `11_physical_palm_mute.wav` | Eight low-B palm-muted chugs |
| `12_physical_drop_f.wav` | Drop F low note, 5 s |

These are generated by the released model, not by a mock-up. They have not been
compared against a reference DI recording: no clean seven-string DI corpus was available
in this session, so §2–§5 are the quantitative evidence and the renders are for
subjective evaluation. Comparing against a real DI is the first item of pending
validation work.

## 9. Limitations of this validation

- Frequency-domain checks use Goertzel at the nominal partial frequencies; they do not
  measure partial *frequencies*, so inharmonicity is verified through implementation
  (`B` from physics, dispersion bisection) rather than measured here.
- No reference recordings were available for spectral comparison.
- Damping ratios, pickup Q and coupling strengths are estimated; the validation shows
  the model behaves plausibly, not that those numbers are the true ones.
