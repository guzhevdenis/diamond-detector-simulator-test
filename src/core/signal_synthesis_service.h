#pragma once
#include "pulse_model.h"
#include <cstddef>
#include <functional>
#include <random>
#include <vector>
class SignalSynthesisService {
public:
    static constexpr std::size_t maxSamples = 800'000'000;
    // Validate before allocating memory or generating event times.
    static std::size_t sampleCount(double durationSec, double sampleRateHz);

    // Optional sampler returns A in amplitude units, never a time scale.
    std::vector<double> synthesize(const PulseModel &pulse,
        const std::vector<double> &eventTimesSec, double durationSec,
        double sampleRateHz, double noiseStdDev, std::mt19937 &rng,
        const std::function<double(std::mt19937 &)> &amplitudeSampler = {}) const;
};
