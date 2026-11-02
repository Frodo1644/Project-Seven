# Physical Model Research

Sources consulted for the Project Seven physical engine, the numbers taken from them,
and the engineering decision each one drove. Only sources that changed an
implementation decision are listed; blocked or unusable sources are recorded at the end
so they are not silently re-tried.

Parameter classification used throughout: **MEASURED**, **MANUFACTURER-SPECIFIED**,
**DERIVED**, **ESTIMATED**.

---

## 1. String mechanics

### 1.1 Geometry and mass

**Grimes, D. R. (2014). "String Theory — The Physics of String Tension."**
*PLoS ONE* 9(7): e102088. MEASURED linear densities for an Ernie Ball Regular Slinky
set (nickel-wound over steel core, plain tin-plated steel), obtained by cutting 30 cm
sections and weighing on a calibrated balance.

The published table prints 3.94 / 6.94 / 11.4 / 23.3 / 43.2 / 69.7 g/m for
.010 / .013 / .017 / .026 / .036 / .046. Those values are a factor of ten above what
the geometry allows: a .010 in steel wire has area 5.07 × 10⁻⁸ m², so at
ρ = 7800 kg/m³ its mass is **0.395 g/m**, and the standard tension formula with
0.395 g/m reproduces the published ~16 lb tension of a .010 high E. The physically
consistent values are therefore taken as 0.394 / 0.694 / 1.14 / 2.33 / 4.32 / 6.97 g/m
(0.394…6.97 × 10⁻³ kg/m).

*Decision*: plain-string mass is **DERIVED** as ρ·πd²/4 (ρ = 7800 kg/m³, verified
against the measured 0.394 g/m). Wound-string mass is scaled from the measured .046
anchor by (d/d₀₄₆)², which reproduces the measured .026 and .036 values to within 4 %
(2.33 → 4.47 predicted vs 4.32 measured).

**Kemp, J. A. (2017). "Tension and stress in guitar strings."** *PLoS ONE* 12(9):
e0184803. MEASURED Young's modulus of plain strings 171–189 GPa against handbook
200–220 GPa; quotes E = 207 GPa and G = 79.3 GPa; also establishes that only the core
carries axial tension in a wound string.

*Decision*: E = 200 GPa for the stiffness/Young's-modulus term (mid-range of both the
measured and handbook values). Wound strings use the core diameter for bending
stiffness, because the helical winding contributes < 1 % of the tension.

**Grimes (2014)**, same paper: measured apparent moduli of wound strings fall with
gauge (.026 → 62 GPa, .036 → 42.2 GPa, .046 → 32.6 GPa). These are *axial* moduli for
the composite section and are not used for bending stiffness; they are recorded here as
the reason tension must be computed from mass, not from a nominal E.

### 1.2 Inharmonicity

**Russell, D. A. (Penn State), "Inharmonicity due to Stiffness for Guitar Strings".**
The standard stiff-string result:

```
B = π² Y S κ² / (T L²)          with κ = d/4 for a cylinder
  = π³ Y d⁴ / (64 T L²)
fₙ = n f₀ √(1 + B n²)
```

*Decision*: **DERIVED** per string from d_core, E, T and L_fret. Because B ∝ 1/L²,
fretting at the 12th fret multiplies B by 4 automatically; no separate fret table is
needed. Range clamped to [1 × 10⁻⁵, 2.5 × 10⁻⁴] to stay inside measured values.

**Murray, C. & Whitfield, S. (2022). "Inharmonicity in plucked guitar strings."**
*Am. J. Phys.* 90(7): 487–493, doi:10.1119/5.0064373. Plain monofilament steel B agrees
with the material-derived prediction within 1–2 %; **wound strings are systematically
more inharmonic** than the core-only prediction, the discrepancy growing with gauge
(hex core digs into the windings).

*Decision*: wound strings receive an **ESTIMATED** ×1.4 bias on B. Flagged as an
estimate because the paper's table was not retrievable (publisher returned 403).

### 1.3 Tension and wave impedance

```
T = 4 L² f² μ          (from f = (1/2L)√(T/μ))
Z₀ = √(T μ)            (Smith, Physical Audio Signal Processing)
```

*Decision*: both **DERIVED** at note start from the design and the sounding pitch.
`Z₀` is what makes the structural coupling string-dependent: measured `Z₀` in the probe
is 0.744 N·s/m for the low B and ≈ 0.17 N·s/m for the high E, so high strings are
coupled far more strongly to the body — the physical origin of dead spots.

### 1.4 Decay

**Paté, A., Le Carrou, J.-L., Fabre, B. (2014). "Predicting the decay time of solid
body electric guitar tones."** *JASA* 135(5): 3045–3055. Establishes that solid-body
decay is predicted from (i) losses in the isolated string and (ii) losses through the
terminations (bridge/nut/body coupling). The numeric T60 tables were not retrievable
(HAL bot-challenge, AIP 403), so only the *structure* of the model is used.

**Grzybowski et al. (2025).** *Sensors* 25(13): 3989. Measured decay behaviour on a
real solid-body electric: higher harmonics always decay faster; the 4th harmonic takes
of order 8 s to fall 75 dB on a stable note; partials stabilise within ~1 s of the pick
attack.

*Decision*: the engine splits loss exactly the way Paté describes. Isolated-string loss
is an **ESTIMATED** T60 curve `12.0·(82.407/f)^0.26` s clamped to 4–16 s; structural
loss is *not* a curve at all — it is the real reflected wave from the modal structure.
The frequency-dependent internal loss is a one-pole plus a one-pole at ~15 kHz inside
the loop, which makes upper partials decay faster as measured.

---

## 2. Magnetic pickup

### 2.1 Equivalent circuit

**Macalister Electronics, "Guitar Pickup Equivalent Circuits"** — VNA measurement and
least-squares fit of a Fender Strat single coil: L = 2.24 H, R = 8.68 kΩ,
C_winding = 233 pF; 4.33 kHz in circuit with 380 pF of cable.

**Guitarhacking, "Deconstructing pickups and tone controls"** — measured with an LCR
meter: **Seymour Duncan JB Trembucker: L = 8.73 H, R_dc = 18.2 kΩ, f_res = 2.76 kHz**
in guitar with 500 k pots.

**Lemme, H., "The Secrets of Electric Guitar Pickups"** — the standard model: ideal L
in series with R, winding capacitance in parallel; f_res = 1/(2π√(LC)); Q = 2–3 with a
high-impedance load; cable adds 300–1000 pF. Also documents the *eddy-current
extension*: a plain parallel R cannot reproduce the measured steeper top-end slope
(≈18 dB/oct) or the peak reduction from metal covers, so an extra pole is needed.

**Elliott, R. (ESP), "Guitar Pickups"** — humbucker assembly L 7.88 H, 15.23 kΩ series,
47 pF; notes that inductance meters mis-read high-DCR coils.

*Decision*: the output stage is a second-order resonant low-pass at **2760 Hz** with the
capacitance **DERIVED** from the measured pair:
C = 1/(4π²f²L) = 381 pF, which matches winding plus a short cable. Q = **ESTIMATED**
2.4 from the published 2–3 band (eddy currents pull it low). A first-order eddy pole at
6.5 kHz provides the measured slope. Values live in `MagneticPickupModel.h` with
provenance comments.

### 2.2 Sensing, aperture, comb

**Tillman, J. D., "Response Effects of Guitar Pickup Position and Width".** Two
separable effects: a *position* comb with nulls where the pickup sits on a node
(f_node = F_open·L/X), and a *width* comb from averaging over the pole footprint.
Worked figures: 1.0 in wide → −3 dB at 1820 Hz, first null 4200 Hz on a 25.5 in low E;
**2.5 in (humbucker) → −3 dB at 728 Hz, first null 1680 Hz**.

**Lemme** — a standard humbucker adds notches near 3 kHz (low E) and 4 kHz (A string)
because it senses at two points in antiphase.

**Russell, D. A., "Physics of Guitar Pickups"** — the pickup responds to string
*velocity*, not displacement, and only to motion along the pole axis.

*Decision*: sensing is a two-tap difference over the **bridge → pickup → bridge** path,
`y = s[n] − s[n − qP]` with q = x/L. Nulls then fall exactly on harmonics with a node
at the pickup (q = k/n), which is the measured rule. The aperture is 5 taps spread over
the **2.5 in MEASURED humbucker footprint**, giving the width comb. Verified: bridge
pickup (q = 0.06) nulls partial 4 relative to a neck pickup at q = 0.30 (measured ratio
3.15 in the probe).

### 2.3 Nonlinearity

**Horton & Moore, "Modeling the magnetic pickup of an electric guitar."**
*Am. J. Phys.* 77(2): 144–150 (2009). Abstract only (PDF 403): the model predicts the
field change due to a ferromagnetic wire with high accuracy.

**Novak et al. (2020).** *JAES* — measured the pickup's two-dimensional nonlinear
transfer function in both the vertical and horizontal string directions.

**Lemme** — flux vs pole–string distance follows a hyperbolic curve, so a sinusoidal
string motion produces even-harmonic distortion that is not in the string's own
spectrum.

*Decision*: flux linkage is **ESTIMATED** as
`λ(y) = λ₀(1 + y/g + (y/g)²)` with y clamped to ±0.6 g and g = 2 mm. The quadratic term
reproduces the measured asymmetry (probe: +0.3 → 1.390, −0.3 → 0.790) while staying
monotonic and bounded. EMF is the finite difference of λ, so output is velocity-like
and bright, as a magnetic pickup is.

---

## 3. Structure: body, neck, bridge

**Russell, D. A., "Modal Analysis of an Electric Guitar" (Penn State).** MEASURED
full-instrument modal analysis, hard-maple solid body, free suspension, roving hammer,
148 points: modes at **55.3, 160.1, 189.4, 300.5, 369.7 Hz**; coherence degraded above
400 Hz.

**Fleischer, H. (1999), "Dead Spots of Electric Guitars and Basses."** ASA/EAA/DAGA '99.
In-situ laser Doppler vibrometry: several neck resonances, including an out-of-plane
neck/fingerboard resonance at **430 Hz**. Key finding: in a solid body the bridge is
essentially immobile and **the neck end is the more mobile termination**; energy leaves
the string through the neck support. Dead spots occur where the point admittance
Y = v/F (its real part, the conductance) is high at a string partial.

**Smith, J. O., *Physical Audio Signal Processing*, §9 "Bridge Modeling".** The method
used here:

```
Z(ω) = force / velocity at the termination
R(ω) = (Z − Z₀)/(Z + Z₀)        force-wave reflectance
```
A passive termination is positive-real, so R is a Schur function (|R| ≤ 1). A parallel
bank of second-order resonators gives `Z = Σ(mᵢs + cᵢ + kᵢ/s)`; each admittance peak is
a body resonance that absorbs energy at nearby partials, shortening their decay, while
antiresonances reflect almost perfectly. Bilinear discretisation preserves
positive-realness.

*Decision*: the structure is implemented as nine modes in the **mobility** form
`Y = Σ (s/mᵢ)/(s² + 2ζᵢωᵢs + ωᵢ²)`, driven by the summed string force and returning the
shared termination velocity. Frequencies 55.3/160.1/189.4/300.5/369.7 (MEASURED),
430 (MEASURED), 610/880/1200 (ESTIMATED). Damping ratios 0.012–0.030 are **ESTIMATED**
(the modal study publishes no Q values; solid bodies are reported as lightly damped).
Peak impedances are **ESTIMATED** from the target reflectance at resonance with a design
reference Z₀ = 0.45 N·s/m: |R| 0.90 → c 8.6, 0.92 → 10.8, 0.95 → 17.6, 0.97 → 29.6.

---

## 4. Sources that could not be used

Recorded so they are not silently re-attempted:

| Source | Blocker |
|---|---|
| Paté et al., JASA 135(5) 3045 | HAL Anubis challenge, AIP 403, RG 403 |
| Paté et al., *Applied Acoustics* 96, 118–131 (2015), modal variability + damping | ScienceDirect 403 |
| Zollner, *Physics of the Electric Guitar*, Ch. 7 | PDF unsupported |
| Murray & Whitfield, AJP 90(7) 487 — Table I of B values | AIP 403 |
| Hjerrild & Christensen, WASPAA 2019 | AAU 403 |
| Rau et al., ISMA 2017 neck admittance | CCRMA PDF unsupported |
| Novak et al., JAES 2020 full text | anti-bot proof-of-work |
| Horton & Moore, AJP 77 144 full text | Rollins 403 |
| Kinman / Seymour Duncan / DiMarzio datasheets with L and C | not published (DCR only) |

Consequence: body damping ratios, pickup Q, wound-string B bias and the structural
coupling strengths are explicitly **ESTIMATED** and centralised so they can be replaced
when measured data becomes available. No estimate is presented as a measurement.
