#pragma once
#include <cstddef>
#include <vector>
struct SignalDiagnostics {
    double maximumV = 0;
    double detectionThresholdV = 0;
    std::size_t detectedPulses = 0;
};
// Uses output samples only; no event times, input amplitudes or pulse parameters.
SignalDiagnostics analyzeSignal(const std::vector<double> &signal);
struct PercentageHistogram {
    std::vector<double> centers;
    std::vector<double> percentages;
};
PercentageHistogram samplePercentages(const std::vector<double> &samples, std::size_t bins = 64);
PercentageHistogram weightedPercentages(const std::vector<double> &amplitudes, const std::vector<double> &weights);
