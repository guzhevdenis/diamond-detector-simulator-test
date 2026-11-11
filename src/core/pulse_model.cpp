#include "pulse_model.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
PulseModel::PulseModel(PulseParameters p) : m_parameters(p) {
    if (!std::isfinite(p.amplitude) || p.amplitude < 0 ||
        !std::isfinite(p.riseTimeNs) || p.riseTimeNs <= 0 ||
        !std::isfinite(p.fallTimeNs) || p.fallTimeNs <= 0 ||
        !std::isfinite(p.beta) || p.beta < 0)
        throw std::invalid_argument("A and beta must be finite and nonnegative; time constants must be finite and positive.");
    // Bracket the global positive maximum on an internal logarithmic grid,
    // independently of output sampling frequency, then refine locally.
    const double low = std::min({p.riseTimeNs, p.fallTimeNs, 100.0}) * 1e-8;
    const double high = std::max({p.riseTimeNs, p.fallTimeNs, 100.0}) * 64.0;
    const double step = std::log(high / low) / 8192.0;
    for (int i = 0; i <= 8192; ++i) {
        const double x = low * std::exp(i * step);
        const double f = shape(x);
        if (f > m_maximum) { m_maximum = f; m_peakTimeNs = x; }
    }
    if (!(m_maximum > 1e-12) || !std::isfinite(m_maximum))
        throw std::invalid_argument("These parameters have no stable positive maximum (Fmax > 0 required).");
    double a = m_peakTimeNs * std::exp(-step), b = m_peakTimeNs * std::exp(step);
    for (int i = 0; i < 100; ++i) {
        const double l = a + (b-a)/3, r = b - (b-a)/3;
        if (shape(l) < shape(r)) a = l; else b = r;
    }
    m_peakTimeNs = (a+b)/2;
    m_maximum = shape(m_peakTimeNs);
}
double PulseModel::shape(double x) const {
    if (x <= 0) return 0;
    const auto &p = m_parameters;
    return std::exp(-x/p.fallTimeNs) - std::exp(-x/p.riseTimeNs)
        - p.beta * (std::exp(-x/100.0) - std::exp(-x/p.riseTimeNs));
}
double PulseModel::value(double x) const { return m_parameters.amplitude * shape(x) / m_maximum; }
double PulseModel::settlingTimeNs() const {
    const auto &p = m_parameters;
    return std::max({p.riseTimeNs, p.fallTimeNs, p.beta > 0 ? 100.0 : 0.0})
        * std::log((2+2*p.beta)/(m_maximum*1e-12));
}
