#include "fpga_processing.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
namespace Fpga {
namespace {
int32_t wrap32(int64_t v) {
    const uint32_t u = static_cast<uint32_t>(v);
    return u <= INT32_MAX ? static_cast<int32_t>(u) : static_cast<int32_t>(int64_t(u) - 4294967296LL);
}
int32_t baselineOf(int32_t accumulator) {
    return accumulator >= 0 ? accumulator / 4096 : -static_cast<int32_t>((-int64_t(accumulator)+4095)/4096);
}
template<class T, std::size_t N>
bool addBin(std::array<T, N> &bins, int32_t value, double minimum, double width) {
    const double index = std::floor((double(value)-minimum)/width);
    if (index < 0 || index >= N) return false;
    ++bins[static_cast<std::size_t>(index)]; return true;
}
struct Frame { int32_t sum = 0, peak = 0, baseline = 0; uint64_t source = 0; bool high = false, valid = false; };
Result run(std::size_t n, const std::function<int16_t(std::size_t)> &read,
           const Settings &s, const std::atomic_bool *cancel, std::atomic_int *progress) {
    if (s.threshold < 0 || s.gateTicks < 3200 || s.peakShift > 30 || s.integralShift > 30 ||
        !std::isfinite(s.hostPeakMin) || !std::isfinite(s.hostIntegralMin) ||
        !std::isfinite(s.hostPeakWidth) || s.hostPeakWidth <= 0 ||
        !std::isfinite(s.hostIntegralWidth) || s.hostIntegralWidth <= 0)
        throw std::invalid_argument("Invalid FPGA settings (gate >= 16 us; positive histogram widths required).");
    Result result; result.settings = s; result.trailingSamples = n % 4;
    const uint64_t cycles = n/4;
    std::array<Frame, 4> pipeline{};
    std::array<int16_t, 4> input{};
    Frame delayed;
    int32_t accumulator = 0;
    uint32_t remaining = 0;
    bool previousWindow = false, previousHigh = false, pendingPileUp = false;
    Pulse pulse;
    Gate gate;
    const uint64_t traceStride = std::max<uint64_t>(1, (cycles+49999)/50000);
    TracePoint trace;
    for (uint64_t cycle = 0; cycle < cycles; ++cycle) {
        if (cycle % 8192 == 0) {
            if (cancel && cancel->load()) throw std::runtime_error("Simulation cancelled.");
            if (progress) progress->store(static_cast<int>(100*cycle/std::max<uint64_t>(1,cycles)));
        }
        if (cycle % s.gateTicks == 0) { gate = Gate{}; gate.number = cycle/s.gateTicks; }
        ++gate.length;
        // Read all old registers before committing the new values.
        const int32_t baseline = baselineOf(accumulator);
        Frame front = pipeline[cycle%4];
        Frame stage1;
        if (cycle > 0) {
            stage1.valid = true; stage1.source = cycle-1; stage1.baseline = baseline;
            stage1.peak = INT32_MIN;
            for (int16_t v : input) {
                const int32_t d = int32_t(v)-baseline;
                stage1.sum += d; stage1.peak = std::max(stage1.peak,d);
                stage1.high = stage1.high || d > s.threshold;
            }
        }
        // Stage 1 -> stage 3/valid output: three more registers.
        pipeline[(cycle+3)%4] = stage1;
        int16_t minimum = INT16_MAX, maximum = INT16_MIN;
        for (int k = 0; k < 4; ++k) {
            input[k] = read(cycle*4+k);
            minimum = std::min(minimum,input[k]); maximum = std::max(maximum,input[k]);
        }
        const bool high = front.valid && front.high;
        const bool window = front.valid && (high || remaining > 0);
        const bool rise = high && !previousHigh;
        const bool start = window && !previousWindow;
        const bool done = front.valid && previousWindow && !window;
        if (pendingPileUp) ++gate.pileUp;
        pendingPileUp = false;
        if (front.valid) {
            if (start) { ++gate.windows; ++result.windows; }
            if (window) ++gate.busy;
            if (rise) ++gate.crossings;
            if (high) ++gate.above;
        }
        if (start) {
            pulse = Pulse{};
            // FPGA timestamp counter starts at 1 (document test: threshold at input tick 3 -> StartTime 4).
            pulse.startTime = front.source+1;
            pulse.firstSampleTick = delayed.valid ? delayed.source : front.source;
            pulse.baseline = delayed.valid ? delayed.baseline : front.baseline;
        }
        if (window && delayed.valid) {
            pulse.integral = wrap32(int64_t(pulse.integral)+delayed.sum);
            pulse.peak = std::max(pulse.peak,delayed.peak);
            ++pulse.length;
            pulse.lastSampleTick = delayed.source;
        }
        if (window && rise) ++pulse.crossings;
        if (done) {
            pulse.doneTick = cycle;
            pulse.pileUp = pulse.crossings >= 2 || pulse.length > s.maxLength;
            ++result.completed; ++gate.completed;
            if (pulse.pileUp) { ++result.pileUp; pendingPileUp = true; }
            if (!addBin(result.hostPeak,pulse.peak,s.hostPeakMin,s.hostPeakWidth)) ++result.hostPeakOut;
            if (!addBin(result.hostIntegral,pulse.integral,s.hostIntegralMin,s.hostIntegralWidth)) ++result.hostIntegralOut;
            if (!pulse.pileUp) {
                ++result.accepted; ++gate.accepted;
                if (!addBin(gate.peakBins,pulse.peak,s.peakMin,std::ldexp(1.0,s.peakShift))) ++gate.peakOut;
                if (!addBin(gate.integralBins,pulse.integral,s.integralMin,std::ldexp(1.0,s.integralShift))) ++gate.integralOut;
            }
            result.pulses.push_back(pulse);
        }
        if (front.valid && !s.baselineHold && !previousWindow && !high)
            accumulator = wrap32(int64_t(accumulator)+front.sum);
        if (front.valid) {
            if (high) remaining = uint32_t(s.tailTicks)+1; // P=1 plus T
            else if (remaining) --remaining;
            previousHigh = high; previousWindow = window;
        }
        delayed = front;
        if (cycle % traceStride == 0) {
            trace = {}; trace.timeNs = cycle*5.0;
            trace.minimumV = minimum*voltsPerCode; trace.maximumV = maximum*voltsPerCode;
            trace.baselineV = baseline*voltsPerCode;
        } else {
            trace.minimumV = std::min(trace.minimumV,minimum*voltsPerCode);
            trace.maximumV = std::max(trace.maximumV,maximum*voltsPerCode);
        }
        trace.window = trace.window || window;
        if ((cycle+1)%traceStride == 0 || cycle+1 == cycles) result.trace.push_back(trace);
        if (gate.length == s.gateTicks) {
            gate.rates = computeRates(gate);
            for (std::size_t i=0;i<512;++i) result.fpgaPeak[i] += gate.peakBins[i];
            for (std::size_t i=0;i<1024;++i) result.fpgaIntegral[i] += gate.integralBins[i];
            result.fpgaPeakOut += gate.peakOut; result.fpgaIntegralOut += gate.integralOut;
            result.gates.push_back(std::move(gate));
        }
    }
    result.partialGateTicks = cycles % s.gateTicks;
    result.unfinishedWindow = previousWindow;
    result.finalBaseline = baselineOf(accumulator);
    if (progress) progress->store(100);
    return result;
}
}
int16_t quantize(double v) {
    if (!std::isfinite(v)) throw std::invalid_argument("Signal contains non-finite samples.");
    // Ideal signed 12-bit ADC over a 2 V full scale. Clamp before conversion.
    const double code = std::clamp(std::round(v*2048.0),-2048.0,2047.0);
    return static_cast<int16_t>(static_cast<int>(code)*16);
}
Rates computeRates(const Gate &g) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (!g.length) return {nan,nan,nan,nan,nan};
    return {g.windows/(g.length*tickSeconds),
        g.busy < g.length ? g.windows/((g.length-g.busy)*tickSeconds) : nan,
        g.above < g.length ? g.crossings/((g.length-g.above)*tickSeconds) : nan,
        double(g.busy)/g.length, g.windows ? double(g.pileUp)/g.windows : nan};
}
Result process(const std::vector<double> &signal, double fs, const Settings &s,
               const std::atomic_bool *cancel, std::atomic_int *progress) {
    if (fs != sampleRate) throw std::invalid_argument("FPGA simulation requires a signal sampled at exactly 800 MHz.");
    if (signal.size() < 4) throw std::invalid_argument("At least four samples are required.");
    uint64_t saturated = 0;
    auto result = run(signal.size(),[&](std::size_t i) {
        const double v = signal[i]; if (v >= 1.0 || v < -1.0) ++saturated;
        return quantize(v);
    },s,cancel,progress);
    result.saturatedSamples = saturated;
    return result;
}
Result processCodes(const std::vector<int16_t> &codes, const Settings &s) {
    return run(codes.size(),[&](std::size_t i) { return codes[i]; },s,nullptr,nullptr);
}
}
