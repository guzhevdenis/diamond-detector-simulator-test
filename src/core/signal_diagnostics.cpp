#include "signal_diagnostics.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <numeric>

SignalDiagnostics analyzeSignal(const std::vector<double> &signal) {
    SignalDiagnostics result;
    if (signal.empty()) return result;
    const auto bounds = std::minmax_element(signal.begin(), signal.end());
    result.maximumV = *bounds.second;
    const double range = *bounds.second - *bounds.first;
    if (!(range > 0)) return result;
    // Robust noise estimate from adjacent differences at at most 100k positions.
    std::vector<double> differences;
    const std::size_t stride = std::max<std::size_t>(1, signal.size() / 100000);
    for (std::size_t i = 1; i < signal.size(); i += stride)
        differences.push_back(std::abs(signal[i] - signal[i-1]));
    auto middle = differences.begin() + differences.size()/2;
    std::nth_element(differences.begin(), middle, differences.end());
    const double noiseSigma = *middle / (0.6744897501960817 * std::sqrt(2.0));
    result.detectionThresholdV = std::max(range * 0.01, 6.0 * noiseSigma);
    // Hysteresis counts a positive peak once after a significant decline.
    // A flat clipped top is one peak; resolved overlapping peaks can be separate.
    double valley = signal.front(), peak = signal.front();
    bool rising = false;
    for (double v : signal) {
        if (!rising) {
            valley = std::min(valley, v);
            if (v - valley >= result.detectionThresholdV && v > result.detectionThresholdV) {
                rising = true;
                peak = v;
            }
        } else {
            peak = std::max(peak, v);
            if (peak - v >= result.detectionThresholdV) {
                ++result.detectedPulses;
                rising = false;
                valley = v;
            }
        }
    }
    if (rising) ++result.detectedPulses; // A pulse cut by the record boundary.
    return result;
}

PercentageHistogram weightedPercentages(const std::vector<double> &x, const std::vector<double> &weights) {
    if (x.size() != weights.size()) throw std::invalid_argument("Histogram sizes differ.");
    std::map<double, double> merged;
    double total = 0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        if (!std::isfinite(x[i]) || !std::isfinite(weights[i]) || weights[i] < 0)
            throw std::invalid_argument("Spectrum contains invalid amplitudes or weights.");
        if (weights[i] == 0) continue;
        merged[x[i]] += weights[i];
        total += weights[i];
    }
    if (!std::isfinite(total)) throw std::invalid_argument("Spectrum weight sum overflow.");
    PercentageHistogram result;
    for (const auto &entry : merged) {
        result.centers.push_back(entry.first);
        result.percentages.push_back(100.0 * (entry.second / total));
    }
    return result;
}

PercentageHistogram samplePercentages(const std::vector<double> &samples, std::size_t bins) {
    if (samples.empty() || bins == 0) return {};
    for (double v : samples)
        if (!std::isfinite(v)) throw std::invalid_argument("Spectrum contains non-finite amplitudes.");
    const auto bounds = std::minmax_element(samples.begin(), samples.end());
    const double low = *bounds.first, high = *bounds.second;
    if (low == high) return {{low}, {100.0}};
    const double width = (high - low) / bins;
    if (!std::isfinite(width) || width <= 0) throw std::invalid_argument("Invalid histogram range.");
    PercentageHistogram result;
    result.percentages.assign(bins, 0);
    for (std::size_t i = 0; i < bins; ++i) result.centers.push_back(low + (i+0.5)*width);
    for (double v : samples) {
        const auto bin = std::min(bins-1, static_cast<std::size_t>((v-low)/width));
        result.percentages[bin] += 1;
    }
    for (double &v : result.percentages) v *= 100.0 / samples.size();
    return result;
}
