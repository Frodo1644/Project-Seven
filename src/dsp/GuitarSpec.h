#pragma once

// Central physical parameter registry for the Project Seven physical engine.
//
// Every number used by the engine is either:
//   MEASURED              - published measurement or manufacturer data
//   DERIVED               - computed from measured values with a documented equation
//   ESTIMATED             - engineering approximation, explicitly labelled
//
// Provenance for each group is given inline and expanded in
// research/PHYSICAL_MODEL_RESEARCH.md.  Do not introduce unexplained constants
// anywhere else in the DSP code: add them here with a provenance comment.

#include <algorithm>
#include <array>
#include <cmath>

#include "params/Parameters.h"

namespace ps::phys
{
constexpr double kPi = 3.14159265358979323846;

// MEASURED (nominal): Fender/Gibson 25.5 in scale length.
constexpr double kScaleLengthMetres = 0.6477;

// MEASURED: volumetric density of steel music wire (Grimes 2014 measured
// linear densities imply 7.77 g/cm^3; Kemp 2017 quotes 7.8 g/cm^3 for the
// alloy).  Used by DERIVED plain-string mass.
constexpr double kSteelDensity = 7800.0;

// MEASURED: Young's modulus of plain steel music wire.  Grimes 2014 measured
// 178.7 GPa (.010), 188.4 GPa (.013), 177.6 GPa (.017); Kemp 2017 measured
// 171-189 GPa against a handbook value of 207 GPa.  200 GPa is the midpoint of
// the handbook range and is used for stiffness only.
constexpr double kPlainYoungsModulus = 200.0e9;

// MEASURED anchor for wound strings: Grimes 2014 measured linear density of a
// .046 nickel-wound string as 6.97 g/m (the paper prints 69.7 with an
// inconsistent unit label; 6.97 g/m is the value consistent with rho = 7.8
// g/cm^3 for the core plus winding and with the standard tension formula, and
// it reproduces the ~16-18 lb tension of a real .046 low E).
constexpr double kWoundLinearDensityAt046 = 6.97e-3;
constexpr double kWoundReferenceDiameter = 0.046 * 0.0254;

// DERIVED: pi^3 / 64, the geometric factor of the stiff-string inharmonicity
// coefficient B = (pi^3 E d^4) / (64 T L^2)   (Russell, Penn State; the
// standard Fletcher/Russell form).
constexpr double kInharmonicityFactor = 0.48447;

// ESTIMATED: measured wound strings are systematically more inharmonic than
// the core-only prediction (Murray & Whitfield 2022).  1.4 is the mid-range
// correction implied by their reported 1-2% fit residuals on plain strings and
// the larger positive bias on hex-core wound strings.
constexpr double kWoundInharmonicityBias = 1.4;

// MEASURED bounds on B for guitar strings (Murray & Whitfield 2022 report
// 1e-5 .. 1e-4 for plain strings and low 1e-4 for thick wound strings).
constexpr double kMinInharmonicity = 1.0e-5;
constexpr double kMaxInharmonicity = 2.5e-4;

// A physical string design: geometry and material, independent of tuning.
struct StringDesign
{
    double gaugeInch = 0.010;
    double diameter = 0.000254;
    double coreDiameter = 0.000254;
    double linearDensity = 3.95e-4; // kg/m
    double youngsModulus = kPlainYoungsModulus;
    bool wound = false;
};

// Plain string mass: DERIVED from a measured density and the measured diameter.
// Wound string mass: MEASURED scaling - Grimes 2014 gives 2.33 / 4.32 / 6.97
// g/m for .026 / .036 / .046, which follow d^2 to within 4 %, so mass is scaled
// from the .046 anchor by (d / d_046)^2.
// Wound core diameter: ESTIMATED as 0.40 * overall diameter (a .046 hex core is
// ~0.016 in; the ratio is 0.35-0.42 across common sets).
inline StringDesign makeStringDesign (double gaugeInch, bool wound)
{
    StringDesign design;
    design.gaugeInch = gaugeInch;
    design.diameter = gaugeInch * 0.0254;
    design.wound = wound;

    const double d = design.diameter;

    if (wound)
    {
        const double ratio = d / kWoundReferenceDiameter;
        design.linearDensity = kWoundLinearDensityAt046 * ratio * ratio;
        design.coreDiameter = 0.40 * d;
        design.youngsModulus = kPlainYoungsModulus;
    }
    else
    {
        const double area = kPi * d * d * 0.25;
        design.linearDensity = kSteelDensity * area;
        design.coreDiameter = d;
        design.youngsModulus = kPlainYoungsModulus;
    }

    return design;
}

// Derived mechanical state of one string at a given sounding pitch and
// vibrating length.
struct StringState
{
    double frequency = 82.407;
    double tension = 70.0;       // N
    double impedance = 0.4;      // N s / m, Z0 = sqrt(T * mu)
    double inharmonicity = 1.0e-5;
    double vibratingLength = kScaleLengthMetres;
};

// Tension from the string wave equation: f = (1/2L) sqrt(T/mu)  =>  T = 4 L^2 f^2 mu.
// Wave impedance Z0 = sqrt(T mu) (Smith, Physical Audio Signal Processing).
inline StringState makeStringState (const StringDesign& design,
                                    double soundingFrequency,
                                    double vibratingLength)
{
    StringState state;
    state.frequency = std::max (10.0, soundingFrequency);
    state.vibratingLength = std::max (0.05, vibratingLength);

    state.tension = 4.0 * state.vibratingLength * state.vibratingLength
                  * state.frequency * state.frequency * design.linearDensity;

    state.impedance = std::sqrt (std::max (1.0e-9, state.tension * design.linearDensity));

    const double dc = design.coreDiameter;
    double b = kInharmonicityFactor * design.youngsModulus * dc * dc * dc * dc
             / (state.tension * state.vibratingLength * state.vibratingLength);

    if (design.wound)
        b *= kWoundInharmonicityBias;

    state.inharmonicity = std::min (kMaxInharmonicity, std::max (kMinInharmonicity, b));
    return state;
}

// The shipped seven-string set: .010 .013 .017 .026 .036 .046 .059
// (plain, plain, plain, wound, wound, wound, wound), low string first.
inline std::array<StringDesign, kNumStrings> defaultStringSet()
{
    return { {
        makeStringDesign (0.059, true),
        makeStringDesign (0.046, true),
        makeStringDesign (0.036, true),
        makeStringDesign (0.026, true),
        makeStringDesign (0.017, false),
        makeStringDesign (0.013, false),
        makeStringDesign (0.010, false)
    } };
}

} // namespace ps::phys
