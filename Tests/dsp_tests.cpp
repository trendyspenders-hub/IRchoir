#include "../Source/PitchTools.h"
#include <iostream>
#include <vector>
#include <stdexcept>
void check(bool test,const char* message) {if(!test)throw std::runtime_error(message);}
int main() {
    for(double rate:{44100.,48000.,96000.}) {
        for(float hz:{110.f,220.f,440.f,880.f}) {
            ir::PitchDetector d;d.prepare(rate);
            for(int s=0;s<static_cast<int>(rate*0.3);++s)d.process(0.4f*std::sin(2*ir::pi*hz*s/rate));
            check(std::abs(d.getHz()-hz)<hz*0.015,"Pitch detection frequency error");
            for(int s=0;s<static_cast<int>(rate*0.2);++s)d.process(0);
            check(d.getHz()==0,"Silence must clear detected note");
        }
    }
    // Pitch shift a sine up/down, verify the dominant output frequency by DFT.
    for(double ratio:{0.5,1.,2.}) {
        constexpr double rate=48000,source=220;
        ir::GrainPhase shifter;std::vector<float> result;
        for(int i=0;i<24000;++i) {
            auto sample=shifter.process(ratio,rate/source*8.,100.,[&](double delay) {return static_cast<float>(std::sin(2*ir::pi*source*(i-delay)/rate));});
            check(std::isfinite(sample)&&std::abs(sample)<=1.001,"Invalid granular output");
            if(i>=12000)result.push_back(sample);
        }
        double best=0,bestHz=0;
        for(int hz=80;hz<=500;++hz) {
            double re=0,im=0;
            for(size_t i=0;i<result.size();++i) {double a=2*ir::pi*hz*i/rate;re+=result[i]*std::cos(a);im+=result[i]*std::sin(a);}
            double power=re*re+im*im;if(power>best){best=power;bestHz=hz;}
        }
        std::cout<<"ratio "<<ratio<<" dominant "<<bestHz<<" expected "<<source*ratio<<std::endl;
        check(std::abs(bestHz-source*ratio)<=12,"Pitch shift did not move spectrum to target");
    }
    ir::Meter mono,opposite;
    for(int i=0;i<1000;++i) {float x=std::sin(i*0.1f);mono.add(x,x);opposite.add(x,-x);}
    check(mono.correlation()>0.999,"Mono correlation");
    check(opposite.correlation()<-0.999 && opposite.monoRms()<1.e-6,"Mono cancellation detection");
    std::cout<<"PASS: pitch detection at 3 sample rates, silence reset, granular +/- octave, correlation and mono cancellation\n";
}
