#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <vector>
#include <cmath>
#include <atomic>
#include "PitchTools.h"
#include "VocalFilePlayer.h"
#include <map>
#include <bitset>

#if defined(IRCHOIR_TESTING)
static juce::File irTestRoot=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("IRChoirSmoke-"+juce::Uuid().toString());
#endif
// Original vocal ensemble DSP. No AI models or sampled voices.
class IRChoir final : public juce::AudioProcessor {
public:
    VocalFilePlayer vocalFile;
    juce::UndoManager undoManager{30000};
    juce::AudioProcessorValueTreeState state;
    static auto parameters() {
        juce::AudioProcessorValueTreeState::ParameterLayout p;
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"mix",1},"Choir mix",0.f,1.f,0.65f));
        p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"voices",1},"Voices",2,32,12));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"spread",1},"Stereo width",0.f,1.f,0.8f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"movement",1},"Detune movement",0.f,1.f,0.4f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gain",1},"Output dB",-24.f,6.f,-3.f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"room",1},"Room",0.f,1.f,0.18f));
        auto add=[&](const char* id,const char* name,float lo,float hi,float def) {
            p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,lo,hi,def));
        };
        add("timing","Timing looseness",0,1,0.3f); add("drift","Pitch drift cents",0,25,5);
        add("vibrato","Vibrato cents",0,40,3); add("intensity","Conductor intensity",0,1,0.7f);
        add("space","Conductor space",0,1,0.5f); add("timbre","Synthetic character amount",0,1,0.25f);
        add("low","Low EQ dB",-12,12,0); add("high","High EQ dB",-12,12,0);
        add("compression","Compression",0,1,0); add("drive","Saturation",0,1,0);
        add("delay","Delay mix",0,0.7f,0); add("delayTime","Delay ms",50,1000,250);
        add("feedback","Delay feedback",0,0.8f,0.25f);
        add("revPredelay","Reverb pre-delay ms",0,200,12);
        add("revDecay","Reverb decay shape",0,1,0.5f);add("revWidth","Reverb width",0,1,1);
        add("revDamping","Reverb damping",0,1,0.55f);
        add("revLowCut","Reverb low cut Hz",20,800,120);add("revHighCut","Reverb high cut Hz",1000,18000,8000);
        const char* revIds[]={"reverbOn","reverbMonoSend","reverbLowCutOn","reverbHighCutOn","reverbFreeze","reverbWetOnly"};
        const char* revNames[]={"Reverb on","Mono send","Low cut on","High cut on","Freeze","Wet only"};
        for(int j=0;j<6;++j)p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{revIds[j],1},revNames[j],j==0));
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"mono",1},"Mono audition",false));
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"harmony",1},"Harmony mode",juce::StringArray{"Unison","Four parts (relative)","MIDI note targets"},0));
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"roomType",1},"Room type",juce::StringArray{"Studio","Hall","Cathedral","Chamber","Plate-style","Tunnel-style"},1));
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"formation",1},"Formation",juce::StringArray{"Semicircle","Cluster","Wide rows"},2));
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"animation",1},"Animation",juce::StringArray{"Full","Subtle","Reduced motion"},1));
        const char* partNames[]={"Bass","Tenor","Alto","Soprano"};
        const int offsets[]={-12,-5,0,7};
        for(int i=0;i<4;++i) {
            auto id=juce::String(i);
            p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"partLevel"+id,1},juce::String(partNames[i])+" level",0.f,1.f,1.f));
            p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"interval"+id,1},juce::String(partNames[i])+" semitones",-12,12,offsets[i]));
        }
        for (int i=0;i<32;++i) {
            auto id=juce::String(i);
            p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"pan"+id,1},"Singer "+juce::String(i+1)+" pan",-1.f,1.f,i<12?static_cast<float>(i/4)-1.f:2.f*(i/4)/7.f-1.f));
            p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"level"+id,1},"Singer "+juce::String(i+1)+" level",0.f,1.f,0.8f));
            p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"mute"+id,1},"Singer "+juce::String(i+1)+" mute",false));
            p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"solo"+id,1},"Singer "+juce::String(i+1)+" solo",false));
            p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"character"+id,1},"Singer "+juce::String(i+1)+" synthetic character",juce::StringArray{"Warm","Airy","Dark","Bright"},i%4));
        }
        return p;
    }
    IRChoir() : AudioProcessor(BusesProperties().withInput("Vocal",juce::AudioChannelSet::stereo(),true).withOutput("Choir",juce::AudioChannelSet::stereo(),true)),state(*this,&undoManager,"IRChoir",parameters()) {
        for(int i=0;i<32;++i) {
            auto id=juce::String(i);
            pans[i]=state.getRawParameterValue("pan"+id); levels[i]=state.getRawParameterValue("level"+id);
            mutes[i]=state.getRawParameterValue("mute"+id); solos[i]=state.getRawParameterValue("solo"+id);
            characters[i]=state.getRawParameterValue("character"+id);
        }
        for(int i=0;i<4;++i) {partLevels[i]=state.getRawParameterValue("partLevel"+juce::String(i));intervals[i]=state.getRawParameterValue("interval"+juce::String(i));}
    }
    const juce::String getName() const override { return "I.R.Choir"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 45.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Vocal Ensemble"; }
    void changeProgramName(int,const juce::String&) override {}
    bool isBusesLayoutSupported(const BusesLayout& b) const override {
        auto c=b.getMainOutputChannelSet();
        return (c==juce::AudioChannelSet::mono() || c==juce::AudioChannelSet::stereo()) && c==b.getMainInputChannelSet();
    }
    void prepareToPlay(double rate,int) override {
        sr=rate; ring.assign(static_cast<size_t>(rate*0.5)+4,0.f); write=0;
        for(auto& d:delayBuffer)d.assign(static_cast<size_t>(rate*1.1)+4,0.f);
        for(auto& d:reverbDelay)d.assign(static_cast<size_t>(rate*0.21)+4,0.f);
        reverbWrite=0;reverbLow.fill(0);reverbHigh.fill(0);
        const float revInit[]={raw("reverbOn")*raw("room"),raw("reverbWetOnly")*raw("reverbOn"),raw("revPredelay")};
        for(size_t j=0;j<revSmooth.size();++j){revSmooth[j].reset(rate,0.04);revSmooth[j].setCurrentAndTargetValue(revInit[j]);}
        delayWrite=0;controlCounter=0; detector.prepare(rate); held.fill(false); sustain.fill(false); sustained.fill(false);
        reverb.setSampleRate(rate); reverb.reset();
        voiceLow.fill(0); eqLow.fill(0); eqHigh.fill(0); compressorEnv=0;
        for(int i=0;i<32;++i) {
            phase[i]=i*0.37; grains[i].phase=i/32.; ratios[i]=1;ratioTargets[i]=1;
            voiceGain[i].reset(rate,0.02); voicePan[i].reset(rate,0.02); voiceGate[i].reset(rate,0.01);voiceGate[i].setCurrentAndTargetValue(1.f);
            voiceGain[i].setCurrentAndTargetValue(0.f); voicePan[i].setCurrentAndTargetValue(*pans[i]);
            meters[i].store(0.f);
        }
        mix.reset(rate,0.03); width.reset(rate,0.03); motion.reset(rate,0.03); gain.reset(rate,0.03); normalization.reset(rate,0.03);
        const float fxInitial[]={juce::Decibels::decibelsToGain(raw("low")),juce::Decibels::decibelsToGain(raw("high")),raw("drive"),raw("compression"),raw("delay"),raw("feedback"),raw("delayTime")};
        for(size_t i=0;i<fxSmoothing.size();++i) {fxSmoothing[i].reset(rate,i==6?0.08:0.02);fxSmoothing[i].setCurrentAndTargetValue(fxInitial[i]);}
        intensitySmoothing.reset(rate,0.02);intensitySmoothing.setCurrentAndTargetValue(raw("intensity"));
        timbreSmoothing.reset(rate,0.02);timbreSmoothing.setCurrentAndTargetValue(raw("timbre"));
        normalization.setCurrentAndTargetValue(1.f/state.getRawParameterValue("voices")->load());
        mix.setCurrentAndTargetValue(*state.getRawParameterValue("mix"));
        width.setCurrentAndTargetValue(*state.getRawParameterValue("spread"));
        motion.setCurrentAndTargetValue(*state.getRawParameterValue("movement"));
        gain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(state.getRawParameterValue("gain")->load()));
    }
    void releaseResources() override {}
    void reset() override {
        std::fill(ring.begin(),ring.end(),0.f);
        for(auto& d:delayBuffer)std::fill(d.begin(),d.end(),0.f);
        for(auto& d:reverbDelay)std::fill(d.begin(),d.end(),0.f);
        reverbWrite=0;reverbLow.fill(0);reverbHigh.fill(0);
        reverb.reset(); held.fill(false);sustained.fill(false);sustain.fill(false);
        detector.prepare(sr);voiceLow.fill(0);eqLow.fill(0);eqHigh.fill(0);compressorEnv=0;
        write=0;delayWrite=0;ratios.fill(1.);for(auto& grain:grains)grain.phase=0;
        for(auto& meter:meters)meter.store(0);
        outputL.store(0);outputR.store(0);clipped.store(false);
    }
    void midiEvent(const juce::MidiMessage& m) {
        int channel=juce::jlimit(0,15,m.getChannel()-1);
        int note=juce::jlimit(0,127,m.getNoteNumber());int key=channel*128+note;
        if(m.isNoteOn()) {held[key]=true;sustained[key]=false;}
        if(m.isNoteOff()) {held[key]=false;sustained[key]=sustain[channel];}
        if(m.isSustainPedalOn())sustain[channel]=true;
        if(m.isSustainPedalOff()) {sustain[channel]=false;for(int i=0;i<128;++i)sustained[channel*128+i]=false;}
        if(m.isAllNotesOff() || m.isAllSoundOff()) {
            for(int i=0;i<128;++i) {held[channel*128+i]=false;sustained[channel*128+i]=false;}
            sustain[channel]=false;
        }
    }
    std::array<float,4> midiTargets() const {
        std::array<int,128> notes{};int size=0;
        for(int note=0;note<128;++note) {
            bool active=false;
            for(int ch=0;ch<16;++ch)active=active || held[ch*128+note] || sustained[ch*128+note];
            if(active)notes[size++]=note;
        }
        std::array<float,4> result{{-1,-1,-1,-1}};
        if(size)for(int i=0;i<4;++i)result[i]=static_cast<float>(notes[std::min(i,size-1)]);
        return result;
    }
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& midi) override {
        juce::ScopedNoDenormals guard;
        const int channels=b.getNumChannels();
        if(ring.empty() || channels<1) {b.clear();return;}
        vocalFile.render(b,sr);
        mix.setTargetValue(raw("mix"));width.setTargetValue(raw("spread"));motion.setTargetValue(raw("movement"));
        gain.setTargetValue(juce::Decibels::decibelsToGain(raw("gain")));
        const int n=juce::jlimit(2,32,static_cast<int>(raw("voices")));
        normalization.setTargetValue(1.f/static_cast<float>(n));
        bool anySolo=false;
        for(int v=0;v<n;++v)anySolo=anySolo || solos[v]->load()>0.5f;
        std::array<float,4> partLevel{},interval{};
        for(int j=0;j<4;++j) {partLevel[j]=partLevels[j]->load();interval[j]=intervals[j]->load();}
        std::array<float,32> colorAmount{};
        const float cutoff[]={2400,4500,1100,7000};
        for(int v=0;v<32;++v) {
            bool audible=v<n && mutes[v]->load()<0.5f && (!anySolo || solos[v]->load()>0.5f);
            voiceGain[v].setTargetValue(audible?levels[v]->load()*partLevel[v%4]:0.f);
            voicePan[v].setTargetValue(pans[v]->load());
            int c=juce::jlimit(0,3,static_cast<int>(characters[v]->load()));
            colorAmount[v]=1.f-std::exp(static_cast<float>(-2*ir::pi*cutoff[c]/sr));
        }
        int processingVoices=n;
        for(int v=n;v<32;++v)if(voiceGain[v].isSmoothing() || voiceGain[v].getCurrentValue()>0.000001f)processingVoices=v+1;
        const float timing=raw("timing"),drift=raw("drift"),vibrato=raw("vibrato"),intensity=raw("intensity"),space=raw("space"),timbre=raw("timbre");
        intensitySmoothing.setTargetValue(intensity);timbreSmoothing.setTargetValue(timbre);
        const int harmony=static_cast<int>(raw("harmony"));
        std::array<float,32> peaks{};
        auto midiIterator=midi.cbegin();auto targets=midiTargets();
        const auto length=ring.size();
        for(int s=0;s<b.getNumSamples();++s) {
            while(midiIterator!=midi.cend() && (*midiIterator).samplePosition<=s) {midiEvent((*midiIterator).getMessage());++midiIterator;targets=midiTargets();}
            float l=b.getSample(0,s),r=channels>1?b.getSample(1,s):l;
            if(!std::isfinite(l))l=0;
            if(!std::isfinite(r))r=0;
            ring[write]=(l+r)*0.5f;
            const float f0=detector.process((l+r)*0.5f);
            const double window=f0>65?std::clamp(sr/f0*8.,sr*0.012,sr*0.13):sr*0.05;
            float wetL=0,wetR=0;
            float intensityNow=intensitySmoothing.getNextValue(),timbreNow=timbreSmoothing.getNextValue();
            const float m=mix.getNextValue(),w=width.getNextValue(),d=motion.getNextValue()*(0.5f+intensityNow);
            for(int v=0;v<processingVoices;++v) {
                const double delay=sr*(0.012+(v%8)*0.0015*timing+0.002*d*std::sin(phase[v]));
                // Pitch modulation at a 16-sample control rate, smoothly interpolated.
                if((controlCounter&15u)==0) {
                    float semitones=0;
                    if(harmony==1)semitones=interval[v%4];
                    if(harmony==2 && targets[v%4]>=0 && f0>65)semitones=std::clamp(targets[v%4]-ir::hzMidi(f0),-12.f,12.f);
                    float cents=drift*std::sin(phase[v]*0.71)+vibrato*std::sin(phase[v]*(12.+v*0.07));
                    ratioTargets[v]=std::pow(2.,(semitones+cents/100.)/12.);
                }
                ratios[v]+=0.001*(ratioTargets[v]-ratios[v]);
                auto read=[&](double distance) {
                    double pos=static_cast<double>(write)-distance;
                    while(pos<0)pos+=static_cast<double>(length);
                    auto a=static_cast<size_t>(pos)%length,z=(a+1)%length;
                    float frac=static_cast<float>(pos-std::floor(pos));return ring[a]+frac*(ring[z]-ring[a]);
                };
                float sample=grains[v].process(ratios[v],window,delay,read);
                voiceLow[v]+=colorAmount[v]*(sample-voiceLow[v]);
                int character=juce::jlimit(0,3,static_cast<int>(characters[v]->load()));
                float colored=character==1 || character==3 ? sample+0.5f*(sample-voiceLow[v]) : voiceLow[v];
                sample+=(colored-sample)*timbreNow;
                // MIDI mode is a gated effect: no held notes means no wet voices.
                bool gate=harmony!=2 || (targets[v%4]>=0 && f0>65);
                voiceGate[v].setTargetValue(gate?1.f:0.f);
                sample*=voiceGain[v].getNextValue()*(0.3f+intensityNow)*voiceGate[v].getNextValue();
                peaks[v]=juce::jmax(peaks[v],std::abs(sample));
                const float pan=w*voicePan[v].getNextValue();
                wetL+=sample*(1.f-pan);wetR+=sample*(1.f+pan);
                phase[v]+=juce::MathConstants<double>::twoPi*(0.17+v*0.031)/sr;
                if(phase[v]>1000*ir::pi)phase[v]-=1000*ir::pi;
            }
            float norm=normalization.getNextValue();
            b.setSample(0,s,(1-m)*l+m*(channels>1?wetL:0.5f*(wetL+wetR))*norm);
            if(channels>1)b.setSample(1,s,(1-m)*r+m*wetR*norm);
            write=(write+1)%length;++controlCounter;
        }
        // Master FX: EQ -> saturation -> linked compressor -> delay -> room -> output.
        float lowGain=juce::Decibels::decibelsToGain(raw("low")),highGain=juce::Decibels::decibelsToGain(raw("high"));
        float drive=raw("drive"),compression=raw("compression"),delayMix=raw("delay"),feedback=raw("feedback");
        const float fxTargets[]={lowGain,highGain,drive,compression,delayMix,feedback,raw("delayTime")};
        for(size_t i=0;i<fxSmoothing.size();++i)fxSmoothing[i].setTargetValue(fxTargets[i]);
        const float aLow=1-std::exp(static_cast<float>(-2*ir::pi*250/sr)),aHigh=1-std::exp(static_cast<float>(-2*ir::pi*3500/sr));
        for(int s=0;s<b.getNumSamples();++s) {
            lowGain=fxSmoothing[0].getNextValue();highGain=fxSmoothing[1].getNextValue();drive=fxSmoothing[2].getNextValue();
            compression=fxSmoothing[3].getNextValue();delayMix=fxSmoothing[4].getNextValue();feedback=fxSmoothing[5].getNextValue();
            double delaySamples=sr*fxSmoothing[6].getNextValue()/1000.;
            float x[2]={b.getSample(0,s),channels>1?b.getSample(1,s):b.getSample(0,s)};
            for(int ch=0;ch<2;++ch) {
                eqLow[ch]+=aLow*(x[ch]-eqLow[ch]);eqHigh[ch]+=aHigh*(x[ch]-eqHigh[ch]);
                x[ch]+=(lowGain-1)*eqLow[ch]+(highGain-1)*(x[ch]-eqHigh[ch]);
                if(drive>0.0001f) {float amount=1+drive*6;x[ch]=std::tanh(x[ch]*amount)/std::tanh(amount);}
            }
            float peak=std::max(std::abs(x[0]),std::abs(x[1]));
            float coeff=static_cast<float>(1-std::exp(-1./(sr*(peak>compressorEnv?0.005:0.1))));
            compressorEnv+=coeff*(peak-compressorEnv);
            float reduction=compressorEnv>0.25f?std::pow(0.25f/compressorEnv,compression*0.75f):1;
            for(int ch=0;ch<2;++ch) {
                x[ch]*=reduction;
                auto& db=delayBuffer[ch];double read=static_cast<double>(delayWrite)-delaySamples;
                if(read<0)read+=static_cast<double>(db.size());
                auto index=static_cast<size_t>(read)%db.size();float fraction=static_cast<float>(read-std::floor(read));
                float echo=db[index]+fraction*(db[(index+1)%db.size()]-db[index]);
                db[delayWrite]=x[ch]+feedback*echo;x[ch]+=delayMix*echo;
            }
            b.setSample(0,s,x[0]);if(channels>1)b.setSample(1,s,x[1]);
            delayWrite=(delayWrite+1)%delayBuffer[0].size();
        }
        // Algorithmic rooms: independent send path, no allocations in processBlock.
        juce::Reverb::Parameters rp;
        const float roomSizes[]={0.25f,0.70f,0.92f,0.48f,0.60f,0.84f};
        int roomType=juce::jlimit(0,5,static_cast<int>(raw("roomType")));
        rp.roomSize=juce::jlimit(0.f,0.98f,roomSizes[roomType]+(raw("revDecay")-0.5f)*0.3f);
        rp.damping=raw("revDamping");rp.wetLevel=1;rp.dryLevel=0;
        rp.width=raw("revWidth");rp.freezeMode=raw("reverbFreeze");reverb.setParameters(rp);
        const bool enabled=raw("reverbOn")>0.5f,monoSend=raw("reverbMonoSend")>0.5f;
        const bool lowCut=raw("reverbLowCutOn")>0.5f,highCut=raw("reverbHighCutOn")>0.5f;
        float lowA=1.f-std::exp(-2.f*static_cast<float>(ir::pi)*raw("revLowCut")/static_cast<float>(sr));
        float highA=1.f-std::exp(-2.f*static_cast<float>(ir::pi)*raw("revHighCut")/static_cast<float>(sr));
        revSmooth[0].setTargetValue(enabled?raw("room"):0.f);
        revSmooth[1].setTargetValue(enabled?raw("reverbWetOnly"):0.f);
        // Additional spacing differentiates the tunnel-style algorithmic preset.
        revSmooth[2].setTargetValue(raw("revPredelay")+(roomType==5?9.f:0.f));
        for(int sample=0;sample<b.getNumSamples();++sample) {
            float dryL=b.getSample(0,sample),dryR=channels>1?b.getSample(1,sample):dryL;
            float send[2]={dryL,dryR};if(monoSend)send[0]=send[1]=(dryL+dryR)*0.5f;
            double delaySamples=juce::jlimit(0.,static_cast<double>(reverbDelay[0].size()-2),revSmooth[2].getNextValue()*sr*0.001);
            for(int ch=0;ch<2;++ch) {
                reverbLow[ch]+=lowA*(send[ch]-reverbLow[ch]);
                float filtered=lowCut?send[ch]-reverbLow[ch]:send[ch];
                reverbHigh[ch]+=highA*(filtered-reverbHigh[ch]);
                reverbDelay[ch][reverbWrite]=highCut?reverbHigh[ch]:filtered;
                double read=static_cast<double>(reverbWrite)-delaySamples;
                while(read<0)read+=static_cast<double>(reverbDelay[ch].size());
                size_t r0=static_cast<size_t>(read),r1=(r0+1)%reverbDelay[ch].size();
                float fraction=static_cast<float>(read-static_cast<double>(r0));
                send[ch]=reverbDelay[ch][r0]+fraction*(reverbDelay[ch][r1]-reverbDelay[ch][r0]);
            }
            reverbWrite=(reverbWrite+1)%reverbDelay[0].size();
            if(channels>1)reverb.processStereo(&send[0],&send[1],1);else reverb.processMono(&send[0],1);
            float wet=revSmooth[0].getNextValue()*(0.2f+space*0.4f),dry=1.f-revSmooth[1].getNextValue();
            b.setSample(0,sample,dryL*dry+send[0]*wet);
            if(channels>1)b.setSample(1,sample,dryR*dry+send[1]*wet);
        }
        ir::Meter meter;bool mono=raw("mono")>0.5f;
        for(int s=0;s<b.getNumSamples();++s) {
            float g=gain.getNextValue(),l=b.getSample(0,s)*g,r=channels>1?b.getSample(1,s)*g:l;
            if(!std::isfinite(l))l=0;
            if(!std::isfinite(r))r=0;
            meter.add(l,r); // Pre-audition correlation reveals stereo cancellation.
            if(mono)l=r=(l+r)*0.5f;
            b.setSample(0,s,l);if(channels>1)b.setSample(1,s,r);
        }
        outputL.store(meter.peakL);outputR.store(meter.peakR);correlation.store(meter.correlation());monoRms.store(meter.monoRms());
        if(meter.peakL>1 || meter.peakR>1)clipped.store(true);
        detectedHz.store(detector.getHz());
        for(int v=0;v<32;++v)meters[v].store(peaks[v],std::memory_order_relaxed);
        midi.clear();
    }
    std::array<std::atomic<float>,32> meters{};
    std::atomic<float> outputL{0},outputR{0},correlation{1},monoRms{0},detectedHz{0};
    std::atomic<bool> clipped{false};
    // Message-thread A/B snapshots survive editor closure; current state recalls in DAW.
    juce::ValueTree snapshotA,snapshotB;
    bool usingB=false;
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    void getStateInformation(juce::MemoryBlock& out) override { auto tree=state.copyState();tree.setProperty("vocalFilePath",vocalFile.path(),nullptr);tree.setProperty("vocalFileEnabled",vocalFile.enabled.load(),nullptr);tree.setProperty("vocalFileLoop",vocalFile.loop.load(),nullptr);auto xml=tree.createXml(); copyXmlToBinary(*xml,out); }
    void setStateInformation(const void* data,int size) override { auto xml=getXmlFromBinary(data,size); if(xml && xml->hasTagName(state.state.getType())) {
        auto tree=juce::ValueTree::fromXml(*xml);state.replaceState(tree);
        vocalFile.stop();vocalFile.loop.store(static_cast<bool>(tree.getProperty("vocalFileLoop",false)));
        auto path=tree.getProperty("vocalFilePath").toString();
        if(path.isNotEmpty())vocalFile.loadFile(juce::File(path),static_cast<bool>(tree.getProperty("vocalFileEnabled",false)));
        vocalFile.enabled.store(static_cast<bool>(tree.getProperty("vocalFileEnabled",false)));
    } }
private:
    float raw(const juce::String& id) const {return state.getRawParameterValue(id)->load();}
    double sr=44100;
    std::vector<float> ring;
    std::array<std::vector<float>,2> delayBuffer;
    size_t write=0,delayWrite=0,reverbWrite=0;
    std::array<std::vector<float>,2> reverbDelay;
    std::array<float,2> reverbLow{},reverbHigh{};
    std::array<juce::SmoothedValue<float>,3> revSmooth;
    std::array<double,32> phase{},ratios{},ratioTargets{};
    uint64_t controlCounter=0;
    std::array<ir::GrainPhase,32> grains;
    ir::PitchDetector detector;
    std::array<bool,2048> held{},sustained{};
    std::array<bool,16> sustain{};
    std::array<float,32> voiceLow{};
    std::array<float,2> eqLow{},eqHigh{};
    float compressorEnv=0;
    std::array<std::atomic<float>*,4> partLevels{},intervals{};
    std::array<std::atomic<float>*,32> pans{},levels{},mutes{},solos{},characters{};
    std::array<juce::SmoothedValue<float>,32> voiceGain,voicePan,voiceGate;
    std::array<juce::SmoothedValue<float>,7> fxSmoothing;
    juce::SmoothedValue<float> intensitySmoothing,timbreSmoothing;
    juce::Reverb reverb;
    juce::SmoothedValue<float> mix,width,motion,gain,normalization;
};


class ChoirEditor final : public juce::AudioProcessorEditor, private juce::Timer, public juce::FileDragAndDropTarget {
    using Attachment=juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment=juce::AudioProcessorValueTreeState::ButtonAttachment;
public:
    explicit ChoirEditor(IRChoir& p):AudioProcessorEditor(p),processor(p) {
        setSize(1180,840);
        setResizable(true,true); setResizeLimits(1180,840,1600,1200);
        const char* ids[]={"voices","mix","spread","movement","room","gain"};
        const char* names[]={"ENSEMBLE SIZE","CHOIR MIX","STEREO WIDTH","HUMAN MOTION","ROOM","OUTPUT dB"};
        for(int i=0;i<6;++i) {
            addAndMakeVisible(knobs[i]); addAndMakeVisible(labels[i]);
            knobs[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,24);
            knobs[i].setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xffedbb79));
            knobs[i].setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
            knobs[i].setTitle(names[i]);
            labels[i].setText(names[i],juce::dontSendNotification);
            labels[i].setJustificationType(juce::Justification::centred);
            labels[i].setColour(juce::Label::textColourId,juce::Colour(0xffa8b0c7));
            attachments[i]=std::make_unique<Attachment>(processor.state,ids[i],knobs[i]);
        }
        addAndMakeVisible(presets);
        presets.addItem("Intimate / 4 voices",1); presets.addItem("Wide / 12 voices",2);
        presets.addItem("Cloud / 24 voices",3); presets.addItem("Massive / 32 voices",4);
        presets.addSeparator();
        presets.addItem("FX / Tight Doubles",5); presets.addItem("FX / Wide Backing",6);
        presets.addItem("FX / Dream Haze",7); presets.addItem("FX / Cathedral",8);
        presets.addItem("FX / Crowd Wash",9);
        presets.setTextWhenNothingSelected("Ensemble presets");
        presets.onChange=[this] { applyPreset(presets.getSelectedId()); };
        addAndMakeVisible(selectedLabel); selectedLabel.setColour(juce::Label::textColourId,juce::Colours::white);
        for(auto* slider:{&singerPan,&singerLevel}) {
            addAndMakeVisible(*slider); slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,65,24);
            slider->setColour(juce::Slider::trackColourId,juce::Colour(0xffedbb79));
        }
        singerPan.setTitle("Selected singer pan"); singerLevel.setTitle("Selected singer level");
        addAndMakeVisible(mute); addAndMakeVisible(solo);
        mute.setButtonText("Mute"); solo.setButtonText("Solo");
        mute.setClickingTogglesState(true); solo.setClickingTogglesState(true);
        addAndMakeVisible(reset); reset.setButtonText("Reset stage");
        reset.onClick=[this] {
            applyFormation(static_cast<int>(value("formation"))+1);
            for(int i=0;i<32;++i) {auto id=juce::String(i);set("mute"+id,0);set("solo"+id,0);}
        };
        setupExtras();
        selectedSet.set(0);select(0); startTimerHz(30);
        resized();
    }
    void resized() override {
        presets.setBounds(350,25,260,30);
        saveName.setBounds(620,25,170,30);savePreset.setBounds(795,25,65,30);
        favourite.setBounds(865,25,65,30);favOnly.setBounds(935,25,90,30);
        ab.setBounds(1030,25,60,30);copyAB.setBounds(1095,25,65,30);
        for(int j=0;j<5;++j)groups[j].setBounds(30+j*110,93,103,28);
        formation.setBounds(600,93,190,28);view.setBounds(800,93,200,28);
        animationBox.setBounds(1010,93,150,28);
        int fxY=getHeight()-250;
        for(size_t i=0;i<extraSliders.size();++i) {
            auto& c=extraSliders[i];bool visible=c.page==view.getSelectedId();c.slider->setVisible(visible);c.label->setVisible(visible);
            c.label->setBounds(30+c.column*185,fxY,180,22);
            c.slider->setBounds(30+c.column*185,fxY+24,180,130);
        }
        harmonyBox.setBounds(30,fxY+164,280,28);roomBox.setBounds(325,fxY+164,180,28);
        monoButton.setBounds(520,fxY+164,140,28);clearClip.setBounds(675,fxY+164,140,28);
        undoButton.setBounds(835,fxY+164,75,28);redoButton.setBounds(920,fxY+164,75,28);
        const bool reverbPage=view.getSelectedId()==7;
        harmonyBox.setVisible(!reverbPage);monoButton.setVisible(!reverbPage);clearClip.setVisible(!reverbPage);
        for(int j=0;j<6;++j) {reverbButtons[j].setVisible(reverbPage);reverbButtons[j].setBounds(230+j*100,fxY+164,95,28);}
        if(reverbPage)roomBox.setBounds(30,fxY+164,190,28);
        loadVocal.setBounds(30,163,105,25);playVocal.setBounds(140,163,70,25);
        loopVocal.setBounds(215,163,65,25);fileInput.setBounds(285,163,110,25);
        fileLabel.setBounds(405,163,getWidth()-720,25);
        helpLabel.setBounds(30,getHeight()-54,getWidth()-60,24);
        for(int i=0;i<6;++i) {knobs[i].setVisible(view.getSelectedId()==1);labels[i].setVisible(view.getSelectedId()==1);}
        const int bottom=getHeight()-250, cell=(getWidth()-60)/6;
        for(int i=0;i<6;++i) {
            labels[i].setBounds(30+i*cell,bottom,cell,22);
            knobs[i].setBounds(30+i*cell,bottom+23,cell,130);
        }
        const int inspector=bottom-68;
        selectedLabel.setBounds(30,inspector,135,25);
        singerPan.setBounds(165,inspector,205,28);
        singerLevel.setBounds(385,inspector,205,28);
        mute.setBounds(605,inspector,75,28); solo.setBounds(690,inspector,75,28);
        characterBox.setBounds(780,inspector,190,28);
        reset.setBounds(getWidth()-180,inspector,150,28);
        groupPan.setBounds(165,inspector+35,205,28);groupLevel.setBounds(385,inspector+35,205,28);
        groupMute.setBounds(605,inspector+35,160,28);groupSolo.setBounds(780,inspector+35,190,28);
    }
    void paint(juce::Graphics& g) override {
        const juce::Colour violet(0xffedbb79), ink(0xff0a101d);
        g.fillAll(ink);
        g.setColour(juce::Colour(0xffe7e9ff)); g.setFont(30.f);
        g.drawText("I.R.Choir",30,20,280,40,juce::Justification::centredLeft);
        g.setFont(12.f); g.setColour(juce::Colour(0xffa8b0c7));
        g.drawText("THE ENSEMBLE IS YOUR INSTRUMENT",31,64,450,20,juce::Justification::centredLeft);
        auto st=stage();
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff242343),st.getCentreX(),st.getY(),ink,st.getCentreX(),st.getBottom(),false));
        g.fillRoundedRectangle(st.expanded(12.f,35.f),18.f);
        for(int row=0;row<4;++row) {
            float y=st.getY()+35+row*(st.getHeight()-70)/3;
            g.setColour(juce::Colour(0xff34364e));
            if(static_cast<int>(value("formation"))==2)g.drawHorizontalLine(static_cast<int>(y+20),st.getX(),st.getRight());
            g.setColour(juce::Colour(0xff858ca6)); g.setFont(11.f);
            g.drawText(juce::String(partNames[row]).substring(0,1),12,static_cast<int>(y-5),20,16,juce::Justification::centred);
        }
        paintConductorAndMeters(g);
        const int n=static_cast<int>(value("voices"));
        for(int i=0;i<n;++i) {
            auto pos=position(i); auto id=juce::String(i);
            bool muted=value("mute"+id)>0.5f, isSolo=value("solo"+id)>0.5f;
            float activity=juce::jlimit(0.f,1.f,displayMeters[i]*3.f);
            int animation=static_cast<int>(value("animation"));
            if(animation==2)activity=0;else if(animation==1)activity*=0.35f;
            auto color=muted?juce::Colour(0xff596078):(isSolo?juce::Colour(0xff6fe3c4):violet);
            if(activity>0.015f) {
                g.setColour(color.withAlpha(activity*0.23f));
                g.fillEllipse(pos.x-25-activity*10,pos.y-28-activity*10,50+activity*20,64+activity*20);
            }
            if(selectedSet.test(static_cast<size_t>(i))) {
                g.setColour(juce::Colours::white.withAlpha(0.85f));
                g.drawRoundedRectangle(pos.x-18,pos.y-23,36,50,10.f,1.4f);
            }
            paintSinger(g,pos,i,color,activity,muted);
            g.setColour(color); g.setFont(11.f);
            g.drawText(juce::String(i+1)+(muted?" M":(isSolo?" S":"")),static_cast<int>(pos.x-21),static_cast<int>(pos.y+30),42,16,juce::Justification::centred);
        }
        g.setColour(juce::Colour(0xffadb4cd)); g.setFont(12.f);
        g.drawText("SHIFT-CLICK: GROUP     DRAG: PAN + LEVEL     DOUBLE-CLICK: MUTE",30,137,getWidth()-320,20,juce::Justification::centred);
        g.drawText("LEFT",static_cast<int>(st.getX()),static_cast<int>(st.getY()+2),100,20,juce::Justification::left);
        g.drawText("RIGHT",static_cast<int>(st.getRight()-100),static_cast<int>(st.getY()+2),100,20,juce::Justification::right);
        g.setColour(juce::Colour(0xff818ba5)); g.setFont(11.f);
        g.drawText("VOCAL ENSEMBLE EFFECT  /  AUDIO-REACTIVE STAGE  /  v0.3 SOURCE PROTOTYPE / SYNTHETIC VOICE COLORS",30,getHeight()-23,getWidth()-60,17,juce::Justification::centred);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        endDrag();processor.undoManager.beginNewTransaction("Stage edit");
        if(pad().contains(e.position)) {
            padDragging=true;
            processor.state.getParameter("intensity")->beginChangeGesture();processor.state.getParameter("space")->beginChangeGesture();
            dragPad(e.position);return;
        }
        int hit=hitSinger(e.position); if(hit<0) {selectedSet.reset();repaint();return;}
        if(e.mods.isShiftDown())selectedSet.flip(static_cast<size_t>(hit));
        else if(!selectedSet.test(static_cast<size_t>(hit))) {selectedSet.reset();selectedSet.set(static_cast<size_t>(hit));}
        if(selectedSet.none())selectedSet.set(static_cast<size_t>(hit));
        select(hit);dragging=true;dragOrigin=e.position;
        for(int i=0;i<32;++i)if(selectedSet.test(static_cast<size_t>(i))) {
            dragPan[i]=value("pan"+juce::String(i));dragLevel[i]=value("level"+juce::String(i));
            processor.state.getParameter("pan"+juce::String(i))->beginChangeGesture();processor.state.getParameter("level"+juce::String(i))->beginChangeGesture();
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if(padDragging) {dragPad(e.position);return;}
        if(!dragging)return;
        auto delta=e.position-dragOrigin;
        for(int i=0;i<32;++i)if(selectedSet.test(static_cast<size_t>(i))) {
            setRaw("pan"+juce::String(i),juce::jlimit(-1.f,1.f,dragPan[i]+2*delta.x/(stage().getWidth()-70)));
            setRaw("level"+juce::String(i),juce::jlimit(0.f,1.f,dragLevel[i]-delta.y/80.f));
        }
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override { endDrag(); }
    void mouseDoubleClick(const juce::MouseEvent& e) override {
        int hit=hitSinger(e.position); if(hit>=0) { selectedSet.reset();selectedSet.set(static_cast<size_t>(hit));select(hit); auto id="mute"+juce::String(hit); set(id,value(id)>0.5f?0.f:1.f); }
    }
    ~ChoirEditor() override { endDrag(); }
    bool isInterestedInFileDrag(const juce::StringArray& files) override {
        return files.size()==1 && juce::File(files[0]).hasFileExtension("wav;aif;aiff;flac");
    }
    void filesDropped(const juce::StringArray& files,int,int) override {
        if(isInterestedInFileDrag(files))processor.vocalFile.loadFile(juce::File(files[0]));
    }
private:
    IRChoir& processor;
    std::array<juce::Slider,6> knobs;
    std::array<juce::Label,6> labels;
    std::array<std::unique_ptr<Attachment>,6> attachments;
    juce::Slider singerPan,singerLevel;
    juce::Label selectedLabel;
    juce::TextButton mute,solo,reset;
    juce::ComboBox presets;
    std::unique_ptr<Attachment> panAttachment,levelAttachment;
    std::unique_ptr<ButtonAttachment> muteAttachment,soloAttachment;
    std::array<float,32> displayMeters{};
    int selected=0; bool dragging=false,padDragging=false;
    const char* partNames[4]={"BASS","TENOR","ALTO","SOPRANO"};
    std::bitset<32> selectedSet;
    std::array<float,32> dragPan{},dragLevel{};
    juce::Point<float> dragOrigin;
    struct Extra {
        int page=0,column=0;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<Attachment> attachment;
    };
    std::vector<Extra> extraSliders;
    std::array<juce::TextButton,5> groups;
    float lastGroupPan=0;
    juce::ComboBox formation,view,animationBox,harmonyBox,roomBox,characterBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> animationAttachment,harmonyAttachment,roomAttachment,characterAttachment;
    juce::Slider groupPan,groupLevel;
    juce::TooltipWindow tooltips{this,500};
    juce::TextButton undoButton,redoButton;
    juce::TextButton groupMute,groupSolo,savePreset,favourite,favOnly,ab,copyAB,monoButton,clearClip;
    std::unique_ptr<ButtonAttachment> monoAttachment;
    juce::TextEditor saveName;
    juce::Label helpLabel,fileLabel;
    juce::TextButton loadVocal,playVocal,loopVocal,fileInput;
    std::unique_ptr<juce::FileChooser> vocalChooser;
    std::array<juce::TextButton,6> reverbButtons;
    std::array<std::unique_ptr<ButtonAttachment>,6> reverbAttachments;
    juce::StringArray userPaths,favourites;
    juce::String currentPreset="Factory:1";
    bool favouritesOnly=false;
    float outputDisplayL=0,outputDisplayR=0;
    juce::File presetFolder() const {
       #if defined(IRCHOIR_TESTING)
        return irTestRoot.getChildFile("Presets");
       #else
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("IRChoir/Presets");
       #endif
    }
    void setupExtras();
    void refreshPresetMenu();
    void choosePreset(int);
    void applyFormation(int);
    void groupAdjust(const char*,float,bool);
    void dragPad(juce::Point<float> point) {
        auto r=pad();setRaw("space",juce::jlimit(0.f,1.f,(point.x-r.getX())/r.getWidth()));
        setRaw("intensity",juce::jlimit(0.f,1.f,1-(point.y-r.getY())/r.getHeight()));repaint();
    }
    juce::Rectangle<float> pad() const {return {static_cast<float>(getWidth()-245),195.f,200.f,200.f};}
    void paintConductorAndMeters(juce::Graphics&);
    // Deterministic visual identities only; these do not imply sampled voice models.
    void paintSinger(juce::Graphics& g,juce::Point<float> pos,int singer,
                     juce::Colour accent,float activity,bool muted) const {
        juce::Graphics::ScopedSaveState saved(g);
        g.addTransform(juce::AffineTransform::translation(pos.x,pos.y));
        g.setOpacity(muted?0.42f:1.f);
        const juce::Colour skins[]{juce::Colour(0xffedc4a2),juce::Colour(0xffba805c),
            juce::Colour(0xff80523e),juce::Colour(0xffd6a37e),juce::Colour(0xffa66b4b)};
        const juce::Colour hairs[]{juce::Colour(0xff292229),juce::Colour(0xff654333),
            juce::Colour(0xffb49b76),juce::Colour(0xff39343d),juce::Colour(0xff9c6950)};
        auto skin=skins[(singer*3+1)%5],hair=hairs[(singer*7+2)%5];
        auto robe=accent.interpolatedWith(juce::Colour(0xff26334b),0.65f);
        g.setColour(juce::Colours::black.withAlpha(0.24f));g.fillEllipse(-15,24,30,5);
        // Hair silhouette and ears sit behind the face.
        g.setColour(hair);g.fillRoundedRectangle(-9,-22,18,singer%3==0?29.f:21.f,8.f);
        g.setColour(skin.darker(0.1f));g.fillEllipse(-10,-15,4,7);g.fillEllipse(6,-15,4,7);
        g.setColour(skin.darker(0.15f));g.fillRect(-3.f,-3.f,6.f,8.f);
        juce::Path body;body.startNewSubPath(-5,1);body.quadraticTo(-13,2,-14,12);
        body.lineTo(-13,25);body.quadraticTo(0,29,13,25);body.lineTo(14,12);
        body.quadraticTo(13,2,5,1);body.closeSubPath();
        g.setGradientFill(juce::ColourGradient(robe.brighter(0.25f),-10,2,robe.darker(0.35f),12,27,false));g.fillPath(body);
        // Stole, seams and cuffs retain a clear silhouette at small editor sizes.
        juce::Path stole;stole.startNewSubPath(-6,2);stole.lineTo(0,8);stole.lineTo(6,2);
        g.setColour(accent.withAlpha(0.85f));g.strokePath(stole,juce::PathStrokeType(2.f));
        g.setColour(accent.withAlpha(0.25f));g.drawLine(0,10,0,25,0.7f);
        g.drawLine(-9,10,-10,22,0.7f);g.drawLine(9,10,10,22,0.7f);
        g.setColour(skin);g.fillRoundedRectangle(-12,19,4,5,2);g.fillRoundedRectangle(8,19,4,5,2);
        g.setColour(skin);g.fillRoundedRectangle(-7.5f,-20,15,19,7);
        g.setColour(skin.brighter(0.15f).withAlpha(0.5f));g.fillEllipse(-5,-16,4,7);
        // Five hairstyles: fringe, side part, curls, close crop and swept hair.
        g.setColour(hair);
        if(singer%5==2) {
            for(int c=0;c<5;++c)g.fillEllipse(-9.f+c*3.5f,-23.f+(c%2),6,7);
        } else {
            juce::Path fringe;fringe.startNewSubPath(-8,-15);fringe.quadraticTo(-9,-25,2,-22);
            fringe.quadraticTo(10,-23,8,-13);fringe.lineTo(4,singer%5==1?-19.f:-16.f);
            fringe.lineTo(-2,-19);fringe.closeSubPath();g.fillPath(fringe);
        }
        g.setColour(juce::Colour(0xff332b32));
        g.drawLine(-5,-12,-2,-12.5f,0.7f);g.drawLine(2,-12.5f,5,-12,0.7f);
        g.fillEllipse(-4.6f,-11,1.7f,1.8f);g.fillEllipse(2.9f,-11,1.7f,1.8f);
        g.setColour(skin.darker(0.25f));g.drawLine(0,-10,0.7f,-7,0.7f);
        g.setColour(juce::Colour(0xff442832));g.fillEllipse(-2.2f,-6,4.4f,1.5f+activity*4.5f);
        if(activity>0.15f) {g.setColour(juce::Colour(0xfff5ddcb));g.fillRoundedRectangle(-1.5f,-5.8f,3,0.8f,0.3f);}
    }

    void addExtra(const juce::String& id,const juce::String& title,int page,int column) {
        Extra c;c.page=page;c.column=column;c.slider=std::make_unique<juce::Slider>();c.label=std::make_unique<juce::Label>();
        addAndMakeVisible(*c.slider);addAndMakeVisible(*c.label);
        c.slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);c.slider->setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,24);
        c.slider->setTitle(title);c.slider->setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xffedbb79));
        c.label->setText(title,juce::dontSendNotification);c.label->setJustificationType(juce::Justification::centred);
        c.attachment=std::make_unique<Attachment>(processor.state,id,*c.slider);extraSliders.push_back(std::move(c));
    }
    float value(const juce::String& id) const { return processor.state.getRawParameterValue(id)->load(); }
    void setRaw(const juce::String& id,float v) {
        auto* p=processor.state.getParameter(id);
        processor.state.getParameterAsValue(id).setValue(p->getNormalisableRange().snapToLegalValue(v));
    }
    void set(const juce::String& id,float v) { auto* p=processor.state.getParameter(id); p->beginChangeGesture(); setRaw(id,v); p->endChangeGesture(); }
    juce::Rectangle<float> stage() const { return {45.f,190.f,static_cast<float>(getWidth()-340),static_cast<float>(getHeight()-590)}; }
    juce::Point<float> position(int i) const {
        auto st=stage(); auto id=juce::String(i);
        float pan=value("pan"+id);
        float y=st.getY()+20+(i%4)*(st.getHeight()-40)/3;
        int shape=static_cast<int>(value("formation"));
        if(shape==0)y+=pan*pan*30;
        if(shape==1)y=st.getY()+35+(i%4)*(st.getHeight()-70)/3;
        return {st.getX()+35+(pan+1)*0.5f*(st.getWidth()-70),y-(value("level"+id)-0.8f)*15};
    }
    int hitSinger(juce::Point<float> point) const {
        int best=-1; float distance=28;
        for(int i=0;i<static_cast<int>(value("voices"));++i) {
            auto d=point.getDistanceFrom(position(i)); if(d<distance) {best=i;distance=d;}
        } return best;
    }
    void endDrag() {
        if(dragging) {
            for(int i=0;i<32;++i)if(selectedSet.test(static_cast<size_t>(i))) {
                processor.state.getParameter("pan"+juce::String(i))->endChangeGesture();processor.state.getParameter("level"+juce::String(i))->endChangeGesture();
            }
            dragging=false;
        }
        if(padDragging) {processor.state.getParameter("intensity")->endChangeGesture();processor.state.getParameter("space")->endChangeGesture();padDragging=false;}
    }
    void select(int i) {
        endDrag(); selected=i; auto id=juce::String(i);
        panAttachment.reset(); levelAttachment.reset(); muteAttachment.reset(); soloAttachment.reset();characterAttachment.reset();
        selectedLabel.setText("SINGER "+juce::String(i+1),juce::dontSendNotification);
        panAttachment=std::make_unique<Attachment>(processor.state,"pan"+id,singerPan);
        levelAttachment=std::make_unique<Attachment>(processor.state,"level"+id,singerLevel);
        muteAttachment=std::make_unique<ButtonAttachment>(processor.state,"mute"+id,mute);
        soloAttachment=std::make_unique<ButtonAttachment>(processor.state,"solo"+id,solo);
        characterAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state,"character"+id,characterBox);
        repaint();
    }
    void applyPreset(int p) {
        if(p<1 || p>9)return;
        processor.undoManager.beginNewTransaction("Factory preset");
        struct Preset {float count,movement,room,width,mix,gain;};
        const Preset bank[]={
            {4,0.12f,0.05f,0.35f,0.65f,-3},
            {12,0.35f,0.18f,0.9f,0.65f,-3},
            {24,0.65f,0.6f,0.9f,0.65f,-3},
            {32,0.8f,0.4f,0.9f,0.65f,-3},
            {2,0.08f,0.f,0.4f,0.35f,-1},
            {12,0.3f,0.12f,1.f,0.7f,-3},
            {24,0.9f,0.75f,1.f,0.85f,-4},
            {16,0.2f,1.f,0.95f,0.75f,-4},
            {32,1.f,0.5f,1.f,0.95f,-5}
        };
        // A factory preset is deterministic: reset all sonic parameters first.
        for(auto* parameter:processor.getParameters()) {
            auto* param=dynamic_cast<juce::RangedAudioParameter*>(parameter);
            if(param && param->getName(100)!="Animation") {param->beginChangeGesture();param->setValueNotifyingHost(param->getDefaultValue());param->endChangeGesture();}
        }
        const auto& fx=bank[p-1];
        set("voices",fx.count); set("movement",fx.movement); set("room",fx.room);
        set("spread",fx.width); set("mix",fx.mix); set("gain",fx.gain);
        for(int i=0;i<32;++i) {
            auto id=juce::String(i); set("mute"+id,0);set("solo"+id,0);set("level"+id,0.8f);
            // Fill the stereo field even for two- and four-voice presets.
            int count=static_cast<int>(fx.count);
            int sectionCount=std::max(1,(count+3-(i%4))/4);
            float pan=i<count?(sectionCount>1?2.f*(i/4)/static_cast<float>(sectionCount-1)-1.f:0.f):2.f*(i/4)/7.f-1.f;
            set("pan"+id,pan);
        }
        if(p==5) {set("timing",0.08f);set("drift",2);set("vibrato",1);}
        if(p==6) {set("compression",0.3f);set("high",1.5f);}
        if(p==7) {set("delay",0.25f);set("delayTime",380);set("high",-3);}
        if(p==8) {set("roomType",2);set("delay",0.12f);set("delayTime",170);}
        if(p==9) {set("drive",0.25f);set("compression",0.4f);set("drift",15);set("vibrato",12);}
        currentPreset="Factory:"+juce::String(p);
    }
    void timerCallback() override {
        fileLabel.setText(processor.vocalFile.status(),juce::dontSendNotification);
        fileInput.setToggleState(processor.vocalFile.enabled.load(),juce::dontSendNotification);
        loopVocal.setToggleState(processor.vocalFile.loop.load(),juce::dontSendNotification);
        playVocal.setButtonText(processor.vocalFile.playing.load()?"Stop":"Play");
        for(int i=0;i<32;++i) displayMeters[i]=juce::jmax(processor.meters[i].load(std::memory_order_relaxed),displayMeters[i]*0.87f);
        if(selected>=static_cast<int>(value("voices")))select(0);
        outputDisplayL=std::max(processor.outputL.load(),outputDisplayL*0.9f);outputDisplayR=std::max(processor.outputR.load(),outputDisplayR*0.9f);
        undoButton.setEnabled(processor.undoManager.canUndo());redoButton.setEnabled(processor.undoManager.canRedo());
        formation.setSelectedId(static_cast<int>(value("formation"))+1,juce::dontSendNotification);
        int mode=static_cast<int>(value("animation"));int hz=mode==2?8:(mode==1?15:30);
        if(getTimerInterval()!=1000/hz)startTimerHz(hz);
        repaint();
    }
};
void ChoirEditor::setupExtras() {
    for(auto* button:{&loadVocal,&playVocal,&loopVocal,&fileInput})addAndMakeVisible(*button);
    addAndMakeVisible(fileLabel);fileLabel.setFont(juce::Font(11.f));fileLabel.setColour(juce::Label::textColourId,juce::Colour(0xffb4bdd1));
    loadVocal.setButtonText("Load vocal");loadVocal.setComponentID("load-vocal");
    playVocal.setButtonText("Play");loopVocal.setButtonText("Loop");fileInput.setButtonText("File input");
    loopVocal.setClickingTogglesState(true);fileInput.setClickingTogglesState(true);
    playVocal.onClick=[this]{processor.vocalFile.togglePlayback();};
    loopVocal.onClick=[this]{processor.vocalFile.loop.store(loopVocal.getToggleState());};
    fileInput.onClick=[this]{processor.vocalFile.enabled.store(fileInput.getToggleState());};
    loadVocal.onClick=[this]{
        vocalChooser=std::make_unique<juce::FileChooser>("Choose a dry vocal recording",juce::File{},"*.wav;*.aif;*.aiff;*.flac");
        juce::Component::SafePointer<ChoirEditor> safe(this);
        vocalChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe](const juce::FileChooser& chooser){
            if(safe && chooser.getResult().existsAsFile())safe->processor.vocalFile.loadFile(chooser.getResult());
        });
    };
    fileInput.setTooltip("On: imported vocal. Off: audio from your DAW track. Playback is manual, independent of the DAW transport.");
    addAndMakeVisible(undoButton);addAndMakeVisible(redoButton);
    undoButton.setButtonText("Undo");redoButton.setButtonText("Redo");
    undoButton.onClick=[this]{endDrag();processor.undoManager.undo();};redoButton.onClick=[this]{endDrag();processor.undoManager.redo();};
    undoButton.setTooltip("Undo parameter edits. Preset-file loading and A/B switching are separate state operations.");redoButton.setTooltip("Redo the last undone parameter edit.");
    const char* names[]={"Bass group","Tenor group","Alto group","Soprano group","All singers"};
    for(int j=0;j<5;++j) {
        addAndMakeVisible(groups[j]);groups[j].setButtonText(names[j]);
        groups[j].setTooltip("Select this section; drag a selected singer or use group controls to edit the selection.");
        groups[j].onClick=[this,j] {
            endDrag();selectedSet.reset();int first=-1;
            for(int i=0;i<static_cast<int>(value("voices"));++i)if(j==4 || i%4==j) {selectedSet.set(static_cast<size_t>(i));if(first<0)first=i;}
            if(first>=0)select(first);
            repaint();
        };
    }
    for(auto* c:{&formation,&view,&animationBox,&harmonyBox,&roomBox,&characterBox})addAndMakeVisible(*c);
    formation.addItemList(juce::StringArray{"Formation: semicircle","Formation: tight cluster","Formation: wide rows"},1);
    formation.setSelectedId(3,juce::dontSendNotification);formation.onChange=[this]{applyFormation(formation.getSelectedId());};
    view.addItemList(juce::StringArray{"Performance controls","Humanize + conductor","EQ + dynamics","Delay rack","Four-part levels","Four-part intervals","Reverb space"},1);
    view.setSelectedId(1,juce::dontSendNotification);view.onChange=[this]{resized();};
    animationBox.addItemList(juce::StringArray{"Full motion","Subtle motion","Reduced motion"},1);
    animationAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state,"animation",animationBox);
    harmonyBox.addItemList(juce::StringArray{"Harmony: unison","Harmony: four relative parts","Harmony: MIDI targets"},1);
    harmonyAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state,"harmony",harmonyBox);
    roomBox.addItemList(juce::StringArray{"Room: studio","Room: hall","Room: cathedral","Room: chamber","Room: plate-style","Room: tunnel-style"},1);
    roomAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state,"roomType",roomBox);
    characterBox.addItemList(juce::StringArray{"Color: warm","Color: airy","Color: dark","Color: bright"},1);
    const char* humanIds[]={"timing","drift","vibrato","intensity","space","timbre"};
    const char* humanNames[]={"TIMING LOOSENESS","PITCH DRIFT (cents)","VIBRATO (cents)","INTENSITY","SPACE","SYNTHETIC CHARACTER"};
    for(int j=0;j<6;++j)addExtra(humanIds[j],humanNames[j],2,j);
    const char* eqIds[]={"low","high","compression","drive"};
    const char* eqNames[]={"LOW EQ dB","HIGH EQ dB","COMPRESSION","SATURATION"};
    for(int j=0;j<4;++j)addExtra(eqIds[j],eqNames[j],3,j);
    const char* revIds[]={"revPredelay","revDecay","revWidth","revDamping","revLowCut","revHighCut"};
    const char* revTitles[]={"PRE-DELAY ms","DECAY SHAPE","REVERB WIDTH","DAMPING","LOW CUT Hz","HIGH CUT Hz"};
    const char* toggleIds[]={"reverbOn","reverbMonoSend","reverbLowCutOn","reverbHighCutOn","reverbFreeze","reverbWetOnly"};
    const char* toggleNames[]={"Reverb on","Mono send","Low cut","High cut","Freeze","Wet only"};
    for(int j=0;j<6;++j) {
        addExtra(revIds[j],revTitles[j],7,j);
        auto& button=reverbButtons[j];addAndMakeVisible(button);button.setButtonText(toggleNames[j]);button.setClickingTogglesState(true);
        reverbAttachments[j]=std::make_unique<ButtonAttachment>(processor.state,toggleIds[j],button);
    }
    addExtra("delay","DELAY MIX",4,0);addExtra("delayTime","DELAY TIME ms",4,1);addExtra("feedback","DELAY FEEDBACK",4,2);
    for(int j=0;j<4;++j) {addExtra("partLevel"+juce::String(j),juce::String(partNames[j])+" LEVEL",5,j);addExtra("interval"+juce::String(j),juce::String(partNames[j])+" SEMITONES",6,j);}
    for(auto* c:{&groupPan,&groupLevel}) {addAndMakeVisible(*c);c->setSliderStyle(juce::Slider::LinearHorizontal);c->setTextBoxStyle(juce::Slider::TextBoxRight,false,60,24);}
    groupPan.setRange(-1,1,0.01);groupPan.setValue(0,juce::dontSendNotification);groupPan.setTitle("Group pan offset");
    groupPan.onDragStart=[this]{processor.undoManager.beginNewTransaction("Group pan");lastGroupPan=0;};
    groupPan.onValueChange=[this]{float now=static_cast<float>(groupPan.getValue());groupAdjust("pan",now-lastGroupPan,true);lastGroupPan=now;};
    groupPan.onDragEnd=[this]{groupPan.setValue(0,juce::dontSendNotification);lastGroupPan=0;};
    groupLevel.setRange(0,1,0.01);groupLevel.setValue(0.8,juce::dontSendNotification);groupLevel.setTitle("Selected group level");
    groupLevel.onDragStart=[this]{processor.undoManager.beginNewTransaction("Group level");};
    groupLevel.onValueChange=[this]{groupAdjust("level",static_cast<float>(groupLevel.getValue()),false);};
    for(auto* c:{&groupMute,&groupSolo,&savePreset,&favourite,&favOnly,&ab,&copyAB,&monoButton,&clearClip})addAndMakeVisible(*c);
    groupMute.setButtonText("Toggle group mute");groupSolo.setButtonText("Toggle group solo");
    groupMute.onClick=[this]{processor.undoManager.beginNewTransaction("Group mute");groupAdjust("mute",value("mute"+juce::String(selected))>0.5f?0.f:1.f,false);};
    groupSolo.onClick=[this]{processor.undoManager.beginNewTransaction("Group solo");groupAdjust("solo",value("solo"+juce::String(selected))>0.5f?0.f:1.f,false);};
    monoButton.setButtonText("Mono audition");monoButton.setClickingTogglesState(true);
    monoAttachment=std::make_unique<ButtonAttachment>(processor.state,"mono",monoButton);
    clearClip.setButtonText("Clear clip light");clearClip.onClick=[this]{processor.clipped.store(false);};
    savePreset.setButtonText("Save");favourite.setButtonText("Star");favOnly.setButtonText("Favourites");
    addAndMakeVisible(saveName);saveName.setComponentID("preset-name");saveName.setTextToShowWhenEmpty("Name your preset",juce::Colours::grey);
    addAndMakeVisible(helpLabel);helpLabel.setComponentID("status");helpLabel.setText("Group sliders below the selected singer affect the whole selection. MIDI mode needs a monophonic vocal plus routed MIDI notes.",juce::dontSendNotification);
    auto favFile=presetFolder().getParentDirectory().getChildFile("favourites.txt");
    favourites.addLines(favFile.loadFileAsString());
    savePreset.onClick=[this] {
        auto name=juce::File::createLegalFileName(saveName.getText().trim()).substring(0,80);
        if(name.isEmpty()) {helpLabel.setText("Enter a preset name first.",juce::dontSendNotification);return;}
        auto folder=presetFolder();auto status=folder.createDirectory();
        if(status.failed()) {helpLabel.setText("Cannot create preset folder: "+status.getErrorMessage(),juce::dontSendNotification);return;}
        auto file=folder.getNonexistentChildFile(name,".irchoir.xml",false);
        auto xml=processor.state.copyState().createXml();
        if(!xml->writeTo(file)) {helpLabel.setText("Preset could not be saved.",juce::dontSendNotification);return;}
        currentPreset=file.getFullPathName();helpLabel.setText("Saved "+file.getFileName(),juce::dontSendNotification);refreshPresetMenu();
    };
    favourite.onClick=[this] {
        if(favourites.contains(currentPreset))favourites.removeString(currentPreset);else favourites.add(currentPreset);
        auto folder=presetFolder().getParentDirectory();auto status=folder.createDirectory();
        if(status.failed() || !folder.getChildFile("favourites.txt").replaceWithText(favourites.joinIntoString("\n")))
            helpLabel.setText("Favourite changed in memory, but could not be saved to disk.",juce::dontSendNotification);
        refreshPresetMenu();
    };
    favOnly.setClickingTogglesState(true);favOnly.onClick=[this]{favouritesOnly=favOnly.getToggleState();refreshPresetMenu();};
    ab.setButtonText(processor.usingB?"B":"A");copyAB.setButtonText("Copy");
    ab.onClick=[this] {
        auto current=processor.state.copyState();
        if(processor.usingB) {processor.snapshotB=current;if(!processor.snapshotA.isValid())processor.snapshotA=current.createCopy();processor.state.replaceState(processor.snapshotA.createCopy());}
        else {processor.snapshotA=current;if(!processor.snapshotB.isValid())processor.snapshotB=current.createCopy();processor.state.replaceState(processor.snapshotB.createCopy());}
        processor.usingB=!processor.usingB;ab.setButtonText(processor.usingB?"B":"A");
    };
    copyAB.onClick=[this] {auto current=processor.state.copyState();if(processor.usingB)processor.snapshotA=current;else processor.snapshotB=current;helpLabel.setText("Copied current sound to the other A/B slot.",juce::dontSendNotification);};
    const char* tips[]={"Number of vocal layers. Larger ensembles use more CPU.","Blend the original vocal with the choir layers. At 100%, only the wet ensemble is heard.","Scale singer panning. Panning does not alter a mono output.","Slow delay modulation; separate pitch and vibrato controls are on the Humanize page.","Algorithmic room blend applied to the full output mix.","Final gain after all effects. Red clip indication means overload; no limiter is applied."};
    for(int i=0;i<6;++i)knobs[i].setTooltip(tips[i]);
    for(auto& control:extraSliders)control.slider->setTooltip(control.label->getText()+". Change this parameter with the knob or its numeric entry.");
    formation.setTooltip("Arrange all singers and reset their pan and level; mute and solo choices remain.");
    characterBox.setTooltip("Synthetic filter colors, not separate recorded or AI singer identities.");
    harmonyBox.setTooltip("MIDI mode requires a clear vocal plus routed MIDI notes. Relative parts use the interval controls.");
    monoButton.setTooltip("Audition the stereo output summed to mono; correlation is measured before summing.");
    singerPan.setTooltip("Pan the selected singer. Global width scales the result.");singerLevel.setTooltip("Level of the selected wet vocal layer.");
    groupPan.setTooltip("Relative pan offset for the selection; returns to zero when released.");groupLevel.setTooltip("Set all selected singers to this level.");
    savePreset.setTooltip("Save the current sound as a new named preset, without replacing existing files.");
    presets.onChange=[this]{choosePreset(presets.getSelectedId());};refreshPresetMenu();
    formation.setSelectedId(static_cast<int>(value("formation"))+1,juce::dontSendNotification);
}
void ChoirEditor::refreshPresetMenu() {
    presets.clear(juce::dontSendNotification);userPaths.clear();
    const char* names[]={"Intimate","Wide","Cloud","Massive","Tight Doubles","Wide Backing","Dream Haze","Cathedral","Crowd Wash"};
    for(int i=0;i<9;++i) {
        auto key="Factory:"+juce::String(i+1);bool star=favourites.contains(key);
        if(!favouritesOnly || star)presets.addItem((star?"* ":"")+juce::String(names[i]),i+1);
    }
    auto files=presetFolder().findChildFiles(juce::File::findFiles,false,"*.irchoir.xml");
    files.sort();
    for(auto file:files) {
        int id=1000+userPaths.size();auto path=file.getFullPathName();userPaths.add(path);
        bool star=favourites.contains(path);
        if(!favouritesOnly || star)presets.addItem((star?"* ":"")+file.getFileNameWithoutExtension(),id);
    }
    presets.setTextWhenNothingSelected(favouritesOnly?"Favourite presets":"Choose preset");
}
void ChoirEditor::choosePreset(int id) {
    endDrag();
    if(id>=1 && id<=9)applyPreset(id);
    else if(id>=1000 && id-1000<userPaths.size()) {
        auto path=userPaths[id-1000];auto xml=juce::XmlDocument::parse(juce::File(path));
        if(xml && xml->hasTagName("IRChoir")) {processor.state.replaceState(juce::ValueTree::fromXml(*xml));currentPreset=path;}
        else helpLabel.setText("This preset could not be read.",juce::dontSendNotification);
    }
}
void ChoirEditor::groupAdjust(const char* prefix,float amount,bool relative) {
    for(int i=0;i<32;++i)if(selectedSet.test(static_cast<size_t>(i))) {
        auto id=juce::String(prefix)+juce::String(i);auto* parameter=processor.state.getParameter(id);
        auto range=parameter->getNormalisableRange();
        set(id,juce::jlimit(range.start,range.end,relative?value(id)+amount:amount));
    }
}
void ChoirEditor::applyFormation(int shape) {
    endDrag();set("formation",static_cast<float>(shape-1));
    const int n=static_cast<int>(value("voices"));
    for(int i=0;i<32;++i) {
        int count=std::max(1,(n+3-(i%4))/4);int index=i/4;
        float pan=count>1?2.f*index/static_cast<float>(count-1)-1.f:0.f;
        pan=juce::jlimit(-1.f,1.f,pan);
        if(shape==2)pan*=0.3f;
        set("pan"+juce::String(i),pan);set("level"+juce::String(i),0.8f);
    }
}
void ChoirEditor::paintConductorAndMeters(juce::Graphics& g) {
    auto r=pad();g.setColour(juce::Colour(0xff24283b));g.fillRoundedRectangle(r,12);
    g.setColour(juce::Colour(0xff626a80));g.drawRoundedRectangle(r,12,1);
    g.drawHorizontalLine(static_cast<int>(r.getCentreY()),r.getX()+10,r.getRight()-10);
    g.drawVerticalLine(static_cast<int>(r.getCentreX()),r.getY()+10,r.getBottom()-10);
    g.setColour(juce::Colour(0xffedbb79));
    g.fillEllipse(r.getX()+value("space")*r.getWidth()-7,r.getBottom()-value("intensity")*r.getHeight()-7,14,14);
    g.setFont(12);g.drawText("CONDUCTOR",r.translated(0,-28).withHeight(20),juce::Justification::centred);
    g.setColour(juce::Colour(0xffadb4cd));g.drawText("SPACE  /  INTENSITY",r.translated(0,r.getHeight()+10).withHeight(20),juce::Justification::centred);
    float y=r.getBottom()+35;
    const float values[]={outputDisplayL,outputDisplayR};
    for(int ch=0;ch<2;++ch) {
        g.setColour(juce::Colour(0xff262d3b));g.fillRect(r.getX(),y+ch*20,r.getWidth(),14.f);
        float db=juce::Decibels::gainToDecibels(values[ch],-60.f);float width=juce::jlimit(0.f,1.f,(db+60)/60)*r.getWidth();
        g.setColour(values[ch]>=1?juce::Colour(0xfff06a72):juce::Colour(0xff72d4b3));g.fillRect(r.getX(),y+ch*20,width,14.f);
        g.setColour(juce::Colours::white);g.drawText(juce::String(ch==0?"L ":"R ")+juce::String(db,1)+" dBFS",r.getX()+4,y+ch*20,190,16,juce::Justification::left);
    }
    g.setColour(processor.clipped.load()?juce::Colour(0xfff06a72):juce::Colour(0xffadb4cd));
    auto text=juce::String(processor.clipped.load()?"CLIP  |  ":"")+"Correlation "+juce::String(processor.correlation.load(),2);
    g.drawText(text,r.getX(),y+42,220,16,juce::Justification::left);
    g.drawText("Mono RMS "+juce::String(juce::Decibels::gainToDecibels(processor.monoRms.load(),-90.f),1)+" dBFS",r.getX(),y+58,230,16,juce::Justification::left);
    float hz=processor.detectedHz.load();
    g.drawText(hz>0?"Vocal guide: "+juce::String(hz,1)+" Hz":"Vocal guide: no stable pitch",r.getX(),y+74,230,16,juce::Justification::left);
}

juce::AudioProcessorEditor* IRChoir::createEditor() { return new ChoirEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new IRChoir(); }
