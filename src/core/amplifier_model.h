#ifndef AMPLIFIER_MODEL_H
#define AMPLIFIER_MODEL_H

#include <vector>
#include <random>

struct AmplifierModelConfig
{
    double gain1 = 1.0;
    double gain2 = 1.0;

    double tauCrNs = 50.0;   // CR time constant [ns]
    double tauRcNs = 200.0;  // RC time constant [ns]
    int    rcOrder = 1;      // 1 or 2

    double noiseSigma = 0.0;

    bool   saturationEnabled = false;
    double saturationMin = -1.0;
    double saturationMax = 1.0;
};

class AmplifierModel
{
public:
    AmplifierModel() = default;

    std::vector<double> process(const std::vector<double> &input,
                                double sampleRateHz,
                                const AmplifierModelConfig &cfg,
                                std::mt19937 &rng) const;
};

#endif // AMPLIFIER_MODEL_H

