#include "signal_synthesis_service.h"

#include "amplitude_spectrum.h"

#include <algorithm>
#include <cmath>

std::vector<double> SignalSynthesisService::synthesize(const std::vector<double> &pulseTemplate,
                                                       const std::vector<double> &eventTimesSec,
                                                       const AmplitudeSpectrum &spectrum,
                                                       double durationSec,
                                                       double sampleRateHz,
                                                       double noiseStdDev,
                                                       double offset,
                                                       std::mt19937 &rng) const
{
    std::vector<double> signal;
    if (pulseTemplate.empty() || durationSec <= 0.0 || sampleRateHz <= 0.0)
        return signal;

    const std::size_t totalSamples =
            static_cast<std::size_t>(std::floor(durationSec * sampleRateHz));
    if (totalSamples == 0)
        return signal;

    signal.assign(totalSamples, offset); 

    const std::size_t tplSize = pulseTemplate.size();

    // logic for signal generation
    for (double t : eventTimesSec) {
        if (t < 0.0 || t >= durationSec)
            continue;

        const double amp = spectrum.hasData() ? spectrum.sample(rng) : 1.0;

        std::size_t startIndex =
                static_cast<std::size_t>(std::llround(t * sampleRateHz));
        if (startIndex >= totalSamples)
            continue;

        for (std::size_t k = 0; k < tplSize; ++k) {
            const std::size_t idx = startIndex + k;
            if (idx >= totalSamples)
                break;
            signal[idx] += amp * pulseTemplate[k];
        }
    }

    if (noiseStdDev > 0.0) {
        std::normal_distribution<double> noise(0.0, noiseStdDev);
        for (double &v : signal) {
            v += noise(rng);
        }
    }

    return signal;
}

