#ifndef SIGNAL_SYNTHESIS_SERVICE_H
#define SIGNAL_SYNTHESIS_SERVICE_H

#include <vector>
#include <random>

class AmplitudeSpectrum;

class SignalSynthesisService
{
public:
    SignalSynthesisService() = default;

    std::vector<double> synthesize(const std::vector<double> &pulseTemplate,
                                   const std::vector<double> &eventTimesSec,
                                   const AmplitudeSpectrum &spectrum,
                                   double durationSec,
                                   double sampleRateHz,
                                   double noiseStdDev,
                                   double offset,
                                   std::mt19937 &rng) const;
};

#endif // SIGNAL_SYNTHESIS_SERVICE_H

