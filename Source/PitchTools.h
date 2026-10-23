#pragma once
#include <array>
#include <algorithm>
#include <cmath>

namespace ir {
constexpr double pi=3.14159265358979323846;
inline float midiHz(float note) { return 440.f*std::pow(2.f,(note-69.f)/12.f); }
inline float hzMidi(float hz) { return 69.f+12.f*std::log2(hz/440.f); }
// Lightweight monophonic guide detector, not a polyphonic transcription model.
class PitchDetector {
public:
    void prepare(double rate) {
        decimation=std::max(1,static_cast<int>(rate/6000.0));
        analysisRate=rate/decimation; count=0; cursor=0; filled=0; hop=0;
        sum=0; frequency=0; history.fill(0);
    }
    float process(float input) {
        sum+=input;
        if(++count<decimation)return frequency;
        history[cursor]=sum/static_cast<float>(decimation); cursor=(cursor+1)%512;
        count=0;sum=0;filled=std::min(512,filled+1);
        if(++hop>=128 && filled==512) {hop=0;analyse();}
        return frequency;
    }
    float getHz() const {return frequency;}
private:
    std::array<float,512> history{};
    std::array<double,192> correlation{};
    int decimation=8,count=0,cursor=0,filled=0,hop=0;
    double analysisRate=6000;
    float sum=0,frequency=0;
    void analyse() {
        double energy=0;
        for(float x:history)energy+=x*x;
        if(energy<0.00005*512) {frequency=0;return;}
        const int lo=std::max(2,static_cast<int>(analysisRate/1000)),hi=std::min(189,static_cast<int>(analysisRate/65));
        double maximum=-1;
        for(int lag=lo;lag<=hi;++lag) {
            double xy=0,xx=0,yy=0;
            for(int j=0;j<512-hi;++j) {
                double a=history[(cursor+j)%512],b=history[(cursor+j+lag)%512];
                xy+=a*b;xx+=a*a;yy+=b*b;
            }
            correlation[lag]=xy/std::sqrt(std::max(1.e-18,xx*yy));
            maximum=std::max(maximum,correlation[lag]);
        }
        if(maximum<0.75) {frequency=0;return;}
        int best=lo;
        for(int lag=lo+1;lag<hi;++lag)
            if(correlation[lag]>0.9*maximum && correlation[lag]>correlation[lag-1] && correlation[lag]>=correlation[lag+1]) {best=lag;break;}
        if(best==lo) {frequency=0;return;}
        double a=correlation[best-1],b=correlation[best],c=correlation[best+1];
        double denom=a-2*b+c;
        double fraction=std::abs(denom)>1.e-9?0.5*(a-c)/denom:0;
        frequency=static_cast<float>(analysisRate/(best+std::clamp(fraction,-0.5,0.5)));
    }
};
// Two overlapping delay grains. Ratios are smoothed by caller; no formant lock.
class GrainPhase {
public:
    double phase=0;
    template<class Read> float process(double ratio,double window,double base,Read&& read) {
        if(std::abs(ratio-1.)<0.00001)return read(base+window*0.5);
        phase+=(1.-ratio)/window;phase-=std::floor(phase);
        double other=phase+0.5;other-=std::floor(other);
        double weight=0.5-0.5*std::cos(2*pi*phase);
        return static_cast<float>(weight*read(base+phase*window)+(1-weight)*read(base+other*window));
    }
};
struct Meter {
    double ll=0,rr=0,lr=0,mid=0;float peakL=0,peakR=0;int count=0;
    void add(float l,float r) {ll+=l*l;rr+=r*r;lr+=l*r;mid+=(l+r)*(l+r)*0.25;peakL=std::max(peakL,std::abs(l));peakR=std::max(peakR,std::abs(r));++count;}
    float correlation() const {return ll*rr>1.e-20?static_cast<float>(lr/std::sqrt(ll*rr)):1.f;}
    float monoRms() const {return count?static_cast<float>(std::sqrt(mid/count)):0.f;}
};
}
