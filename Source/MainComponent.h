#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "PianoRollWindow.h"
#include "AudioClipEditorWindow.h"

// A custom LookAndFeel for a premium, flat dark aesthetic
class ModernDawLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ModernDawLookAndFeel()
    {
        // Window colours
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff121214));

        // Slider colours
        setColour (juce::Slider::thumbColourId, juce::Colour (0xff00b4d8));
        setColour (juce::Slider::trackColourId, juce::Colour (0xff0077b6));
        setColour (juce::Slider::backgroundColourId, juce::Colour (0xff1a1a1e));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1a1a1e));
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe2e2e6));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff2a2a30));

        // Button colours
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff242428));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff00b4d8));
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xffe2e2e6));
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);

        // Label colours
        setColour (juce::Label::textColourId, juce::Colour (0xffe2e2e6));

        // ComboBox colours
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff242428));
        setColour (juce::ComboBox::textColourId, juce::Colour (0xffe2e2e6));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff2a2a30));
        setColour (juce::ComboBox::arrowColourId, juce::Colour (0xffe2e2e6));
        setColour (juce::ComboBox::buttonColourId, juce::Colour (0xff242428));

        // PopupMenu colours
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1a1a1e));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xffe2e2e6));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff00b4d8));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::PopupMenu::headerTextColourId, juce::Colour (0xff8e8e93));

        // ScrollBar colours
        setColour (juce::ScrollBar::backgroundColourId, juce::Colour (0xff121214));
        setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff2d2d34));
    }
    
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto baseColour = button.getToggleState()
            ? button.findColour (juce::TextButton::buttonOnColourId)
            : button.findColour (juce::TextButton::buttonColourId);

        if (shouldDrawButtonAsDown)
            baseColour = baseColour.withMultipliedAlpha (1.2f);
        else if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.withMultipliedAlpha (1.1f);

        // Flat fill
        g.setColour (baseColour);
        g.fillRoundedRectangle (bounds, 4.0f);

        // Flat outline
        g.setColour (juce::Colour (0xff2a2a30));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        auto bounds = slider.getLocalBounds().toFloat().reduced(2.0f);
        bool isHorizontal = slider.isHorizontal();
        float trackSize = 4.0f; // thinner/cleaner track
        
        // Draw marking lines for vertical mixer faders
        if (!isHorizontal)
        {
            float trackTop = bounds.getY() + 8.0f;
            float trackBottom = bounds.getBottom() - 8.0f;
            float trackHeight = trackBottom - trackTop;
            
            g.setColour (juce::Colour (0xff3a3a42));
            for (int step = 0; step <= 10; ++step)
            {
                float yTick = trackTop + (trackHeight * (step / 10.0f));
                bool isMajor = (step % 2 == 0);
                float tickLength = isMajor ? 5.0f : 3.0f;
                
                // Left tick mark
                g.drawLine (bounds.getCentreX() - 16.0f, yTick, bounds.getCentreX() - 16.0f + tickLength, yTick, 1.0f);
                // Right tick mark
                g.drawLine (bounds.getCentreX() + 16.0f, yTick, bounds.getCentreX() + 16.0f - tickLength, yTick, 1.0f);
            }
        }

        juce::Rectangle<float> track;
        if (isHorizontal)
            track = juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - trackSize/2.0f, bounds.getWidth(), trackSize);
        else
            track = juce::Rectangle<float> (bounds.getCentreX() - trackSize/2.0f, bounds.getY(), trackSize, bounds.getHeight());
            
        g.setColour (juce::Colour (0xff1a1a1e)); // Flat dark background for track
        g.fillRoundedRectangle (track, trackSize / 2.0f);
        
        juce::Rectangle<float> activeTrack = track;
        if (isHorizontal)
            activeTrack.setWidth (sliderPos - bounds.getX());
        else
            activeTrack.setTop (sliderPos);
            
        juce::Colour fillColour = slider.findColour (juce::Slider::trackColourId);
        g.setColour (fillColour);
        g.fillRoundedRectangle (activeTrack, trackSize / 2.0f);
        
        juce::Rectangle<float> thumb;
        if (isHorizontal)
        {
            float thumbSize = 12.0f;
            thumb = juce::Rectangle<float> (sliderPos - thumbSize/2.0f, bounds.getCentreY() - thumbSize/2.0f, thumbSize, thumbSize);
            g.setColour (slider.findColour (juce::Slider::thumbColourId));
            g.fillEllipse (thumb);
            g.setColour (juce::Colour (0xff121214));
            g.drawEllipse (thumb.reduced (0.5f), 1.0f);
        }
        else
        {
            // Vertical premium fader cap
            float thumbWidth = 20.0f;
            float thumbHeight = 12.0f;
            thumb = juce::Rectangle<float> (bounds.getCentreX() - thumbWidth/2.0f, sliderPos - thumbHeight/2.0f, thumbWidth, thumbHeight);
            
            // Console silver-grey fader cap
            g.setColour (juce::Colour (0xff323236));
            g.fillRoundedRectangle (thumb, 1.5f);
            g.setColour (juce::Colour (0xff242428));
            g.drawRoundedRectangle (thumb.reduced (0.5f), 1.5f, 1.0f);
            
            // Center indicator line (accent color)
            g.setColour (slider.findColour (juce::Slider::thumbColourId));
            g.drawHorizontalLine ((int)sliderPos, thumb.getX() + 1.0f, thumb.getRight() - 1.0f);
        }
    }
};

class ArrangementComponent;

class ItemComponent : public juce::Component
{
public:
    ItemComponent (Item* i, double pxPerSec, int trackIdx, ArrangementComponent* arr);
    
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

    void mouseUp (const juce::MouseEvent& e) override;

    Item* getItem() { return item; }
    int getTrackIdx() { return currentTrackIdx; }

    void resized() override
    {
        if (item && item->getType() == Item::Type::Midi)
        {
            editBtn.setBounds (getWidth() - 45, 5, 40, 20);
        }
    }

private:
    void splitItem (float xPos);

    juce::TextButton editBtn { "Edit" };

    Item* item;
    double pixelsPerSecond;
    int currentTrackIdx;
    ArrangementComponent* arrangement;
    juce::ComponentDragger dragger;

    enum class DragMode { Move, TrimLeft, TrimRight, None };
    DragMode dragMode = DragMode::None;
    bool hasSavedUndoForCurrentDrag = false;
};

class VuMeterComponent : public juce::Component
{
public:
    void setLevel (float newLevel) { level = newLevel; }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff151518));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 1.0f);
        
        if (level > 0.0f)
        {
            float dB = juce::Decibels::gainToDecibels (level, -60.0f);
            float proportion = juce::jmap (dB, -60.0f, 6.0f, 0.0f, 1.0f);
            proportion = juce::jlimit (0.0f, 1.0f, proportion);
            
            auto fillRect = getLocalBounds().toFloat().withTrimmedTop (getHeight() * (1.0f - proportion));
            
            g.setColour (juce::Colour (0xff2ecd7a)); // Flat neon green
            g.fillRoundedRectangle (fillRect, 1.0f);
        }
    }
private:
    float level = 0.0f;
};

class TrackHeaderComponent : public juce::Component
{
public:
    TrackHeaderComponent (Track* t, AudioEngine& engine, std::function<void()> onRepaintNeeded)
        : track (t), audioEngine (engine), triggerRepaint(onRepaintNeeded)
    {
        // Name Label
        addAndMakeVisible (nameLabel);
        nameLabel.setText (track->getName(), juce::dontSendNotification);
        nameLabel.setEditable (false, true, false);
        nameLabel.setFont (juce::Font (12.0f, juce::Font::bold));
        nameLabel.onTextChange = [this] {
            track->setName (nameLabel.getText());
            if (triggerRepaint) triggerRepaint();
        };

        // Type Label
        addAndMakeVisible (typeLabel);
        typeLabel.setText (track->getTrackType() == Track::Type::Midi ? "MIDI" : "AUDIO", juce::dontSendNotification);
        typeLabel.setFont (juce::Font (9.0f, juce::Font::plain));
        typeLabel.setColour (juce::Label::textColourId, juce::Colours::grey);

        // Mute Button
        addAndMakeVisible (muteBtn);
        muteBtn.setClickingTogglesState (true);
        muteBtn.setToggleState (track->getMute(), juce::dontSendNotification);
        muteBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffea4335));
        muteBtn.onClick = [this] { 
            track->setMute (muteBtn.getToggleState()); 
            if (triggerRepaint) triggerRepaint();
        };

        // Solo Button
        addAndMakeVisible (soloBtn);
        soloBtn.setClickingTogglesState (true);
        soloBtn.setToggleState (track->getSolo(), juce::dontSendNotification);
        soloBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xfffbc02d));
        soloBtn.onClick = [this] { 
            track->setSolo (soloBtn.getToggleState()); 
            if (triggerRepaint) triggerRepaint();
        };

        // Arm Button
        addAndMakeVisible (armBtn);
        armBtn.setClickingTogglesState (true);
        armBtn.setToggleState (track->getArmed(), juce::dontSendNotification);
        armBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffea4335));
        armBtn.onClick = [this] { 
            track->setArmed (armBtn.getToggleState()); 
            audioEngine.updateRouting();
            if (triggerRepaint) triggerRepaint();
        };

        // Monitor Button
        addAndMakeVisible (monBtn);
        monBtn.setClickingTogglesState (true);
        monBtn.setToggleState (track->getMonitor(), juce::dontSendNotification);
        monBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff00b4d8));
        monBtn.onClick = [this] { 
            track->setMonitor (monBtn.getToggleState()); 
            audioEngine.updateRouting();
            if (triggerRepaint) triggerRepaint();
        };

        // Automation Button
        addAndMakeVisible (autoBtn);
        autoBtn.setClickingTogglesState (false);
        autoBtn.setToggleState (track->getAutomationEnabled(), juce::dontSendNotification);
        autoBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xfffbc02d));
        autoBtn.onClick = [this] {
            juce::PopupMenu menu;
            menu.addItem (1, "Volume Automation", true, track->getVolumeAutomationEnabled());
            menu.addItem (2, "Pan Automation", true, track->getPanAutomationEnabled());
            
            bool hasPlugins = false;
            for (int slot = 0; slot < 8; ++slot)
            {
                if (track->getPlugin (slot))
                {
                    hasPlugins = true;
                    break;
                }
            }
            
            if (hasPlugins)
            {
                menu.addSeparator();
                for (int slot = 0; slot < 8; ++slot)
                {
                    if (auto* plugin = track->getPlugin (slot))
                    {
                        juce::PopupMenu pluginMenu;
                        auto params = plugin->getParameters();
                        for (int pIdx = 0; pIdx < params.size(); ++pIdx)
                        {
                            if (auto* param = params[pIdx])
                            {
                                juce::String paramName = param->getName (30);
                                if (paramName.trim().isEmpty())
                                    paramName = "Parameter " + juce::String (pIdx + 1);
                                    
                                auto* pa = track->getOrCreatePluginAutomation (slot, pIdx);
                                int itemID = 1000 + (slot * 10000) + pIdx;
                                pluginMenu.addItem (itemID, paramName, true, pa != nullptr && pa->enabled);
                            }
                        }
                        
                        juce::String slotName = "Slot " + juce::String (slot + 1) + ": " + plugin->getName();
                        menu.addSubMenu (slotName, pluginMenu);
                    }
                }
            }
            
            menu.addSeparator();
            menu.addItem (100, "Read Mode (Play Automation)", true, !track->getAutomationWriteEnabled());
            menu.addItem (101, "Write Mode (Record Slider Moves)", true, track->getAutomationWriteEnabled());
            
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&autoBtn),
                [this] (int result) {
                    if (result == 1)
                    {
                        track->setVolumeAutomationEnabled (!track->getVolumeAutomationEnabled());
                        updateStates();
                        if (triggerRepaint) triggerRepaint();
                    }
                    else if (result == 2)
                    {
                        track->setPanAutomationEnabled (!track->getPanAutomationEnabled());
                        updateStates();
                        if (triggerRepaint) triggerRepaint();
                    }
                    else if (result == 100)
                    {
                        track->setAutomationWriteEnabled (false);
                        updateStates();
                    }
                    else if (result == 101)
                    {
                        track->setAutomationWriteEnabled (true);
                        updateStates();
                    }
                    else if (result >= 1000)
                    {
                        int slot = (result - 1000) / 10000;
                        int pIdx = (result - 1000) % 10000;
                        
                        auto* pa = track->getOrCreatePluginAutomation (slot, pIdx);
                        if (pa != nullptr)
                            pa->enabled = !pa->enabled;
                        
                        updateStates();
                        if (triggerRepaint) triggerRepaint();
                    }
                });
        };

        // Volume Slider
        addAndMakeVisible (volSlider);
        volSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        volSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        volSlider.setRange (0.0, 2.0, 0.01);
        volSlider.setValue (track->getVolume());
        volSlider.onValueChange = [this] {
            track->setVolume ((float)volSlider.getValue());
        };
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (5);
        
        auto topRow = r.removeFromTop (20);
        nameLabel.setBounds (topRow.removeFromLeft (130));
        typeLabel.setBounds (topRow);

        if (getHeight() >= 65)
        {
            muteBtn.setVisible (true);
            soloBtn.setVisible (true);
            armBtn.setVisible (true);
            monBtn.setVisible (true);
            autoBtn.setVisible (true);
            volSlider.setVisible (true);

            r.removeFromTop (4); // Gap
            auto midRow = r.removeFromTop (20);
            int btnW = midRow.getWidth() / 5;
            muteBtn.setBounds (midRow.removeFromLeft (btnW).reduced (1));
            soloBtn.setBounds (midRow.removeFromLeft (btnW).reduced (1));
            armBtn.setBounds (midRow.removeFromLeft (btnW).reduced (1));
            monBtn.setBounds (midRow.removeFromLeft (btnW).reduced (1));
            autoBtn.setBounds (midRow.reduced (1));

            r.removeFromTop (4); // Gap
            volSlider.setBounds (r.removeFromTop (12));
        }
        else if (getHeight() >= 45)
        {
            muteBtn.setVisible (true);
            soloBtn.setVisible (true);
            armBtn.setVisible (true);
            monBtn.setVisible (true);
            autoBtn.setVisible (true);
            volSlider.setVisible (false);

            r.removeFromTop (4); // Gap
            auto midRow = r.removeFromTop (20);
            int btnW = midRow.getWidth() / 5;
            muteBtn.setBounds (midRow.removeFromLeft (btnW).reduced (1));
            soloBtn.setBounds (midRow.removeFromLeft (btnW).reduced (1));
            armBtn.setBounds (midRow.removeFromLeft (btnW).reduced (1));
            monBtn.setBounds (midRow.removeFromLeft (btnW).reduced (1));
            autoBtn.setBounds (midRow.reduced (1));
        }
        else
        {
            muteBtn.setVisible (false);
            soloBtn.setVisible (false);
            armBtn.setVisible (false);
            monBtn.setVisible (false);
            autoBtn.setVisible (false);
            volSlider.setVisible (false);
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (juce::Colour (0xff1a1a1e));
        g.fillRoundedRectangle (bounds, 4.0f);
        
        g.setColour (juce::Colour (0xff2a2a30));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    }

    void updateStates()
    {
        muteBtn.setToggleState (track->getMute(), juce::dontSendNotification);
        soloBtn.setToggleState (track->getSolo(), juce::dontSendNotification);
        armBtn.setToggleState (track->getArmed(), juce::dontSendNotification);
        monBtn.setToggleState (track->getMonitor(), juce::dontSendNotification);
        autoBtn.setToggleState (track->getAutomationEnabled(), juce::dontSendNotification);
        volSlider.setValue (track->getVolume(), juce::dontSendNotification);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        if (e.y >= getHeight() - 6)
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        else
            setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.y >= getHeight() - 6)
        {
            isResizing = true;
            initialHeight = track->getHeight();
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (isResizing)
        {
            int deltaY = e.getDistanceFromDragStartY();
            track->setHeight (initialHeight + deltaY);
            if (onHeightChanged)
                onHeightChanged();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (isResizing)
        {
            isResizing = false;
            setMouseCursor (juce::MouseCursor::NormalCursor);
        }
    }

    std::function<void()> onHeightChanged;

private:
    Track* track;
    AudioEngine& audioEngine;
    std::function<void()> triggerRepaint;
    bool isResizing = false;
    int initialHeight = 80;

    juce::Label nameLabel;
    juce::Label typeLabel;
    juce::TextButton muteBtn { "M" };
    juce::TextButton soloBtn { "S" };
    juce::TextButton armBtn { "A" };
    juce::TextButton monBtn { "Mon" };
    juce::TextButton autoBtn { "Auto" };
    juce::Slider volSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackHeaderComponent)
};

class ArrangementComponent : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    static constexpr int headerWidth = 240;
    Item* selectedItem = nullptr;
    int selectedTrackIdx = -1;

    enum class EditTool { Select, Split, Eraser, Mute };
    
    void setCurrentTool (EditTool tool) { currentTool = tool; }
    EditTool getCurrentTool() const { return currentTool; }
    
    void deleteItem (ItemComponent* comp)
    {
        if (comp != nullptr)
        {
            auto* track = audioEngine.getTrack (comp->getTrackIdx());
            track->extractItem (comp->getItem());
            if (selectedItem == comp->getItem())
                selectedItem = nullptr;
            updateItems();
            repaint();
        }
    }
    
    std::function<void()> onSaveUndoStateRequested;
    void saveUndoState() { if (onSaveUndoStateRequested) onSaveUndoStateRequested(); }

    ArrangementComponent (AudioEngine& engine) : audioEngine(engine)
    {
        setWantsKeyboardFocus (true);
        addAndMakeVisible (headersContainer);
        addAndMakeVisible (timelineContainer);
        timelineContainer.setInterceptsMouseClicks (false, true);
        addMouseListener (this, true); // Listen to children for scrubbing
    }
    
    void resized() override
    {
        headersContainer.setBounds (0, 30, headerWidth, getHeight() - 30);
        timelineContainer.setBounds (headerWidth, 30, getWidth() - headerWidth, getHeight() - 30);
        updateTrackHeaders();
    }
    
    double getScrollX() const { return scrollX; }
    AudioEngine& getAudioEngine() { return audioEngine; }
    
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto file : files)
            if (file.endsWithIgnoreCase (".wav"))
                return true;
        return false;
    }

    void filesDropped (const juce::StringArray& files, int x, int y) override;

    int draggedTrackIdx = -1;
    std::function<void(Item*)> onPianoRollRequested;
    std::function<void(Item*)> onAudioClipEditorRequested;
    std::function<void()> onTrackStatusChanged;

    void openPianoRoll (Item* item)
    {
        if (onPianoRollRequested)
            onPianoRollRequested (item);
    }

    void openAudioClipEditor (Item* item)
    {
        if (onAudioClipEditorRequested)
            onAudioClipEditorRequested (item);
    }

    void generateLFO (AutomationEnvelope& env, double startTime, double endTime, const juce::String& shape, double periodSec)
    {
        const juce::ScopedLock sl (env.lock);
        saveUndoState();
        
        std::vector<AutomationPoint> newPoints;
        for (const auto& pt : env.points)
        {
            if (pt.time < startTime || pt.time > endTime)
                newPoints.push_back (pt);
        }
        env.points = newPoints;
        
        double step = periodSec / 16.0;
        if (shape == "Square") step = periodSec / 2.0;
        
        for (double t = startTime; t <= endTime; t += step)
        {
            float val = 0.5f;
            double phase = std::fmod (t - startTime, periodSec) / periodSec;
            
            if (shape == "Sine")
            {
                val = 0.5f + 0.5f * (float)std::sin (2.0 * juce::MathConstants<double>::pi * phase);
            }
            else if (shape == "Triangle")
            {
                val = (phase < 0.5) ? (float)(phase * 2.0) : (float)(2.0 - phase * 2.0);
            }
            else if (shape == "Saw")
            {
                val = (float)phase;
            }
            else if (shape == "Square")
            {
                val = (phase < 0.5) ? 1.0f : 0.0f;
            }
            else if (shape == "Random")
            {
                static juce::Random rand;
                val = rand.nextFloat();
            }
            
            env.addOrUpdatePoint (t, val);
            
            if (shape == "Square" && t + step <= endTime)
            {
                env.addOrUpdatePoint (t + step - 0.001, val);
            }
        }
        
        repaint();
    }

    void applyPreset (AutomationEnvelope& env, double startTime, double duration, const juce::String& presetName)
    {
        const juce::ScopedLock sl (env.lock);
        saveUndoState();
        
        double endTime = startTime + duration;
        
        std::vector<AutomationPoint> newPoints;
        for (const auto& pt : env.points)
        {
            if (pt.time < startTime || pt.time > endTime)
                newPoints.push_back (pt);
        }
        env.points = newPoints;
        
        if (presetName == "Fade In")
        {
            env.addOrUpdatePoint (startTime, 0.0f);
            env.addOrUpdatePoint (endTime, 1.0f);
            if (!env.points.empty())
                env.points.front().curve = 0.5f;
        }
        else if (presetName == "Fade Out")
        {
            env.addOrUpdatePoint (startTime, 1.0f);
            env.addOrUpdatePoint (endTime, 0.0f);
            if (!env.points.empty())
                env.points.front().curve = -0.5f;
        }
        else if (presetName == "Exponential Rise")
        {
            env.addOrUpdatePoint (startTime, 0.0f);
            env.addOrUpdatePoint (endTime, 1.0f);
            if (!env.points.empty())
                env.points.front().curve = 0.8f;
        }
        else if (presetName == "Sidechain Quarter")
        {
            double beat = 60.0 / audioEngine.getBpm();
            for (double t = startTime; t < endTime; t += beat)
            {
                env.addOrUpdatePoint (t, 0.0f);
                env.addOrUpdatePoint (t + beat * 0.1, 0.0f);
                env.addOrUpdatePoint (t + beat * 0.8, 1.0f);
                env.addOrUpdatePoint (t + beat - 0.001, 1.0f);
            }
        }
        else if (presetName == "Sidechain Half")
        {
            double beat = 2.0 * (60.0 / audioEngine.getBpm());
            for (double t = startTime; t < endTime; t += beat)
            {
                env.addOrUpdatePoint (t, 0.0f);
                env.addOrUpdatePoint (t + beat * 0.1, 0.0f);
                env.addOrUpdatePoint (t + beat * 0.8, 1.0f);
                env.addOrUpdatePoint (t + beat - 0.001, 1.0f);
            }
        }
        
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.originalComponent != this && e.originalComponent != &timelineContainer) return;
        if (e.x < headerWidth) return; // Clicked in headers area

        double pos = (e.position.x - headerWidth + scrollX) / pixelsPerSecond;
        pos = juce::jmax (0.0, pos);

        if (e.y < 30) // Clicked in ruler
        {
            if (e.mods.isShiftDown())
            {
                audioEngine.setPlayPosition (pos);
                repaint();
                return;
            }
            
            loopDragStart = pos;
            audioEngine.setLoopRange (pos, pos);
            isDraggingLoop = true;
            audioEngine.setPlayPosition (pos);
            repaint();
            return;
        }

        if (e.y >= 30) // Clicked in tracks area
        {
            int targetY = e.y - 30 + (int)scrollY;
            int currentY = 0;
            int trackIdx = -1;
            ActiveAutomationLane clickedLane;
            bool clickedInLane = false;
            float clickedLaneStartY = 0.0f;

            for (int i = 0; i < audioEngine.getNumTracks(); ++i)
            {
                auto* track = audioEngine.getTrack(i);
                if (track == nullptr) continue;
                int trackH = track->getHeight();
                int nextY = currentY + trackH + 10;
                
                if (targetY >= currentY && targetY < currentY + trackH)
                {
                    trackIdx = i;
                    clickedInLane = false;
                    break;
                }
                
                auto lanes = track->getActiveAutomationLanes();
                for (const auto& lane : lanes)
                {
                    int startY = nextY;
                    int endY = startY + 50;
                    if (targetY >= startY && targetY < endY)
                    {
                        trackIdx = i;
                        clickedInLane = true;
                        clickedLane = lane;
                        clickedLaneStartY = (float)startY;
                        break;
                    }
                    nextY += 50 + 10;
                }
                
                if (clickedInLane)
                    break;
                
                currentY = nextY;
            }

            if (trackIdx >= 0 && trackIdx < audioEngine.getNumTracks())
            {
                selectedTrackIdx = trackIdx;
                selectedItem = nullptr;
                repaint();

                auto* track = audioEngine.getTrack (trackIdx);
                if (clickedInLane && clickedLane.envelope != nullptr)
                {
                    float innerY = 30.0f + clickedLaneStartY - (float)scrollY;
                    float innerH = 50.0f;
                    
                    auto& env = *clickedLane.envelope;
                    const juce::ScopedLock sl (env.lock);
                    
                    // 1. Check if clicked on a point
                    int clickedPtIdx = -1;
                    for (size_t p = 0; p < env.points.size(); ++p)
                    {
                        float ptX = (float)headerWidth + (float)(env.points[p].time * pixelsPerSecond) - (float)scrollX;
                        float ptY = innerY + innerH * (1.0f - env.points[p].value);
                        
                        if (juce::Point<float>(ptX, ptY).getDistanceFrom (e.position) < 8.0f)
                        {
                            clickedPtIdx = (int)p;
                            break;
                        }
                    }
                    
                    if (clickedPtIdx != -1)
                    {
                        if (e.mods.isRightButtonDown() || e.getNumberOfClicks() > 1)
                        {
                            saveUndoState();
                            env.removePoint (clickedPtIdx);
                            selectedAutomationTrackIdx = -1;
                            selectedAutomationPointIdx = -1;
                            selectedAutomationLaneType = 0;
                            selectedAutomationSlotIndex = -1;
                            selectedAutomationParamIndex = -1;
                            repaint();
                            return;
                        }
                        
                        selectedAutomationTrackIdx = trackIdx;
                        selectedAutomationPointIdx = clickedPtIdx;
                        selectedAutomationLaneType = clickedLane.laneType;
                        selectedAutomationSlotIndex = clickedLane.slotIndex;
                        selectedAutomationParamIndex = clickedLane.parameterIndex;
                        return;
                    }
                    
                    // 2. Check if clicked near a curve segment handle
                    int clickedCurveSegmentIdx = -1;
                    if (env.points.size() >= 2)
                    {
                        for (size_t i = 0; i < env.points.size() - 1; ++i)
                        {
                            const auto& ptA = env.points[i];
                            const auto& ptB = env.points[i+1];
                            double tMid = (ptA.time + ptB.time) / 2.0;
                            float valMid = env.getValueAt (tMid, ptA.value);
                            
                            float ptX = (float)headerWidth + (float)(tMid * pixelsPerSecond) - (float)scrollX;
                            float ptY = innerY + innerH * (1.0f - valMid);
                            
                            if (juce::Point<float>(ptX, ptY).getDistanceFrom (e.position) < 10.0f)
                            {
                                clickedCurveSegmentIdx = (int)i;
                                break;
                            }
                        }
                    }
                    
                    if (clickedCurveSegmentIdx != -1)
                    {
                        saveUndoState();
                        isDraggingCurve = true;
                        selectedCurvePointIdx = clickedCurveSegmentIdx;
                        selectedAutomationTrackIdx = trackIdx;
                        selectedAutomationLaneType = clickedLane.laneType;
                        selectedAutomationSlotIndex = clickedLane.slotIndex;
                        selectedAutomationParamIndex = clickedLane.parameterIndex;
                        return;
                    }

                    // 3. Right-clicked on empty lane: show context menu for LFO & Presets
                    if (e.mods.isRightButtonDown())
                    {
                        double clickedTime = (e.position.x - headerWidth + scrollX) / pixelsPerSecond;
                        clickedTime = juce::jmax (0.0, clickedTime);
                        
                        double snapInterval = audioEngine.getSnapIntervalSeconds();
                        if (snapInterval > 0.0)
                            clickedTime = std::round (clickedTime / snapInterval) * snapInterval;
                            
                        juce::PopupMenu rightClickMenu;
                        rightClickMenu.addItem (1, "Add Automation Point");
                        
                        juce::PopupMenu lfoMenu;
                        lfoMenu.addItem (10, "Sine (Quarter Note)");
                        lfoMenu.addItem (11, "Sine (Eighth Note)");
                        lfoMenu.addItem (12, "Triangle (Quarter Note)");
                        lfoMenu.addItem (13, "Saw (Quarter Note)");
                        lfoMenu.addItem (14, "Square (Quarter Note)");
                        lfoMenu.addItem (15, "Random (Quarter Note)");
                        rightClickMenu.addSubMenu ("Generate LFO Pattern", lfoMenu);
                        
                        juce::PopupMenu presetMenu;
                        presetMenu.addItem (20, "Fade In");
                        presetMenu.addItem (21, "Fade Out");
                        presetMenu.addItem (22, "Exponential Rise");
                        presetMenu.addItem (23, "Sidechain Ducking (1/4)");
                        presetMenu.addItem (24, "Sidechain Ducking (1/2)");
                        rightClickMenu.addSubMenu ("Apply Preset Shape", presetMenu);
                        
                        rightClickMenu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                            [this, clickedLane, clickedTime] (int result) {
                                if (result <= 0) return;
                                
                                auto* t = audioEngine.getTrack (selectedTrackIdx);
                                if (t == nullptr || clickedLane.envelope == nullptr) return;
                                auto& env = *clickedLane.envelope;
                                const juce::ScopedLock sl (env.lock);
                                
                                double startRange = clickedTime;
                                double endRange = clickedTime + 4.0; // 4 seconds default
                                if (audioEngine.getLoopEnd() > audioEngine.getLoopStart())
                                {
                                    startRange = audioEngine.getLoopStart();
                                    endRange = audioEngine.getLoopEnd();
                                }
                                
                                double bpm = audioEngine.getBpm();
                                double quarterSec = 60.0 / bpm;
                                
                                if (result == 1)
                                {
                                    saveUndoState();
                                    env.addOrUpdatePoint (clickedTime, 0.5f);
                                    repaint();
                                }
                                else if (result == 10) generateLFO (env, startRange, endRange, "Sine", quarterSec);
                                else if (result == 11) generateLFO (env, startRange, endRange, "Sine", quarterSec / 2.0);
                                else if (result == 12) generateLFO (env, startRange, endRange, "Triangle", quarterSec);
                                else if (result == 13) generateLFO (env, startRange, endRange, "Saw", quarterSec);
                                else if (result == 14) generateLFO (env, startRange, endRange, "Square", quarterSec);
                                else if (result == 15) generateLFO (env, startRange, endRange, "Random", quarterSec);
                                else if (result == 20) applyPreset (env, startRange, endRange - startRange, "Fade In");
                                else if (result == 21) applyPreset (env, startRange, endRange - startRange, "Fade Out");
                                else if (result == 22) applyPreset (env, startRange, endRange - startRange, "Exponential Rise");
                                else if (result == 23) applyPreset (env, startRange, endRange - startRange, "Sidechain Quarter");
                                else if (result == 24) applyPreset (env, startRange, endRange - startRange, "Sidechain Half");
                            });
                        return;
                    }
                    
                    // 4. Double-clicked or regular clicked to add a point
                    double ptTime = (e.position.x - headerWidth + scrollX) / pixelsPerSecond;
                    if (ptTime < 0.0) ptTime = 0.0;
                    
                    double snapInterval = audioEngine.getSnapIntervalSeconds();
                    if (snapInterval > 0.0)
                        ptTime = std::round (ptTime / snapInterval) * snapInterval;
                        
                    float ptVal = 1.0f - (e.position.y - innerY) / innerH;
                    ptVal = juce::jlimit (0.0f, 1.0f, ptVal);
                    
                    saveUndoState();
                    env.addOrUpdatePoint (ptTime, ptVal);
                    
                    // Find point index
                    for (size_t p = 0; p < env.points.size(); ++p)
                    {
                        if (std::abs (env.points[p].time - ptTime) < 0.01)
                        {
                            selectedAutomationTrackIdx = trackIdx;
                            selectedAutomationPointIdx = (int)p;
                            selectedAutomationLaneType = clickedLane.laneType;
                            selectedAutomationSlotIndex = clickedLane.slotIndex;
                            selectedAutomationParamIndex = clickedLane.parameterIndex;
                            break;
                        }
                    }
                    repaint();
                    return;
                }
            }
        }

        setPlayheadFromMouse (e);
    }
    
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.originalComponent != this && e.originalComponent != &timelineContainer) return;
        if (draggedTrackIdx != -1) return;

        if (isDraggingCurve && selectedAutomationTrackIdx != -1 && selectedCurvePointIdx != -1)
        {
            auto* track = audioEngine.getTrack (selectedAutomationTrackIdx);
            if (track != nullptr)
            {
                AutomationEnvelope* env = nullptr;
                if (selectedAutomationLaneType == 1)
                    env = &track->getVolumeAutomation();
                else if (selectedAutomationLaneType == 2)
                    env = &track->getPanAutomation();
                else if (selectedAutomationLaneType == 3)
                {
                    auto* pa = track->getOrCreatePluginAutomation (selectedAutomationSlotIndex, selectedAutomationParamIndex);
                    if (pa != nullptr)
                        env = &pa->envelope;
                }
                
                if (env != nullptr)
                {
                    const juce::ScopedLock sl (env->lock);
                    if (selectedCurvePointIdx < (int)env->points.size() - 1)
                    {
                        int currentY = 0;
                        int laneStartY = 0;
                        bool foundLane = false;
                        for (int i = 0; i <= selectedAutomationTrackIdx; ++i)
                        {
                            auto* t = audioEngine.getTrack(i);
                            if (t != nullptr)
                            {
                                int trackH = t->getHeight();
                                int nextY = currentY + trackH + 10;
                                if (i == selectedAutomationTrackIdx)
                                {
                                    auto lanes = t->getActiveAutomationLanes();
                                    for (const auto& lane : lanes)
                                    {
                                        if (lane.laneType == selectedAutomationLaneType &&
                                            lane.slotIndex == selectedAutomationSlotIndex &&
                                            lane.parameterIndex == selectedAutomationParamIndex)
                                        {
                                            laneStartY = nextY;
                                            foundLane = true;
                                            break;
                                        }
                                        nextY += 50 + 10;
                                    }
                                    break;
                                }
                                nextY += t->getTotalHeight() - t->getHeight();
                                currentY = nextY;
                            }
                        }
                        
                        if (foundLane)
                        {
                            float innerY = 30.0f + (float)laneStartY - (float)scrollY;
                            float innerH = 50.0f;
                            
                            float newVal = 1.0f - (e.position.y - innerY) / innerH;
                            newVal = juce::jlimit (0.0f, 1.0f, newVal);
                            
                            auto& ptA = env->points[selectedCurvePointIdx];
                            auto& ptB = env->points[selectedCurvePointIdx + 1];
                            
                            float linVal = (ptA.value + ptB.value) / 2.0f;
                            float maxDiff = std::abs (ptB.value - ptA.value) / 2.0f;
                            if (maxDiff > 0.01f)
                            {
                                float diff = newVal - linVal;
                                if (ptB.value < ptA.value)
                                    diff = -diff;
                                    
                                float c = diff / maxDiff;
                                ptA.curve = juce::jlimit (-1.0f, 1.0f, c);
                            }
                            repaint();
                            return;
                        }
                    }
                }
            }
        }

        if (selectedAutomationTrackIdx != -1 && selectedAutomationPointIdx != -1 && selectedAutomationLaneType != 0)
        {
            auto* track = audioEngine.getTrack (selectedAutomationTrackIdx);
            if (track != nullptr)
            {
                int currentY = 0;
                int laneStartY = 0;
                bool foundLane = false;
                
                for (int i = 0; i <= selectedAutomationTrackIdx; ++i)
                {
                    auto* t = audioEngine.getTrack(i);
                    if (t != nullptr)
                    {
                        int trackH = t->getHeight();
                        int nextY = currentY + trackH + 10;
                        
                        if (i == selectedAutomationTrackIdx)
                        {
                            auto lanes = t->getActiveAutomationLanes();
                            for (const auto& lane : lanes)
                            {
                                if (lane.laneType == selectedAutomationLaneType &&
                                    lane.slotIndex == selectedAutomationSlotIndex &&
                                    lane.parameterIndex == selectedAutomationParamIndex)
                                {
                                    laneStartY = nextY;
                                    foundLane = true;
                                    break;
                                }
                                nextY += 50 + 10;
                            }
                            break;
                        }
                        
                        nextY += t->getTotalHeight() - t->getHeight();
                        currentY = nextY;
                    }
                }
                    
                if (foundLane)
                {
                    float innerY = 30.0f + (float)laneStartY - (float)scrollY;
                    float innerH = 50.0f;
                    
                    double newTime = (e.position.x - headerWidth + scrollX) / pixelsPerSecond;
                    if (newTime < 0.0) newTime = 0.0;
                    
                    double snapInterval = audioEngine.getSnapIntervalSeconds();
                    if (snapInterval > 0.0)
                        newTime = std::round (newTime / snapInterval) * snapInterval;
                        
                    float newVal = 1.0f - (e.position.y - innerY) / innerH;
                    newVal = juce::jlimit (0.0f, 1.0f, newVal);
                    
                    AutomationEnvelope* env = nullptr;
                    if (selectedAutomationLaneType == 1)
                        env = &track->getVolumeAutomation();
                    else if (selectedAutomationLaneType == 2)
                        env = &track->getPanAutomation();
                    else if (selectedAutomationLaneType == 3)
                    {
                        auto* pa = track->getOrCreatePluginAutomation (selectedAutomationSlotIndex, selectedAutomationParamIndex);
                        if (pa != nullptr)
                            env = &pa->envelope;
                    }
                        
                    if (env != nullptr)
                    {
                        const juce::ScopedLock sl (env->lock);
                        if (selectedAutomationPointIdx >= 0 && selectedAutomationPointIdx < (int)env->points.size())
                        {
                            env->points[selectedAutomationPointIdx].time = newTime;
                            env->points[selectedAutomationPointIdx].value = newVal;
                            
                            // Re-sort
                            std::sort (env->points.begin(), env->points.end(), [](const AutomationPoint& a, const AutomationPoint& b) {
                                return a.time < b.time;
                            });
                            
                            // Preserving the selected index
                            for (size_t p = 0; p < env->points.size(); ++p)
                            {
                                if (env->points[p].time == newTime && env->points[p].value == newVal)
                                {
                                    selectedAutomationPointIdx = (int)p;
                                    break;
                                }
                            }
                        }
                        repaint();
                        return;
                    }
                }
            }
        }

        if (e.x < headerWidth) return;

        double pos = (e.position.x - headerWidth + scrollX) / pixelsPerSecond;
        pos = juce::jmax (0.0, pos);

        if (isDraggingLoop)
        {
            double start = juce::jmin (loopDragStart, pos);
            double end = juce::jmax (loopDragStart, pos);
            audioEngine.setLoopRange (start, end);
            repaint();
            return;
        }

        setPlayheadFromMouse (e);
    }
    
    void mouseUp (const juce::MouseEvent& e) override;
    
    void mouseMove (const juce::MouseEvent& e) override
    {
        if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        {
            // Convert e's position from child to this component
            auto localPos = getLocalPoint (e.eventComponent, e.position);
            juce::MouseEvent localEvent = e.getEventRelativeTo (this);
            setPlayheadFromMouse (localEvent);
        }
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (e.originalComponent != this && e.originalComponent != &timelineContainer) return;
        if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        {
            float delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
            if (delta == 0.0f) return;
            
            double zoomFactor = (delta > 0) ? 1.1 : (1.0 / 1.1);
            
            double mouseTime = (e.position.x - headerWidth + scrollX) / pixelsPerSecond;
            
            pixelsPerSecond *= zoomFactor;
            pixelsPerSecond = juce::jlimit (5.0, 5000.0, pixelsPerSecond);
            
            scrollX = mouseTime * pixelsPerSecond - (e.position.x - headerWidth);
            scrollX = juce::jmax (0.0, scrollX);
            
            updateItems();
            repaint();
            return;
        }

        if (wheel.deltaX != 0.0f || e.mods.isShiftDown())
        {
            float delta = (wheel.deltaX != 0.0f) ? wheel.deltaX : wheel.deltaY;
            scrollX -= delta * 500.0f;
            scrollX = juce::jmax (0.0, scrollX);
        }
        else if (wheel.deltaY != 0.0f)
        {
            scrollY -= wheel.deltaY * 500.0f;
            scrollY = juce::jmax (0.0, scrollY);
        }
        updateItems();
        repaint();
    }

    void setPlayheadFromMouse (const juce::MouseEvent& e)
    {
        if (e.position.x < headerWidth) return;
        double pos = (e.position.x - headerWidth + scrollX) / pixelsPerSecond;
        pos = juce::jmax (0.0, pos);
        audioEngine.setPlayPosition (pos);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff121214));

        // --- DRAW RULER (Top 30 pixels) ---
        g.setColour (juce::Colour (0xff16161a));
        g.fillRect (headerWidth, 0, getWidth() - headerWidth, 30);
        
        // Corner header box (top left headerWidth x 30)
        g.setColour (juce::Colour (0xff1c1c20));
        g.fillRect (0, 0, headerWidth, 30);
        g.setColour (juce::Colour (0xffe2e2e6));
        g.setFont (juce::Font(12.0f, juce::Font::bold));
        g.drawText ("TRACKS", 0, 0, headerWidth, 30, juce::Justification::centred);
        
        g.setColour (juce::Colour (0xff8e8e93));
        g.setFont (12.0f);
        
        int numSeconds = (getWidth() - headerWidth + scrollX) / (int)pixelsPerSecond;
        int startSec = scrollX / pixelsPerSecond;
        for (int i = startSec; i <= numSeconds + 1; ++i)
        {
            int x = headerWidth + (int)(i * pixelsPerSecond) - (int)scrollX;
            if (x < headerWidth) continue; // Clip to ruler area
            
            g.drawVerticalLine (x, 20.0f, 30.0f);
            
            int halfX = headerWidth + (int)((i + 0.5) * pixelsPerSecond) - (int)scrollX;
            if (halfX >= headerWidth)
                g.drawVerticalLine (halfX, 25.0f, 30.0f);
            
            int mins = i / 60;
            int secs = i % 60;
            juce::String timeStr = juce::String::formatted ("%02d:%02d", mins, secs);
            g.drawText (timeStr, x - 20, 0, 40, 20, juce::Justification::centred, false);
        }

        // Playhead Marker in ruler
        double playPos = audioEngine.getPlayPosition();
        int playheadX = headerWidth + (int)(playPos * pixelsPerSecond) - (int)scrollX;

        // --- DRAW LOOP REGION ---
        if (audioEngine.getLoopEnd() > audioEngine.getLoopStart())
        {
            int loopStartX = headerWidth + (int)(audioEngine.getLoopStart() * pixelsPerSecond) - (int)scrollX;
            int loopEndX = headerWidth + (int)(audioEngine.getLoopEnd() * pixelsPerSecond) - (int)scrollX;
            
            loopStartX = juce::jmax(headerWidth, loopStartX);
            loopEndX = juce::jmax(headerWidth, loopEndX);
            
            if (loopEndX > loopStartX)
            {
                // Loop bar in ruler
                g.setColour(juce::Colour(0xff00b4d8));
                g.fillRect(loopStartX, 0, loopEndX - loopStartX, 3);
                g.setColour(juce::Colour(0xff00b4d8).withAlpha(0.12f));
                g.fillRect(loopStartX, 3, loopEndX - loopStartX, 27);
                
                g.setColour(juce::Colour (0xffe2e2e6).withAlpha(0.5f));
                g.drawVerticalLine(loopStartX, 0.0f, 30.0f);
                g.drawVerticalLine(loopEndX, 0.0f, 30.0f);
            }
        }

        if (playheadX >= headerWidth)
        {
            juce::Path marker;
            marker.addTriangle ((float)playheadX - 6.0f, 0.0f, (float)playheadX + 6.0f, 0.0f, (float)playheadX, 12.0f);
            g.setColour (juce::Colour (0xffea4335)); // Modern red playhead marker
            g.fillPath (marker);
        }

        // --- CLIP TO X >= headerWidth, Y >= 30 FOR TRACK LANES ---
        g.reduceClipRegion (headerWidth, 30, getWidth() - headerWidth, getHeight() - 30);

        // --- DRAW TRACK & AUTOMATION BACKGROUNDS ---
        auto arrBounds = getLocalBounds();
        int y = 30 - (int)scrollY; 
        
        for (int i = 0; i < audioEngine.getNumTracks(); ++i)
        {
            auto* track = audioEngine.getTrack(i);
            if (track == nullptr) continue;
            float trackH = (float)track->getHeight();
            juce::Rectangle<float> trackBounds ((float)headerWidth + 4.0f, (float)y, arrBounds.getWidth() - (float)headerWidth - 8.0f, trackH);
            
            if (i == selectedTrackIdx)
                g.setColour (juce::Colour (0xff232328));
            else
                g.setColour (juce::Colour (0xff18181c));
            
            g.fillRoundedRectangle (trackBounds, 4.0f);
            g.setColour (juce::Colour (0xff2a2a30));
            g.drawRoundedRectangle (trackBounds, 4.0f, 1.0f);

            y += trackH + 10;

            for (const auto& lane : track->getActiveAutomationLanes())
            {
                // Draw automation sub-lane background
                juce::Rectangle<float> autoBounds ((float)headerWidth + 12.0f, (float)y, arrBounds.getWidth() - (float)headerWidth - 24.0f, 50.0f);
                g.setColour (juce::Colour (0xff101012));
                g.fillRoundedRectangle (autoBounds, 4.0f);
                g.setColour (juce::Colour (0xff1c1c20));
                g.drawRoundedRectangle (autoBounds, 4.0f, 1.0f);

                // Draw a small text label on the left side of the lane
                g.setColour (juce::Colour (0xff8e8e93));
                g.setFont (10.0f);
                g.drawText (lane.name, autoBounds.getX() + 8.0f, autoBounds.getY() + 4.0f, 300.0f, 15.0f, juce::Justification::left);

                y += 50 + 10;
            }
        }

        // --- DRAW AUTOMATION CURVES ---
        auto drawCurve = [&g, this, minX = (float)headerWidth, maxX = (float)getWidth()]
            (const AutomationEnvelope& envelope, float innerY, float innerH, juce::Colour color, float defaultVal)
        {
            const juce::ScopedLock sl (envelope.lock);
            juce::Path curvePath;
            if (envelope.points.empty())
            {
                float yVal = innerY + innerH * (1.0f - defaultVal);
                g.setColour (color.withAlpha (0.4f));
                g.drawHorizontalLine ((int)yVal, minX, maxX);
            }
            else
            {
                // Start of timeline (time = 0)
                float firstVal = envelope.points.front().value;
                float firstPtX = (float)headerWidth - (float)scrollX;
                float firstPtY = innerY + innerH * (1.0f - firstVal);
                curvePath.startNewSubPath (juce::jmax(minX, firstPtX), firstPtY);
                
                for (size_t i = 0; i < envelope.points.size() - 1; ++i)
                {
                    const auto& ptA = envelope.points[i];
                    const auto& ptB = envelope.points[i+1];
                    
                    float xA = (float)headerWidth + (float)(ptA.time * pixelsPerSecond) - (float)scrollX;
                    float xB = (float)headerWidth + (float)(ptB.time * pixelsPerSecond) - (float)scrollX;
                    
                    // Sample every 4 pixels
                    int numSamples = juce::jmax (1, (int)((xB - xA) / 4.0f));
                    for (int s = 1; s <= numSamples; ++s)
                    {
                        float t = (float)s / (float)numSamples;
                        double timeVal = ptA.time + t * (ptB.time - ptA.time);
                        float val = envelope.getValueAt (timeVal, ptA.value);
                        
                        float ptX = xA + t * (xB - xA);
                        float ptY = innerY + innerH * (1.0f - val);
                        curvePath.lineTo (ptX, ptY);
                    }
                }
                
                // End of timeline (extend last value)
                float lastVal = envelope.points.back().value;
                float lastPtY = innerY + innerH * (1.0f - lastVal);
                curvePath.lineTo (maxX, lastPtY);
                
                // Draw the curve line
                g.setColour (color.withAlpha (0.8f));
                g.strokePath (curvePath, juce::PathStrokeType (2.0f));
                
                // Draw curve segment handles
                for (size_t i = 0; i < envelope.points.size() - 1; ++i)
                {
                    const auto& ptA = envelope.points[i];
                    const auto& ptB = envelope.points[i+1];
                    double tMid = (ptA.time + ptB.time) / 2.0;
                    float valMid = envelope.getValueAt (tMid, ptA.value);
                    
                    float ptX = (float)headerWidth + (float)(tMid * pixelsPerSecond) - (float)scrollX;
                    float ptY = innerY + innerH * (1.0f - valMid);
                    
                    if (ptX >= minX && ptX <= maxX)
                    {
                        g.setColour (color.withAlpha (0.4f));
                        g.fillEllipse (ptX - 3.0f, ptY - 3.0f, 6.0f, 6.0f);
                        g.setColour (juce::Colours::white.withAlpha (0.6f));
                        g.drawEllipse (ptX - 3.0f, ptY - 3.0f, 6.0f, 6.0f, 1.0f);
                    }
                }
                
                // Draw filled circles at points
                for (const auto& pt : envelope.points)
                {
                    float ptX = (float)headerWidth + (float)(pt.time * pixelsPerSecond) - (float)scrollX;
                    float ptY = innerY + innerH * (1.0f - pt.value);
                    
                    if (ptX >= minX && ptX <= maxX)
                    {
                        g.setColour (juce::Colours::white);
                        g.fillEllipse (ptX - 4.0f, ptY - 4.0f, 8.0f, 8.0f);
                        g.setColour (color);
                        g.drawEllipse (ptX - 4.0f, ptY - 4.0f, 8.0f, 8.0f, 1.5f);
                    }
                }
            }
        };

        int autoY = 30 - (int)scrollY;
        for (int i = 0; i < audioEngine.getNumTracks(); ++i)
        {
            auto* track = audioEngine.getTrack(i);
            if (track == nullptr) continue;
            float trackH = (float)track->getHeight();
            
            autoY += trackH + 10;

            for (const auto& lane : track->getActiveAutomationLanes())
            {
                drawCurve (*lane.envelope, (float)autoY, 50.0f, lane.color, lane.defaultValue);
                autoY += 50 + 10;
            }
        }

        // --- DRAW VERTICAL GRID LINES ---
        g.setColour (juce::Colour (0x1fffffff)); // 0.08f alpha white
        for (int i = startSec; i <= numSeconds + 1; ++i)
        {
            int x = headerWidth + (int)(i * pixelsPerSecond) - (int)scrollX;
            if (x >= headerWidth)
            {
                g.drawVerticalLine (x, 30.0f, (float)getHeight());
            }
            
            int halfX = headerWidth + (int)((i + 0.5) * pixelsPerSecond) - (int)scrollX;
            if (halfX >= headerWidth)
            {
                g.setColour (juce::Colour (0x0cffffff)); // 0.03f alpha white
                g.drawVerticalLine (halfX, 30.0f, (float)getHeight());
                g.setColour (juce::Colour (0x1fffffff));
            }
        }

        // --- DRAW LOOP REGION OVERLAY ---
        if (audioEngine.getLoopEnd() > audioEngine.getLoopStart())
        {
            int loopStartX = headerWidth + (int)(audioEngine.getLoopStart() * pixelsPerSecond) - (int)scrollX;
            int loopEndX = headerWidth + (int)(audioEngine.getLoopEnd() * pixelsPerSecond) - (int)scrollX;
            loopStartX = juce::jmax(headerWidth, loopStartX);
            loopEndX = juce::jmax(headerWidth, loopEndX);
            if (loopEndX > loopStartX)
            {
                g.setColour(juce::Colour(0xff00b4d8).withAlpha(0.08f));
                g.fillRect(loopStartX, 30, loopEndX - loopStartX, getHeight() - 30);
                g.setColour(juce::Colour (0xffe2e2e6).withAlpha(0.3f));
                g.drawVerticalLine(loopStartX, 30.0f, (float)getHeight());
                g.drawVerticalLine(loopEndX, 30.0f, (float)getHeight());
            }
        }

        if (playheadX >= headerWidth)
        {
            g.setColour (juce::Colour (0xffea4335)); // Modern red playhead line
            g.drawLine ((float)playheadX, 30.0f, (float)playheadX, (float)getHeight(), 1.5f);
        }
    }
    
    void updateTrackHeaders()
    {
        if (headerComps.size() != audioEngine.getNumTracks())
        {
            headerComps.clear();
            for (int i = 0; i < audioEngine.getNumTracks(); ++i)
            {
                auto* track = audioEngine.getTrack(i);
                auto* comp = new TrackHeaderComponent (track, audioEngine, [this] {
                    repaint();
                    if (onTrackStatusChanged) onTrackStatusChanged();
                });
                comp->onHeightChanged = [this] {
                    updateItems();
                    repaint();
                };
                headersContainer.addAndMakeVisible (comp);
                headerComps.add (comp);
            }
        }

        int y = -(int)scrollY;
        for (int i = 0; i < headerComps.size(); ++i)
        {
            auto* track = audioEngine.getTrack(i);
            headerComps[i]->setBounds (0, y, headerWidth, track->getHeight());
            headerComps[i]->updateStates();
            y += track->getTotalHeight() + 10;
        }
    }

    void updateItems()
    {
        updateTrackHeaders();

        itemComps.clear();
        int y = -(int)scrollY; // Relative to timelineContainer (which starts at y=30)
        for (int i = 0; i < audioEngine.getNumTracks(); ++i)
        {
            auto* track = audioEngine.getTrack(i);
            int trackH = track->getHeight();
            for (auto& item : track->getItems())
            {
                if (item)
                {
                    auto* comp = new ItemComponent (item.get(), pixelsPerSecond, i, this);
                    timelineContainer.addAndMakeVisible(comp);
                    int x = (int)(item->getStartTime() * pixelsPerSecond) - (int)scrollX;
                    int w = (int)(item->getLength() * pixelsPerSecond);
                    comp->setBounds (x, y, juce::jmax(1, w), trackH);
                    itemComps.add (comp);
                }
            }
            y += track->getTotalHeight() + 10;
        }
    }

    void handleItemDragEnd (ItemComponent* comp)
    {
        int relativeY = comp->getBounds().getCentreY();
        int targetY = relativeY + (int)scrollY;
        
        int targetTrackIdx = -1;
        int currentY = 0;
        for (int i = 0; i < audioEngine.getNumTracks(); ++i)
        {
            auto* t = audioEngine.getTrack(i);
            int nextY = currentY + t->getTotalHeight() + 10;
                
            if (targetY >= currentY && targetY < nextY)
            {
                targetTrackIdx = i;
                break;
            }
            currentY = nextY;
        }
        if (targetTrackIdx == -1)
            targetTrackIdx = audioEngine.getNumTracks() - 1;
        targetTrackIdx = juce::jlimit (0, audioEngine.getNumTracks() - 1, targetTrackIdx);

        if (targetTrackIdx != comp->getTrackIdx())
        {
            // Move item to new track
            auto* oldTrack = audioEngine.getTrack(comp->getTrackIdx());
            auto* newTrack = audioEngine.getTrack(targetTrackIdx);
            
            auto extractedItem = oldTrack->extractItem (comp->getItem());
            if (extractedItem)
            {
                newTrack->addItem (std::move (extractedItem));
            }
        }
        updateItems();
    }

    void splitItem (ItemComponent* comp, float xPosInComp)
    {
        Item* item = comp->getItem();
        if (!item) return;

        double splitTimeSec = xPosInComp / pixelsPerSecond;
        if (splitTimeSec <= 0 || splitTimeSec >= item->getLength()) return;

        // Create new item
        auto newItem = std::make_unique<Item>();
        newItem->copyAudioDataFrom (*item);
        newItem->setStartTime (item->getStartTime() + splitTimeSec);
        newItem->setSourceOffset (item->getSourceOffset() + splitTimeSec);
        newItem->setLength (item->getLength() - splitTimeSec);

        // Adjust old item
        item->setLength (splitTimeSec);

        // Add to track
        auto* track = audioEngine.getTrack (comp->getTrackIdx());
        track->addItem (std::move (newItem));
        
        updateItems();
    }

    void spliceAtPlayhead()
    {
        double playPos = audioEngine.getPlayPosition();
        
        for (int i = 0; i < audioEngine.getNumTracks(); ++i)
        {
            auto* track = audioEngine.getTrack(i);
            std::vector<std::unique_ptr<Item>> newItems;
            
            for (auto& item : track->getItems())
            {
                if (item && playPos > item->getStartTime() && playPos < item->getStartTime() + item->getLength())
                {
                    double splitOffsetSec = playPos - item->getStartTime();
                    
                    auto newItem = std::make_unique<Item>();
                    newItem->copyAudioDataFrom (*item);
                    newItem->setStartTime (item->getStartTime() + splitOffsetSec);
                    newItem->setSourceOffset (item->getSourceOffset() + splitOffsetSec);
                    newItem->setLength (item->getLength() - splitOffsetSec);
                    
                    item->setLength (splitOffsetSec);
                    newItems.push_back (std::move (newItem));
                }
            }
            
            for (auto& newItem : newItems)
            {
                track->addItem (std::move (newItem));
            }
        }
        
        updateItems();
        repaint();
    }
    
    void updateHeaderStates()
    {
        for (auto* hc : headerComps)
            if (hc != nullptr)
                hc->updateStates();
    }

private:
    EditTool currentTool = EditTool::Select;
    AudioEngine& audioEngine;
    juce::Component headersContainer;
    juce::Component timelineContainer;
    double scrollX = 0.0;
    double scrollY = 0.0;
    juce::OwnedArray<TrackHeaderComponent> headerComps;
    juce::OwnedArray<ItemComponent> itemComps;
    double pixelsPerSecond = 50.0; // 50 pixels = 1 second

    bool isDraggingLoop = false;
    double loopDragStart = 0.0;
    int selectedAutomationTrackIdx = -1;
    int selectedAutomationPointIdx = -1;
    int selectedAutomationLaneType = 0; // 0 = none, 1 = volume, 2 = pan, 3 = plugin
    int selectedAutomationSlotIndex = -1;
    int selectedAutomationParamIndex = -1;
    bool isDraggingCurve = false;
    int selectedCurvePointIdx = -1;
};

class PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow (juce::AudioProcessor* p)
        : DocumentWindow (p->getName(), juce::Colour (0xff1a1a1e), juce::DocumentWindow::allButtons),
          processor (p)
    {
        setUsingNativeTitleBar (true);
        
        juce::AudioProcessorEditor* editor = processor->createEditorIfNeeded();
        if (editor == nullptr || editor->getWidth() == 0 || editor->getHeight() == 0)
        {
            if (editor) delete editor;
            editor = new juce::GenericAudioProcessorEditor (*processor);
            editor->setSize (600, 400);
        }

        if (editor != nullptr)
        {
            setContentOwned (editor, true);
            setOpaque (true);
            setResizable (editor->isResizable(), false);
            setTopLeftPosition (100, 100);
            setAlwaysOnTop (true);
            setVisible (true);
            toFront (true);
        }
        else
        {
            setVisible (false);
            juce::MessageManager::getInstance()->callAsync([this]() { delete this; });
        }
    }
    
    ~PluginWindow() override
    {
        if (auto* editor = processor->getActiveEditor())
        {
            processor->editorBeingDeleted (editor);
            setContentOwned (nullptr, false);
        }
    }

    void closeButtonPressed() override
    {
        delete this;
    }

private:
    juce::AudioProcessor* processor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};

class MainComponent;

class MixerWindow : public juce::DocumentWindow
{
public:
    MixerWindow (const juce::String& name, juce::Component* content, MainComponent* owner)
        : DocumentWindow (name, juce::Colour (0xff121214), DocumentWindow::allButtons), mainComp(owner)
    {
        setContentNonOwned (content, false);
        setUsingNativeTitleBar (false);
        setResizable (true, false);
        
        if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            auto rect = display->userArea;
            setBounds (rect.getX(), rect.getBottom() - 525, rect.getWidth(), 525);
        }
        else
        {
            setBounds (0, 525, 1000, 525);
        }
        
        setVisible (true);
    }

    void closeButtonPressed() override;
    
    void resized() override;
    
private:
    MainComponent* mainComp;
};

class TimecodeDisplay : public juce::Component, public juce::Timer
{
public:
    TimecodeDisplay (AudioEngine& eng) : audioEngine (eng)
    {
        startTimer (30);
    }
    
    ~TimecodeDisplay() override
    {
        stopTimer();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // LCD glass panel background (sleek modern dark grey/black look)
        g.setColour (juce::Colour (0xff141416));
        g.fillRoundedRectangle (bounds, 4.0f);
        
        // Inner borders for glassmorphism / depth
        g.setColour (juce::Colour (0xff2c2c34));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
        
        double playPos = audioEngine.getPlayPosition();
        double bpm = audioEngine.getBpm();
        
        // Compute Bars.Beats.Ticks
        double beatLengthSecs = 60.0 / bpm;
        double totalBeats = playPos / beatLengthSecs;
        int bar = (int)(totalBeats / 4.0) + 1;
        int beat = (int)(std::fmod (totalBeats, 4.0)) + 1;
        int tick = (int)(std::fmod (totalBeats, 1.0) * 960.0);
        
        // Compute Time: Minutes.Seconds.Milliseconds
        int totalMs = (int)(playPos * 1000.0);
        int mins = (totalMs / 60000) % 60;
        int secs = (totalMs / 1000) % 60;
        int ms = totalMs % 1000;
        
        // Let's divide the width
        float midX = bounds.getWidth() * 0.52f;
        
        // Vertical divider
        g.setColour (juce::Colour (0xff242428));
        g.drawVerticalLine ((int)midX, 2.0f, bounds.getHeight() - 2.0f);
        
        // 1. Bars & Beats Pane (Left)
        auto leftPane = bounds.withRight (midX).reduced (4.0f);
        g.setColour (juce::Colour (0xffe2e2e6));
        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));
        juce::String barsStr = juce::String::formatted ("%03d . %02d . %03d", bar, beat, tick);
        g.drawText (barsStr, leftPane.withBottom (leftPane.getBottom() - 10.0f), juce::Justification::centred);
        
        g.setColour (juce::Colour (0xff6e6e73));
        g.setFont (juce::Font (7.0f, juce::Font::bold));
        g.drawText ("BAR     BEAT    TICK", leftPane.withTop (leftPane.getBottom() - 12.0f), juce::Justification::centred);
        
        // 2. Timecode Pane (Right)
        auto rightPane = bounds.withLeft (midX).reduced (4.0f);
        g.setColour (juce::Colour (0xff00b4d8)); // Flat cyan
        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));
        juce::String timeStr = juce::String::formatted ("%02d:%02d.%03d", mins, secs, ms);
        g.drawText (timeStr, rightPane.withBottom (rightPane.getBottom() - 10.0f), juce::Justification::centred);
        
        g.setColour (juce::Colour (0xff6e6e73));
        g.setFont (juce::Font (7.0f, juce::Font::bold));
        g.drawText ("TIMECODE", rightPane.withTop (rightPane.getBottom() - 12.0f), juce::Justification::centred);
    }
    
    void timerCallback() override
    {
        repaint();
    }

private:
    AudioEngine& audioEngine;
};

class TransportButton : public juce::Button
{
public:
    enum class IconType { Play, Pause, Stop, Record, Loop, Metronome,
                          Select, Split, Eraser, MuteTool, Undo, Redo };
    
    TransportButton (IconType type)
        : Button (juce::String()), iconType (type) {}
        
    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Draw background (flat dark theme style)
        juce::Colour bgColour (0xff242428);
        if (getToggleState())
        {
            if (iconType == IconType::Record)
                bgColour = juce::Colour (0xffd90429);
            else if (iconType == IconType::Loop || iconType == IconType::Metronome ||
                     iconType == IconType::Select || iconType == IconType::Split ||
                     iconType == IconType::Eraser || iconType == IconType::MuteTool)
                bgColour = juce::Colour (0xff00b4d8);
        }
        else if (shouldDrawButtonAsDown)
        {
            bgColour = juce::Colour (0xff1a1a1d);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            bgColour = juce::Colour (0xff2f2f35);
        }
        
        g.setColour (bgColour);
        g.fillRoundedRectangle (bounds, 4.0f);
        
        // Draw border (flat subtle border)
        g.setColour (juce::Colour (0xff121214));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
        
        // Draw Icon keeping aspect ratio centered
        float iconSize = juce::jmin (bounds.getWidth(), bounds.getHeight()) - 16.0f;
        iconSize = juce::jmax (4.0f, iconSize);
        auto iconBounds = bounds.withSizeKeepingCentre (iconSize, iconSize);
        g.setColour (getToggleState() ? juce::Colours::white : (iconType == IconType::Record ? juce::Colour (0xffea4335) : juce::Colour (0xffe2e2e6)));
        
        juce::Path p;
        if (iconType == IconType::Play)
        {
            p.startNewSubPath (iconBounds.getX() + 2.0f, iconBounds.getY());
            p.lineTo (iconBounds.getRight(), iconBounds.getCentreY());
            p.lineTo (iconBounds.getX() + 2.0f, iconBounds.getBottom());
            p.closeSubPath();
            g.fillPath (p);
        }
        else if (iconType == IconType::Pause)
        {
            float w = iconBounds.getWidth() * 0.3f;
            g.fillRect (iconBounds.getX() + 1.0f, iconBounds.getY(), w, iconBounds.getHeight());
            g.fillRect (iconBounds.getRight() - w - 1.0f, iconBounds.getY(), w, iconBounds.getHeight());
        }
        else if (iconType == IconType::Stop)
        {
            g.fillRect (iconBounds.reduced (1.0f));
        }
        else if (iconType == IconType::Record)
        {
            g.fillEllipse (iconBounds.getCentreX() - iconBounds.getWidth()*0.4f,
                           iconBounds.getCentreY() - iconBounds.getHeight()*0.4f,
                           iconBounds.getWidth()*0.8f, iconBounds.getHeight()*0.8f);
        }
        else if (iconType == IconType::Loop)
        {
            p.addCentredArc (iconBounds.getCentreX(), iconBounds.getCentreY(),
                             iconBounds.getWidth() * 0.4f, iconBounds.getHeight() * 0.4f,
                             0.0f, 0.4f, juce::MathConstants<double>::pi * 1.5f, true);
            g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
            
            juce::Path arrowHead;
            float arrowSize = 3.5f;
            float endX = iconBounds.getCentreX() + iconBounds.getWidth() * 0.4f * std::cos (juce::MathConstants<double>::pi * 1.5f - juce::MathConstants<double>::pi * 0.5f);
            float endY = iconBounds.getCentreY() + iconBounds.getHeight() * 0.4f * std::sin (juce::MathConstants<double>::pi * 1.5f - juce::MathConstants<double>::pi * 0.5f);
            
            arrowHead.startNewSubPath (endX - arrowSize, endY - arrowSize);
            arrowHead.lineTo (endX + arrowSize, endY);
            arrowHead.lineTo (endX - arrowSize, endY + arrowSize);
            arrowHead.closeSubPath();
            g.fillPath (arrowHead);
        }
        else if (iconType == IconType::Metronome)
        {
            float cx = iconBounds.getCentreX();
            float cy = iconBounds.getCentreY();
            float r = iconBounds.getWidth() * 0.5f;
            
            p.startNewSubPath (cx, cy - r);
            p.lineTo (cx + r * 0.6f, cy + r);
            p.lineTo (cx - r * 0.6f, cy + r);
            p.closeSubPath();
            g.strokePath (p, juce::PathStrokeType (1.5f));
            
            g.drawLine (cx, cy + r * 0.7f, cx + r * 0.3f, cy - r * 0.2f, 2.0f);
            g.fillEllipse (cx + r * 0.3f - 2.5f, cy - r * 0.2f - 2.5f, 5.0f, 5.0f);
        }
        else if (iconType == IconType::Select)
        {
            float cx = iconBounds.getX();
            float cy = iconBounds.getY();
            float w = iconBounds.getWidth();
            float h = iconBounds.getHeight();
            
            p.startNewSubPath (cx + w * 0.1f, cy + h * 0.1f);
            p.lineTo (cx + w * 0.9f, cy + h * 0.45f);
            p.lineTo (cx + w * 0.5f, cy + h * 0.55f);
            p.lineTo (cx + w * 0.7f, cy + h * 0.9f);
            p.lineTo (cx + w * 0.55f, cy + h * 0.95f);
            p.lineTo (cx + w * 0.35f, cy + h * 0.6f);
            p.lineTo (cx + w * 0.1f, cy + h * 0.7f);
            p.closeSubPath();
            g.fillPath (p);
        }
        else if (iconType == IconType::Split)
        {
            float cx = iconBounds.getCentreX();
            float cy = iconBounds.getCentreY();
            float w = iconBounds.getWidth();
            float h = iconBounds.getHeight();

            p.startNewSubPath (cx - w*0.3f, cy + h*0.3f);
            p.lineTo (cx + w*0.4f, cy - h*0.4f);
            p.startNewSubPath (cx - w*0.3f, cy - h*0.3f);
            p.lineTo (cx + w*0.4f, cy + h*0.4f);
            g.strokePath (p, juce::PathStrokeType (2.0f));
            
            g.drawEllipse (cx - w*0.45f, cy + h*0.15f, w*0.3f, h*0.3f, 2.0f);
            g.drawEllipse (cx - w*0.45f, cy - h*0.45f, w*0.3f, h*0.3f, 2.0f);
        }
        else if (iconType == IconType::Eraser)
        {
            float cx = iconBounds.getCentreX();
            float cy = iconBounds.getCentreY();
            float w = iconBounds.getWidth();
            float h = iconBounds.getHeight();
            
            juce::Path eraserPath;
            eraserPath.startNewSubPath (cx - w*0.4f, cy + h*0.1f);
            eraserPath.lineTo (cx - w*0.1f, cy - h*0.3f);
            eraserPath.lineTo (cx + w*0.4f, cy + h*0.1f);
            eraserPath.lineTo (cx + w*0.1f, cy + h*0.5f);
            eraserPath.closeSubPath();
            g.fillPath (eraserPath);
            
            g.saveState();
            g.reduceClipRegion (eraserPath);
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            juce::Path line;
            line.startNewSubPath (cx + w*0.15f, cy - h*0.1f);
            line.lineTo (cx - w*0.15f, cy + h*0.3f);
            g.strokePath (line, juce::PathStrokeType (1.5f));
            g.restoreState();
        }
        else if (iconType == IconType::MuteTool)
        {
            float cx = iconBounds.getX();
            float cy = iconBounds.getY();
            float w = iconBounds.getWidth();
            float h = iconBounds.getHeight();
            
            p.startNewSubPath (cx + w * 0.1f, cy + h * 0.35f);
            p.lineTo (cx + w * 0.3f, cy + h * 0.35f);
            p.lineTo (cx + w * 0.5f, cy + h * 0.15f);
            p.lineTo (cx + w * 0.5f, cy + h * 0.85f);
            p.lineTo (cx + w * 0.3f, cy + h * 0.65f);
            p.lineTo (cx + w * 0.1f, cy + h * 0.65f);
            p.closeSubPath();
            g.fillPath (p);
            
            juce::Path xPath;
            float xL = cx + w * 0.65f;
            float xR = cx + w * 0.85f;
            float xT = cy + h * 0.35f;
            float xB = cy + h * 0.65f;
            
            xPath.startNewSubPath (xL, xT);
            xPath.lineTo (xR, xB);
            xPath.startNewSubPath (xR, xT);
            xPath.lineTo (xL, xB);
            g.strokePath (xPath, juce::PathStrokeType (2.0f));
        }
        else if (iconType == IconType::Undo)
        {
            float cx = iconBounds.getCentreX();
            float cy = iconBounds.getCentreY();
            float w = iconBounds.getWidth();
            float h = iconBounds.getHeight();
            
            p.addCentredArc (cx, cy, w * 0.35f, h * 0.35f, 0.0f, -0.4f, juce::MathConstants<double>::pi * 1.3f, true);
            g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
            
            juce::Path arrowHead;
            float arrowSize = 3.0f;
            float endX = cx + w * 0.35f * std::cos (juce::MathConstants<double>::pi * 1.3f - juce::MathConstants<double>::pi * 0.5f);
            float endY = cy + h * 0.35f * std::sin (juce::MathConstants<double>::pi * 1.3f - juce::MathConstants<double>::pi * 0.5f);
            
            arrowHead.startNewSubPath (endX - arrowSize, endY - arrowSize);
            arrowHead.lineTo (endX + arrowSize, endY);
            arrowHead.lineTo (endX - arrowSize, endY + arrowSize);
            arrowHead.closeSubPath();
            g.fillPath (arrowHead);
        }
        else if (iconType == IconType::Redo)
        {
            float cx = iconBounds.getCentreX();
            float cy = iconBounds.getCentreY();
            float w = iconBounds.getWidth();
            float h = iconBounds.getHeight();
            
            p.addCentredArc (cx, cy, w * 0.35f, h * 0.35f, 0.0f, 0.4f, -juce::MathConstants<double>::pi * 0.3f, true);
            g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
            
            juce::Path arrowHead;
            float arrowSize = 3.0f;
            float endX = cx + w * 0.35f * std::cos (-juce::MathConstants<double>::pi * 0.3f + juce::MathConstants<double>::pi * 0.5f);
            float endY = cy + h * 0.35f * std::sin (-juce::MathConstants<double>::pi * 0.3f + juce::MathConstants<double>::pi * 0.5f);
            
            arrowHead.startNewSubPath (endX - arrowSize, endY - arrowSize);
            arrowHead.lineTo (endX + arrowSize, endY);
            arrowHead.lineTo (endX - arrowSize, endY + arrowSize);
            arrowHead.closeSubPath();
            g.fillPath (arrowHead);
        }
    }
    
private:
    IconType iconType;
};

class ProjectUndoManager
{
public:
    ProjectUndoManager (AudioEngine& eng) : audioEngine (eng) {}

    void saveUndoState()
    {
        if (isRestoring) return;

        if (auto xml = audioEngine.saveProjectToXml())
        {
            redoStack.clear();
            undoStack.push_back (xml->toString());
            if (undoStack.size() > 50)
                undoStack.erase (undoStack.begin());
        }
    }

    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }

    void undo()
    {
        if (undoStack.empty()) return;

        if (auto currentXml = audioEngine.saveProjectToXml())
        {
            redoStack.push_back (currentXml->toString());
        }

        auto stateXmlStr = undoStack.back();
        undoStack.pop_back();

        restoreState (stateXmlStr);
    }

    void redo()
    {
        if (redoStack.empty()) return;

        if (auto currentXml = audioEngine.saveProjectToXml())
        {
            undoStack.push_back (currentXml->toString());
        }

        auto stateXmlStr = redoStack.back();
        redoStack.pop_back();

        restoreState (stateXmlStr);
    }

    void clear()
    {
        undoStack.clear();
        redoStack.clear();
    }

private:
    void restoreState (const juce::String& xmlStr)
    {
        isRestoring = true;
        if (auto xml = juce::XmlDocument::parse (xmlStr))
        {
            audioEngine.loadProjectFromXml (*xml);
        }
        isRestoring = false;

        if (onStateRestored)
            onStateRestored();
    }

    AudioEngine& audioEngine;
    std::vector<juce::String> undoStack;
    std::vector<juce::String> redoStack;
    bool isRestoring = false;

public:
    std::function<void()> onStateRestored;
};

class MainComponent  : public juce::Component, public juce::Timer, public juce::MenuBarModel
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    bool keyPressed (const juce::KeyPress& key) override;
    
    void updateTracksUI();
    void layoutMixer();
    void openSettingsWindow();
    void newProject();
    void saveProject();
    void openProject();
    void undo();
    void redo();
    void setMixerDocked (bool docked);
    void setMixerVisible (bool visible);
    bool getMixerDocked() const { return isMixerDocked; }
    bool getMixerVisible() const { return isMixerVisible; }
    void updateBpmGlobally (double newBpm);

    // MenuBarModel overrides
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

private:
    class MixerResizer : public juce::Component
    {
    public:
        MixerResizer (MainComponent& owner);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
    private:
        MainComponent& mainComp;
        int initialHeight = 0;
    };

    MixerResizer mixerResizer;
    int dockedMixerHeight = 390;

    AudioEngine audioEngine;
    ModernDawLookAndFeel modernLookAndFeel;
    
    juce::MenuBarComponent menuBar;
    
    // Top Bar (Transport & Toolbar)
    juce::Component topBar;
    juce::TextButton addTrackButton { "Add Audio Track" };
    juce::TextButton addMidiTrackButton { "Add MIDI Track" };
    TransportButton playButton { TransportButton::IconType::Play };
    TransportButton pauseButton { TransportButton::IconType::Pause };
    TransportButton stopButton { TransportButton::IconType::Stop };
    TransportButton recordButton { TransportButton::IconType::Record };
    juce::TextButton loadWavButton { "Load WAV (Track 1)" };
    juce::TextButton loadVstButton { "Load Plugin (Track 1)" };
    juce::TextButton spliceButton { "Splice" };
    juce::TextButton addMidiButton { "Add MIDI Item" };
    juce::TextButton showMidiEditorButton { "MIDI Editor" };
    TransportButton loopButton { TransportButton::IconType::Loop };
    juce::TextButton autoFadeButton { "Auto-Fade" };
    TransportButton metronomeButton { TransportButton::IconType::Metronome };
    TransportButton selectToolBtn { TransportButton::IconType::Select };
    TransportButton splitToolBtn { TransportButton::IconType::Split };
    TransportButton eraserToolBtn { TransportButton::IconType::Eraser };
    TransportButton muteToolBtn { TransportButton::IconType::MuteTool };
    TransportButton undoBtn { TransportButton::IconType::Undo };
    TransportButton redoBtn { TransportButton::IconType::Redo };
    ProjectUndoManager undoManager { audioEngine };
    TimecodeDisplay timecodeDisplay { audioEngine };
    juce::Slider bpmSlider;
    juce::Label bpmLabel;
    juce::ComboBox snapComboBox;
    juce::Label snapLabel;
    
    // Arrangement View
    ArrangementComponent arrangementView { audioEngine };

    // Mixer View
    std::unique_ptr<MixerWindow> mixerWindow;
    juce::Viewport mixerViewport;
    juce::Component mixerView;
    juce::OwnedArray<juce::Slider> mixerSliders;
    juce::OwnedArray<juce::Slider> mixerPanSliders;
    juce::OwnedArray<juce::TextButton> mixerMuteButtons;
    juce::OwnedArray<juce::TextButton> mixerSoloButtons;
    juce::OwnedArray<juce::TextButton> mixerArmButtons;
    juce::OwnedArray<juce::TextButton> mixerMonitorButtons;
    juce::OwnedArray<juce::ComboBox> mixerRoutingComboBoxes;
    juce::OwnedArray<juce::ComboBox> mixerInputComboBoxes;
    juce::OwnedArray<juce::Label> mixerLabels;
    juce::OwnedArray<VuMeterComponent> mixerMeters;
    juce::OwnedArray<juce::TextButton> mixerFxSlots;
    juce::OwnedArray<juce::TextButton> mixerFxRemoveButtons;

    bool isMixerDocked = true;
    bool isMixerVisible = true;

    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::Array<juce::File> systemPluginFiles;

    void scanSystemPlugins();
    void loadWav(bool atPlayhead = false);
    void loadWavToSelectedTrack(bool atPlayhead = false);
    void loadVst();
    void loadVstIntoSlot(int trackIdx, int slotIdx);
    void loadPluginFileIntoSlot(const juce::File& file, int trackIdx, int slotIdx);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
