#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <vector>
namespace Fpga {
constexpr double sampleRate = 800000000.0;
constexpr double voltsPerCode = 2.0 / 65535.0;
constexpr double tickSeconds = 5e-9;
struct Settings {
    int32_t threshold = 50;
    uint16_t tailTicks = 3;
    bool baselineHold = false;
    uint32_t maxLength = UINT32_MAX;
    uint32_t gateTicks = 200000;
    int32_t peakMin = 0, integralMin = 0;
    uint8_t peakShift = 6, integralShift = 13;
    double hostPeakMin = 0, hostPeakWidth = 64;
    double hostIntegralMin = 0, hostIntegralWidth = 512;
};
struct Pulse {
    uint64_t startTime = 0, firstSampleTick = 0, lastSampleTick = 0, doneTick = 0;
    int32_t integral = 0, peak = INT32_MIN, baseline = 0;
    uint32_t length = 0, crossings = 0;
    bool pileUp = false;
};
struct Rates {
    double measured = 0, corrected = 0, corrected2 = 0, deadFraction = 0, pileUpFraction = 0;
};
struct Gate {
    uint64_t number = 0;
    uint32_t length = 0, windows = 0, busy = 0, crossings = 0, above = 0, pileUp = 0, completed = 0, accepted = 0;
    uint32_t peakOut = 0, integralOut = 0;
    std::array<uint32_t, 512> peakBins{};
    std::array<uint32_t, 1024> integralBins{};
    Rates rates;
};
struct TracePoint { double timeNs = 0, minimumV = 0, maximumV = 0, baselineV = 0; bool window = false; };
struct Result {
    Settings settings;
    std::vector<Pulse> pulses;
    std::vector<Gate> gates;
    std::vector<TracePoint> trace;
    std::array<uint64_t, 512> hostPeak{}, fpgaPeak{};
    std::array<uint64_t, 1024> hostIntegral{}, fpgaIntegral{};
    uint64_t hostPeakOut = 0, hostIntegralOut = 0, fpgaPeakOut = 0, fpgaIntegralOut = 0;
    uint64_t completed = 0, pileUp = 0, accepted = 0, windows = 0, saturatedSamples = 0;
    uint64_t trailingSamples = 0, partialGateTicks = 0;
    bool unfinishedWindow = false;
    int32_t finalBaseline = 0;
};
int16_t quantize(double volts);
Rates computeRates(const Gate &gate);
Result process(const std::vector<double> &signal, double fs, const Settings &settings,
               const std::atomic_bool *cancel = nullptr, std::atomic_int *progress = nullptr);
// Test entry point: accepts exact I16 codes, including the document's VI test stimuli.
Result processCodes(const std::vector<int16_t> &codes, const Settings &settings);
}
