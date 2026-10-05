#include "amplitude_spectrum_analyzer.h"

#include <algorithm>
#include <cmath>

AmplitudeSpectrumResult AmplitudeSpectrumAnalyzer::compute(
        const std::vector<double> &times,
        const std::vector<double> &amplitudes,
        double sampleRateHz,
        double threshold,
        double windowSeconds,
        int binCount,
        double startTimeSeconds,
        double endTimeSeconds) const
{
    AmplitudeSpectrumResult result;
    const std::size_t n = amplitudes.size();
    if (n == 0 || binCount <= 0)
        return result;

    // If a time range is provided, restrict analysis to that sub-window
    std::size_t startIndex = 0;
    std::size_t endIndex = n;
    if (!times.empty() && (startTimeSeconds > 0.0 || endTimeSeconds > 0.0)) {
        if (startTimeSeconds > 0.0) {
            auto itStart = std::lower_bound(times.begin(), times.end(), startTimeSeconds);
            startIndex = static_cast<std::size_t>(std::distance(times.begin(), itStart));
        }
        if (endTimeSeconds > 0.0 && endTimeSeconds > startTimeSeconds) {
            auto itEnd = std::upper_bound(times.begin(), times.end(), endTimeSeconds);
            endIndex = static_cast<std::size_t>(std::distance(times.begin(), itEnd));
        }
        if (startIndex >= endIndex || startIndex >= n)
            return result;
        if (endIndex > n)
            endIndex = n;
    }

    // Determine window in samples
    int windowSamples = 0;
    if (windowSeconds > 0.0 && sampleRateHz > 0.0) {
        windowSamples = static_cast<int>(std::round(windowSeconds * sampleRateHz));
    }
    if (windowSamples <= 0)
        windowSamples = 1;

    // Peak detection: simple threshold + local maximum within window
    std::vector<double> peaks;
    std::size_t i = startIndex;
    while (i < endIndex) {
        if (amplitudes[i] >= threshold) {
            std::size_t start = i;
            std::size_t end = std::min<std::size_t>(endIndex, i + static_cast<std::size_t>(windowSamples));
            auto it = std::max_element(amplitudes.begin() + start, amplitudes.begin() + end);
            peaks.push_back(*it);
            i = end; // skip ahead to avoid double counting
        } else {
            ++i;
        }
    }

    if (peaks.empty())
        return result;

    const auto [minIt, maxIt] = std::minmax_element(peaks.begin(), peaks.end());
    const double minA = *minIt;
    const double maxA = *maxIt;
    if (minA == maxA)
        return result;

    const double width = (maxA - minA) / static_cast<double>(binCount);
    result.binCenters.resize(static_cast<std::size_t>(binCount));
    result.binCounts.assign(static_cast<std::size_t>(binCount), 0.0);

    for (int b = 0; b < binCount; ++b) {
        const double center = minA + (b + 0.5) * width;
        result.binCenters[static_cast<std::size_t>(b)] = center;
    }

    for (double a : peaks) {
        int idx = static_cast<int>((a - minA) / width);
        if (idx < 0)
            idx = 0;
        if (idx >= binCount)
            idx = binCount - 1;
        result.binCounts[static_cast<std::size_t>(idx)] += 1.0;
    }

    return result;
}

