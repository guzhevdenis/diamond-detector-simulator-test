#include "event_time_sampler.h"

#include <algorithm>
#include <cmath>

/* std::vector<double> EventTimeSampler::generateTimes(Distribution dist,
                                                    double rateHz,
                                                    double durationSec,
                                                    std::mt19937 &rng) const
{
    std::vector<double> times;
    if (rateHz <= 0.0 || durationSec <= 0.0)
        return times;

    if (dist == Distribution::FixedInterval) {
        const double interval = 1.0 / rateHz;
        for (double t = 0.0; t < durationSec; t += interval) {
            times.push_back(t);
        }
        return times;
    }

    if (dist == Distribution::Poisson) {
        std::exponential_distribution<double> exp(rateHz);
        double t = 0.0;
        while (t < durationSec) {
            t += exp(rng);
            if (t < durationSec)
                times.push_back(t);
        }
    }

    return times;
} */

void EventTimeSampler::setDistribution(std::unique_ptr<Distribution> d){ 
    m_distribution = std::move(d);
}
std::vector<double> EventTimeSampler::generateTimes(double rateHz, double durationSec, std::mt19937 &rng) const
{
    if (!m_distribution)
        return {};
    return m_distribution->generateTimes(rateHz, durationSec, rng);
}
