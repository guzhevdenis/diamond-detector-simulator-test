#include "Distribution.h"
#include <random>
#include <cmath>
#include <vector>

std::vector<double> PoissonDistribution::generateTimes(double rateHz, double durationSec, std::mt19937 &rng) const
{
    std::vector<double> times;
    std::exponential_distribution<double> exp(rateHz);
    double t = 0.0;
    while (t < durationSec) {
        t += exp(rng);
        if (t < durationSec)
            times.push_back(t);
    }
    return times;
}

std::vector<double> FixedIntervalDistribution::generateTimes(double rateHz, double durationSec, std::mt19937 &rng) const
{
    std::vector<double> times;
    const double interval = 1.0 / rateHz;
    for (double t = 0.0; t < durationSec; t += interval) {
        times.push_back(t);
    }
    return times;
}

