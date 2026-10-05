#ifndef AMPLITUDE_SPECTRUM_ANALYZER_H
#define AMPLITUDE_SPECTRUM_ANALYZER_H

#include <vector>

struct AmplitudeSpectrumResult
{
    std::vector<double> binCenters;
    std::vector<double> binCounts;
};

class AmplitudeSpectrumAnalyzer
{
public:
    AmplitudeSpectrumAnalyzer() = default;

    AmplitudeSpectrumResult compute(const std::vector<double> &times,
                                    const std::vector<double> &amplitudes,
                                    double sampleRateHz,
                                    double threshold,
                                    double windowSeconds,
                                    int binCount,
                                    double startTimeSeconds = 0.0,
                                    double endTimeSeconds = 0.0) const;
};

#endif // AMPLITUDE_SPECTRUM_ANALYZER_H

