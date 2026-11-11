#include "signal_synthesis_service.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
std::vector<double> SignalSynthesisService::synthesize(const PulseModel &pulse,
    const std::vector<double> &events, double duration, double fs, double noiseStdDev,
    std::mt19937 &rng, const std::function<double(std::mt19937 &)> &sampler) const {
    if (!std::isfinite(duration) || duration <= 0 || !std::isfinite(fs) || fs <= 0 ||
        !std::isfinite(noiseStdDev) || noiseStdDev < 0)
        throw std::invalid_argument("Invalid duration, sampling frequency or noise level.");
    const double count = std::floor(duration*fs);
    if (!std::isfinite(count) || count > 20000000)
        throw std::invalid_argument("Signal exceeds the limit of 20 million samples.");
    std::vector<double> signal(static_cast<std::size_t>(count), 0.0);
    for (double t0 : events) {
        if (!std::isfinite(t0) || t0 < 0 || t0 >= duration) continue;
        const double amplitude = sampler ? sampler(rng) : pulse.parameters().amplitude;
        if (!std::isfinite(amplitude) || amplitude < 0)
            throw std::invalid_argument("Event amplitude A must be finite and nonnegative. Check the amplitude distribution.");
        // Exact fractional event time; no rounding to an output sample.
        const auto first = static_cast<std::size_t>(std::ceil(t0*fs));
        const double end = std::min(count, std::ceil((t0+pulse.settlingTimeNs()*1e-9)*fs));
        for (std::size_t k = first; k < static_cast<std::size_t>(end); ++k)
            signal[k] += amplitude * pulse.shape((static_cast<double>(k)/fs-t0)*1e9) / pulse.maximum();
    }
    if (noiseStdDev > 0) {
        std::normal_distribution<double> noise(0, noiseStdDev);
        for (double &v : signal) v += noise(rng);
    }
    return signal;
}
