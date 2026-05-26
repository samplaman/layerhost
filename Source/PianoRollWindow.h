#pragma once
#include <JuceHeader.h>
#include "Item.h"

class PianoRollComponent : public juce::Component
{
public:
    PianoRollComponent (AudioEngine& engine)
        : audioEngine (engine)
    {
        setWantsKeyboardFocus (true);
        setSize(2000, 128 * 12); // 128 keys * 12 pixels = 1536 pixels high
    }

    void setItem(Item* itemToEdit)
    {
        item = itemToEdit;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff121214));

        for (int i = 0; i < 128; ++i)
        {
            int y = i * noteHeight;
            
            int noteNum = 127 - i;
            int nInOct = noteNum % 12;
            bool isBlack = (nInOct == 1 || nInOct == 3 || nInOct == 6 || nInOct == 8 || nInOct == 10);
            
            juce::String noteName;
            switch (nInOct)
            {
                case 0:  noteName = "C";  break;
                case 1:  noteName = "C#"; break;
                case 2:  noteName = "D";  break;
                case 3:  noteName = "D#"; break;
                case 4:  noteName = "E";  break;
                case 5:  noteName = "F";  break;
                case 6:  noteName = "F#"; break;
                case 7:  noteName = "G";  break;
                case 8:  noteName = "G#"; break;
                case 9:  noteName = "A";  break;
                case 10: noteName = "A#"; break;
                case 11: noteName = "B";  break;
            }
            noteName += juce::String (noteNum / 12 - 1);

            if (isBlack)
            {
                g.setColour(juce::Colour(0xff1a1a1e));
                g.fillRect(0, y, 40, noteHeight);
                
                g.setColour(juce::Colour(0xff6e6e73));
                g.setFont(juce::Font(7.5f));
                g.drawText(noteName, 2, y, 35, noteHeight, juce::Justification::centredRight);
            }
            else
            {
                g.setColour(juce::Colour(0xff2d2d34));
                g.fillRect(0, y, 60, noteHeight);
                g.setColour(juce::Colour(0xff1e1e24));
                g.drawRect(0, y, 60, noteHeight, 1);
                
                if (nInOct == 0)
                {
                    g.setColour(juce::Colour(0xffffffff));
                    g.setFont(juce::Font(9.0f, juce::Font::bold));
                }
                else
                {
                    g.setColour(juce::Colour(0xffa1a1a6));
                    g.setFont(juce::Font(8.0f));
                }
                g.drawText(noteName, 2, y, 54, noteHeight, juce::Justification::centredRight);
            }
            
            // Grid line
            g.setColour(isBlack ? juce::Colour(0xff141417) : juce::Colour(0xff18181c));
            g.fillRect(60, y, getWidth() - 60, noteHeight);
            g.setColour(juce::Colour(0xff222227));
            g.drawLine(60.0f, (float)y, (float)getWidth(), (float)y);
        }

        // Draw vertical grid lines (beats - minor)
        g.setColour(juce::Colour(0xff1e1e24));
        for (int x = 60; x < getWidth(); x += 50)
        {
            g.drawVerticalLine(x, 0.0f, (float)getHeight());
        }

        // Draw major vertical grid lines (bars - major)
        g.setColour(juce::Colour(0xff2a2a32));
        for (int x = 60; x < getWidth(); x += 200)
        {
            g.drawVerticalLine(x, 0.0f, (float)getHeight());
        }

        // Draw notes
        if (item)
        {
            auto& seq = item->getMidiSequence();
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* ev = seq.getEventPointer(i);
                if (ev->message.isNoteOn())
                {
                    int noteNum = ev->message.getNoteNumber();
                    double timeSec = ev->message.getTimeStamp();
                    double lengthSec = 0.5; // default

                    // find corresponding note off
                    auto* noteOff = seq.getEventPointer(seq.getIndexOfMatchingKeyUp(i));
                    if (noteOff)
                        lengthSec = noteOff->message.getTimeStamp() - timeSec;

                    int x = 60 + (int)(timeSec * pixelsPerSecond);
                    int y = (127 - noteNum) * noteHeight;
                    int w = (int)(lengthSec * pixelsPerSecond);
                    
                    int velocity = ev->message.getVelocity();
                    float velocityRatio = velocity / 127.0f;
                    float alpha = 0.35f + 0.65f * velocityRatio; // minimum 35% opacity
                    
                    g.setColour (juce::Colour (0xff00b4d8).withAlpha (alpha)); // Flat cyan note with velocity opacity
                    g.fillRoundedRectangle (x, y, juce::jmax (5, w), noteHeight - 1, 2.0f);
                    g.setColour (juce::Colour (0xff4fc3f7).withAlpha (alpha)); // Soft outline
                    g.drawRoundedRectangle (x, y, juce::jmax (5, w), noteHeight - 1, 2.0f, 1.0f);
                }
            }
        }
    }

    int findNoteAt (int mouseX, int mouseY, bool& isNearRightEdge)
    {
        if (!item || mouseX < 60) return -1;
        
        auto& seq = item->getMidiSequence();
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* ev = seq.getEventPointer(i);
            if (ev->message.isNoteOn())
            {
                int noteNum = ev->message.getNoteNumber();
                int y = (127 - noteNum) * noteHeight;
                
                if (mouseY >= y && mouseY < y + noteHeight)
                {
                    double timeSec = ev->message.getTimeStamp();
                    double lengthSec = 0.5;
                    auto* noteOff = seq.getEventPointer (seq.getIndexOfMatchingKeyUp (i));
                    if (noteOff)
                        lengthSec = noteOff->message.getTimeStamp() - timeSec;
                    
                    int startX = 60 + (int)(timeSec * pixelsPerSecond);
                    int endX = startX + (int)(lengthSec * pixelsPerSecond);
                    
                    if (mouseX >= startX && mouseX <= endX)
                    {
                        isNearRightEdge = (mouseX >= endX - 8);
                        return i;
                    }
                }
            }
        }
        return -1;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        if (e.x < 60) return;
        if (!item) return;

        bool isNearEdge = false;
        int clickedNoteIdx = findNoteAt (e.x, e.y, isNearEdge);

        if (clickedNoteIdx != -1)
        {
            if (e.mods.isRightButtonDown())
            {
                auto& seq = item->getMidiSequence();
                int offIdx = seq.getIndexOfMatchingKeyUp (clickedNoteIdx);
                if (offIdx != -1)
                    seq.deleteEvent (offIdx, true);
                seq.deleteEvent (clickedNoteIdx, true);
                
                item->updateMidiLength();
                if (onItemsChanged) onItemsChanged();
                repaint();
                return;
            }

            activeNoteIndex = clickedNoteIdx;
            auto& seq = item->getMidiSequence();
            activeNoteOffIndex = seq.getIndexOfMatchingKeyUp (clickedNoteIdx);
            
            auto* evOn = seq.getEventPointer (activeNoteIndex);
            dragStartNoteOnTime = evOn->message.getTimeStamp();
            dragStartNoteNum = evOn->message.getNoteNumber();
            
            if (activeNoteOffIndex != -1)
                dragStartNoteOffTime = seq.getEventPointer (activeNoteOffIndex)->message.getTimeStamp();
            else
                dragStartNoteOffTime = dragStartNoteOnTime + 0.5;

            dragStartMouse = e.getPosition();
            noteDragMode = isNearEdge ? NoteDragMode::Resize : NoteDragMode::Move;
        }
        else
        {
            if (e.mods.isRightButtonDown()) return;

            double timeSec = (e.x - 60) / pixelsPerSecond;
            double snapInterval = audioEngine.getSnapIntervalSeconds();
            if (snapInterval > 0.0)
                timeSec = std::round (timeSec / snapInterval) * snapInterval;

            int noteNum = 127 - (e.y / noteHeight);
            
            item->getMidiSequence().addEvent (juce::MidiMessage::noteOn (1, noteNum, (juce::uint8)100), timeSec);
            item->getMidiSequence().addEvent (juce::MidiMessage::noteOff (1, noteNum, (juce::uint8)0), timeSec + 0.5);
            item->getMidiSequence().updateMatchedPairs();
            item->updateMidiLength();
            if (onItemsChanged) onItemsChanged();
            repaint();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (!item || noteDragMode == NoteDragMode::None || activeNoteIndex == -1) return;

        auto& seq = item->getMidiSequence();
        auto* evOn = seq.getEventPointer (activeNoteIndex);
        if (!evOn) return;

        double deltaX = (e.x - dragStartMouse.x) / pixelsPerSecond;

        if (noteDragMode == NoteDragMode::Resize)
        {
            double newOffTime = dragStartNoteOffTime + deltaX;
            double snapInterval = audioEngine.getSnapIntervalSeconds();
            if (snapInterval > 0.0)
                newOffTime = std::round (newOffTime / snapInterval) * snapInterval;
            
            newOffTime = juce::jmax (dragStartNoteOnTime + 0.05, newOffTime);
            auto* evOff = seq.getEventPointer (activeNoteOffIndex);
            if (evOff)
            {
                evOff->message.setTimeStamp (newOffTime);
            }
        }
        else if (noteDragMode == NoteDragMode::Move)
        {
            int deltaY = (dragStartMouse.y - e.y) / noteHeight;
            int newNoteNum = juce::jlimit (0, 127, dragStartNoteNum + deltaY);
            double newOnTime = dragStartNoteOnTime + deltaX;
            double snapInterval = audioEngine.getSnapIntervalSeconds();
            if (snapInterval > 0.0)
                newOnTime = std::round (newOnTime / snapInterval) * snapInterval;
            
            newOnTime = juce::jmax (0.0, newOnTime);
            double noteLen = dragStartNoteOffTime - dragStartNoteOnTime;
            double newOffTime = newOnTime + noteLen;

            evOn->message.setTimeStamp (newOnTime);
            evOn->message.setNoteNumber (newNoteNum);

            auto* evOff = seq.getEventPointer (activeNoteOffIndex);
            if (evOff)
            {
                evOff->message.setTimeStamp (newOffTime);
                evOff->message.setNoteNumber (newNoteNum);
            }
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (noteDragMode != NoteDragMode::None)
        {
            if (item)
            {
                item->getMidiSequence().updateMatchedPairs();
                item->updateMidiLength();
                if (onItemsChanged) onItemsChanged();
            }
            noteDragMode = NoteDragMode::None;
            activeNoteIndex = -1;
            activeNoteOffIndex = -1;
            repaint();
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        bool isNearEdge = false;
        int hoverIdx = findNoteAt (e.x, e.y, isNearEdge);
        if (hoverIdx != -1 && isNearEdge)
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        else
            setMouseCursor (juce::MouseCursor::NormalCursor);
    }

private:
    AudioEngine& audioEngine;
    Item* item = nullptr;
    int noteHeight = 12;
    double pixelsPerSecond = 100.0;
    
    int activeNoteIndex = -1;
    int activeNoteOffIndex = -1;
    enum class NoteDragMode { None, Move, Resize };
    NoteDragMode noteDragMode = NoteDragMode::None;
    double dragStartNoteOnTime = 0.0;
    double dragStartNoteOffTime = 0.0;
    int dragStartNoteNum = 0;
    juce::Point<int> dragStartMouse;
public:
    std::function<void()> onItemsChanged;
};

class VelocityDrawerComponent : public juce::Component
{
public:
    VelocityDrawerComponent (AudioEngine& engine)
        : audioEngine (engine)
    {
        setWantsKeyboardFocus (true);
    }

    void setItem (Item* itemToEdit)
    {
        item = itemToEdit;
        repaint();
    }

    void setHorizontalOffset (int offset)
    {
        horizontalOffset = offset;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff121214));

        // Draw vertical grid lines (beats - minor)
        g.setColour (juce::Colour (0xff1e1e24));
        for (int x = 60; x < getWidth() + horizontalOffset; x += 50)
        {
            int drawX = x - horizontalOffset;
            if (drawX >= 60)
                g.drawVerticalLine (drawX, 0.0f, (float)getHeight());
        }

        // Draw major vertical grid lines (bars - major)
        g.setColour (juce::Colour (0xff2a2a32));
        for (int x = 60; x < getWidth() + horizontalOffset; x += 200)
        {
            int drawX = x - horizontalOffset;
            if (drawX >= 60)
                g.drawVerticalLine (drawX, 0.0f, (float)getHeight());
        }

        // Draw border at x = 60
        g.setColour (juce::Colour (0xff28282e));
        g.drawVerticalLine (60, 0.0f, (float)getHeight());
        
        // Draw "VELOCITY" label
        g.setColour (juce::Colour (0xff8e8e93));
        g.setFont (juce::Font (10.0f, juce::Font::bold));
        g.drawText ("VELOCITY", 5, 0, 50, getHeight(), juce::Justification::centred);

        // Draw velocity bars
        if (item != nullptr)
        {
            auto& seq = item->getMidiSequence();
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* ev = seq.getEventPointer (i);
                if (ev->message.isNoteOn())
                {
                    double timeSec = ev->message.getTimeStamp();
                    int x = 60 + (int)(timeSec * pixelsPerSecond) - horizontalOffset;
                    if (x < 60) continue;

                    int velocity = ev->message.getVelocity();
                    float pct = velocity / 127.0f;
                    
                    int barHeight = (int)(pct * (getHeight() - 20));
                    int y = getHeight() - 10 - barHeight;

                    g.setColour (juce::Colour (0xff00b4d8));
                    g.fillRect (x - 1, y, 2, barHeight);
                    
                    g.setColour (juce::Colour (0xff00b4d8));
                    g.fillEllipse ((float)(x - 4), (float)(y - 4), 8.0f, 8.0f);
                    g.setColour (juce::Colours::white);
                    g.drawEllipse ((float)(x - 4), (float)(y - 4), 8.0f, 8.0f, 1.0f);
                }
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        activeNoteIdx = findNoteAt (e.x, e.y);
        if (activeNoteIdx != -1)
        {
            updateVelocityFromMouse (e.y);
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (activeNoteIdx != -1)
        {
            updateVelocityFromMouse (e.y);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        activeNoteIdx = -1;
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        int noteIdx = findNoteAt (e.x, e.y);
        if (noteIdx != -1)
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        else
            setMouseCursor (juce::MouseCursor::NormalCursor);
    }

private:
    int findNoteAt (int mouseX, int mouseY)
    {
        if (item == nullptr || mouseX < 60) return -1;

        auto& seq = item->getMidiSequence();
        int bestIndex = -1;
        float bestDistance = 999999.0f;

        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* ev = seq.getEventPointer (i);
            if (ev->message.isNoteOn())
            {
                double timeSec = ev->message.getTimeStamp();
                int x = 60 + (int)(timeSec * pixelsPerSecond) - horizontalOffset;
                
                if (std::abs (mouseX - x) <= 8)
                {
                    int velocity = ev->message.getVelocity();
                    float pct = velocity / 127.0f;
                    int barHeight = (int)(pct * (getHeight() - 20));
                    int y = getHeight() - 10 - barHeight;

                    float dy = (float)(mouseY - y);
                    float dist = std::abs (dy);

                    if (dist < bestDistance)
                    {
                        bestDistance = dist;
                        bestIndex = i;
                    }
                }
            }
        }
        return bestIndex;
    }

    void updateVelocityFromMouse (int mouseY)
    {
        if (item == nullptr || activeNoteIdx == -1) return;

        int availableHeight = getHeight() - 20;
        int relativeY = getHeight() - 10 - mouseY;
        float pct = juce::jlimit (0.0f, 1.0f, (float)relativeY / (float)availableHeight);
        int newVel = juce::jlimit (1, 127, (int)(pct * 127.0f));

        auto& seq = item->getMidiSequence();
        auto* ev = seq.getEventPointer (activeNoteIdx);
        if (ev != nullptr && ev->message.isNoteOn())
        {
            ev->message.setVelocity ((juce::uint8)newVel);
            
            repaint();
            if (onVelocityChanged)
                onVelocityChanged();
        }
    }

    AudioEngine& audioEngine;
    Item* item = nullptr;
    int horizontalOffset = 0;
    double pixelsPerSecond = 100.0;
    int activeNoteIdx = -1;

public:
    std::function<void()> onVelocityChanged;
};

class PianoRollContentComponent : public juce::Component, public juce::ScrollBar::Listener
{
public:
    PianoRollContentComponent (AudioEngine& engine)
        : audioEngine (engine),
          pianoRollComp (engine),
          velocityDrawer (engine)
    {
        setWantsKeyboardFocus (true);
        
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&pianoRollComp, false);
        viewport.setScrollBarsShown (true, true, true, true);
        
        addAndMakeVisible (velocityDrawer);
        
        viewport.getHorizontalScrollBar().addListener (this);
        
        pianoRollComp.onItemsChanged = [this] {
            velocityDrawer.repaint();
            if (onItemsChanged) onItemsChanged();
        };
        
        velocityDrawer.onVelocityChanged = [this] {
            pianoRollComp.repaint();
            if (onItemsChanged) onItemsChanged();
        };
    }
    
    ~PianoRollContentComponent() override
    {
        viewport.getHorizontalScrollBar().removeListener (this);
    }
    
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::leftKey)
        {
            double bpm = audioEngine.getBpm();
            double tickLengthSecs = 60.0 / (bpm * 960.0);
            double currentPos = audioEngine.getPlayPosition();
            audioEngine.setPlayPosition (juce::jmax (0.0, currentPos - tickLengthSecs));
            pianoRollComp.repaint();
            if (onItemsChanged) onItemsChanged();
            return true;
        }
        if (key == juce::KeyPress::rightKey)
        {
            double bpm = audioEngine.getBpm();
            double tickLengthSecs = 60.0 / (bpm * 960.0);
            double currentPos = audioEngine.getPlayPosition();
            audioEngine.setPlayPosition (currentPos + tickLengthSecs);
            pianoRollComp.repaint();
            if (onItemsChanged) onItemsChanged();
            return true;
        }
        if (key == juce::KeyPress::spaceKey)
        {
            audioEngine.setPlaying (!audioEngine.getPlaying());
            pianoRollComp.repaint();
            if (onItemsChanged) onItemsChanged();
            return true;
        }
        return false;
    }
    
    void setItem (Item* item)
    {
        pianoRollComp.setItem (item);
        velocityDrawer.setItem (item);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds();
        velocityDrawer.setBounds (bounds.removeFromBottom (100));
        viewport.setBounds (bounds);
    }
    
    void scrollBarMoved (juce::ScrollBar* scrollBar, double newRangeStart) override
    {
        if (scrollBar == &viewport.getHorizontalScrollBar())
        {
            velocityDrawer.setHorizontalOffset ((int)newRangeStart);
        }
    }
    
    juce::Viewport& getViewport() { return viewport; }
    PianoRollComponent& getPianoRollComp() { return pianoRollComp; }

    std::function<void()> onItemsChanged;

private:
    AudioEngine& audioEngine;
    juce::Viewport viewport;
    PianoRollComponent pianoRollComp;
    VelocityDrawerComponent velocityDrawer;
};

class PianoRollWindow : public juce::DocumentWindow
{
public:
    static inline juce::Array<PianoRollWindow*> openWindows;

    PianoRollWindow (AudioEngine& engine)
        : DocumentWindow("Piano Roll", juce::Colour(0xff1a1a1e), DocumentWindow::closeButton),
          contentComp (engine)
    {
        setUsingNativeTitleBar(true);
        setContentNonOwned(&contentComp, false);
        
        setResizable(true, true);
        centreWithSize(800, 600);
        
        contentComp.getPianoRollComp().setSize(2000, 128 * 12);
        contentComp.getViewport().setViewPosition(0, 529);
        
        contentComp.onItemsChanged = [this] {
            if (onItemsChanged) onItemsChanged();
        };

        openWindows.add (this);
    }

    ~PianoRollWindow() override
    {
        openWindows.removeFirstMatchingValue (this);
    }

    static void updateAllOpenWindows()
    {
        for (auto* w : openWindows)
        {
            if (w != nullptr)
            {
                w->repaint();
                w->contentComp.repaint();
                w->contentComp.getPianoRollComp().repaint();
            }
        }
    }

    void setItem(Item* item)
    {
        contentComp.setItem(item);
    }

    std::function<void()> onItemsChanged;

    void closeButtonPressed() override
    {
        delete this;
    }

private:
    PianoRollContentComponent contentComp;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollWindow)
};
