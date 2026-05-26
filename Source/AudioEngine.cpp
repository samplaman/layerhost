#include "AudioEngine.h"
#include <juce_clap_hosting/juce_clap_hosting.h>

AudioEngine::AudioEngine() : playHead(*this)
{
    mainGraph = std::make_unique<juce::AudioProcessorGraph>();
    
    threadPool = std::make_unique<juce::ThreadPool> (juce::SystemStats::getNumCpus());
    for (int i = 0; i < 128; ++i)
        trackJobs.push_back (std::make_unique<TrackJob>());
        
    deviceManager.initialiseWithDefaultDevices (2, 2);
    deviceManager.addAudioCallback (this);
}

AudioEngine::~AudioEngine()
{
    deviceManager.removeMidiInputDeviceCallback("", this);
    deviceManager.removeAudioCallback (this);
    processorPlayer.setProcessor (nullptr);
    threadPool = nullptr; // block and destroy thread pool first
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    currentSampleRate = device->getCurrentSampleRate();
    processorPlayer.audioDeviceAboutToStart (device);
}

void AudioEngine::audioDeviceStopped()
{
    processorPlayer.audioDeviceStopped();
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                                    float* const* outputChannelData, int numOutputChannels,
                                                    int numSamples, const juce::AudioIODeviceCallbackContext& context)
{
    const juce::ScopedTryLock sl (engineLock);
    if (sl.isLocked())
    {
        // Run pre-rendering of eligible tracks in parallel using our ThreadPool
        int jobCount = 0;
        std::atomic<int> jobsRemaining { 0 };
        double playheadSec = playPosition.load();

        for (auto* track : tracks)
        {
            if (track != nullptr && track->isEligibleForPreRender())
            {
                if (jobCount < (int)trackJobs.size())
                {
                    trackJobs[jobCount]->setup (track, numSamples, playheadSec, currentSampleRate, isPlaying, &jobsRemaining);
                    jobsRemaining++;
                    threadPool->addJob (trackJobs[jobCount].get(), false);
                    jobCount++;
                }
            }
        }

        // Wait for parallel track rendering jobs to complete
        while (jobsRemaining.load() > 0)
        {
            juce::Thread::yield();
        }

        processorPlayer.audioDeviceIOCallbackWithContext (inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples, context);

        if (isPlaying)
        {
            if (metronomeEnabled)
            {
                double bps = bpm / 60.0;
                double blockStartSec = playPosition.load();
                double blockEndSec = playPosition.load() + ((double)numSamples / currentSampleRate);
                
                double beatStart = blockStartSec * bps;
                double beatEnd = blockEndSec * bps;
                
                if (std::floor(beatStart) != std::floor(beatEnd))
                {
                    double nextBeatInSecs = (std::floor(beatStart) + 1.0) / bps;
                    int sampleOffset = (int)((nextBeatInSecs - playPosition.load()) * currentSampleRate);
                    sampleOffset = juce::jlimit (0, numSamples - 1, sampleOffset);
                    
                    int clickBeatIndex = (int)std::floor(beatEnd);
                    bool isDownbeat = (clickBeatIndex % 4 == 0);
                    
                    float freq = isDownbeat ? 1200.0f : 800.0f;
                    metronomePhaseIncrement = (float)(2.0 * juce::MathConstants<double>::pi * freq / currentSampleRate);
                    metronomePhase = 0.0f;
                    metronomeToneSampleCount = (int)(0.03 * currentSampleRate); // 30ms click
                    metronomeClickStartOffset = sampleOffset;
                }
            }
            
            if (metronomeToneSampleCount > 0)
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    if (i >= metronomeClickStartOffset && metronomeToneSampleCount > 0)
                    {
                        float envelope = (float)metronomeToneSampleCount / (0.03f * (float)currentSampleRate);
                        float clickSample = std::sin (metronomePhase) * 0.15f * envelope;
                        metronomePhase += metronomePhaseIncrement;
                        
                        for (int ch = 0; ch < numOutputChannels; ++ch)
                        {
                            if (outputChannelData[ch] != nullptr)
                                outputChannelData[ch][i] += clickSample;
                        }
                        metronomeToneSampleCount--;
                    }
                }
                metronomeClickStartOffset = 0;
            }

            playPosition.store (playPosition.load() + (double)numSamples / currentSampleRate);
            if (isLooping && loopEnd > loopStart)
            {
                if (playPosition.load() >= loopEnd)
                    playPosition.store (loopStart + std::fmod (playPosition.load() - loopEnd, loopEnd - loopStart));
            }

            lastCallbackTime.store (juce::Time::getMillisecondCounterHiRes());
            lastNumSamples.store (numSamples);
        }
        else
        {
            metronomeToneSampleCount = 0;
            metronomeClickStartOffset = 0;
        }
    }
    else
    {
        metronomeToneSampleCount = 0;
        metronomeClickStartOffset = 0;
        for (int i = 0; i < numOutputChannels; ++i)
            if (outputChannelData[i] != nullptr)
                juce::FloatVectorOperations::clear (outputChannelData[i], numSamples);
    }
}

void AudioEngine::initialise()
{
    formatManager.registerBasicFormats();
    pluginFormatManager.addDefaultFormats();
    pluginFormatManager.addFormat (new juce::CLAPPluginFormat());

    processorPlayer.setProcessor (mainGraph.get());
    mainGraph->setPlayHead (&playHead);

    mainGraph->clear();

    audioInputNode = mainGraph->addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    midiInputNode = mainGraph->addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (juce::AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode));
    audioOutputNode = mainGraph->addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
    midiOutputNode = mainGraph->addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (juce::AudioProcessorGraph::AudioGraphIOProcessor::midiOutputNode));

    auto mTrack = std::make_unique<Track>();
    mTrack->setName("Master");
    mTrack->setAsMasterTrack(true);
    masterTrack = mTrack.get();
    masterNode = mainGraph->addNode(std::move(mTrack));

    for (int i = 0; i < 2; ++i)
        mainGraph->addConnection({ { masterNode->nodeID, i }, { audioOutputNode->nodeID, i } });

    auto midiInputs = juce::MidiInput::getAvailableDevices();
    for (auto& input : midiInputs)
        deviceManager.setMidiInputDeviceEnabled(input.identifier, true);
    deviceManager.addMidiInputDeviceCallback("", this);
}

void AudioEngine::setPlaying (bool shouldPlay)
{
    if (isPlaying == shouldPlay) return;
    isPlaying = shouldPlay;

    if (isPlaying)
    {
        lastCallbackTime.store (juce::Time::getMillisecondCounterHiRes());
    }

    if (!isPlaying && isRecording)
    {
        for (auto* t : tracks) t->stopRecording(playPosition.load());
    }
    else if (isPlaying && isRecording)
    {
        for (auto* t : tracks) t->startRecording(playPosition.load());
    }
}

void AudioEngine::setRecording (bool shouldRecord)
{
    if (isRecording == shouldRecord) return;
    isRecording = shouldRecord;
    
    if (isPlaying && isRecording)
    {
        for (auto* t : tracks) t->startRecording(playPosition.load());
    }
    else if (!isRecording)
    {
        for (auto* t : tracks) t->stopRecording(playPosition.load());
    }
}

void AudioEngine::handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message)
{
    if (isRecording && isPlaying)
    {
        for (auto* t : tracks)
        {
            if (t->getArmed())
                t->addMidiEvent (message, playPosition.load());
        }
    }
}

void AudioEngine::addTrack(Track::Type type, int midiChannel)
{
    const juce::ScopedLock sl (engineLock);

    while (trackJobs.size() <= tracks.size())
    {
        trackJobs.push_back (std::make_unique<TrackJob>());
    }

    auto newTrack = std::make_unique<Track>(type);
    if (type == Track::Type::Midi)
    {
        newTrack->setMidiChannel (midiChannel);
        juce::String chStr = (midiChannel == 0) ? "Omni" : ("Ch " + juce::String (midiChannel));
        newTrack->setName ("MIDI " + juce::String(tracks.size() + 1) + " (" + chStr + ")");
    }
    else
    {
        newTrack->setName ("Track " + juce::String(tracks.size() + 1));
    }
    auto* trackPtr = newTrack.get();
    
    auto node = mainGraph->addNode (std::move (newTrack));
    trackNodes.push_back (node);
    
    tracks.push_back (trackPtr);
    updateRouting();
}

void AudioEngine::removeTrack(int index)
{
    const juce::ScopedLock sl (engineLock);
    if (index >= 0 && index < tracks.size())
    {
        mainGraph->removeNode (trackNodes[index].get());
        tracks.erase (tracks.begin() + index);
        trackNodes.erase (trackNodes.begin() + index);
        updateRouting();
    }
}

void AudioEngine::updateRouting()
{
    // Disconnect all tracks from output and from each other
    for (auto node : trackNodes)
    {
        mainGraph->disconnectNode(node->nodeID);
    }
    mainGraph->disconnectNode(masterNode->nodeID);

    // Reconnect master to output
    for (int i = 0; i < 2; ++i)
        mainGraph->addConnection({ { masterNode->nodeID, i }, { audioOutputNode->nodeID, i } });

    // Reconnect tracks to their destinations
    for (size_t i = 0; i < tracks.size(); ++i)
    {
        auto* track = tracks[i];
        auto node = trackNodes[i];
        
        int destIdx = track->getRouting();
        juce::AudioProcessorGraph::Node::Ptr destNode = masterNode;
        if (destIdx >= 0 && destIdx < (int)tracks.size() && destIdx != (int)i)
        {
            destNode = trackNodes[destIdx];
        }

        for (int ch = 0; ch < 2; ++ch)
            mainGraph->addConnection({ { node->nodeID, ch }, { destNode->nodeID, ch } });
            
        // If armed or monitor enabled, connect input to it
        if ((track->getArmed() || track->getMonitor()) && track->getTrackType() == Track::Type::Audio && audioInputNode != nullptr)
        {
            int inCh = track->getInputChannel();
            if (inCh == -1) // Stereo
            {
                mainGraph->addConnection({ { audioInputNode->nodeID, 0 }, { node->nodeID, 0 } });
                mainGraph->addConnection({ { audioInputNode->nodeID, 1 }, { node->nodeID, 1 } });
            }
            else // Mono input duplicated to stereo
            {
                mainGraph->addConnection({ { audioInputNode->nodeID, inCh }, { node->nodeID, 0 } });
                mainGraph->addConnection({ { audioInputNode->nodeID, inCh }, { node->nodeID, 1 } });
            }
        }
        else if ((track->getArmed() || track->getMonitor()) && track->getTrackType() == Track::Type::Midi && midiInputNode != nullptr)
        {
            mainGraph->addConnection ({ { midiInputNode->nodeID, juce::AudioProcessorGraph::midiChannelIndex },
                                        { node->nodeID, juce::AudioProcessorGraph::midiChannelIndex } });
        }
    }
}

juce::AudioProcessor* AudioEngine::loadPlugin (const juce::String& filePath, Track* track, int slotIdx)
{
    juce::File file (filePath);
    if (!file.exists()) return nullptr;
    
    juce::String formatName = file.hasFileExtension (".clap") ? "CLAP" : "VST3";
    juce::AudioPluginFormat* format = nullptr;
    for (int i = 0; i < pluginFormatManager.getNumFormats(); ++i)
    {
        if (auto* f = pluginFormatManager.getFormat(i))
        {
            if (f->getName() == formatName)
            {
                format = f;
                break;
            }
        }
    }
    
    if (format != nullptr)
    {
        juce::OwnedArray<juce::PluginDescription> typesFound;
        format->findAllTypesForFile (typesFound, filePath);
        if (typesFound.size() > 0)
        {
            auto* audioDevice = deviceManager.getCurrentAudioDevice();
            double sampleRate = audioDevice ? audioDevice->getCurrentSampleRate() : 44100.0;
            int bufferSize = audioDevice ? audioDevice->getDefaultBufferSize() : 512;
            
            juce::String error;
            auto plugin = pluginFormatManager.createPluginInstance (*typesFound[0], sampleRate, bufferSize, error);
            if (plugin != nullptr)
            {
                auto* p = plugin.get();
                track->setPlugin (slotIdx, std::move (plugin), sampleRate, bufferSize);
                return p;
            }
        }
    }
    return nullptr;
}

std::unique_ptr<juce::XmlElement> AudioEngine::saveProjectToXml()
{
    const juce::ScopedLock sl (engineLock);
    auto root = std::make_unique<juce::XmlElement> ("LAYERHOST_PROJECT");
    root->setAttribute ("bpm", bpm);
    root->setAttribute ("metronome", metronomeEnabled ? 1 : 0);
    root->setAttribute ("snap", (int)snapDivision);

    // Save Master Track
    if (masterTrack != nullptr)
    {
        auto* masterXml = root->createNewChildElement ("MASTER_TRACK");
        masterXml->setAttribute ("volume", masterTrack->getVolume());
        masterXml->setAttribute ("pan", masterTrack->getPan());
        masterXml->setAttribute ("mute", masterTrack->getMute() ? 1 : 0);
        
        // Save Master FX Chain
        for (int slot = 0; slot < 8; ++slot)
        {
            if (auto* plugin = masterTrack->getPlugin (slot))
            {
                if (auto* instance = dynamic_cast<juce::AudioPluginInstance*> (plugin))
                {
                    auto* pluginXml = masterXml->createNewChildElement ("PLUGIN");
                    pluginXml->setAttribute ("slot", slot);
                    pluginXml->setAttribute ("filePath", instance->getPluginDescription().fileOrIdentifier);
                    
                    juce::MemoryBlock state;
                    plugin->getStateInformation (state);
                    pluginXml->setAttribute ("state", juce::String::toHexString (state.getData(), (int)state.getSize()));
                }
            }
        }
    }

    // Save Tracks
    for (auto* track : tracks)
    {
        auto* trackXml = root->createNewChildElement ("TRACK");
        trackXml->setAttribute ("type", track->getTrackType() == Track::Type::Midi ? "midi" : "audio");
        trackXml->setAttribute ("name", track->getName());
        trackXml->setAttribute ("volume", track->getVolume());
        trackXml->setAttribute ("pan", track->getPan());
        trackXml->setAttribute ("mute", track->getMute() ? 1 : 0);
        trackXml->setAttribute ("solo", track->getSolo() ? 1 : 0);
        trackXml->setAttribute ("armed", track->getArmed() ? 1 : 0);
        trackXml->setAttribute ("monitor", track->getMonitor() ? 1 : 0);
        trackXml->setAttribute ("inputChannel", track->getInputChannel());
        trackXml->setAttribute ("midiChannel", track->getMidiChannel());
        trackXml->setAttribute ("routing", track->getRouting());
        trackXml->setAttribute ("height", track->getHeight());
        trackXml->setAttribute ("volumeAutomationEnabled", track->getVolumeAutomationEnabled() ? 1 : 0);
        trackXml->setAttribute ("panAutomationEnabled", track->getPanAutomationEnabled() ? 1 : 0);

        // Save Volume Automation Points
        auto* volAutoXml = trackXml->createNewChildElement ("VOLUME_AUTOMATION");
        {
            auto& volAuto = track->getVolumeAutomation();
            const juce::ScopedLock sl (volAuto.lock);
            for (const auto& point : volAuto.points)
            {
                auto* ptXml = volAutoXml->createNewChildElement ("POINT");
                ptXml->setAttribute ("time", point.time);
                ptXml->setAttribute ("value", point.value);
                ptXml->setAttribute ("curve", (double)point.curve);
            }
        }

        // Save Pan Automation Points
        auto* panAutoXml = trackXml->createNewChildElement ("PAN_AUTOMATION");
        {
            auto& panAuto = track->getPanAutomation();
            const juce::ScopedLock sl (panAuto.lock);
            for (const auto& point : panAuto.points)
            {
                auto* ptXml = panAutoXml->createNewChildElement ("POINT");
                ptXml->setAttribute ("time", point.time);
                ptXml->setAttribute ("value", point.value);
                ptXml->setAttribute ("curve", (double)point.curve);
            }
        }

        // Save Plugin Parameter Automations
        for (const auto& pa : track->getPluginAutomations())
        {
            if (pa != nullptr)
            {
                const juce::ScopedLock sl (pa->envelope.lock);
                if (pa->enabled || !pa->envelope.points.empty())
                {
                    auto* plugAutoXml = trackXml->createNewChildElement ("PLUGIN_AUTOMATION");
                    plugAutoXml->setAttribute ("slot", pa->slotIndex);
                    plugAutoXml->setAttribute ("paramIdx", pa->parameterIndex);
                    plugAutoXml->setAttribute ("enabled", pa->enabled ? 1 : 0);
                    
                    for (const auto& point : pa->envelope.points)
                    {
                        auto* ptXml = plugAutoXml->createNewChildElement ("POINT");
                        ptXml->setAttribute ("time", point.time);
                        ptXml->setAttribute ("value", point.value);
                        ptXml->setAttribute ("curve", (double)point.curve);
                    }
                }
            }
        }

        // Save FX Chain
        for (int slot = 0; slot < 8; ++slot)
        {
            if (auto* plugin = track->getPlugin (slot))
            {
                if (auto* instance = dynamic_cast<juce::AudioPluginInstance*> (plugin))
                {
                    auto* pluginXml = trackXml->createNewChildElement ("PLUGIN");
                    pluginXml->setAttribute ("slot", slot);
                    pluginXml->setAttribute ("filePath", instance->getPluginDescription().fileOrIdentifier);
                    
                    juce::MemoryBlock state;
                    plugin->getStateInformation (state);
                    pluginXml->setAttribute ("state", juce::String::toHexString (state.getData(), (int)state.getSize()));
                }
            }
        }

        // Save Items
        for (auto& item : track->getItems())
        {
            if (item != nullptr)
            {
                auto* itemXml = trackXml->createNewChildElement ("ITEM");
                itemXml->setAttribute ("type", item->getType() == Item::Type::Midi ? "midi" : "audio");
                itemXml->setAttribute ("startTime", item->getStartTime());
                itemXml->setAttribute ("length", item->getLength());
                itemXml->setAttribute ("sourceOffset", item->getSourceOffset());
                itemXml->setAttribute ("fadeIn", item->getFadeIn());
                itemXml->setAttribute ("fadeOut", item->getFadeOut());
                itemXml->setAttribute ("filePath", item->getFilePath());
                itemXml->setAttribute ("muted", item->getMuted() ? 1 : 0);

                if (item->getType() == Item::Type::Midi)
                {
                    auto& seq = item->getMidiSequence();
                    for (int evIdx = 0; evIdx < seq.getNumEvents(); ++evIdx)
                    {
                        auto* ev = seq.getEventPointer (evIdx);
                        if (ev != nullptr)
                        {
                            auto* evXml = itemXml->createNewChildElement ("MIDI_EVENT");
                            evXml->setAttribute ("time", ev->message.getTimeStamp());
                            
                            juce::String hex = juce::String::toHexString (ev->message.getRawData(), ev->message.getRawDataSize());
                            evXml->setAttribute ("bytes", hex);
                        }
                    }
                }
            }
        }
    }

    return root;
}

bool AudioEngine::loadProjectFromXml (const juce::XmlElement& xml)
{
    const juce::ScopedLock sl (engineLock);

    if (!xml.hasTagName ("LAYERHOST_PROJECT") && !xml.hasTagName ("VIBEDAW_PROJECT"))
        return false;

    // Stop playback
    setPlaying (false);

    // Clear existing tracks
    while (!tracks.empty())
        removeTrack (0);

    // Clear master plugins
    if (masterTrack != nullptr)
    {
        for (int slot = 0; slot < 8; ++slot)
            masterTrack->setPlugin (slot, nullptr, 44100.0, 512);
    }

    // Restore BPM, Metronome, Snap
    bpm = xml.getDoubleAttribute ("bpm", 120.0);
    metronomeEnabled = xml.getIntAttribute ("metronome", 0) != 0;
    snapDivision = (SnapDivision)xml.getIntAttribute ("snap", 0);

    // Restore Master Track properties
    if (auto* masterXml = xml.getChildByName ("MASTER_TRACK"))
    {
        if (masterTrack != nullptr)
        {
            masterTrack->setVolume ((float)masterXml->getDoubleAttribute ("volume", 1.0));
            masterTrack->setPan ((float)masterXml->getDoubleAttribute ("pan", 0.0));
            masterTrack->setMute (masterXml->getIntAttribute ("mute", 0) != 0);

            // Restore Master FX Chain
            auto* pluginXml = masterXml->getChildByName ("PLUGIN");
            while (pluginXml != nullptr)
            {
                int slot = pluginXml->getIntAttribute ("slot", 0);
                juce::String filePath = pluginXml->getStringAttribute ("filePath");
                juce::String stateHex = pluginXml->getStringAttribute ("state");

                if (auto* plugin = loadPlugin (filePath, masterTrack, slot))
                {
                    if (!stateHex.isEmpty())
                    {
                        juce::MemoryBlock state;
                        state.loadFromHexString (stateHex);
                        plugin->setStateInformation (state.getData(), (int)state.getSize());
                    }
                }
                pluginXml = pluginXml->getNextElementWithTagName ("PLUGIN");
            }
        }
    }

    // Restore Tracks
    auto* trackXml = xml.getChildByName ("TRACK");
    while (trackXml != nullptr)
    {
        juce::String typeStr = trackXml->getStringAttribute ("type");
        Track::Type type = (typeStr == "midi") ? Track::Type::Midi : Track::Type::Audio;

        // Add track
        addTrack (type);
        auto* track = tracks.back();

        track->setName (trackXml->getStringAttribute ("name"));
        track->setVolume ((float)trackXml->getDoubleAttribute ("volume", 1.0));
        track->setPan ((float)trackXml->getDoubleAttribute ("pan", 0.0));
        track->setMute (trackXml->getIntAttribute ("mute", 0) != 0);
        track->setSolo (trackXml->getIntAttribute ("solo", 0) != 0);
        track->setArmed (trackXml->getIntAttribute ("armed", 0) != 0);
        track->setMonitor (trackXml->getIntAttribute ("monitor", 0) != 0);
        track->setInputChannel (trackXml->getIntAttribute ("inputChannel", -1));
        track->setMidiChannel (trackXml->getIntAttribute ("midiChannel", 0));
        track->setRouting (trackXml->getIntAttribute ("routing", -1));
        track->setHeight (trackXml->getIntAttribute ("height", 80));
        track->setVolumeAutomationEnabled (trackXml->getIntAttribute ("volumeAutomationEnabled", trackXml->getIntAttribute ("automationEnabled", 0)) != 0);
        track->setPanAutomationEnabled (trackXml->getIntAttribute ("panAutomationEnabled", 0) != 0);

        // Restore Volume Automation Points
        track->getVolumeAutomation().clear();
        if (auto* autoXml = trackXml->getChildByName ("VOLUME_AUTOMATION"))
        {
            auto* ptXml = autoXml->getChildByName ("POINT");
            while (ptXml != nullptr)
            {
                double ptTime = ptXml->getDoubleAttribute ("time");
                float ptValue = (float)ptXml->getDoubleAttribute ("value");
                float ptCurve = (float)ptXml->getDoubleAttribute ("curve", 0.0);
                track->getVolumeAutomation().addOrUpdatePoint (ptTime, ptValue, ptCurve);
                ptXml = ptXml->getNextElementWithTagName ("POINT");
            }
        }
        else if (auto* autoXml = trackXml->getChildByName ("AUTOMATION")) // fallback
        {
            auto* ptXml = autoXml->getChildByName ("POINT");
            while (ptXml != nullptr)
            {
                double ptTime = ptXml->getDoubleAttribute ("time");
                float ptValue = (float)ptXml->getDoubleAttribute ("value");
                float ptCurve = (float)ptXml->getDoubleAttribute ("curve", 0.0);
                track->getVolumeAutomation().addOrUpdatePoint (ptTime, ptValue, ptCurve);
                ptXml = ptXml->getNextElementWithTagName ("POINT");
            }
        }

        // Restore Pan Automation Points
        track->getPanAutomation().clear();
        if (auto* autoXml = trackXml->getChildByName ("PAN_AUTOMATION"))
        {
            auto* ptXml = autoXml->getChildByName ("POINT");
            while (ptXml != nullptr)
            {
                double ptTime = ptXml->getDoubleAttribute ("time");
                float ptValue = (float)ptXml->getDoubleAttribute ("value");
                float ptCurve = (float)ptXml->getDoubleAttribute ("curve", 0.0);
                track->getPanAutomation().addOrUpdatePoint (ptTime, ptValue, ptCurve);
                ptXml = ptXml->getNextElementWithTagName ("POINT");
            }
        }

        // Restore FX Chain
        auto* pluginXml = trackXml->getChildByName ("PLUGIN");
        while (pluginXml != nullptr)
        {
            int slot = pluginXml->getIntAttribute ("slot", 0);
            juce::String filePath = pluginXml->getStringAttribute ("filePath");
            juce::String stateHex = pluginXml->getStringAttribute ("state");

            if (auto* plugin = loadPlugin (filePath, track, slot))
            {
                if (!stateHex.isEmpty())
                {
                    juce::MemoryBlock state;
                    state.loadFromHexString (stateHex);
                    plugin->setStateInformation (state.getData(), (int)state.getSize());
                }
            }
            pluginXml = pluginXml->getNextElementWithTagName ("PLUGIN");
        }

        // Restore Plugin Parameter Automations
        track->clearPluginAutomations();
        auto* plugAutoXml = trackXml->getChildByName ("PLUGIN_AUTOMATION");
        while (plugAutoXml != nullptr)
        {
            int slot = plugAutoXml->getIntAttribute ("slot", -1);
            int paramIdx = plugAutoXml->getIntAttribute ("paramIdx", -1);
            bool enabled = plugAutoXml->getIntAttribute ("enabled", 0) != 0;
            
            if (slot >= 0 && paramIdx >= 0)
            {
                auto* pa = track->getOrCreatePluginAutomation (slot, paramIdx);
                if (pa != nullptr)
                {
                    pa->enabled = enabled;
                    pa->envelope.clear();
                    
                    auto* ptXml = plugAutoXml->getChildByName ("POINT");
                    while (ptXml != nullptr)
                    {
                        double ptTime = ptXml->getDoubleAttribute ("time");
                        float ptValue = (float)ptXml->getDoubleAttribute ("value");
                        float ptCurve = (float)ptXml->getDoubleAttribute ("curve", 0.0);
                        pa->envelope.addOrUpdatePoint (ptTime, ptValue, ptCurve);
                        ptXml = ptXml->getNextElementWithTagName ("POINT");
                    }
                }
            }
            plugAutoXml = plugAutoXml->getNextElementWithTagName ("PLUGIN_AUTOMATION");
        }

        // Restore Items
        auto* itemXml = trackXml->getChildByName ("ITEM");
        while (itemXml != nullptr)
        {
            auto item = std::make_unique<Item>();
            juce::String itemTypeStr = itemXml->getStringAttribute ("type");
            item->setType (itemTypeStr == "midi" ? Item::Type::Midi : Item::Type::Audio);
            item->setStartTime (itemXml->getDoubleAttribute ("startTime"));
            item->setLength (itemXml->getDoubleAttribute ("length"));
            item->setSourceOffset (itemXml->getDoubleAttribute ("sourceOffset"));
            item->setFadeIn (itemXml->getDoubleAttribute ("fadeIn"));
            item->setFadeOut (itemXml->getDoubleAttribute ("fadeOut"));
            item->setMuted (itemXml->getIntAttribute ("muted", 0) != 0);
            
            juce::String itemFilePath = itemXml->getStringAttribute ("filePath");
            if (!itemFilePath.isEmpty() && item->getType() == Item::Type::Audio)
            {
                juce::File f (itemFilePath);
                if (f.existsAsFile())
                {
                    item->loadFile (f, formatManager, currentSampleRate);
                }
            }
            
            if (item->getType() == Item::Type::Midi)
            {
                auto* evXml = itemXml->getChildByName ("MIDI_EVENT");
                while (evXml != nullptr)
                {
                    double time = evXml->getDoubleAttribute ("time");
                    juce::String bytesHex = evXml->getStringAttribute ("bytes");
                    
                    if (!bytesHex.isEmpty())
                    {
                        juce::MemoryBlock mb;
                        mb.loadFromHexString (bytesHex);
                        juce::MidiMessage msg (mb.getData(), (int)mb.getSize(), time);
                        item->getMidiSequence().addEvent (msg);
                    }
                    
                    evXml = evXml->getNextElementWithTagName ("MIDI_EVENT");
                }
                item->getMidiSequence().updateMatchedPairs();
            }

            track->addItem (std::move (item));
            itemXml = itemXml->getNextElementWithTagName ("ITEM");
        }

        trackXml = trackXml->getNextElementWithTagName ("TRACK");
    }

    updateRouting();
    return true;
}
