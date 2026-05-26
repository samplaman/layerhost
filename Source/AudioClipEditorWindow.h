#pragma once
#include <JuceHeader.h>
#include "Item.h"

class AudioClipEditorComponent : public juce::Component
{
public:
    AudioClipEditorComponent()
    {
        // Control buttons
        silenceButton.setButtonText ("Silence");
        silenceButton.onClick = [this] { silenceSelection(); };
        addAndMakeVisible (silenceButton);

        reverseButton.setButtonText ("Reverse");
        reverseButton.onClick = [this] { reverseSelection(); };
        addAndMakeVisible (reverseButton);

        gainButton.setButtonText ("Gain +3dB");
        gainButton.onClick = [this] { applyGain (1.4125f); };
        addAndMakeVisible (gainButton);

        attenuateButton.setButtonText ("Gain -3dB");
        attenuateButton.onClick = [this] { applyGain (0.7079f); };
        addAndMakeVisible (attenuateButton);

        fadeInButton.setButtonText ("Fade In");
        fadeInButton.onClick = [this] { fadeInSelection(); };
        addAndMakeVisible (fadeInButton);

        fadeOutButton.setButtonText ("Fade Out");
        fadeOutButton.onClick = [this] { fadeOutSelection(); };
        addAndMakeVisible (fadeOutButton);

        normalizeButton.setButtonText ("Normalize");
        normalizeButton.onClick = [this] { normalizeSelection(); };
        addAndMakeVisible (normalizeButton);
    }

    void setItem (Item* newItem)
    {
        item = newItem;
        selectionStartSample = 0;
        selectionEndSample = 0;
        repaint();
    }

    std::function<void()> onBufferModified;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff121214));

        if (!item || item->getType() != Item::Type::Audio)
        {
            g.setColour (juce::Colour (0xff8e8e93));
            g.drawFittedText ("No audio clip selected", getLocalBounds(), juce::Justification::centred, 1);
            return;
        }

        const auto& buffer = item->getBuffer();
        int totalSamples = buffer.getNumSamples();
        if (totalSamples == 0) return;

        // Waveform bounds
        juce::Rectangle<int> waveBounds = getWaveBounds();

        g.setColour (juce::Colour (0xff1a1a1e));
        g.fillRect (waveBounds);
        
        g.setColour (juce::Colour (0xff2a2a30));
        g.drawRect (waveBounds, 1);

        // Selection overlay
        if (selectionStartSample != selectionEndSample)
        {
            int sStart = std::min (selectionStartSample, selectionEndSample);
            int sEnd = std::max (selectionStartSample, selectionEndSample);

            float xStart = (float)sStart / totalSamples * waveBounds.getWidth() + waveBounds.getX();
            float xEnd = (float)sEnd / totalSamples * waveBounds.getWidth() + waveBounds.getX();

            g.setColour (juce::Colour (0x3000b4d8)); // Transparent cyan
            g.fillRect (xStart, (float)waveBounds.getY(), xEnd - xStart, (float)waveBounds.getHeight());
        }

        // Draw center line
        g.setColour (juce::Colour (0xff2a2a30));
        g.drawHorizontalLine (waveBounds.getCentreY(), (float)waveBounds.getX(), (float)waveBounds.getRight());

        // Draw waveform
        const float* readPtr = buffer.getReadPointer (0); // Draw channel 0
        int numPixels = waveBounds.getWidth();
        float samplesPerPixel = (float)totalSamples / (float)numPixels;

        g.setColour (juce::Colour (0xff00b4d8)); // Flat cyan
        
        for (int x = 0; x < numPixels; ++x)
        {
            int startSample = (int)(x * samplesPerPixel);
            int endSample = (int)((x + 1) * samplesPerPixel);
            startSample = juce::jlimit (0, totalSamples - 1, startSample);
            endSample = juce::jlimit (0, totalSamples, endSample);

            float minVal = 0.0f;
            float maxVal = 0.0f;

            for (int i = startSample; i < endSample; ++i)
            {
                float val = readPtr[i];
                if (val < minVal) minVal = val;
                if (val > maxVal) maxVal = val;
            }

            float drawX = (float)(waveBounds.getX() + x);
            float yMin = waveBounds.getCentreY() + minVal * (waveBounds.getHeight() / 2.0f);
            float yMax = waveBounds.getCentreY() + maxVal * (waveBounds.getHeight() / 2.0f);

            g.drawVerticalLine ((int)drawX, yMin, yMax);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto controlsArea = bounds.removeFromTop (45).reduced (5);

        juce::FlexBox fb;
        fb.flexDirection = juce::FlexBox::Direction::row;
        fb.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
        fb.alignContent = juce::FlexBox::AlignContent::center;

        juce::FlexItem::Margin margin (0, 3, 0, 3);

        fb.items.add (juce::FlexItem (silenceButton).withFlex (1.0f).withMargin (margin));
        fb.items.add (juce::FlexItem (reverseButton).withFlex (1.0f).withMargin (margin));
        fb.items.add (juce::FlexItem (gainButton).withFlex (1.0f).withMargin (margin));
        fb.items.add (juce::FlexItem (attenuateButton).withFlex (1.0f).withMargin (margin));
        fb.items.add (juce::FlexItem (fadeInButton).withFlex (1.0f).withMargin (margin));
        fb.items.add (juce::FlexItem (fadeOutButton).withFlex (1.0f).withMargin (margin));
        fb.items.add (juce::FlexItem (normalizeButton).withFlex (1.0f).withMargin (margin));

        fb.performLayout (controlsArea);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        juce::Rectangle<int> waveBounds = getWaveBounds();
        if (!item || !waveBounds.contains (e.getPosition())) return;

        const auto& buffer = item->getBuffer();
        int totalSamples = buffer.getNumSamples();
        if (totalSamples == 0) return;

        double relX = (double)(e.x - waveBounds.getX()) / waveBounds.getWidth();
        selectionStartSample = juce::jlimit (0, totalSamples - 1, (int)(relX * totalSamples));
        selectionEndSample = selectionStartSample;
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        juce::Rectangle<int> waveBounds = getWaveBounds();
        if (!item) return;

        const auto& buffer = item->getBuffer();
        int totalSamples = buffer.getNumSamples();
        if (totalSamples == 0) return;

        double relX = (double)(e.x - waveBounds.getX()) / waveBounds.getWidth();
        selectionEndSample = juce::jlimit (0, totalSamples, (int)(relX * totalSamples));
        repaint();
    }

private:
    juce::Rectangle<int> getWaveBounds() const
    {
        return getLocalBounds().withTrimmedTop (45).reduced (8);
    }

    void getSelectionRange (int& sStart, int& sEnd)
    {
        if (!item) return;
        int totalSamples = item->getBuffer().getNumSamples();
        if (selectionStartSample == selectionEndSample)
        {
            // If nothing is selected, treat the whole file as selected
            sStart = 0;
            sEnd = totalSamples;
        }
        else
        {
            sStart = std::min (selectionStartSample, selectionEndSample);
            sEnd = std::max (selectionStartSample, selectionEndSample);
            sStart = juce::jlimit (0, totalSamples, sStart);
            sEnd = juce::jlimit (0, totalSamples, sEnd);
        }
    }

    void silenceSelection()
    {
        if (!item) return;
        auto& buffer = item->getAudioBuffer();
        int sStart, sEnd;
        getSelectionRange (sStart, sEnd);
        if (sStart >= sEnd) return;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* writePtr = buffer.getWritePointer (ch);
            std::fill (writePtr + sStart, writePtr + sEnd, 0.0f);
        }

        repaint();
        if (onBufferModified) onBufferModified();
    }

    void reverseSelection()
    {
        if (!item) return;
        auto& buffer = item->getAudioBuffer();
        int sStart, sEnd;
        getSelectionRange (sStart, sEnd);
        if (sStart >= sEnd) return;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* writePtr = buffer.getWritePointer (ch);
            std::reverse (writePtr + sStart, writePtr + sEnd);
        }

        repaint();
        if (onBufferModified) onBufferModified();
    }

    void applyGain (float gain)
    {
        if (!item) return;
        auto& buffer = item->getAudioBuffer();
        int sStart, sEnd;
        getSelectionRange (sStart, sEnd);
        if (sStart >= sEnd) return;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* writePtr = buffer.getWritePointer (ch);
            for (int i = sStart; i < sEnd; ++i)
                writePtr[i] *= gain;
        }

        repaint();
        if (onBufferModified) onBufferModified();
    }

    void fadeInSelection()
    {
        if (!item) return;
        auto& buffer = item->getAudioBuffer();
        int sStart, sEnd;
        getSelectionRange (sStart, sEnd);
        int len = sEnd - sStart;
        if (len <= 0) return;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* writePtr = buffer.getWritePointer (ch);
            for (int i = sStart; i < sEnd; ++i)
            {
                float progress = (float)(i - sStart) / (float)len;
                writePtr[i] *= progress;
            }
        }

        repaint();
        if (onBufferModified) onBufferModified();
    }

    void fadeOutSelection()
    {
        if (!item) return;
        auto& buffer = item->getAudioBuffer();
        int sStart, sEnd;
        getSelectionRange (sStart, sEnd);
        int len = sEnd - sStart;
        if (len <= 0) return;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* writePtr = buffer.getWritePointer (ch);
            for (int i = sStart; i < sEnd; ++i)
            {
                float progress = 1.0f - (float)(i - sStart) / (float)len;
                writePtr[i] *= progress;
            }
        }

        repaint();
        if (onBufferModified) onBufferModified();
    }

    void normalizeSelection()
    {
        if (!item) return;
        auto& buffer = item->getAudioBuffer();
        int sStart, sEnd;
        getSelectionRange (sStart, sEnd);
        if (sStart >= sEnd) return;

        float peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* readPtr = buffer.getReadPointer (ch);
            for (int i = sStart; i < sEnd; ++i)
                peak = std::max (peak, std::abs (readPtr[i]));
        }

        if (peak > 0.0001f)
        {
            float gain = 1.0f / peak;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                float* writePtr = buffer.getWritePointer (ch);
                for (int i = sStart; i < sEnd; ++i)
                    writePtr[i] *= gain;
            }
        }

        repaint();
        if (onBufferModified) onBufferModified();
    }

    Item* item = nullptr;
    int selectionStartSample = 0;
    int selectionEndSample = 0;

    juce::TextButton silenceButton;
    juce::TextButton reverseButton;
    juce::TextButton gainButton;
    juce::TextButton attenuateButton;
    juce::TextButton fadeInButton;
    juce::TextButton fadeOutButton;
    juce::TextButton normalizeButton;
};

class AudioClipEditorWindow : public juce::DocumentWindow
{
public:
    AudioClipEditorWindow()
        : DocumentWindow ("Audio Clip Editor", juce::Colour (0xff1a1a1e), DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentNonOwned (&contentComp, false);
        setResizable (true, true);
        centreWithSize (900, 450);
        
        contentComp.onBufferModified = [this] {
            if (onBufferModified) onBufferModified();
        };
    }

    void setItem (Item* item)
    {
        contentComp.setItem (item);
    }

    void closeButtonPressed() override
    {
        delete this;
    }

    std::function<void()> onBufferModified;

private:
    AudioClipEditorComponent contentComp;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioClipEditorWindow)
};
