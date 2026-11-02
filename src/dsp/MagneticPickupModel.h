#pragma once

namespace ps
{
// Passive humbucker: magnetic flux-linkage sensing plus the electrical network.
//
// The pickup is not an EQ curve.  Two physical stages are modelled:
//
// 1. Variable-reluctance sensing.  The ferromagnetic string shunts flux
//    through the pole piece, so the flux linkage is a nonlinear function of the
//    string's displacement along the pole axis,
//        lambda(y) = lambda0 * (1 + y/g + (y/g)^2),     |y| < 0.6 g
//    (Lemme; Horton & Moore 2009; Novak et al. 2020 measure this curve in both
//    the vertical and horizontal directions).  The induced voltage is
//    e(t) = -N dPhi/dt, i.e. proportional to string *velocity* for small
//    motion, with the physically correct even-harmonic growth at large motion.
//    The spatial average over the pole footprint (aperture) is applied by the
//    string model, which is what produces the true position comb and the
//    humbucker's aperture roll-off.
//
// 2. The coil's electrical network.  Series R + L, winding/cable capacitance
//    across the coil, loaded by the 500 k volume pot: a second-order resonant
//    low-pass.  Eddy currents in the pole pieces and cover add a further pole
//    that steepens the top-end slope, so an extra first-order section is
//    included (Lemme's eddy-current extension).
//
// Derived from measured values: L = 8.73 H, Rdc = 18.2 kOhm, f_res = 2.76 kHz
// in-guitar (Seymour Duncan JB measurement) => C_total = 1/(4 pi^2 f^2 L)
// = 380 pF, which matches winding capacitance plus a short cable.  Q = 2.4 is
// ESTIMATED from the published "Q = 2-3 with a high-impedance load" range;
// eddy currents pull the real value toward the lower end.
class MagneticPickupModel
{
public:
    void prepare (double sampleRate);
    void reset();

    // Nonlinear flux linkage normalised to the rest flux.  `displacement` is in
    // units of the pole-to-string gap.
    static double fluxLinkage (double displacement) noexcept;

    // Runs one sample of induced EMF through the electrical network.
    float processSample (float emf) noexcept;

    double getResonantFrequency() const noexcept { return resonanceFrequency; }
    double getWindingCapacitance() const noexcept { return totalCapacitance; }
    double getInductance() const noexcept { return inductance; }

private:
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

        float process (float x) noexcept
        {
            const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
            return y;
        }

        void reset() noexcept { x1 = x2 = y1 = y2 = 0.0f; }

        void set (double bb0, double bb1, double bb2,
                  double aa0, double aa1, double aa2) noexcept
        {
            const double inv = 1.0 / aa0;
            b0 = static_cast<float> (bb0 * inv);
            b1 = static_cast<float> (bb1 * inv);
            b2 = static_cast<float> (bb2 * inv);
            a1 = static_cast<float> (aa1 * inv);
            a2 = static_cast<float> (aa2 * inv);
        }
    };

    void updateCoefficients (double sampleRate);

    double fs = 48000.0;
    double inductance = 8.73;         // H, measured
    double dcResistance = 18200.0;    // Ohm, measured
    double totalCapacitance = 380.0e-12; // F, derived
    double resonanceFrequency = 2760.0;
    double qualityFactor = 2.4;

    Biquad coilResonance;
    Biquad eddyRolloff;
    Biquad dcBlock;
    float outputTrim = 1.0f;
};
} // namespace ps
