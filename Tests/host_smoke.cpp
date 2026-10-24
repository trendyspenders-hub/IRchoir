#include "../Source/Plugin.cpp"
#include <iostream>
#include <stdexcept>
#include <set>
static void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
static void param(IRChoir& p,const char* id,float v) {auto* a=p.state.getParameter(id);require(a!=nullptr,"Missing parameter");a->setValueNotifyingHost(a->convertTo0to1(v));}
static void findComponents(juce::Component& c,std::vector<juce::ComboBox*>& boxes,std::vector<juce::TextButton*>& buttons) {
    if(auto* box=dynamic_cast<juce::ComboBox*>(&c))boxes.push_back(box);
    if(auto* button=dynamic_cast<juce::TextButton*>(&c))buttons.push_back(button);
    for(auto* child:c.getChildren())findComponents(*child,boxes,buttons);
}
int main() {
    try {
        juce::ScopedJuceInitialiser_GUI gui;
        IRChoir p; require(p.acceptsMidi(),"MIDI input disabled");
        auto stereo=p.getBusesLayout();require(p.isBusesLayoutSupported(stereo),"Stereo rejected");
        auto mono=stereo;mono.inputBuses.set(0,juce::AudioChannelSet::mono());mono.outputBuses.set(0,juce::AudioChannelSet::mono());
        require(p.isBusesLayoutSupported(mono),"Mono rejected");
        for(double rate:{44100.,48000.,96000.})for(int block:{1,64,512}) {
            p.prepareToPlay(rate,block);param(p,"voices",32);
            for(int mode=0;mode<3;++mode) {
                param(p,"harmony",static_cast<float>(mode));
                juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;
                const int iterations=mode==2?static_cast<int>(rate*0.13)/block+1:8;
                for(int k=0;k<iterations;++k) {
                    midi.clear();if(k==0 && mode==2) {midi.addEvent(juce::MidiMessage::noteOn(1,60,static_cast<juce::uint8>(100)),0);midi.addEvent(juce::MidiMessage::noteOn(2,67,static_cast<juce::uint8>(100)),block>1?block/2:0);}
                    for(int s=0;s<block;++s) {float x=0.2f*std::sin(static_cast<float>(2*ir::pi*220*(k*block+s)/rate));audio.setSample(0,s,x);audio.setSample(1,s,x);}
                    p.processBlock(audio,midi);
                    for(int ch=0;ch<2;++ch)for(int s=0;s<block;++s)require(std::isfinite(audio.getSample(ch,s)),"Non-finite output");
                }
            }
            p.reset();p.releaseResources();
        }
        p.prepareToPlay(48000,128);
        // Channel-aware note-off, sustain and panic handling.
        p.midiEvent(juce::MidiMessage::noteOn(1,60,static_cast<juce::uint8>(100)));
        p.midiEvent(juce::MidiMessage::noteOn(2,60,static_cast<juce::uint8>(100)));
        p.midiEvent(juce::MidiMessage::noteOff(1,60));require(p.midiTargets()[0]==60,"Cross-channel note cut off");
        p.midiEvent(juce::MidiMessage::controllerEvent(2,64,127));p.midiEvent(juce::MidiMessage::noteOff(2,60));
        require(p.midiTargets()[0]==60,"Sustain did not hold");
        p.midiEvent(juce::MidiMessage::controllerEvent(2,64,0));require(p.midiTargets()[0]<0,"Sustain release stuck");
        p.midiEvent(juce::MidiMessage::noteOn(1,70,static_cast<juce::uint8>(100)));p.midiEvent(juce::MidiMessage::allNotesOff(1));require(p.midiTargets()[0]<0,"Panic failed");
        // State recall and malformed state must not corrupt parameters.
        param(p,"low",6);param(p,"character4",3);param(p,"pan4",0.7f);
        juce::MemoryBlock snapshot;p.getStateInformation(snapshot);param(p,"low",-6);
        p.setStateInformation(snapshot.getData(),static_cast<int>(snapshot.getSize()));
        require(std::abs(p.state.getRawParameterValue("low")->load()-6)<0.01f,"State recall failed");
        require(p.state.getRawParameterValue("character4")->load()==3,"Voice color not recalled");
        const float beforeInvalid=p.state.getRawParameterValue("low")->load();
        const char junk[]="invalid";p.setStateInformation(junk,7);require(p.state.getRawParameterValue("low")->load()==beforeInvalid,"Invalid state damaged processor");
        // Master rack extremes, block silence, mono and sanitization.
        for(const char* id:{"low","high","compression","drive","delay","feedback","room","gain"}) {
            auto* a=p.state.getParameter(id);a->setValueNotifyingHost(1.f);
        }
        param(p,"mono",1);param(p,"mix",0);
        for(int k=0;k<50;++k) {
            juce::AudioBuffer<float> audio(2,128);audio.clear();juce::MidiBuffer midi;
            if(k==0) {audio.setSample(0,0,1);audio.setSample(1,0,-1);}
            p.processBlock(audio,midi);
            for(int s=0;s<128;++s) {require(std::isfinite(audio.getSample(0,s)),"FX instability");require(audio.getSample(0,s)==audio.getSample(1,s),"Mono audition mismatch");}
        }
        require(p.setBusesLayout(mono),"Cannot select mono");p.prepareToPlay(48000,128);
        juce::AudioBuffer<float> monoAudio(1,128);monoAudio.clear();juce::MidiBuffer noMidi;p.processBlock(monoAudio,noMidi);
        require(p.setBusesLayout(stereo),"Cannot restore stereo");
        // Reverb send: exact bypass, delayed wet onset and all room/toggle combinations.
        {
            IRChoir r;param(r,"mix",0);param(r,"gain",0);param(r,"room",1);
            param(r,"reverbOn",0);param(r,"reverbWetOnly",1);r.prepareToPlay(48000,128);
            juce::AudioBuffer<float> dry(2,128);juce::MidiBuffer midi;
            for(int n=0;n<128;++n){dry.setSample(0,n,0.1f);dry.setSample(1,n,-0.1f);}
            r.processBlock(dry,midi);
            require(std::abs(dry.getSample(0,100)-0.1f)<0.00001f,"Reverb bypass changed dry signal");
            param(r,"reverbOn",1);param(r,"revPredelay",100);r.prepareToPlay(48000,128);
            dry.clear();dry.setSample(0,0,1);dry.setSample(1,0,1);r.processBlock(dry,midi);
            require(dry.getMagnitude(0,128)<0.00001f,"Wet-only leaked dry signal before pre-delay");
            float tail=0;
            for(int k=0;k<100;++k){dry.clear();r.processBlock(dry,midi);tail=std::max(tail,dry.getMagnitude(0,128));}
            require(tail>0.00001f,"Reverb did not produce a delayed tail");
            for(int room=0;room<6;++room)for(int bits=0;bits<32;++bits) {
                param(r,"roomType",static_cast<float>(room));
                const char* ids[]={"reverbMonoSend","reverbLowCutOn","reverbHighCutOn","reverbFreeze","reverbWetOnly"};
                for(int j=0;j<5;++j)param(r,ids[j],static_cast<float>((bits>>j)&1));
                dry.clear();dry.setSample(0,0,0.5f);r.processBlock(dry,midi);
                for(int ch=0;ch<2;++ch)for(int n=0;n<128;++n)require(std::isfinite(dry.getSample(ch,n)),"Reverb room/toggle instability");
            }
            param(r,"reverbFreeze",1);juce::MemoryBlock state;r.getStateInformation(state);param(r,"reverbFreeze",0);
            r.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
            require(r.state.getRawParameterValue("reverbFreeze")->load()==1,"Reverb state not recalled");
        }
        // Direct file import: decode a real WAV and pass it through the processor.
        {
            irTestRoot.createDirectory();auto file=irTestRoot.getChildFile("Test vocal.wav");
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(file.createOutputStream().release(),44100,1,16,{},0));
            require(writer!=nullptr,"Cannot create import test WAV");
            juce::AudioBuffer<float> source(1,4410);
            for(int n=0;n<4410;++n)source.setSample(0,n,0.2f*std::sin(static_cast<float>(2*ir::pi*220*n/44100.)));
            require(writer->writeFromAudioSampleBuffer(source,0,4410),"Cannot write import WAV");writer.reset();
            IRChoir fileProcessor;param(fileProcessor,"mix",0);param(fileProcessor,"room",0);param(fileProcessor,"gain",0);
            require(fileProcessor.vocalFile.loadForTest(file),"WAV import failed");
            require(!fileProcessor.vocalFile.playing.load(),"Import auto-played");
            fileProcessor.prepareToPlay(48000,256);fileProcessor.vocalFile.togglePlayback();
            juce::AudioBuffer<float> out(2,256);juce::MidiBuffer midi;float energy=0;
            for(int k=0;k<25;++k){out.clear();fileProcessor.processBlock(out,midi);energy+=out.getMagnitude(0,256);}
            require(energy>0.1f,"Imported vocal did not reach DSP output");
            require(!fileProcessor.vocalFile.playing.load(),"Playback failed to stop at file end");
            fileProcessor.vocalFile.loop.store(true);fileProcessor.vocalFile.togglePlayback();
            for(int k=0;k<40;++k){out.clear();fileProcessor.processBlock(out,midi);}
            require(fileProcessor.vocalFile.playing.load(),"Loop failed across file end");
            // The imported recording also feeds the actual ensemble, not just preview playback.
            param(fileProcessor,"mix",1);fileProcessor.vocalFile.stop();fileProcessor.vocalFile.togglePlayback();
            float singerPeak=0;
            for(int k=0;k<30;++k){out.clear();fileProcessor.processBlock(out,midi);for(auto& peak:fileProcessor.meters)singerPeak=std::max(singerPeak,peak.load());}
            require(singerPeak>0.001f,"Imported vocal did not drive the choir layers");
            param(fileProcessor,"mix",0);fileProcessor.prepareToPlay(48000,256);
            fileProcessor.vocalFile.stop();out.clear();fileProcessor.processBlock(out,midi);
            require(out.getMagnitude(0,256)<0.00001f,"Stopped file leaked audio");
            fileProcessor.vocalFile.enabled.store(false);out.clear();out.applyGain(0);out.setSample(0,128,0.2f);fileProcessor.processBlock(out,midi);
            require(std::abs(out.getSample(0,128)-0.2f)<0.00001f,"DAW input switch failed");
            require(!fileProcessor.vocalFile.loadForTest(irTestRoot.getChildFile("missing.wav")),"Missing file accepted");
            require(fileProcessor.vocalFile.path()==file.getFullPathName(),"Failed import discarded prior vocal");
            juce::MemoryBlock saved;fileProcessor.getStateInformation(saved);
            auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));
            require(xml && xml->getStringAttribute("vocalFilePath")==file.getFullPathName(),"Project did not remember file reference");
        }
        // Each named design changes only the reverb and survives undo/state recall.
        {
            IRChoir spaces;param(spaces,"voices",2);param(spaces,"low",3);param(spaces,"delay",0.1f);
            spaces.prepareToPlay(48000,128);
            juce::MidiBuffer midi;juce::AudioBuffer<float> audio(2,128);
            std::set<juce::String> names;
            for(size_t i=0;i<ir::spacePresets.size();++i) {
                const auto& preset=ir::spacePresets[i];require(names.insert(preset.name).second,"Duplicate place name");
                spaces.applySpacePreset(i);
                require(spaces.state.state.getProperty("spacePresetName").toString()==preset.name,"Place label not saved");
                require(std::abs(spaces.state.getRawParameterValue("revPredelay")->load()-preset.predelay)<0.001f,"Place controls not applied");
                require(std::abs(spaces.state.getRawParameterValue("low")->load()-3)<0.00001f,"Place preset changed EQ");
                require(std::abs(spaces.state.getRawParameterValue("delay")->load()-0.1f)<0.00001f,"Place preset changed delay");
                require(spaces.state.getRawParameterValue("voices")->load()==2,"Place preset changed choir");
                for(int k=0;k<12;++k){audio.clear();if(k==0){audio.setSample(0,0,0.2f);audio.setSample(1,0,0.1f);}spaces.processBlock(audio,midi);
                    for(int ch=0;ch<2;++ch)for(int n=0;n<128;++n)require(std::isfinite(audio.getSample(ch,n)),"Place design generated non-finite audio");}
            }
            spaces.undoManager.clearUndoHistory();spaces.applySpacePreset(0);spaces.applySpacePreset(1);
            require(spaces.undoManager.undo(),"Place undo missing");
            require(spaces.state.state.getProperty("spacePresetName").toString()==ir::spacePresets[0].name,"Place undo did not restore label");
            require(std::abs(spaces.state.getRawParameterValue("revPredelay")->load()-ir::spacePresets[0].predelay)<0.001f,"Place undo did not restore controls");
            require(spaces.undoManager.redo(),"Place redo missing");
            juce::MemoryBlock saved;spaces.getStateInformation(saved);spaces.applySpacePreset(3);
            spaces.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
            require(spaces.state.state.getProperty("spacePresetName").toString()==ir::spacePresets[1].name,"Place state recall failed");
        }
        // Instantiate the actual native editor, resize, exercise every factory preset/page/formation/group.
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());require(editor!=nullptr,"No editor");
        editor->setSize(1180,960);editor->setSize(1500,1100);editor->setSize(1180,960);
        std::vector<juce::ComboBox*> boxes;std::vector<juce::TextButton*> buttons;findComponents(*editor,boxes,buttons);
        for(auto* box:boxes) {
            if(box->getNumItems()==9)for(int id=1;id<=9;++id)box->setSelectedId(id,juce::sendNotificationSync);
            if(box->getItemText(0)=="Performance controls")for(int id=1;id<=7;++id)box->setSelectedId(id,juce::sendNotificationSync);
            if(box->getItemText(0)=="Formation: semicircle")for(int id=1;id<=3;++id)box->setSelectedId(id,juce::sendNotificationSync);
        }
        auto* placeBox=dynamic_cast<juce::ComboBox*>(editor->findChildWithID("place-preset"));
        require(placeBox && placeBox->getNumItems()==55,"Place browser incomplete");
        for(int id=1;id<=55;++id)placeBox->setSelectedId(id,juce::sendNotificationSync);
        for(auto* button:buttons) {
            auto name=button->getButtonText();
            if(name.contains("group") || name=="All singers" || name=="A" || name=="B" || name=="Copy" || name=="Clear clip light")
                if(button->onClick)button->onClick();
        }
        // Custom presets, favourites and A/B use an isolated temporary folder.
        juce::TextButton *save=nullptr,*star=nullptr,*filter=nullptr,*abButton=nullptr;
        for(auto* button:buttons) {
            auto text=button->getButtonText();if(text=="Save")save=button;if(text=="Star")star=button;if(text=="Favourites")filter=button;
            if(text=="A" || text=="B")abButton=button;
        }
        require(save && star && filter && abButton,"Preset UI missing");
        auto* name=dynamic_cast<juce::TextEditor*>(editor->findChildWithID("preset-name"));require(name!=nullptr,"No preset name input");
        name->setText("Smoke preset");param(p,"low",5);save->onClick();
        auto files=irTestRoot.getChildFile("Presets").findChildFiles(juce::File::findFiles,false,"*.irchoir.xml");require(files.size()==1,"Preset not saved");
        param(p,"low",-5);
        for(auto* box:boxes)if(box->indexOfItemId(1000)>=0)box->setSelectedId(1000,juce::sendNotificationSync);
        require(std::abs(p.state.getRawParameterValue("low")->load()-5)<0.00001f,"Custom preset not loaded");
        star->onClick();require(irTestRoot.getChildFile("favourites.txt").loadFileAsString().isNotEmpty(),"Favourite not saved");
        filter->setToggleState(true,juce::dontSendNotification);filter->onClick();
        param(p,"low",4);p.usingB=false;abButton->onClick();param(p,"low",-4);abButton->onClick();
        require(std::abs(p.state.getRawParameterValue("low")->load()-4)<0.00001f,"A/B recall failed");
        // Exercise the real conductor drag handlers.
        auto event=[&](float x,float y) {auto time=juce::Time::getCurrentTime();return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),{x,y},juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,editor.get(),editor.get(),time,{x,y},time,1,false);};
        editor->mouseDown(event(1000,260));editor->mouseDrag(event(1100,230));editor->mouseUp(event(1100,230));
        require(p.state.getRawParameterValue("space")->load()>0.7f,"Conductor drag did not update sound");
        // Capture a clean performance view after real test audio, rather than the mutated stress-test state.
        filter->setToggleState(false,juce::dontSendNotification);filter->onClick();
        for(auto* box:boxes)if(box->indexOfItemId(1000)>=0)box->setSelectedId(2,juce::sendNotificationSync);
        for(auto* box:boxes) {
            if(box->getItemText(0)=="Performance controls")box->setSelectedId(1,juce::sendNotificationSync);
            if(box->getItemText(0)=="Formation: semicircle")box->setSelectedId(1,juce::sendNotificationSync);
        }
        editor->setSize(1180,840);
        // Empty-stage click clears group selection; drag and double-click a real singer.
        p.state.copyState();p.undoManager.clearUndoHistory();
        editor->mouseDown(event(20,80));editor->mouseUp(event(20,80));
        editor->mouseDown(event(80,240));editor->mouseDrag(event(100,232));editor->mouseUp(event(100,232));
        require(p.state.getRawParameterValue("pan0")->load()>-1,"Singer pan drag failed");
        require(p.state.getRawParameterValue("level0")->load()>0.8f,"Singer level drag failed");
        require(p.undoManager.undo(),"Undo not recorded");
        require(std::abs(p.state.getRawParameterValue("pan0")->load()+1)<0.001f,"Undo pan failed");
        require(p.undoManager.redo(),"Redo not recorded");
        require(p.state.getRawParameterValue("pan0")->load()>-1,"Redo pan failed");
        editor->mouseDoubleClick(event(100,238));require(p.state.getRawParameterValue("mute0")->load()>0.5f,"Singer double-click mute failed");
        editor->mouseDoubleClick(event(100,238));
        p.clipped.store(false);
        p.prepareToPlay(48000,256);
        for(int k=0;k<32;++k) {
            juce::AudioBuffer<float> input(2,256);juce::MidiBuffer noNotes;
            for(int n=0;n<256;++n) {float x=0.25f*std::sin(static_cast<float>(2*ir::pi*220*(k*256+n)/48000.));input.setSample(0,n,x);input.setSample(1,n,x);}
            p.processBlock(input,noNotes);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil(80);
        name->clear();
        if(auto* status=dynamic_cast<juce::Label*>(editor->findChildWithID("status")))
            status->setText("Select singers to shape your ensemble. Drag to adjust stereo position and level.",juce::dontSendNotification);
        // Paint the native editor to a PNG for visual inspection.
        auto image=editor->createComponentSnapshot(editor->getLocalBounds());juce::PNGImageFormat png;
        juce::File output=juce::File::getCurrentWorkingDirectory().getChildFile("IRChoir-editor-smoke.png");
        juce::FileOutputStream stream(output);require(stream.openedOk(),"Cannot write editor snapshot");stream.setPosition(0);require(stream.truncate().wasOk(),"Cannot replace old snapshot");require(png.writeImageToStream(image,stream),"Snapshot encoding failed");
        for(auto* box:boxes)if(box->getItemText(0)=="Performance controls")box->setSelectedId(7,juce::sendNotificationSync);
        placeBox->setSelectedId(11,juce::sendNotificationSync);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(80);
        auto reverbImage=editor->createComponentSnapshot(editor->getLocalBounds());
        juce::FileOutputStream reverbStream(juce::File::getCurrentWorkingDirectory().getChildFile("IRChoir-reverb-smoke.png"));
        reverbStream.setPosition(0);require(reverbStream.truncate().wasOk(),"Reverb snapshot truncate failed");
        require(png.writeImageToStream(reverbImage,reverbStream),"Reverb snapshot failed");
        editor.reset();irTestRoot.deleteRecursively();
        std::cout<<"PASS: processor sample rates/block sizes/modes, MIDI sustain/channels/panic, state recall, FX extremes, mono, native editor and factory UI controls\n";
        return 0;
    } catch(const std::exception& e) {irTestRoot.deleteRecursively();std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
