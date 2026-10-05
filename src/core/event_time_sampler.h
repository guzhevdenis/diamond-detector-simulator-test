#ifndef EVENT_TIME_SAMPLER_H
#define EVENT_TIME_SAMPLER_H
#include "Distribution.h"
#include <vector>
#include <random>

class EventTimeSampler
{
    private:
    std::unique_ptr<Distribution> m_distribution;
public:
    EventTimeSampler() = default;
    std::vector<double> generateTimes(double rateHz, double durationSec, std::mt19937 &rng) const;
    void setDistribution(std::unique_ptr<Distribution> d);
};

#endif // EVENT_TIME_SAMPLER_H
