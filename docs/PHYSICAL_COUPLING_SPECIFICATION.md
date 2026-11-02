# Physical Coupling Specification

The requirement that separates this engine from a bank of independent synthesisers is
that the subsystems interact. This document states each coupling, its equations, how it
is implemented, and what is approximated.

Notation: `f⁺` incident force wave, `f⁻` reflected force wave, `Z₀ = √(Tμ)` string wave
impedance, `v` termination velocity, `F` termination force, `Y` structural admittance.

---

## 1. String ↔ Structure (bridge and neck)

**Physics.** The string is not terminated at a rigid wall. Its end moves with the
structure, so the returning wave depends on the structure's mechanical impedance
(Smith, *PASP* §9). Energy the structure absorbs leaves the string permanently.

**Implementation.** Two-phase render, every sample:

```
per string:   f⁺ⱼ = pullIncident()            // delayed, filtered, lossy wave
junction:     F   = 2Σf⁺ⱼ − v ΣZ₀ⱼ
              v   = Y(F)                       // 9-mode admittance
per string:   f⁻ⱼ = f⁺ⱼ − Z₀ⱼ·v              // pushReflected(f⁻ⱼ)
```

`Y(F)` is evaluated with the instantaneous term solved implicitly:

```
C  = zero-input response of the modes        (−a₁y₁ − a₂y₂ − b₀F[n−2])
v  = (2 b₀ Σf⁺ + C) / (1 + b₀ ΣZ₀)
```

No iteration and no added latency: the reflection is returned in the same sample.

**Why it is not fake.** `|f⁻| ≤ |f⁺|` holds at every sample because `Y` is positive-real,
so the structure can never put more energy into a string than it took. The decay of each
partial is therefore an emergent property: partials near a structural mode lose energy
faster (the measured dead-spot mechanism), partials between modes ring longer.

**Approximation.** All strings drive one effective point on the structure. A
position-dependent mobility matrix (each string at its own saddle, with the neck end as
a second port) is the next refinement; see the roadmap.

---

## 2. String ↔ String (through the structure)

**Physics.** Strings are not directly coupled; they are coupled mechanically through the
shared body, bridge and neck. Fleischer's measurements show the neck termination is the
more mobile one in a solid body.

**Implementation.** This falls out of the same junction for free: `F` is the **sum** over
all active voices, so one string's force moves the structure, and that motion appears in
every other string's `f⁻ⱼ` scaled by its own `Z₀ⱼ`. No extra state, no extra cost.

**Consequence.** A low string being played modulates the decay of a high string that is
still ringing, and a chord's strings share one structural response instead of each
having a private decay curve.

---

## 3. Bridge ↔ Body ↔ Neck

**Physics.** Bridge, body and neck are one structure with distributed modes; the
measured modes at 55–370 Hz are body bending and torsion, and the 430 Hz mode is the
neck.

**Implementation.** A single nine-mode admittance covers all three: the modes are not
separated by component, they are the measured modes of the assembled instrument. The
bridge's higher stiffness appears as the 1.2 kHz mode.

**Approximation.** One-dimensional (normal-to-body) motion only. Rotational saddle
dynamics are not modelled.

---

## 4. Pick ↔ String

**Physics.** A pick displaces the string over a finite contact width and releases it;
the contact time sets how much high-frequency content the collision contains.

**Implementation.** Initial state = triangular displacement at the pick position,
box-averaged over the pick thickness (`0.9 mm`), then smoothed by a one-pole whose
corner is `1/(2π·t_contact)` with
`t_contact = 0.24 ms + 0.9 ms·(1 − velocity)(0.5 + 0.5·intensity)`.
Depth is `1.35 mm · velocity^1.30`. A 5 ms high-passed noise burst is added to the
string state.

**Consequence.** Velocity changes brightness and attack shape, not just amplitude —
which is what the specification requires.

---

## 5. Palm / Hand ↔ String

**Physics.** The palm rests on the string near the bridge, adding a distributed contact
that absorbs energy and preferentially kills high partials. The fretting hand, on
release, grabs the string and stops it.

**Implementation.** A second loop-loss value (`T60 ≈ 0.55·(82.4/f)^0.18` s, clamped
0.25–0.9 s) and a second low-pass corner (850 Hz), blended in by a smoothed `dampCurrent`
so articulation changes cannot click. `noteOff` sets the damped loss from a T60 of
0.14 s (palm) or 0.28 s (open) and drives the same blend to 1.

**Why it is not an envelope.** The energy is removed from the delay line, so the string
stops vibrating; there is no separate output fade.

**Approximation.** Contact is modelled as lumped loss plus a low-pass rather than a
position-dependent contact impedance. The measured effect (fast, bright-suppressing
decay with a tight attack) is reproduced; the exact contact mechanics are not.

---

## 6. String ↔ Pickup

**Physics.** The ferromagnetic string shunts flux through the pole piece; the coil's EMF
is `−dλ/dt`, and the pickup averages over its pole footprint.

**Implementation.** Aperture-averaged two-tap difference (see architecture) → nonlinear
flux map → finite difference → passive RLC network.

**Why it is not an EQ.** The spectrum emerges from (a) the string's actual motion at the
pickup position, (b) the aperture width, (c) the nonlinear flux map, and (d) the measured
coil/cable resonance. Nothing applies a fixed tone curve to the voice sum.

**Approximation.** One pole piece per string is assumed; the humbucker's two antiphase
coils are not modelled as two separate sensing points (which would add the measured
3–4 kHz notch on the low strings). The RLC is linear and time-invariant; measured
L(f) and R(f) variation from eddy currents is represented by the fixed 6.5 kHz pole.

---

## 7. Energy accounting

| Path | Mechanism | Guarantee |
|---|---|---|
| String internal | loop loss + HF one-pole | `g < 1` always |
| String → structure | junction reflectance | `Y` positive-real ⇒ `|f⁻| ≤ |f⁺|` |
| Structure | modal damping `ζ ≥ 0.012` | bilinear poles inside unit circle |
| Structure → string | reflected wave | same junction |
| Pickup | passive RLC + DC block | stable, no feedback into strings |

No node in the graph can amplify: there is no gain stage inside any loop, and the only
gain constants (`outputTrim`, `emfScale`) are *outside* the coupled system.
