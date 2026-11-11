#pragma once
struct PulseParameters {
    double amplitude = 1.0; // Positive peak in volts.
    double riseTimeNs = 6.0;
    double fallTimeNs = 20.0;
    double beta = 0.08;
};
// Specification v1.0: tau_u = 100 ns, y0 = 0. All model times are in ns.
class PulseModel {
public:
    explicit PulseModel(PulseParameters parameters = {});
    double shape(double xNs) const;
    double value(double xNs) const;
    double maximum() const { return m_maximum; }
    double peakTimeNs() const { return m_peakTimeNs; }
    // Beyond this time the absolute normalized tail is below 1e-12.
    double settlingTimeNs() const;
    const PulseParameters &parameters() const { return m_parameters; }
private:
    PulseParameters m_parameters;
    double m_maximum = 0.0;
    double m_peakTimeNs = 0.0;
};
