#include <juce_audio_utils/juce_audio_utils.h>
#include <chrono>
#include <iostream>
extern juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    for(int voices:{2,12,32}) {
        std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
        for(auto* parameter:processor->getParameters())
            if(auto* p=dynamic_cast<juce::RangedAudioParameter*>(parameter))
                if(p->getParameterID()=="voices")p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(voices)));
        processor->prepareToPlay(48000,256);
        juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;
        // Warm caches and let gain ramps settle before timing.
        for(int k=0;k<32;++k) {audio.clear();processor->processBlock(audio,midi);}
        auto start=std::chrono::steady_clock::now();
        constexpr int blocks=375; // 2 seconds of audio.
        for(int k=0;k<blocks;++k) {
            for(int s=0;s<256;++s) {
                float x=0.2f*std::sin(static_cast<float>(2*juce::MathConstants<double>::pi*220*(k*256+s)/48000));
                audio.setSample(0,s,x);audio.setSample(1,s,x);
            }
            processor->processBlock(audio,midi);
        }
        auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::cout<<voices<<" layers: "<<elapsed<<" seconds to render 2 seconds; "<<elapsed/2*100<<"% of one core's real-time budget\n";
    }
}
