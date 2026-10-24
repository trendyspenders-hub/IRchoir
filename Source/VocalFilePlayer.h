#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

// Decoding, allocation and old-clip deletion stay off the audio callback.
class VocalFilePlayer final : private juce::Thread, private juce::AsyncUpdater {
public:
    VocalFilePlayer():juce::Thread("I.R.Choir vocal import") {formats.registerBasicFormats();}
    ~VocalFilePlayer() override {cancelPendingUpdate();stopThread(-1);}
    std::atomic<bool> enabled{false},playing{false},loop{false},loading{false};
    bool isReady() const {return ready.load();}
    void loadFile(const juce::File& file,bool activate=true,bool replaceReference=false) {
        const juce::ScopedLock lock(requestLock);
        requested=file;requestedActivate=activate;requestedGeneration=++generation;
        loading.store(true);
        if(replaceReference) {
            ready.store(false);playing.store(false);rewind.store(true);enabled.store(activate && file!=juce::File{});
            const juce::ScopedLock info(infoLock);referencePath=file.getFullPathName();
            message=file==juce::File{}?"Drop a WAV, AIFF or FLAC vocal here":"Restoring "+file.getFileName();
        }
        triggerAsyncUpdate();
    }
    bool togglePlayback() {
        if(playing.exchange(false))return true;
        if(!ready.load() || loading.load()) {report(loading.load()?"Wait for the vocal to finish loading.":"Load or relink a vocal before pressing Play.");return false;}
        rewind.store(true);enabled.store(true);playing.store(true);return true;
    }
    void stop() {playing.store(false);rewind.store(true);}
    juce::String status() const {const juce::ScopedLock lock(infoLock);return message;}
    juce::String path() const {const juce::ScopedLock lock(infoLock);return referencePath;}
    void render(juce::AudioBuffer<float>& output,double hostRate) {
        if(!enabled.load())return;
        const juce::SpinLock::ScopedTryLockType lock(audioLock);
        output.clear();
        if(!lock.isLocked() || !clip || !ready.load() || !playing.load() || hostRate<=0)return;
        if(rewind.exchange(false))position=0;
        const int length=clip->getNumSamples();
        const double step=clipRate/hostRate,fade=std::max(1.,clipRate*0.005);
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
    bool loadForTest(const juce::File& file) {stopThread(-1);return decode(file,true,++generation);}
   #endif
private:
    juce::AudioFormatManager formats;
    juce::CriticalSection requestLock;
    mutable juce::CriticalSection infoLock;
    juce::SpinLock audioLock;
    juce::File requested,workerFile;
    bool requestedActivate=true,workerActivate=true;
    uint64_t requestedGeneration=0,workerGeneration=0;
    std::atomic<uint64_t> generation{0};
    std::unique_ptr<juce::AudioBuffer<float>> clip;
    juce::String message="Drop a WAV, AIFF or FLAC vocal here",referencePath;
    double clipRate=44100,position=0;
    std::atomic<bool> rewind{true},ready{false};
    void report(const juce::String& text) {const juce::ScopedLock lock(infoLock);message=text;}
    bool fail(const juce::String& text,uint64_t token) {
        const juce::ScopedLock lock(requestLock);if(token==generation.load())report(text);return false;
    }
    void handleAsyncUpdate() override {
        stopThread(-1);
        {const juce::ScopedLock lock(requestLock);workerFile=requested;workerActivate=requestedActivate;workerGeneration=requestedGeneration;}
        if(!startThread()) {fail("Could not start vocal loading. Try Load vocal again.",workerGeneration);loading.store(false);}
    }
    void run() override {
        if(workerFile==juce::File{}) {
            std::unique_ptr<juce::AudioBuffer<float>> retired;
            const juce::ScopedLock request(requestLock);
            if(workerGeneration==generation.load()) {
                {const juce::SpinLock::ScopedLockType lock(audioLock);clip.swap(retired);ready.store(false);}
                loading.store(false);
            }
        } else {
            decode(workerFile,workerActivate,workerGeneration);
            if(workerGeneration==generation.load())loading.store(false);
        }
    }
    bool decode(const juce::File& file,bool activate,uint64_t token) {
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if(!reader)return fail("Cannot read "+file.getFileName()+". Load vocal to relink; previous audio is not used for project recall.",token);
        if(reader->numChannels<1 || reader->numChannels>2 || reader->sampleRate<8000 || reader->sampleRate>192000 ||
           reader->lengthInSamples<2 || reader->lengthInSamples/reader->sampleRate>300. ||
           reader->lengthInSamples>static_cast<juce::int64>(128*1024*1024/(sizeof(float)*reader->numChannels)))
            return fail("Use mono/stereo audio, 8-192 kHz, up to 5 minutes and 128 MiB decoded.",token);
        auto decoded=std::make_unique<juce::AudioBuffer<float>>(static_cast<int>(reader->numChannels),static_cast<int>(reader->lengthInSamples));
        for(int offset=0;offset<decoded->getNumSamples();offset+=4096) {
            if(threadShouldExit() || token!=generation.load())return false;
            int count=std::min(4096,decoded->getNumSamples()-offset);
            if(!reader->read(decoded.get(),offset,count,offset,true,true))return fail("Audio decoding failed. Use Load vocal to choose another file.",token);
            for(int ch=0;ch<decoded->getNumChannels();++ch)for(int n=offset;n<offset+count;++n)
                if(!std::isfinite(decoded->getSample(ch,n)))decoded->setSample(ch,n,0);
        }
        // Requests and publication share a lock, so an older worker cannot undo a later restore.
        const juce::ScopedLock request(requestLock);
        if(threadShouldExit() || token!=generation.load())return false;
        {
            const juce::SpinLock::ScopedLockType lock(audioLock);
            clip.swap(decoded);clipRate=reader->sampleRate;position=0;
            playing.store(false);rewind.store(true);enabled.store(activate);ready.store(true);
        }
        {const juce::ScopedLock info(infoLock);referencePath=file.getFullPathName();message=file.getFileName()+"  |  "+juce::String(reader->lengthInSamples/reader->sampleRate,1)+" s";}
        return true;
    }
};
