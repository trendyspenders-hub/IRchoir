#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

// Decode on a worker; the audio callback only reads bounded in-memory data.
class VocalFilePlayer final : private juce::Thread, private juce::AsyncUpdater {
public:
    VocalFilePlayer():juce::Thread("I.R.Choir vocal import") {formats.registerBasicFormats();}
    ~VocalFilePlayer() override {cancelPendingUpdate();stopThread(-1);}
    std::atomic<bool> enabled{false},playing{false},loop{false},loading{false};
    void loadFile(const juce::File& file,bool activate=true) {
        {const juce::ScopedLock lock(requestLock);requested=file;requestedActivate=activate;}
        triggerAsyncUpdate();
    }
    void togglePlayback() {if(playing.exchange(false))return;rewind.store(true);enabled.store(true);playing.store(true);}
    void stop() {playing.store(false);rewind.store(true);}
    juce::String status() const {const juce::ScopedLock lock(infoLock);return message;}
    juce::String path() const {const juce::ScopedLock lock(infoLock);return loadedPath;}
    void render(juce::AudioBuffer<float>& output,double hostRate) {
        if(!enabled.load())return;
        const juce::SpinLock::ScopedTryLockType lock(audioLock);
        output.clear();
        if(!lock.isLocked() || !clip || !playing.load() || hostRate<=0)return;
        if(rewind.exchange(false))position=0;
        const int length=clip->getNumSamples();
        const double step=clipRate/hostRate;
        const double fade=std::max(1.,clipRate*0.005);
        for(int n=0;n<output.getNumSamples();++n) {
            if(position>=length) {
                if(loop.load())position=std::fmod(position,static_cast<double>(length));
                else {playing.store(false);rewind.store(true);break;}
            }
            int a=static_cast<int>(position),b=std::min(a+1,length-1);
            float fraction=static_cast<float>(position-a);
            float envelope=static_cast<float>(std::min({1.,position/fade,(length-position)/fade}));
            for(int ch=0;ch<output.getNumChannels();++ch) {
                int src=std::min(ch,clip->getNumChannels()-1);
                float x=clip->getSample(src,a),y=clip->getSample(src,b);
                output.setSample(ch,n,(x+fraction*(y-x))*envelope);
            }
            position+=step;
        }
    }
   #if defined(IRCHOIR_TESTING)
    bool loadForTest(const juce::File& file) {stopThread(-1);return decode(file,true);}
   #endif
private:
    juce::AudioFormatManager formats;
    juce::CriticalSection requestLock;
    mutable juce::CriticalSection infoLock;
    juce::SpinLock audioLock;
    juce::File requested,workerFile;
    bool requestedActivate=true,workerActivate=true;
    std::unique_ptr<juce::AudioBuffer<float>> clip;
    juce::String message="Drop a WAV, AIFF or FLAC vocal here",loadedPath;
    double clipRate=44100,position=0;
    std::atomic<bool> rewind{true};
    void report(const juce::String& text) {const juce::ScopedLock lock(infoLock);message=text;}
    void handleAsyncUpdate() override {
        stopThread(-1);
        {const juce::ScopedLock lock(requestLock);workerFile=requested;workerActivate=requestedActivate;}
        loading.store(true);report("Loading "+workerFile.getFileName());startThread();
    }
    void run() override {decode(workerFile,workerActivate);loading.store(false);}
    bool decode(const juce::File& file,bool activate) {
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if(!reader) {report("Cannot read this file. Choose an uncompressed WAV, AIFF or FLAC.");return false;}
        if(reader->numChannels<1 || reader->numChannels>2 || reader->sampleRate<8000 || reader->sampleRate>192000 ||
           reader->lengthInSamples<2 || reader->lengthInSamples/reader->sampleRate>300. ||
           reader->lengthInSamples>static_cast<juce::int64>(128*1024*1024/(sizeof(float)*reader->numChannels))) {
            report("Use mono/stereo audio, 8-192 kHz, up to 5 minutes and 128 MiB decoded.");return false;
        }
        auto decoded=std::make_unique<juce::AudioBuffer<float>>(static_cast<int>(reader->numChannels),static_cast<int>(reader->lengthInSamples));
        for(int offset=0;offset<decoded->getNumSamples();offset+=4096) {
            if(threadShouldExit())return false;
            int count=std::min(4096,decoded->getNumSamples()-offset);
            if(!reader->read(decoded.get(),offset,count,offset,true,true)) {report("Audio decoding failed; previous vocal retained.");return false;}
        }
        for(int ch=0;ch<decoded->getNumChannels();++ch)for(int n=0;n<decoded->getNumSamples();++n)
            if(!std::isfinite(decoded->getSample(ch,n)))decoded->setSample(ch,n,0);
        {
            const juce::SpinLock::ScopedLockType lock(audioLock);
            clip.swap(decoded);clipRate=reader->sampleRate;position=0;
            playing.store(false);rewind.store(true);enabled.store(activate);
        } // Previous clip is destroyed here on the worker, never in render().
        {const juce::ScopedLock lock(infoLock);loadedPath=file.getFullPathName();message=file.getFileName()+"  |  "+juce::String(reader->lengthInSamples/reader->sampleRate,1)+" s";}
        return true;
    }
};
