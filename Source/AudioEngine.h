#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "Track.h"

class AudioEngine : public juce::AudioIODeviceCallback, public juce::MidiInputCallback
{
public:
    AudioEngine();
    ~AudioEngine();

    void initialise();
    
    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }
    juce::AudioProcessorGraph& getGraph() { return *mainGraph; }

    void addTrack (Track::Type type = Track::Type::Audio, int midiChannel = 0);
    void removeTrack (int index);
    void moveTrack (int fromIndex, int toIndex)
    {
        if (fromIndex >= 0 && fromIndex < tracks.size() && toIndex >= 0 && toIndex < tracks.size())
        {
            auto t = tracks[fromIndex];
            tracks.erase (tracks.begin() + fromIndex);
            tracks.insert (tracks.begin() + toIndex, t);
            
            auto n = trackNodes[fromIndex];
            trackNodes.erase (trackNodes.begin() + fromIndex);
            trackNodes.insert (trackNodes.begin() + toIndex, n);
        }
    }
    int getNumTracks() const { return tracks.size(); }
    Track* getTrack (int index) const 
    { 
        if (index >= 0 && index < (int)tracks.size())
            return tracks[index]; 
        return nullptr;
    }
    Track* getMasterTrack() const { return masterTrack; }

    std::unique_ptr<juce::XmlElement> saveProjectToXml();
    bool loadProjectFromXml (const juce::XmlElement& xml);
    juce::AudioProcessor* loadPlugin (const juce::String& filePath, Track* track, int slotIdx);

    void updateRouting();

    juce::AudioFormatManager& getFormatManager() { return formatManager; }
    juce::AudioPluginFormatManager& getPluginFormatManager() { return pluginFormatManager; }

    void setPlaying (bool shouldPlay);
    bool getPlaying() const { return isPlaying; }
    void setRecording (bool shouldRecord);
    bool getRecording() const { return isRecording; }
    
    void setLooping (bool shouldLoop) { isLooping = shouldLoop; }
    bool getLooping() const { return isLooping; }
    void setLoopRange (double start, double end) { loopStart = start; loopEnd = end; }
    double getLoopStart() const { return loopStart; }
    double getLoopEnd() const { return loopEnd; }

    void setPlayPosition (double timeInSeconds) 
    { 
        playPosition.store (timeInSeconds); 
        lastCallbackTime.store (juce::Time::getMillisecondCounterHiRes());
    }
    
    double getPlayPosition() const
    {
        if (isPlaying)
        {
            double elapsedMs = juce::Time::getMillisecondCounterHiRes() - lastCallbackTime.load();
            double elapsedSec = elapsedMs / 1000.0;
            
            double maxEstimate = 2.0 * (lastNumSamples.load() > 0 ? (double)lastNumSamples.load() : 512.0) / (currentSampleRate > 0.0 ? currentSampleRate : 44100.0);
            if (elapsedSec > maxEstimate)
                elapsedSec = maxEstimate;
                
            double estimatedPos = playPosition.load() + elapsedSec;
            if (isLooping && loopEnd > loopStart)
            {
                if (estimatedPos >= loopEnd)
                    estimatedPos = loopStart + std::fmod (estimatedPos - loopEnd, loopEnd - loopStart);
            }
            return estimatedPos;
        }
        return playPosition.load();
    }
    double getCurrentSampleRate() const { return currentSampleRate; }

    bool getMetronomeEnabled() const { return metronomeEnabled; }
    void setMetronomeEnabled (bool enabled) { metronomeEnabled = enabled; }
    double getBpm() const { return bpm; }
    void setBpm (double newBpm) { bpm = juce::jlimit (20.0, 300.0, newBpm); }

    enum class SnapDivision
    {
        None,
        Bar,
        HalfBar,
        Beat,      // Quarter note
        HalfBeat,  // 8th note
        QuarterBeat,// 16th note
        EighthBeat  // 32nd note
    };

    SnapDivision getSnapDivision() const { return snapDivision; }
    void setSnapDivision (SnapDivision division) { snapDivision = division; }

    double getSnapIntervalSeconds() const
    {
        if (snapDivision == SnapDivision::None)
            return 0.0;

        double beatLengthSecs = 60.0 / bpm;
        switch (snapDivision)
        {
            case SnapDivision::Bar:          return beatLengthSecs * 4.0;
            case SnapDivision::HalfBar:       return beatLengthSecs * 2.0;
            case SnapDivision::Beat:          return beatLengthSecs;
            case SnapDivision::HalfBeat:      return beatLengthSecs * 0.5;
            case SnapDivision::QuarterBeat:   return beatLengthSecs * 0.25;
            case SnapDivision::EighthBeat:    return beatLengthSecs * 0.125;
            default:                          return 0.0;
        }
    }

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

private:
    class GlobalPlayHead : public juce::AudioPlayHead
    {
    public:
        GlobalPlayHead (AudioEngine& e) : engine (e) {}
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setIsPlaying (engine.isPlaying);
            info.setTimeInSeconds (engine.getPlayPosition());
            info.setTimeInSamples ((int64_t)(engine.getPlayPosition() * engine.currentSampleRate));
            info.setBpm (engine.bpm);
            info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
            info.setPpqPosition (engine.getPlayPosition() * (engine.bpm / 60.0));
            return info;
        }
        AudioEngine& engine;
    };

    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer processorPlayer;
    std::unique_ptr<juce::AudioProcessorGraph> mainGraph;
    
    juce::AudioFormatManager formatManager;
    juce::AudioPluginFormatManager pluginFormatManager;
    GlobalPlayHead playHead;

    bool isPlaying = false;
    bool isRecording = false;
    bool isLooping = false;
    std::atomic<double> playPosition { 0.0 };
    std::atomic<double> lastCallbackTime { 0.0 };
    std::atomic<int> lastNumSamples { 0 };
    double loopStart = 0.0;
    double loopEnd = 4.0;
    double currentSampleRate = 44100.0;

    bool metronomeEnabled = false;
    double bpm = 120.0;
    SnapDivision snapDivision = SnapDivision::None;
    int metronomeToneSampleCount = 0;
    float metronomePhase = 0.0f;
    float metronomePhaseIncrement = 0.0f;
    int metronomeClickStartOffset = 0;

    juce::AudioProcessorGraph::Node::Ptr audioInputNode;
    juce::AudioProcessorGraph::Node::Ptr midiInputNode;
    juce::AudioProcessorGraph::Node::Ptr audioOutputNode;
    juce::AudioProcessorGraph::Node::Ptr midiOutputNode;

    Track* masterTrack = nullptr;
    juce::AudioProcessorGraph::Node::Ptr masterNode;

    std::vector<Track*> tracks;
    std::vector<juce::AudioProcessorGraph::Node::Ptr> trackNodes;

    class TrackJob : public juce::ThreadPoolJob
    {
    public:
        TrackJob() : ThreadPoolJob ("TrackJob") {}
        
        void setup (Track* t, int numSmp, double playheadPos, double sampleRate, bool isPl, std::atomic<int>* counter)
        {
            track = t;
            numSamples = numSmp;
            playheadSec = playheadPos;
            targetSampleRate = sampleRate;
            isPlaying = isPl;
            jobsRemaining = counter;
        }
        
        juce::ThreadPoolJob::JobStatus runJob() override
        {
            if (track != nullptr)
            {
                track->preRender (numSamples, playheadSec, targetSampleRate, isPlaying);
            }
            if (jobsRemaining != nullptr)
            {
                (*jobsRemaining)--;
            }
            return juce::ThreadPoolJob::jobHasFinished;
        }
        
        Track* track = nullptr;
        int numSamples = 0;
        double playheadSec = 0.0;
        double targetSampleRate = 0.0;
        bool isPlaying = false;
        std::atomic<int>* jobsRemaining = nullptr;
    };
    
    std::unique_ptr<juce::ThreadPool> threadPool;
    std::vector<std::unique_ptr<TrackJob>> trackJobs;
    juce::CriticalSection engineLock;

    friend class GlobalPlayHead;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
