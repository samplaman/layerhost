#pragma once
#include <JuceHeader.h>
#include "Item.h"

struct AutomationPoint {
    double time = 0.0;
    float value = 1.0f;
    float curve = 0.0f; // -1.0 to 1.0
};

class AutomationEnvelope {
public:
    std::vector<AutomationPoint> points;
    mutable juce::CriticalSection lock;

    AutomationEnvelope() = default;

    AutomationEnvelope (const AutomationEnvelope& other)
    {
        const juce::ScopedLock sl (other.lock);
        points = other.points;
    }

    AutomationEnvelope& operator= (const AutomationEnvelope& other)
    {
        if (this != &other)
        {
            const juce::ScopedLock sl1 (lock);
            const juce::ScopedLock sl2 (other.lock);
            points = other.points;
        }
        return *this;
    }

    AutomationEnvelope (AutomationEnvelope&& other) noexcept
    {
        const juce::ScopedLock sl (other.lock);
        points = std::move (other.points);
    }

    AutomationEnvelope& operator= (AutomationEnvelope&& other) noexcept
    {
        if (this != &other)
        {
            const juce::ScopedLock sl1 (lock);
            const juce::ScopedLock sl2 (other.lock);
            points = std::move (other.points);
        }
        return *this;
    }
    
    static float interpolateCurve (float x, float curve)
    {
        if (std::abs (curve) < 0.001f)
            return x;
        if (curve > 0.0f)
            return std::pow (x, 1.0f + curve * 3.0f);
        else
            return 1.0f - std::pow (1.0f - x, 1.0f - curve * 3.0f);
    }
    
    float getValueAt (double time, float defaultValue = 1.0f) const
    {
        const juce::ScopedLock sl (lock);
        if (points.empty()) return defaultValue;
        if (time <= points.front().time) return points.front().value;
        if (time >= points.back().time) return points.back().value;
        
        for (size_t i = 0; i < points.size() - 1; ++i)
        {
            if (time >= points[i].time && time <= points[i+1].time)
            {
                double tRange = points[i+1].time - points[i].time;
                if (tRange <= 0.0) return points[i].value;
                double ratio = (time - points[i].time) / tRange;
                double curvedRatio = interpolateCurve ((float)ratio, points[i].curve);
                return points[i].value + (float)curvedRatio * (points[i+1].value - points[i].value);
            }
        }
        return defaultValue;
    }
    
    void addOrUpdatePoint (double time, float value, float curve = 0.0f)
    {
        const juce::ScopedLock sl (lock);
        for (auto& p : points)
        {
            if (std::abs(p.time - time) < 0.05)
            {
                p.value = value;
                p.curve = curve;
                return;
            }
        }
        points.push_back ({ time, value, curve });
        std::sort (points.begin(), points.end(), [](const AutomationPoint& a, const AutomationPoint& b) {
            return a.time < b.time;
        });
    }
    
    void removePoint (int idx)
    {
        const juce::ScopedLock sl (lock);
        if (idx >= 0 && idx < (int)points.size())
            points.erase (points.begin() + idx);
    }

    void clear()
    {
        const juce::ScopedLock sl (lock);
        points.clear();
    }
};

struct PluginAutomation {
    int slotIndex = -1;
    int parameterIndex = -1;
    bool enabled = false;
    AutomationEnvelope envelope;
};

struct ActiveAutomationLane {
    ActiveAutomationLane() = default;
    ActiveAutomationLane (int lt, int si, int pi, const juce::String& n, AutomationEnvelope* env, float defVal, juce::Colour col)
        : laneType (lt), slotIndex (si), parameterIndex (pi), name (n), envelope (env), defaultValue (defVal), color (col)
    {}

    int laneType = 0; // 1 = volume, 2 = pan, 3 = plugin
    int slotIndex = -1;
    int parameterIndex = -1;
    juce::String name;
    AutomationEnvelope* envelope = nullptr;
    float defaultValue = 0.5f;
    juce::Colour color;
};

class SimpleSynthVoice : public juce::SynthesiserVoice
{
public:
    SimpleSynthVoice() {}
    bool canPlaySound (juce::SynthesiserSound* sound) override { return sound != nullptr; }
    
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        frequency = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        phase = 0.0;
        phaseIncrement = (2.0 * juce::MathConstants<double>::pi * frequency) / getSampleRate();
        amplitude = velocity * 0.12f;
        isActive = true;
    }
    
    void stopNote (float, bool) override
    {
        isActive = false;
        clearCurrentNote();
    }
    
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (!isActive) return;
        
        for (int s = 0; s < numSamples; ++s)
        {
            float val = (float)std::sin (phase) * amplitude;
            for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
            {
                outputBuffer.addSample (ch, startSample + s, val);
            }
            phase = std::fmod (phase + phaseIncrement, 2.0 * juce::MathConstants<double>::pi);
        }
    }
    
private:
    double frequency = 0.0;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    float amplitude = 0.0f;
    bool isActive = false;
};

class SimpleSynthSound : public juce::SynthesiserSound
{
public:
    SimpleSynthSound() {}
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class Track : public juce::AudioProcessor
{
public:
    enum class Type { Audio, Midi };

    Track (Type t = Type::Audio)
        : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          trackType (t)
    {
        if (trackType == Type::Midi)
        {
            for (int i = 0; i < 8; ++i)
                synth.addVoice (new SimpleSynthVoice());
            synth.addSound (new SimpleSynthSound());
        }
    }

    ~Track() override = default;

    const juce::String getName() const override { return name; }
    Type getTrackType() const { return trackType; }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        if (trackType == Type::Midi)
            synth.setCurrentPlaybackSampleRate (sampleRate);

        for (auto& item : items)
            if (item != nullptr)
                item->resampleTo (sampleRate);

        for (auto& fx : fxChain)
            if (fx != nullptr)
                fx->prepareToPlay (sampleRate, samplesPerBlock);
    }

    void releaseResources() override
    {
        for (auto& fx : fxChain)
            if (fx != nullptr)
                fx->releaseResources();
    }

    bool isEligibleForPreRender() const
    {
        return !isMasterTrack && !isArmed && !monitorEnabled;
    }

    void preRender (int numSamples, double playheadSec, double targetSampleRate, bool isPlaying)
    {
        if (isMasterTrack)
            return;

        preRenderBuffer.setSize (2, numSamples, false, false, true);
        preRenderBuffer.clear();
        preRenderMidi.clear();

        // 1. Render items (audio clips and MIDI) only if playing
        if (isPlaying)
        {
            for (auto& item : items)
            {
                if (item != nullptr)
                {
                    if (item->getType() == Item::Type::Audio && trackType == Type::Audio)
                        item->process (preRenderBuffer, playheadSec, targetSampleRate, numSamples);
                    else if (item->getType() == Item::Type::Midi && trackType == Type::Midi)
                        item->processMidi (preRenderMidi, playheadSec, targetSampleRate, numSamples);
                }
            }
        }

        // 1.5. Render built-in synth if MIDI track and no active instrument plugin loaded
        if (trackType == Type::Midi)
        {
            bool hasCustomSynth = false;
            for (auto& fx : fxChain)
            {
                if (fx != nullptr && fx->acceptsMidi() && !fx->isSuspended())
                {
                    hasCustomSynth = true;
                    break;
                }
            }
            
            if (!hasCustomSynth)
            {
                synth.renderNextBlock (preRenderBuffer, preRenderMidi, 0, numSamples);
            }
        }

        // 2. Process FX Chain
        for (int slot = 0; slot < 8; ++slot)
        {
            auto* fx = fxChain[slot].get();
            if (fx != nullptr && !fx->isSuspended())
            {
                if (automationWriteEnabled && isPlaying)
                {
                    auto params = fx->getParameters();
                    for (int pIdx = 0; pIdx < params.size(); ++pIdx)
                    {
                        if (auto* param = params[pIdx])
                        {
                            float curVal = param->getValue();
                            bool found = false;
                            for (auto& lpv : lastParamValues)
                            {
                                if (lpv.slotIndex == slot && lpv.parameterIndex == pIdx)
                                {
                                    if (std::abs (curVal - lpv.lastValue) > 0.005f)
                                    {
                                        auto* pa = getOrCreatePluginAutomation (slot, pIdx);
                                        if (pa != nullptr)
                                        {
                                            pa->enabled = true;
                                            pa->envelope.addOrUpdatePoint (playheadSec, curVal);
                                        }
                                        lpv.lastValue = curVal;
                                    }
                                    found = true;
                                    break;
                                }
                            }
                            if (!found)
                            {
                                lastParamValues.push_back ({ slot, pIdx, curVal });
                            }
                        }
                    }
                }
                else
                {
                    const juce::ScopedLock sl (pluginAutomationsLock);
                    if (!pluginAutomations.empty())
                    {
                        auto params = fx->getParameters();
                        for (auto& pa : pluginAutomations)
                        {
                            if (pa != nullptr && pa->slotIndex == slot && pa->enabled)
                            {
                                if (pa->parameterIndex >= 0 && pa->parameterIndex < params.size())
                                {
                                    if (auto* param = params[pa->parameterIndex])
                                    {
                                        float val = pa->envelope.getValueAt (playheadSec, param->getDefaultValue());
                                        param->setValue (val);
                                        
                                        bool found = false;
                                        for (auto& lpv : lastParamValues)
                                        {
                                            if (lpv.slotIndex == slot && lpv.parameterIndex == pa->parameterIndex)
                                            {
                                                lpv.lastValue = val;
                                                found = true;
                                                break;
                                            }
                                        }
                                        if (!found)
                                        {
                                            lastParamValues.push_back ({ slot, pa->parameterIndex, val });
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                
                fx->processBlock (preRenderBuffer, preRenderMidi);
            }
        }

        // 4. Apply Track Volume (with Automation) and Pan
        float currentVol = volume;
        if (volumeAutomationEnabled)
        {
            if (automationWriteEnabled && isPlaying)
            {
                float curVol = volume;
                if (lastVolumeValue < 0.0f) lastVolumeValue = curVol;
                if (std::abs (curVol - lastVolumeValue) > 0.005f)
                {
                    float normVal = curVol / 2.0f;
                    volumeAutomation.addOrUpdatePoint (playheadSec, juce::jlimit (0.0f, 1.0f, normVal));
                    lastVolumeValue = curVol;
                }
            }
            else
            {
                currentVol = volumeAutomation.getValueAt (playheadSec, volume / 2.0f) * 2.0f;
                volume = currentVol;
                lastVolumeValue = currentVol;
            }
        }

        float currentPan = pan;
        if (panAutomationEnabled)
        {
            if (automationWriteEnabled && isPlaying)
            {
                float curPan = pan;
                if (lastPanValue < -90.0f) lastPanValue = curPan;
                if (std::abs (curPan - lastPanValue) > 0.005f)
                {
                    float normVal = (curPan + 1.0f) / 2.0f;
                    panAutomation.addOrUpdatePoint (playheadSec, juce::jlimit (0.0f, 1.0f, normVal));
                    lastPanValue = curPan;
                }
            }
            else
            {
                float val = panAutomation.getValueAt (playheadSec, (pan + 1.0f) / 2.0f);
                currentPan = -1.0f + 2.0f * val;
                pan = currentPan;
                lastPanValue = currentPan;
            }
        }

        if (preRenderBuffer.getNumChannels() >= 2)
        {
            float leftGain = currentVol * ((currentPan > 0.0f) ? (1.0f - currentPan) : 1.0f);
            float rightGain = currentVol * ((currentPan < 0.0f) ? (1.0f + currentPan) : 1.0f);
            preRenderBuffer.applyGain (0, 0, numSamples, leftGain);
            preRenderBuffer.applyGain (1, 0, numSamples, rightGain);
        }
        else
        {
            preRenderBuffer.applyGain (currentVol);
        }

        // Calculate peak level for level meters
        float maxPeak = 0.0f;
        for (int ch = 0; ch < preRenderBuffer.getNumChannels(); ++ch)
        {
            maxPeak = juce::jmax (maxPeak, preRenderBuffer.getMagnitude (ch, 0, numSamples));
        }
        currentPeak = maxPeak;

        hasPreRendered.store (true);
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
    {
        // 0. Capture live audio input if recording
        if (isArmed && trackType == Type::Audio && currentRecordingItem != nullptr)
        {
            currentRecordingItem->appendAudioData (buffer.getArrayOfReadPointers(), buffer.getNumChannels(), buffer.getNumSamples());
        }

        if (isEligibleForPreRender())
        {
            if (hasPreRendered.load())
            {
                buffer.copyFrom (0, 0, preRenderBuffer, 0, 0, buffer.getNumSamples());
                if (buffer.getNumChannels() >= 2 && preRenderBuffer.getNumChannels() >= 2)
                    buffer.copyFrom (1, 0, preRenderBuffer, 1, 0, buffer.getNumSamples());
                midiMessages.clear();
                midiMessages.addEvents (preRenderMidi, 0, buffer.getNumSamples(), 0);
                hasPreRendered.store (false);

                // Apply Mute / Solo
                if (isMuted || (numTracksSoloed > 0 && !isSolo))
                {
                    buffer.clear();
                    midiMessages.clear();
                    currentPeak *= 0.95f; // Release even when muted to let meter drop smoothly
                }

                return;
            }
        }
        
        juce::Optional<juce::AudioPlayHead::PositionInfo> posInfo;
        if (auto* ph = getPlayHead())
            posInfo = ph->getPosition();

        double playheadSec = 0.0;
        if (posInfo.hasValue() && posInfo->getTimeInSeconds().hasValue())
            playheadSec = *posInfo->getTimeInSeconds();

        // 0. Handle Input Monitoring
        if (!isMasterTrack)
        {
            if (!monitorEnabled && !isArmed)
                buffer.clear(); // Ensure we don't monitor if not enabled
            else if (!monitorEnabled && isArmed)
                buffer.clear(); // If armed but monitor off, don't pass live audio to output
        }

        // 0.5. Filter live incoming MIDI messages by channel if a channel is specified
        if (trackType == Type::Midi && midiChannel != 0)
        {
            juce::MidiBuffer filtered;
            for (const auto metadata : midiMessages)
            {
                auto msg = metadata.getMessage();
                if (msg.getChannel() == midiChannel)
                    filtered.addEvent (msg, metadata.samplePosition);
            }
            midiMessages = filtered;
        }

        bool isPlaying = posInfo.hasValue() && posInfo->getIsPlaying();

        // 1. Render items (audio clips and MIDI) only if playing
        if (isPlaying)
        {
            for (auto& item : items)
            {
                if (item != nullptr)
                {
                    if (item->getType() == Item::Type::Audio && trackType == Type::Audio)
                        item->process (buffer, playheadSec, getSampleRate(), buffer.getNumSamples());
                    else if (item->getType() == Item::Type::Midi && trackType == Type::Midi)
                        item->processMidi (midiMessages, playheadSec, getSampleRate(), buffer.getNumSamples());
                }
            }
        }

        // 1.5. Render built-in synth if MIDI track and no active instrument plugin loaded
        if (trackType == Type::Midi)
        {
            bool hasCustomSynth = false;
            for (auto& fx : fxChain)
            {
                if (fx != nullptr && fx->acceptsMidi() && !fx->isSuspended())
                {
                    hasCustomSynth = true;
                    break;
                }
            }
            
            if (!hasCustomSynth)
            {
                synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
            }
        }

        // 2. Process FX Chain
        for (int slot = 0; slot < 8; ++slot)
        {
            auto* fx = fxChain[slot].get();
            if (fx != nullptr && !fx->isSuspended())
            {
                if (automationWriteEnabled && isPlaying)
                {
                    // Write Mode: Poll and write parameter changes
                    auto params = fx->getParameters();
                    for (int pIdx = 0; pIdx < params.size(); ++pIdx)
                    {
                        if (auto* param = params[pIdx])
                        {
                            float curVal = param->getValue();
                            bool found = false;
                            for (auto& lpv : lastParamValues)
                            {
                                if (lpv.slotIndex == slot && lpv.parameterIndex == pIdx)
                                {
                                    if (std::abs (curVal - lpv.lastValue) > 0.005f)
                                    {
                                        auto* pa = getOrCreatePluginAutomation (slot, pIdx);
                                        if (pa != nullptr)
                                        {
                                            pa->enabled = true;
                                            pa->envelope.addOrUpdatePoint (playheadSec, curVal);
                                        }
                                        lpv.lastValue = curVal;
                                    }
                                    found = true;
                                    break;
                                }
                            }
                            if (!found)
                            {
                                lastParamValues.push_back ({ slot, pIdx, curVal });
                            }
                        }
                    }
                }
                else
                {
                    // Read Mode: Only iterate over active automated parameters (highly optimized!)
                    const juce::ScopedLock sl (pluginAutomationsLock);
                    if (!pluginAutomations.empty())
                    {
                        auto params = fx->getParameters();
                        for (auto& pa : pluginAutomations)
                        {
                            if (pa != nullptr && pa->slotIndex == slot && pa->enabled)
                            {
                                if (pa->parameterIndex >= 0 && pa->parameterIndex < params.size())
                                {
                                    if (auto* param = params[pa->parameterIndex])
                                    {
                                        float val = pa->envelope.getValueAt (playheadSec, param->getDefaultValue());
                                        param->setValue (val);
                                        
                                        bool found = false;
                                        for (auto& lpv : lastParamValues)
                                        {
                                            if (lpv.slotIndex == slot && lpv.parameterIndex == pa->parameterIndex)
                                            {
                                                lpv.lastValue = val;
                                                found = true;
                                                break;
                                            }
                                        }
                                        if (!found)
                                        {
                                            lastParamValues.push_back ({ slot, pa->parameterIndex, val });
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                
                fx->processBlock (buffer, midiMessages);
            }
        }

        // 3. Apply Mute / Solo
        if (isMuted || (numTracksSoloed > 0 && !isSolo))
        {
            buffer.clear();
            currentPeak *= 0.95f; // Release even when muted to let meter drop smoothly
            return;
        }

        // 4. Apply Track Volume (with Automation) and Pan
        float currentVol = volume;
        if (volumeAutomationEnabled)
        {
            if (automationWriteEnabled && isPlaying)
            {
                float curVol = volume;
                if (lastVolumeValue < 0.0f) lastVolumeValue = curVol;
                if (std::abs (curVol - lastVolumeValue) > 0.005f)
                {
                    float normVal = curVol / 2.0f;
                    volumeAutomation.addOrUpdatePoint (playheadSec, juce::jlimit (0.0f, 1.0f, normVal));
                    lastVolumeValue = curVol;
                }
            }
            else
            {
                currentVol = volumeAutomation.getValueAt (playheadSec, volume / 2.0f) * 2.0f;
                volume = currentVol;
                lastVolumeValue = currentVol;
            }
        }

        float currentPan = pan;
        if (panAutomationEnabled)
        {
            if (automationWriteEnabled && isPlaying)
            {
                float curPan = pan;
                if (lastPanValue < -90.0f) lastPanValue = curPan;
                if (std::abs (curPan - lastPanValue) > 0.005f)
                {
                    float normVal = (curPan + 1.0f) / 2.0f;
                    panAutomation.addOrUpdatePoint (playheadSec, juce::jlimit (0.0f, 1.0f, normVal));
                    lastPanValue = curPan;
                }
            }
            else
            {
                float val = panAutomation.getValueAt (playheadSec, (pan + 1.0f) / 2.0f);
                currentPan = -1.0f + 2.0f * val;
                pan = currentPan;
                lastPanValue = currentPan;
            }
        }

        if (buffer.getNumChannels() >= 2)
        {
            float leftGain = currentVol * ((currentPan > 0.0f) ? (1.0f - currentPan) : 1.0f);
            float rightGain = currentVol * ((currentPan < 0.0f) ? (1.0f + currentPan) : 1.0f);
            buffer.applyGain (0, 0, buffer.getNumSamples(), leftGain);
            buffer.applyGain (1, 0, buffer.getNumSamples(), rightGain);
        }
        else
        {
            buffer.applyGain (currentVol);
        }

        // 4.5. Apply Master Limiter if this is the Master Track
        if (isMasterTrack)
        {
            const float threshold = 0.99f; // -0.1 dBFS
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                float maxVal = 0.0f;
                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                {
                    maxVal = juce::jmax (maxVal, std::abs (buffer.getSample (channel, sample)));
                }
                
                if (maxVal > limiterEnvelope)
                    limiterEnvelope = maxVal;
                else
                    limiterEnvelope = limiterEnvelope * 0.999f + maxVal * 0.001f;
                
                if (limiterEnvelope > threshold)
                {
                    float gain = threshold / limiterEnvelope;
                    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                    {
                        buffer.setSample (channel, sample, buffer.getSample (channel, sample) * gain);
                    }
                }
            }
        }

        // 5. Calculate Peak for VU Meter
        float peak = buffer.getMagnitude (0, buffer.getNumSamples());
        if (peak > currentPeak)
            currentPeak = peak;
        else
            currentPeak *= 0.95f; // Release
    }

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    void setVolume(float newVol) { volume = newVol; }
    float getVolume() const { return volume; }
    
    void setHeight (int newHeight) { height = juce::jlimit (40, 100, newHeight); }
    int getHeight() const { return height; }
    
    void setPan(float newPan) { pan = newPan; }
    float getPan() const { return pan; }

    void setMute(bool shouldMute) { isMuted = shouldMute; }
    bool getMute() const { return isMuted; }

    void setSolo(bool shouldSolo) 
    { 
        if (isSolo == shouldSolo) return;
        isSolo = shouldSolo; 
        if (isSolo) numTracksSoloed++; else numTracksSoloed--;
    }
    bool getSolo() const { return isSolo; }
    
    void setArmed(bool shouldArm) { isArmed = shouldArm; }
    bool getArmed() const { return isArmed; }

    void setMonitor(bool shouldMonitor) { monitorEnabled = shouldMonitor; }
    bool getMonitor() const { return monitorEnabled; }

    void setVolumeAutomationEnabled (bool enabled) { volumeAutomationEnabled = enabled; }
    bool getVolumeAutomationEnabled() const { return volumeAutomationEnabled; }
    AutomationEnvelope& getVolumeAutomation() { return volumeAutomation; }
    const AutomationEnvelope& getVolumeAutomation() const { return volumeAutomation; }

    void setPanAutomationEnabled (bool enabled) { panAutomationEnabled = enabled; }
    bool getPanAutomationEnabled() const { return panAutomationEnabled; }
    AutomationEnvelope& getPanAutomation() { return panAutomation; }
    const AutomationEnvelope& getPanAutomation() const { return panAutomation; }

    bool getAutomationEnabled() const
    {
        const juce::ScopedLock sl (pluginAutomationsLock);
        if (volumeAutomationEnabled || panAutomationEnabled)
            return true;
        for (const auto& pa : pluginAutomations)
        {
            if (pa != nullptr && pa->enabled)
                return true;
        }
        return false;
    }

    bool getAutomationWriteEnabled() const { return automationWriteEnabled; }
    void setAutomationWriteEnabled (bool enabled) { automationWriteEnabled = enabled; }

    int getTotalHeight() const
    {
        const juce::ScopedLock sl (pluginAutomationsLock);
        int h = height;
        if (volumeAutomationEnabled)
            h += 50 + 10;
        if (panAutomationEnabled)
            h += 50 + 10;
        for (const auto& pa : pluginAutomations)
        {
            if (pa != nullptr && pa->enabled)
                h += 50 + 10;
        }
        return h;
    }

    std::vector<PluginAutomation*> getPluginAutomations() const
    {
        const juce::ScopedLock sl (pluginAutomationsLock);
        std::vector<PluginAutomation*> list;
        for (auto& pa : pluginAutomations)
            if (pa != nullptr)
                list.push_back (pa.get());
        return list;
    }
    
    void clearPluginAutomations()
    {
        const juce::ScopedLock sl (pluginAutomationsLock);
        pluginAutomations.clear();
    }
    
    PluginAutomation* getOrCreatePluginAutomation (int slotIndex, int parameterIndex)
    {
        const juce::ScopedLock sl (pluginAutomationsLock);
        for (auto& pa : pluginAutomations)
        {
            if (pa != nullptr && pa->slotIndex == slotIndex && pa->parameterIndex == parameterIndex)
                return pa.get();
        }
        
        auto newPa = std::make_unique<PluginAutomation>();
        newPa->slotIndex = slotIndex;
        newPa->parameterIndex = parameterIndex;
        newPa->enabled = false;
        
        auto* ptr = newPa.get();
        pluginAutomations.push_back (std::move (newPa));
        return ptr;
    }

    std::vector<ActiveAutomationLane> getActiveAutomationLanes()
    {
        const juce::ScopedLock sl (pluginAutomationsLock);
        std::vector<ActiveAutomationLane> lanes;
        if (volumeAutomationEnabled)
        {
            float val = volume / 2.0f;
            if (val > 1.0f) val = 1.0f;
            lanes.push_back ({ 1, -1, -1, "Volume Automation", &volumeAutomation, val, juce::Colour (0xfffbc02d) });
        }
        if (panAutomationEnabled)
        {
            float val = (pan + 1.0f) / 2.0f;
            lanes.push_back ({ 2, -1, -1, "Pan Automation", &panAutomation, val, juce::Colour (0xff00b4d8) });
        }
        for (auto& pa : pluginAutomations)
        {
            if (pa != nullptr && pa->enabled)
            {
                juce::String laneName = "Plugin Automation";
                float defaultVal = 0.5f;
                if (auto* plugin = getPlugin (pa->slotIndex))
                {
                    auto params = plugin->getParameters();
                    if (pa->parameterIndex >= 0 && pa->parameterIndex < params.size())
                    {
                        if (auto* param = params[pa->parameterIndex])
                        {
                            laneName = plugin->getName() + ": " + param->getName (20);
                            defaultVal = param->getDefaultValue();
                        }
                    }
                }
                lanes.push_back ({ 3, pa->slotIndex, pa->parameterIndex, laneName, &pa->envelope, defaultVal, juce::Colour (0xffe040fb) });
            }
        }
        return lanes;
    }

    void setInputChannel(int channel) { inputChannel = channel; }
    int getInputChannel() const { return inputChannel; }

    void setMidiChannel (int channel) { midiChannel = channel; }
    int getMidiChannel() const { return midiChannel; }

    float getPeakLevel() const { return currentPeak; }
    
    void setAsMasterTrack (bool isMaster) { isMasterTrack = isMaster; }
    bool getIsMasterTrack() const { return isMasterTrack; }
    
    void setName (const juce::String& newName) { name = newName; }

    int getRouting() const { return routingDestination; }
    void setRouting(int dest) { routingDestination = dest; }

    void addItem (std::unique_ptr<Item> item) { items.push_back(std::move(item)); }
    std::unique_ptr<Item> extractItem (Item* itemToExtract)
    {
        auto it = std::find_if (items.begin(), items.end(), 
            [itemToExtract](const std::unique_ptr<Item>& i) { return i.get() == itemToExtract; });
        if (it != items.end())
        {
            auto extracted = std::move (*it);
            items.erase (it);
            return extracted;
        }
        return nullptr;
    }
    const std::vector<std::unique_ptr<Item>>& getItems() const { return items; }
    void setPlugin (int slotIdx, std::unique_ptr<juce::AudioProcessor> plugin, double explicitSampleRate = 44100.0, int explicitBlockSize = 512)
    {
        if (slotIdx >= 0 && slotIdx < 8)
        {
            if (plugin != nullptr)
            {
                double sr = getSampleRate() > 0 ? getSampleRate() : explicitSampleRate;
                int bs = getBlockSize() > 0 ? getBlockSize() : explicitBlockSize;
                plugin->prepareToPlay (sr, bs);
            }
            fxChain[slotIdx] = std::move(plugin);
        }
    }
    
    juce::AudioProcessor* getPlugin (int slotIdx) const
    {
        if (slotIdx >= 0 && slotIdx < 8)
            return fxChain[slotIdx].get();
        return nullptr;
    }
    
    void addPlugin (std::unique_ptr<juce::AudioProcessor> plugin, double explicitSampleRate = 44100.0, int explicitBlockSize = 512)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (fxChain[i] == nullptr)
            {
                setPlugin (i, std::move(plugin), explicitSampleRate, explicitBlockSize);
                break;
            }
        }
    }

    void startRecording (double startTime)
    {
        if (isArmed)
        {
            auto newItem = std::make_unique<Item>();
            newItem->setType(trackType == Type::Midi ? Item::Type::Midi : Item::Type::Audio);
            newItem->setStartTime(startTime);
            currentRecordingItem = newItem.get();
            items.push_back(std::move(newItem));
        }
    }

    void stopRecording (double stopTime)
    {
        if (currentRecordingItem)
        {
            auto* itemToSave = currentRecordingItem;
            currentRecordingItem = nullptr; // Detach from live capture first

            itemToSave->setLength(juce::jmax(0.1, stopTime - itemToSave->getStartTime()));
            
            juce::File dir = juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Layerhost_Recordings");
            dir.createDirectory();
            
            if (itemToSave->getType() == Item::Type::Audio && itemToSave->getBuffer().getNumSamples() > 0)
            {
                juce::WavAudioFormat wavFormat;
                juce::File file = dir.getChildFile("Track_" + name + "_" + juce::String(juce::Time::currentTimeMillis()) + ".wav");
                
                std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(new juce::FileOutputStream(file),
                                                                                          getSampleRate(),
                                                                                          itemToSave->getBuffer().getNumChannels(),
                                                                                          24, {}, 0));
                if (writer != nullptr)
                {
                    writer->writeFromAudioSampleBuffer(itemToSave->getBuffer(), 0, itemToSave->getBuffer().getNumSamples());
                    itemToSave->setFilePath (file.getFullPathName());
                }
            }
            else if (itemToSave->getType() == Item::Type::Midi && itemToSave->getMidiSequence().getNumEvents() > 0)
            {
                juce::File file = dir.getChildFile("Track_" + name + "_" + juce::String(juce::Time::currentTimeMillis()) + ".mid");
                juce::MidiFile midiFile;
                midiFile.setTicksPerQuarterNote(960);
                midiFile.addTrack(itemToSave->getMidiSequence());
                
                juce::FileOutputStream stream(file);
                midiFile.writeTo(stream);
            }
        }
    }

    void addMidiEvent (const juce::MidiMessage& msg, double currentPlayheadSec)
    {
        if (currentRecordingItem)
        {
            if (midiChannel != 0 && msg.getChannel() != midiChannel)
                return;

            double relativeTime = currentPlayheadSec - currentRecordingItem->getStartTime();
            juce::MidiMessage copy = msg;
            copy.setTimeStamp(relativeTime);
            currentRecordingItem->getMidiSequence().addEvent(copy);
            currentRecordingItem->getMidiSequence().updateMatchedPairs();
        }
    }

private:
    Type trackType = Type::Audio;
    juce::String name;
    float volume = 1.0f;
    float pan = 0.0f;
    int height = 80;
    float currentPeak = 0.0f;
    bool isMuted = false;
    bool isSolo = false;
    bool isArmed = false;
    static std::atomic<int> numTracksSoloed;
    int routingDestination = -1; // -1 = Master
    int inputChannel = -1; // -1 = All/Stereo, 0+ = specific channel
    int midiChannel = 0; // 0 = Omni/All, 1-16 = specific channel
    bool monitorEnabled = false;
    bool isMasterTrack = false;
    float limiterEnvelope = 0.0f;
    
    std::vector<std::unique_ptr<Item>> items;
    Item* currentRecordingItem = nullptr;
    std::vector<std::unique_ptr<juce::AudioProcessor>> fxChain { 8 };
    juce::Synthesiser synth;

    AutomationEnvelope volumeAutomation;
    AutomationEnvelope panAutomation;
    bool volumeAutomationEnabled = false;
    bool panAutomationEnabled = false;
    std::vector<std::unique_ptr<PluginAutomation>> pluginAutomations;
    mutable juce::CriticalSection pluginAutomationsLock;

    bool automationWriteEnabled = false;
    float lastVolumeValue = -1.0f;
    float lastPanValue = -99.0f;

    struct LastParamValue {
        int slotIndex;
        int parameterIndex;
        float lastValue;
    };
    std::vector<LastParamValue> lastParamValues;

    juce::AudioBuffer<float> preRenderBuffer;
    juce::MidiBuffer preRenderMidi;
    std::atomic<bool> hasPreRendered { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Track)
};
