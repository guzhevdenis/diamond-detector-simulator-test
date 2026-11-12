#include "fpga_processing.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <numeric>
#include <algorithm>
void check(bool ok,const char *message) { if (!ok) throw std::runtime_error(message); }
void near(double a,double b,double eps,const char *m) { check(std::abs(a-b)<eps,m); }
template<class F> void rejects(F f) { bool caught=false; try { f(); } catch (const std::exception &) { caught=true; } check(caught,"Expected rejection"); }
int main() {
 try {
    Fpga::Settings s; s.baselineHold=true;
    std::vector<int16_t> samples(12800,0);
    const int16_t fixture[][4]={{0,0,10,30},{60,120,200,250},{300,180,120,80},{100,30,25,15},{18,10,25,5},{15,10,5,0},{30,40,10,0}};
    for (int i=0;i<7;++i) for (int k=0;k<4;++k) samples[(i+2)*4+k]=fixture[i][k];
    s.gateTicks=3200;
    auto r=Fpga::processCodes(samples,s);
    check(r.pulses.size()==1,"Document single pulse");
    auto p=r.pulses[0];
    check(p.integral==1688 && p.peak==300 && p.length==7 && p.crossings==1,"Document Integral/Peak/Length/Crossings");
    check(p.startTime==4 && p.doneTick==14 && p.firstSampleTick==2 && p.lastSampleTick==8,"Document timestamps and window");
    check(r.gates.size()==1 && r.gates[0].windows==1 && r.gates[0].busy==7 && r.gates[0].above==3,"Gate counters");
    samples[7*4]=60;
    r=Fpga::processCodes(samples,s); p=r.pulses[0];
    check(p.doneTick==16 && p.length==9 && p.crossings==2 && p.pileUp,"Document pile-up case");
    check(r.completed==1 && r.accepted==0 && r.gates[0].pileUp==1,"FPGA rejects pile-up");
    check(std::accumulate(r.hostPeak.begin(),r.hostPeak.end(),uint64_t(0))==1,"Host retains pile-up");
    s.maxLength=7; samples[7*4]=15;
    check(!Fpga::processCodes(samples,s).pulses[0].pileUp,"MaxLength strict comparison");
    s.maxLength=6; check(Fpga::processCodes(samples,s).pulses[0].pileUp,"Length-based pile-up"); s.maxLength=UINT32_MAX;
    s.peakMin=1000; r=Fpga::processCodes(samples,s);
    check(r.gates[0].peakOut==1 && r.gates[0].accepted==1,"FPGA out of range invariant");
    s.peakMin=0;
    std::vector<int16_t> atThreshold(12800,0); atThreshold[100]=50;
    check(Fpga::processCodes(atThreshold,s).pulses.empty(),"Strict greater-than threshold");
    check(Fpga::quantize(1)==32752 && Fpga::quantize(-1)==-32768 && Fpga::quantize(0)==0,"ADC rails and zero");
    check(Fpga::quantize(.1)%16==0,"12 bits left aligned");
    rejects([&] { Fpga::process({0,0,0,0},4e8,s); });
    std::atomic_bool cancelled(true); rejects([&] { Fpga::process(std::vector<double>(12800),8e8,s,&cancelled); });
    auto shortRecord=Fpga::process(std::vector<double>(101),8e8,s);
    check(shortRecord.trailingSamples==1 && shortRecord.partialGateTicks==25 && shortRecord.gates.empty(),"No fabricated partial gate or sample");
    Fpga::Gate g; g.windows=3680; g.crossings=4000; g.busy=126000; g.above=80000; g.length=200000; g.pileUp=368;
    auto rates=Fpga::computeRates(g);
    near(rates.measured,3.68e6,1e-5,"Document measured rate"); near(rates.corrected,9945945.945945946,1e-5,"Document corrected rate"); near(rates.corrected2,6666666.666666667,1e-5,"Document corrected2"); near(rates.deadFraction,.63,1e-10,"Dead fraction"); near(rates.pileUpFraction,.1,1e-10,"Pile-up fraction");
    g.busy=g.length; g.above=g.length; check(std::isnan(Fpga::computeRates(g).corrected) && std::isnan(Fpga::computeRates(g).corrected2),"No live time");
    g.windows=0; check(std::isnan(Fpga::computeRates(g).pileUpFraction),"No windows");
    // Windows open in one gate, Done and pile-up accounting can belong to the next.
    std::vector<int16_t> boundary(25600,0);
    for (int i=0;i<7;++i) for (int k=0;k<4;++k) boundary[(i+2+3186)*4+k]=fixture[i][k];
    r=Fpga::processCodes(boundary,s);
    check(r.pulses[0].doneTick==3200 && r.gates[0].windows==1 && r.gates[0].completed==0 && r.gates[1].completed==1,"Gate-boundary Done ownership");
    check(r.gates[0].busy+r.gates[1].busy==7,"BusyTicks split at gate boundary");
    for (const auto &gate:r.gates) {
        check(std::accumulate(gate.peakBins.begin(),gate.peakBins.end(),uint64_t(0))+gate.peakOut==gate.accepted,"Peak gate invariant");
        check(std::accumulate(gate.integralBins.begin(),gate.integralBins.end(),uint64_t(0))+gate.integralOut==gate.accepted,"Integral gate invariant");
    }
    auto unfinished=Fpga::processCodes(std::vector<int16_t>(100,100),s);
    check(unfinished.unfinishedWindow && unfinished.completed==0,"Open window is not a completed record");
    s.baselineHold=false; s.threshold=1000;
    r=Fpga::processCodes(std::vector<int16_t>(80000,160),s);
    check(std::abs(r.finalBaseline-160)<=1 && r.pulses.empty(),"Baseline converges without false pulses");
    s.baselineHold=true; check(Fpga::processCodes(std::vector<int16_t>(80000,160),s).finalBaseline==0,"BaselineHold");
    s.threshold=50;
    std::vector<double> voltageRecord(12800,0);
    std::fill(voltageRecord.begin()+400,voltageRecord.begin()+416,.25);
    r=Fpga::process(voltageRecord,8e8,s);
    check(r.pulses.size()==1 && r.pulses[0].peak==8192 && r.pulses[0].integral==131072,"Volts to ADC to measured pulse");
    s.baselineHold=false; s.threshold=1000;
    check(std::abs(Fpga::processCodes(std::vector<int16_t>(80000,-160),s).finalBaseline+160)<=1,"Signed baseline shift");
    rejects([&] { Fpga::process({0,std::numeric_limits<double>::quiet_NaN(),0,0},8e8,s); });
    std::cout<<"FPGA tests passed: document pulse/pile-up, timing, rates, bins, baseline, ADC, boundaries, cancellation.\n";
 } catch(const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
}
