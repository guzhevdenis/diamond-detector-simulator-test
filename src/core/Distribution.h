#ifndef DISTRIBUTION_H
#define DISTRIBUTION_H

#include <vector>
#include <random>

class Distribution
{
    public:
    virtual std::vector<double> generateTimes(double rateHz, double durationSec, std::mt19937 &rng) const = 0;
    virtual ~Distribution() = default;
};

class FixedIntervalDistribution: public Distribution
{
    public:
    FixedIntervalDistribution() = default;
    std::vector<double> generateTimes(double rateHz, double durationSec, std::mt19937 &rng) const override;
};

class PoissonDistribution: public Distribution
{
    public:
    PoissonDistribution() = default;
    std::vector<double> generateTimes(double rateHz, double durationSec, std::mt19937 &rng) const override;
};

#endif // DISTRIBUTION_H