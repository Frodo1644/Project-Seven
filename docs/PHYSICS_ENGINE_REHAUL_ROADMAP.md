# Physics Engine Rehaul Roadmap

Status legend: **DONE**, **PARTIAL**, **PLANNED**.

## Phase A — Audit
**DONE.** `docs/EXISTING_ENGINE_AUDIT.md`. The 1.x engine was kept as the reference and
is still selectable via `EngineParameters::useLegacyEngine`.

## Phase B — Research
**DONE** for the parameters the implementation uses; see
`research/PHYSICAL_MODEL_RESEARCH.md`. Four parameter groups remain estimated
(body damping, pickup Q, wound-string inharmonicity bias, structural coupling strength)
because the primary sources are paywalled. They are centralised and labelled.

## Phase C — Architecture and coupling specification
**DONE.** `docs/PHYSICAL_ENGINE_ARCHITECTURE.md`,
`docs/PHYSICAL_COUPLING_SPECIFICATION.md`.

## Phase D — Incremental implementation

| # | Subsystem | Status | Notes |
|---|---|---|---|
| 1 | String physics | **DONE** | Stiff waveguide, physically derived B and Z₀, per-round-trip loss, exact pitch |
| 2 | Excitation | **DONE** | Pick/string collision with thickness and velocity dependence |
| 3 | Magnetic pickup | **DONE** | Aperture two-tap sensing, nonlinear flux, passive humbucker RLC |
| 4 | Mechanical coupling | **DONE (reduced order)** | 9-mode shared junction; one coupling point |
| 5 | Articulation | **PARTIAL** | Palm mute and note-off damping physical; harmonics are modal excitation, not a touched-node model |

## Phase E — Integration
**DONE.** Shared junction across all voices; single pickup stage; parameters consistent
between string, structure and pickup.

## Phase F — Performance
**DONE** for the current design. 2.0 % of one core at 7 voices / 48 kHz. The junction is
the hot spot (9 modes × 2 state updates per voice per sample); if more modes are added,
the next step is to run the structure at half rate with a polyphase interpolation of the
reflected wave.

## Phase G — Build and validation
**DONE.** VST3 Release build, host smoke test, 111 engine checks + 27 physics checks,
audio renders in `build/artifacts`, all at 0 failures.

---

## Remaining work, in priority order

1. **Tension modulation — DONE.** `df/f = beta A^2` with
   `beta = pi^2 E A_core / (8 L^2 T)` derived from geometry, applied as a loop-delay
   ratio with a 25-cent clamp. Measured drift 4.5 cents (hard) and 2.2 cents (soft) on
   the low B, settling within the first second. Amplitude-dependent intermodulation
   between simultaneously sounding strings follows from the same term.
2. **Position-dependent mobility (PARTIAL).** Give each string its own structural port
   (saddle position) and add the neck as a second port, so dead spots depend on fret
   position as Fleischer measured, not only on pitch.
3. **Humbucker two-point sensing (PLANNED).** Two antiphase apertures to reproduce the
   measured 3–4 kHz notch on the low strings.
4. **Measured parameter replacement (PLANNED).** Replace the four estimated groups as
   soon as Paté et al. (JASA 2014 / Applied Acoustics 2015) and a measured humbucker
   impedance sweep are available.
5. **Touched-node harmonics (PARTIAL).** A natural harmonic is currently a modal initial
   condition on the open string; a real harmonic also has the finger's damping at the
   node, which suppresses neighbouring partials more than the current 8 % sideband.
6. **Multi-pickup support (PLANNED).** Neck/bridge selection with separate positions and
   electrical loads; the sensing model already takes position and aperture as parameters.
