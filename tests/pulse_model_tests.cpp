#include "pulse_model.h"
#include "signal_synthesis_service.h"
#include "signal_diagnostics.h"
#include <numeric>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
void near(double a, double b, double tolerance, const char *message) {
    require(std::abs(a-b) <= tolerance, message);
}
template<class F> void rejects(F fn) {
    bool rejected = false;
    try { fn(); } catch (const std::invalid_argument &) { rejected = true; }
    require(rejected, "Invalid input was accepted");
}
double width(const PulseModel &p) {
    double first = -1, last = -1;
    for (int i = 0; i < 100000; ++i) {
        const double t = i * .001;
        if (p.value(t) >= p.parameters().amplitude/2) {
            if (first < 0) first = t;
            last = t;
        }
    }
    return last-first;
}
int main() {
    try {
        const PulseModel p;
        near(p.value(-1), 0, 0, "Causality");
        near(p.value(0), 0, 0, "Onset");
        near(p.value(p.peakTimeNs()), 1.0, 1e-12, "Peak height in volts");
        require(p.value(200) < 0, "Negative tail missing");
        require(std::abs(p.value(p.settlingTimeNs())) < 1e-12, "Tail bound");
        PulseParameters parameters;
        parameters.beta = 0;
        const PulseModel noTail(parameters);
        const double peak = std::log(20.0/6)/(1.0/6-1.0/20);
        near(noTail.peakTimeNs(), peak, 1e-6, "Analytic beta=0 peak time");
        for (int i = 0; i < 10000; ++i) require(noTail.value(i*.1) >= 0, "beta=0 tail");
        const double referenceWidth = width(p);
        for (double amplitude : {1e-6, 0.001, 0.1, 1.0, 10.0}) {
            parameters = {}; parameters.amplitude = amplitude;
            PulseModel scaled(parameters);
            near(width(scaled), referenceWidth, .001, "FWHM changed with A");
            for (double t : {1.0, 10.0, 50.0, 200.0})
                near(scaled.value(t)/amplitude, p.value(t)/p.parameters().amplitude, 1e-14, "Shape changed with A");
        }
        SignalSynthesisService service;
        // Check allocation boundaries without allocating a maximum-size signal.
        require(service.sampleCount(1.0, 8e8) == 800000000, "One second at 800 MHz");
        const double limit = SignalSynthesisService::maxSamples;
        require(service.sampleCount(1, limit) == SignalSynthesisService::maxSamples, "Exact sample limit");
        require(service.sampleCount(1, limit + 0.5) == SignalSynthesisService::maxSamples, "Fractional sample count");
        rejects([&] { service.sampleCount(1, limit + 1); });
        rejects([&] { service.sampleCount(0, 8e8); });
        rejects([&] { service.sampleCount(1, std::numeric_limits<double>::quiet_NaN()); });
        rejects([&] { service.sampleCount(1e300, 1e300); });
        std::mt19937 rng(123);
        const double t0 = 100.37e-9;
        for (double fs : {4e8, 8e8, 2e9}) {
            const auto signal = service.synthesize(p, {t0}, 4e-6, fs, 0, rng);
            for (std::size_t k = 0; k < signal.size(); ++k)
                near(signal[k], p.value((k/fs-t0)*1e9), 1e-12, "Output grid / fractional t0");
        }
        auto summed = service.synthesize(p, {t0, t0+30e-9}, 4e-6, 8e8, 0, rng);
        for (std::size_t k = 0; k < summed.size(); ++k)
            near(summed[k], p.value((k/8e8-t0)*1e9)+p.value((k/8e8-t0-30e-9)*1e9), 3e-8, "Pile-up");
        auto sampled = service.synthesize(p, {t0}, 4e-6, 8e8, 0, rng,
            [](std::mt19937 &) { return 25.0; });
        near(sampled[100], p.value((100/8e8-t0)*1e9)*25/p.parameters().amplitude, 1e-12, "Sampler returns absolute A");
        const auto raw = service.synthesize(p, {t0, t0+1e-6}, 4e-6, 8e8, 0, rng);
        const auto clipped = service.synthesize(p, {t0, t0+1e-6}, 4e-6, 8e8, 0, rng, {}, {true, 0.05});
        for (std::size_t k = 0; k < raw.size(); ++k)
            near(clipped[k], std::clamp(raw[k], -0.05, 0.05), 0, "Symmetric ADC clipping");
        require(*std::min_element(clipped.begin(), clipped.end()) == -0.05, "Negative ADC rail");
        require(*std::max_element(clipped.begin(), clipped.end()) == 0.05, "Positive ADC rail");
        auto diagnostic = analyzeSignal(clipped);
        near(diagnostic.maximumV, 0.05, 0, "Diagnostics uses clipped output");
        require(diagnostic.detectedPulses == 2, "Clipped plateaus counted once");
        require(analyzeSignal(raw).detectedPulses == 2, "Separated model pulses");
        const auto overlap = service.synthesize(p, {t0, t0}, 4e-6, 8e8, 0, rng);
        require(analyzeSignal(overlap).detectedPulses == 1, "Count is measured, not event count");
        require(analyzeSignal({}).detectedPulses == 0, "Empty diagnostics");
        require(analyzeSignal(std::vector<double>(100, 0)).detectedPulses == 0, "Zero diagnostics");
        std::vector<double> measured(100, 0);
        measured.insert(measured.end(), {0.2, 0.4, 0.6, 0.6, 0.3, 0, -0.1, 0, 0.2, 0.7, 0.2, 0});
        measured.resize(250, 0);
        require(analyzeSignal(measured).detectedPulses == 2, "Waveform-only peaks and plateau");
        std::mt19937 noiseRawRng(7), noiseClippedRng(7);
        const auto noisy = service.synthesize(p, {}, 1e-6, 8e8, 2, noiseRawRng);
        const auto noisyClipped = service.synthesize(p, {}, 1e-6, 8e8, 2, noiseClippedRng, {}, {true, 1});
        for (std::size_t k = 0; k < noisy.size(); ++k)
            near(noisyClipped[k], std::clamp(noisy[k], -1.0, 1.0), 0, "Clip after noise");
        rejects([&] { service.synthesize(p, {}, 1e-6, 8e8, 0, rng, {}, {true, 0}); });
        const auto constantHistogram = samplePercentages({0.4, 0.4, 0.4});
        require(constantHistogram.centers.size() == 1, "Constant histogram is visible");
        near(constantHistogram.percentages[0], 100, 0, "Constant histogram share");
        const auto weighted = weightedPercentages({2, 1, 2}, {1, 2, 3});
        require(weighted.centers == std::vector<double>({1, 2}), "Sort and merge file spectrum");
        near(weighted.percentages[1], 100.0*4/6, 1e-12, "File spectrum normalization");
        std::mt19937 histogramRng(11);
        std::normal_distribution<double> normal(1, 0.1);
        std::exponential_distribution<double> exponential(1);
        std::lognormal_distribution<double> logNormal(0, .25);
        for (int mode = 0; mode < 3; ++mode) {
            std::vector<double> population;
            for (int i = 0; i < 50000; ++i)
                population.push_back(mode == 0 ? normal(histogramRng) : mode == 1 ? exponential(histogramRng) : logNormal(histogramRng));
            const auto histogram = samplePercentages(population);
            require(histogram.centers.size() == 64, "Built-in histogram bins");
            near(std::accumulate(histogram.percentages.begin(), histogram.percentages.end(), 0.0), 100, 1e-10, "Percentages total 100");
        }
        auto baseline = service.synthesize(p, {}, 1e-6, 8e8, 0, rng);
        require(std::all_of(baseline.begin(), baseline.end(), [](double v) { return v == 0; }), "Zero baseline");
        std::mt19937 r1(42), r2(42);
        require(service.synthesize(p, {t0}, 1e-6, 8e8, 1, r1) ==
                service.synthesize(p, {t0}, 1e-6, 8e8, 1, r2), "Noise reproducibility");
        rejects([] { PulseParameters q; q.riseTimeNs = 0; PulseModel bad(q); });
        rejects([] { PulseParameters q; q.fallTimeNs = q.riseTimeNs; q.beta = 0; PulseModel bad(q); });
        rejects([] { PulseParameters q; q.beta = std::numeric_limits<double>::quiet_NaN(); PulseModel bad(q); });
        rejects([&] { service.synthesize(p, {t0}, 1e-6, 8e8, 0, rng,
            [](std::mt19937 &) { return -1; }); });
        rejects([&] { service.synthesize(p, {}, 1, 1e12, 0, rng); });
        std::cout << "Passed: peak normalization, beta=0, amplitude-independent FWHM, tail, sampling, fractional onset, overlap, distributions, baseline, noise, invalid inputs.\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
