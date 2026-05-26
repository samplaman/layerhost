#include "Item.h"

Item::Item() {}
Item::~Item() {}

bool Item::loadFile (const juce::File& file, juce::AudioFormatManager& formatManager, double targetSampleRate)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader != nullptr)
    {
        filePath = file.getFullPathName();
        double originalRate = reader->sampleRate;
        int numChannels = (int) reader->numChannels;
        
        // Pad tempBuffer by 128 samples to avoid LagrangeInterpolator reading past bounds
        juce::AudioBuffer<float> tempBuffer (numChannels, (int)reader->lengthInSamples + 128);
        tempBuffer.clear();
        reader->read (&tempBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
        
        if (originalRate != targetSampleRate && originalRate > 0 && targetSampleRate > 0)
        {
            double ratio = originalRate / targetSampleRate;
            int newNumSamples = (int)(reader->lengthInSamples / ratio);
            audioData.setSize (numChannels, newNumSamples);
            
            for (int ch = 0; ch < numChannels; ++ch)
            {
                juce::LagrangeInterpolator resampler;
                resampler.process (ratio, tempBuffer.getReadPointer(ch), audioData.getWritePointer(ch), newNumSamples);
            }
            length = newNumSamples / targetSampleRate;
            loadedSampleRate = targetSampleRate;
        }
        else
        {
            // Just copy the loaded portion (without the padded elements)
            audioData.setSize (numChannels, (int)reader->lengthInSamples);
            for (int ch = 0; ch < numChannels; ++ch)
                audioData.copyFrom (ch, 0, tempBuffer.getReadPointer(ch), (int)reader->lengthInSamples);
                
            length = reader->lengthInSamples / (originalRate > 0 ? originalRate : 44100.0);
            loadedSampleRate = originalRate > 0 ? originalRate : 44100.0;
        }
        return true;
    }
    return false;
}

void Item::resampleTo (double newSampleRate)
{
    if (type != Type::Audio)
        return;

    if (loadedSampleRate == newSampleRate || loadedSampleRate <= 0.0 || newSampleRate <= 0.0)
        return;
        
    int numChannels = audioData.getNumChannels();
    int oldNumSamples = audioData.getNumSamples();
    if (numChannels == 0 || oldNumSamples == 0)
        return;
        
    double ratio = loadedSampleRate / newSampleRate;
    int newNumSamples = (int)(oldNumSamples / ratio);
    
    // Add 128 samples padding to the temp buffer for safe resampling
    juce::AudioBuffer<float> tempBuffer (numChannels, oldNumSamples + 128);
    tempBuffer.clear();
    for (int ch = 0; ch < numChannels; ++ch)
        tempBuffer.copyFrom (ch, 0, audioData.getReadPointer(ch), oldNumSamples);
        
    audioData.setSize (numChannels, newNumSamples);
    
    for (int ch = 0; ch < numChannels; ++ch)
    {
        juce::LagrangeInterpolator resampler;
        resampler.process (ratio, tempBuffer.getReadPointer(ch), audioData.getWritePointer(ch), newNumSamples);
    }
    
    loadedSampleRate = newSampleRate;
}

void Item::copyAudioDataFrom (const Item& other)
{
    type = other.type;
    audioData.makeCopyOf (other.audioData);
    midiSequence = other.midiSequence;
    length = other.length;
    startTime = other.startTime;
    sourceOffset = other.sourceOffset;
    isMuted = other.isMuted;
    filePath = other.filePath;
    loadedSampleRate = other.loadedSampleRate;
}

void Item::appendAudioData (const float* const* channelData, int numChannels, int numSamples)
{
    int currentSamples = audioData.getNumSamples();
    int newSamples = currentSamples + numSamples;
    
    if (audioData.getNumChannels() < numChannels)
        audioData.setSize (numChannels, newSamples, true);
    else
        audioData.setSize (audioData.getNumChannels(), newSamples, true);
        
    for (int ch = 0; ch < numChannels; ++ch)
    {
        audioData.copyFrom (ch, currentSamples, channelData[ch], numSamples);
    }
}

void Item::process (juce::AudioBuffer<float>& buffer, double playheadPositionSec, double targetSampleRate, int numSamples)
{
    if (isMuted)
        return;

    if (audioData.getNumChannels() == 0 || audioData.getNumSamples() == 0)
        return;

    double blockStartTime = playheadPositionSec;
    double blockEndTime = blockStartTime + (numSamples / targetSampleRate);
    double itemEndTime = startTime + length;

    if (blockEndTime < startTime || blockStartTime > itemEndTime)
        return; 
    
    int startSampleInBlock = 0;
    int endSampleInBlock = numSamples;
    
    int readOffset = juce::roundToInt (sourceOffset * targetSampleRate);
    
    if (blockStartTime < startTime)
    {
        startSampleInBlock = juce::roundToInt ((startTime - blockStartTime) * targetSampleRate);
    }
    else
    {
        readOffset += juce::roundToInt ((blockStartTime - startTime) * targetSampleRate);
    }

    if (blockEndTime > itemEndTime)
    {
        endSampleInBlock = numSamples - juce::roundToInt ((blockEndTime - itemEndTime) * targetSampleRate);
    }

    int numSamplesToCopy = endSampleInBlock - startSampleInBlock;
    if (numSamplesToCopy <= 0) return;
    
    if (readOffset + numSamplesToCopy > audioData.getNumSamples())
    {
        numSamplesToCopy = audioData.getNumSamples() - readOffset;
    }

    if (numSamplesToCopy > 0)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            int sourceChannel = juce::jmin (channel, audioData.getNumChannels() - 1);
            
            if (fadeInDuration <= 0.0 && fadeOutDuration <= 0.0)
            {
                buffer.addFrom (channel, startSampleInBlock, audioData, sourceChannel, readOffset, numSamplesToCopy);
            }
            else
            {
                const float* src = audioData.getReadPointer(sourceChannel, readOffset);
                float* dst = buffer.getWritePointer(channel, startSampleInBlock);
                
                for (int i = 0; i < numSamplesToCopy; ++i)
                {
                    double currentSampleTime = blockStartTime + ((startSampleInBlock + i) / targetSampleRate);
                    double itemRelTime = currentSampleTime - startTime;
                    
                    float gain = 1.0f;
                    
                    if (fadeInDuration > 0.0 && itemRelTime < fadeInDuration)
                    {
                        gain = (float)(itemRelTime / fadeInDuration);
                        if (gain < 0.0f) gain = 0.0f;
                    }
                    else if (fadeOutDuration > 0.0 && itemRelTime > (length - fadeOutDuration))
                    {
                        gain = (float)((length - itemRelTime) / fadeOutDuration);
                        if (gain < 0.0f) gain = 0.0f;
                    }
                    
                    dst[i] += src[i] * gain;
                }
            }
        }
    }
}

void Item::processMidi (juce::MidiBuffer& midiMessages, double playheadPositionSec, double targetSampleRate, int numSamples)
{
    if (isMuted) return;
    if (type != Type::Midi) return;
    if (midiSequence.getNumEvents() == 0) return;

    double blockStartTime = playheadPositionSec;
    double blockEndTime = blockStartTime + (numSamples / targetSampleRate);
    double itemEndTime = startTime + length;

    if (blockEndTime < startTime || blockStartTime > itemEndTime)
        return;

    for (int i = 0; i < midiSequence.getNumEvents(); ++i)
    {
        auto* ev = midiSequence.getEventPointer (i);
        // Time stamps are assumed to be in seconds relative to item start
        double eventTimeGlobal = startTime + ev->message.getTimeStamp();
        
        if (eventTimeGlobal >= blockStartTime && eventTimeGlobal < blockEndTime)
        {
            int sampleOffset = juce::roundToInt ((eventTimeGlobal - blockStartTime) * targetSampleRate);
            sampleOffset = juce::jlimit (0, numSamples - 1, sampleOffset);
            midiMessages.addEvent (ev->message, sampleOffset);
        }
    }
}

