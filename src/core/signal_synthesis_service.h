#pragma once
#include "pulse_model.h"
#include <functional>
#include <random>
#include <vector>
class SignalSynthesisService {
public:
    // Optional sampler returns A in amplitude units, never a time scale.
    std::vector<double> synthesize(const PulseModel &pulse,
        const std::vector<double> &eventTimesSec, double durationSec,
        double sampleRateHz, double noiseStdDev, std::mt19937 &rng,
        const std::function<double(std::mt19937 &)> &amplitudeSampler = {}) const;
};
