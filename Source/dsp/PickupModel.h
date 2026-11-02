#pragma once

namespace ps
{
// Magnetic pickup electrical response with guitar body resonance.
// 
// The pickup model simulates:
// - Low-end rolloff from pickup inductance/input impedance
// - Resonant upper-mid peak from coil inductance and cable capacitance
// - Gentle top-end rolloff above usable range
// - Guitar body resonance (fundamental, sustain, air modes)
// - DC blocking to remove any residual DC
//
// No amp, cabinet, EQ or drive effects - this is the pure instrument response.

class PickupModel
{
public:
    void prepare (double sampleRate);
    void reset();
    float processSample (float input) noexcept;

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

        void setCoefficients (double bb0, double bb1, double bb2,
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
    Biquad highPass;          // 28 Hz high-pass (remove DC/subsonic)
    Biquad resonance;         // ~4300 Hz coil/cable resonance
    Biquad topRolloff;        // ~9500 Hz top-end rolloff
    Biquad bodyResonance;     // Guitar body resonance ~80 Hz
    Biquad bodySustain;       // Body sustain nodes ~300 Hz
    Biquad bodyAir;           // Body air resonance ~1000 Hz
    float dcBlockX = 0.0f;
    float dcBlockY = 0.0f;
    float outputTrim = 1.0f;
};
} // namespace ps
