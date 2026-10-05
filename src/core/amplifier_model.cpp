#include "amplifier_model.h"

#include <algorithm>
#include <cmath>

std::vector<double> AmplifierModel::process(const std::vector<double> &input,
                                            double sampleRateHz,
                                            const AmplifierModelConfig &cfg,
                                            std::mt19937 &rng) const
{
    std::vector<double> x = input;
    const std::size_t n = x.size();
    if (n == 0 || sampleRateHz <= 0.0)
        return {};

    // First gain stage
    if (cfg.gain1 != 1.0) {
        for (double &v : x)
            v *= cfg.gain1;
    }

    const double dt = 1.0 / sampleRateHz;

    // CR high-pass (differentiator-like) stage
    if (cfg.tauCrNs > 0.0) {
        const double tauCr = cfg.tauCrNs * 1e-9;
        const double alphaCr = tauCr / (tauCr + dt);
        double yPrev = 0.0;
        double xPrev = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double xi = x[i];
            const double y = alphaCr * (yPrev + xi - xPrev);
            xPrev = xi;
            yPrev = y;
            x[i] = y;
        }
    }

    // RC low-pass (shaper) stage, possibly multi-order (cascade)
    if (cfg.tauRcNs > 0.0 && cfg.rcOrder > 0) {
        const double tauRc = cfg.tauRcNs * 1e-9;
        const double alphaRc = dt / (tauRc + dt);
        const int order = std::max(1, std::min(cfg.rcOrder, 2)); // limit to 1 or 2 for now

        for (int stage = 0; stage < order; ++stage) {
            double yPrev = 0.0;
            for (std::size_t i = 0; i < n; ++i) {
                const double xi = x[i];
                const double y = alphaRc * xi + (1.0 - alphaRc) * yPrev;
                yPrev = y;
                x[i] = y;
            }
        }
    }

    // Second gain stage
    if (cfg.gain2 != 1.0) {
        for (double &v : x)
            v *= cfg.gain2;
    }

    // Add output noise
    if (cfg.noiseSigma > 0.0) {
        std::normal_distribution<double> noise(0.0, cfg.noiseSigma);
        for (double &v : x) {
            v += noise(rng);
        }
    }

    // Saturation / clipping
    if (cfg.saturationEnabled) {
        const double vmin = cfg.saturationMin;
        const double vmax = cfg.saturationMax;
        for (double &v : x) {
            if (v < vmin)
                v = vmin;
            else if (v > vmax)
                v = vmax;
        }
    }

    return x;
}

