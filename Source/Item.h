#pragma once
#include <JuceHeader.h>

class Item
{
public:
    enum class Type { Audio, Midi };
    Item();
    ~Item();

    bool loadFile (const juce::File& file, juce::AudioFormatManager& formatManager, double targetSampleRate);
    void copyAudioDataFrom (const Item& other);
    void appendAudioData (const float* const* channelData, int numChannels, int numSamples);
    void resampleTo (double newSampleRate);

    void setStartTime (double timeInSeconds) { startTime = timeInSeconds; }
    double getStartTime() const { return startTime; }
    double getLength() const { return length; }

    // Renders the item's audio into the track buffer based on the global timeline position
    void process (juce::AudioBuffer<float>& buffer, double playheadPositionSec, double sampleRate, int numSamples);
    void processMidi (juce::MidiBuffer& midiMessages, double playheadPositionSec, double sampleRate, int numSamples);

    const juce::AudioBuffer<float>& getBuffer() const { return audioData; }
    juce::AudioBuffer<float>& getAudioBuffer() { return audioData; }
    juce::MidiMessageSequence& getMidiSequence() { return midiSequence; }

    void updateMidiLength()
    {
        if (type == Type::Midi)
        {
            double maxTime = 0.0;
            for (int i = 0; i < midiSequence.getNumEvents(); ++i)
            {
                auto* ev = midiSequence.getEventPointer(i);
                if (ev->message.getTimeStamp() > maxTime)
                    maxTime = ev->message.getTimeStamp();
            }
            if (maxTime > 0.1) // Don't make it 0
                length = maxTime;
        }
    }

    Type getType() const { return type; }
    void setType(Type newType) { type = newType; }

    void setSourceOffset (double offsetInSeconds) { sourceOffset = offsetInSeconds; }
    double getSourceOffset() const { return sourceOffset; }
    void setLength (double newLength) { length = newLength; }

    void setFadeIn (double durationSecs) { fadeInDuration = durationSecs; }
    void setFadeOut (double durationSecs) { fadeOutDuration = durationSecs; }
    double getFadeIn() const { return fadeInDuration; }
    double getFadeOut() const { return fadeOutDuration; }

    void setFilePath (const juce::String& path) { filePath = path; }
    juce::String getFilePath() const { return filePath; }

    void setMuted (bool shouldBeMuted) { isMuted = shouldBeMuted; }
    bool getMuted() const { return isMuted; }

private:
    Type type = Type::Audio;
    double startTime = 0.0;
    double length = 0.0;
    double sourceOffset = 0.0;
    double fadeInDuration = 0.0;
    double fadeOutDuration = 0.0;
    juce::String filePath;
    bool isMuted = false;
    double loadedSampleRate = 44100.0;
    juce::AudioBuffer<float> audioData;
    juce::MidiMessageSequence midiSequence;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Item)
};
