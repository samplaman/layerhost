#include "MainComponent.h"

void ItemComponent::mouseUp (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::Move && !e.mods.isAltDown())
    {
        if (arrangement)
            arrangement->handleItemDragEnd (this);
    }
}

void ItemComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (item)
    {
        if (item->getType() == Item::Type::Midi)
        {
            if (arrangement)
                arrangement->openPianoRoll (item);
        }
        else if (item->getType() == Item::Type::Audio)
        {
            if (arrangement)
                arrangement->openAudioClipEditor (item);
        }
    }
}

void ItemComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isAltDown() || arrangement->getCurrentTool() != ArrangementComponent::EditTool::Select) return;
    
    if (!hasSavedUndoForCurrentDrag)
    {
        arrangement->saveUndoState();
        hasSavedUndoForCurrentDrag = true;
    }
    
    if (dragMode == DragMode::Move)
    {
        dragger.dragComponent (this, e, nullptr);
        if (item)
        {
            double newTime = (getX() + arrangement->getScrollX()) / pixelsPerSecond;
            double snapInterval = arrangement->getAudioEngine().getSnapIntervalSeconds();
            if (snapInterval > 0.0)
                newTime = std::round (newTime / snapInterval) * snapInterval;
            
            newTime = juce::jmax (0.0, newTime);
            item->setStartTime (newTime);
            setBounds ((int)(newTime * pixelsPerSecond) - (int)arrangement->getScrollX(), getY(), getWidth(), getHeight());
        }
    }
    else if (dragMode == DragMode::TrimRight)
    {
        int newWidth = juce::jmax (10, (int)e.getEventRelativeTo(this).position.x);
        setBounds (getX(), getY(), newWidth, getHeight());
        if (item) item->setLength (newWidth / pixelsPerSecond);
    }
    else if (dragMode == DragMode::TrimLeft)
    {
        int dx = e.getEventRelativeTo(this).position.x;
        if (dx != 0 && getWidth() - dx > 10)
        {
            int newX = getX() + dx;
            double newTime = (newX + arrangement->getScrollX()) / pixelsPerSecond;
            
            if (newTime < 0.0)
            {
                dx += (int)(-newTime * pixelsPerSecond);
                newX = -(int)arrangement->getScrollX();
                newTime = 0.0;
            }
            
            int newWidth = getWidth() - dx;
            setBounds (newX, getY(), newWidth, getHeight());
            if (item)
            {
                item->setStartTime (newTime);
                item->setSourceOffset (item->getSourceOffset() + (dx / pixelsPerSecond));
                item->setLength (newWidth / pixelsPerSecond);
            }
        }
    }
}

void ArrangementComponent::filesDropped (const juce::StringArray& files, int x, int y)
{
    double dropTime = 0.0;
    if (x >= headerWidth)
    {
        dropTime = (x - headerWidth + scrollX) / pixelsPerSecond;
        double snapInterval = audioEngine.getSnapIntervalSeconds();
        if (snapInterval > 0.0)
            dropTime = std::round (dropTime / snapInterval) * snapInterval;
    }
    dropTime = juce::jmax (0.0, dropTime);
    
    int targetTrackIdx = -1;
    int currentY = 30 - (int)scrollY;
    for (int i = 0; i < audioEngine.getNumTracks(); ++i)
    {
        auto* t = audioEngine.getTrack(i);
        int nextY = currentY + t->getTotalHeight() + 10;
            
        if (y >= currentY && y < nextY)
        {
            targetTrackIdx = i;
            break;
        }
        currentY = nextY;
    }

    saveUndoState();
    
    for (auto file : files)
    {
        if (file.endsWithIgnoreCase (".wav"))
        {
            auto item = std::make_unique<Item>();
            if (item->loadFile (juce::File(file), audioEngine.getFormatManager(), audioEngine.getCurrentSampleRate()))
            {
                item->setStartTime (dropTime);
                
                if (targetTrackIdx == -1)
                {
                    audioEngine.addTrack();
                    targetTrackIdx = audioEngine.getNumTracks() - 1;
                    if (auto* mainComp = findParentComponentOfClass<MainComponent>())
                        mainComp->updateTracksUI();
                }
                
                audioEngine.getTrack(targetTrackIdx)->addItem (std::move (item));
                updateItems();
                repaint();
                break; // Only load first for now
            }
        }
    }
}

void ItemComponent::splitItem (float xPos)
{
    if (arrangement)
        arrangement->splitItem (this, xPos);
}

void ArrangementComponent::mouseUp (const juce::MouseEvent& e)
{
    selectedAutomationTrackIdx = -1;
    selectedAutomationPointIdx = -1;
    selectedAutomationLaneType = 0;
    selectedAutomationSlotIndex = -1;
    selectedAutomationParamIndex = -1;
    isDraggingCurve = false;
    selectedCurvePointIdx = -1;

    if (isDraggingLoop)
    {
        isDraggingLoop = false;
        return;
    }

    if (draggedTrackIdx != -1)
    {
        int dropIdx = -1;
        int currentY = 30 - (int)scrollY;
        for (int i = 0; i < audioEngine.getNumTracks(); ++i)
        {
            auto* t = audioEngine.getTrack(i);
            int nextY = currentY + t->getTotalHeight() + 10;
                
            if (e.y >= currentY && e.y < nextY)
            {
                dropIdx = i;
                break;
            }
            currentY = nextY;
        }

        if (dropIdx == -1)
            dropIdx = audioEngine.getNumTracks() - 1;

        dropIdx = juce::jlimit (0, audioEngine.getNumTracks() - 1, dropIdx);
        
        if (dropIdx != draggedTrackIdx)
        {
            saveUndoState();
            audioEngine.moveTrack (draggedTrackIdx, dropIdx);
            if (auto* mainComp = findParentComponentOfClass<MainComponent>())
                mainComp->updateTracksUI();

            updateItems();
            repaint();
        }
        draggedTrackIdx = -1;
    }
}


void MixerWindow::resized()
{
    DocumentWindow::resized();
        
    if (mainComp)
        mainComp->layoutMixer();
}

void MixerWindow::closeButtonPressed()
{
    if (mainComp != nullptr)
        mainComp->setMixerVisible (false);
    else
        setVisible (false);
}

ItemComponent::ItemComponent (Item* i, double pxPerSec, int trackIdx, ArrangementComponent* arr)
    : item(i), pixelsPerSecond(pxPerSec), currentTrackIdx(trackIdx), arrangement(arr)
{
    if (item && item->getType() == Item::Type::Midi)
    {
        addAndMakeVisible (editBtn);
        editBtn.onClick = [this] {
            arrangement->onPianoRollRequested(item);
        };
    }
}

void ItemComponent::paint (juce::Graphics& g)
{
    if (item)
    {
        g.saveState();
        if (item->getMuted())
            g.setOpacity (0.35f);

        if (item->getType() == Item::Type::Audio)
        {
            g.setColour (juce::Colour (0x6600b4d8)); // Flat cyan background
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
            
            g.setColour (juce::Colour (0xff00b4d8)); // Flat cyan outline
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);

            g.setColour (juce::Colour (0xff0077b6)); // Solid dark cyan handle bars
            g.fillRect (0, 0, 4, getHeight());
            g.fillRect (getWidth() - 4, 0, 4, getHeight());
            const auto& buf = item->getBuffer();
            if (buf.getNumChannels() > 0 && buf.getNumSamples() > 0)
            {
                juce::Rectangle<int> clipBounds = g.getClipBounds();
                int startX = juce::jmax(0, clipBounds.getX());
                int endX = juce::jmin(getWidth(), clipBounds.getRight());

                auto* readPtr = buf.getReadPointer(0);
                int numSamplesTotal = buf.getNumSamples();
                int startSample = (int)(item->getSourceOffset() * 44100.0);
                int endSample = startSample + (int)(item->getLength() * 44100.0);
                
                if (startSample >= 0 && endSample <= numSamplesTotal && startSample < endSample && getWidth() > 0)
                {
                    juce::Path visibleWaveformPath;
                    float step = (endSample - startSample) / (float)getWidth();
                    if (step < 1.0f) step = 1.0f;
                    
                    for (int i = startX; i < endX; ++i)
                    {
                        int sStart = startSample + (int)(i * step);
                        int sEnd = startSample + (int)((i + 1) * step);
                        sStart = juce::jlimit (0, numSamplesTotal - 1, sStart);
                        sEnd = juce::jlimit (0, numSamplesTotal - 1, sEnd);
                        
                        float minVal = 1.0f;
                        float maxVal = -1.0f;
                        
                        if (sEnd > sStart)
                        {
                            int skip = juce::jmax(1, (sEnd - sStart) / 100);
                            for (int s = sStart; s < sEnd; s += skip)
                            {
                                float v = readPtr[s];
                                if (v < minVal) minVal = v;
                                if (v > maxVal) maxVal = v;
                            }
                        }
                        else
                        {
                            minVal = maxVal = readPtr[sStart];
                        }
                        
                        float yMin = juce::jmap (maxVal, -1.0f, 1.0f, (float)getHeight(), 0.0f);
                        float yMax = juce::jmap (minVal, -1.0f, 1.0f, (float)getHeight(), 0.0f);
                        
                        if (i == startX) visibleWaveformPath.startNewSubPath ((float)i, yMin);
                        else visibleWaveformPath.lineTo ((float)i, yMin);
                        visibleWaveformPath.lineTo ((float)i, yMax);
                    }
                    
                    g.setColour (juce::Colour (0xffe2e2e6));
                    g.strokePath (visibleWaveformPath, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved));
                }
            }
        }
        else if (item->getType() == Item::Type::Midi)
        {
            g.setColour (juce::Colour (0x669c27b0)); // Flat purple background
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
            
            g.setColour (juce::Colour (0xff9c27b0)); // Flat purple outline
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);

            g.setColour (juce::Colour (0xff7b1fa2)); // Solid dark purple handle bars
            g.fillRect (0, 0, 4, getHeight());
            g.fillRect (getWidth() - 4, 0, 4, getHeight());

            // Draw MIDI note preview
            auto& seq = item->getMidiSequence();
            int numEvents = seq.getNumEvents();
            if (numEvents > 0)
            {
                int minNote = 127;
                int maxNote = 0;
                bool hasNotes = false;
                
                // Find pitch range
                for (int i = 0; i < numEvents; ++i)
                {
                    auto* ev = seq.getEventPointer(i);
                    if (ev->message.isNoteOn())
                    {
                        int noteNum = ev->message.getNoteNumber();
                        if (noteNum < minNote) minNote = noteNum;
                        if (noteNum > maxNote) maxNote = noteNum;
                        hasNotes = true;
                    }
                }
                
                if (hasNotes)
                {
                    int noteDiff = maxNote - minNote;
                    if (noteDiff == 0) noteDiff = 12; // fallback to an octave height range
                    
                    float marginX = 8.0f;
                    float marginY = 6.0f;
                    float drawWidth = getWidth() - 2.0f * marginX;
                    float drawHeight = getHeight() - 2.0f * marginY;
                    double itemLength = item->getLength();
                    
                    if (itemLength > 0.0)
                    {
                        for (int i = 0; i < numEvents; ++i)
                        {
                            auto* ev = seq.getEventPointer(i);
                            if (ev->message.isNoteOn())
                            {
                                int noteNum = ev->message.getNoteNumber();
                                double timeSec = ev->message.getTimeStamp();
                                double lengthSec = 0.5; // default fallback
                                
                                int matchIdx = seq.getIndexOfMatchingKeyUp (i);
                                if (matchIdx >= 0 && matchIdx < numEvents)
                                {
                                    auto* noteOff = seq.getEventPointer (matchIdx);
                                    if (noteOff)
                                        lengthSec = noteOff->message.getTimeStamp() - timeSec;
                                }
                                
                                // Map time to x bounds
                                float xStart = marginX + (float)(timeSec / itemLength) * drawWidth;
                                float xEnd = marginX + (float)((timeSec + lengthSec) / itemLength) * drawWidth;
                                xStart = juce::jlimit (marginX, marginX + drawWidth, xStart);
                                xEnd = juce::jlimit (marginX, marginX + drawWidth, xEnd);
                                
                                if (xEnd > xStart)
                                {
                                    // Map pitch to y (higher pitch is higher up / lower Y value)
                                    float pitchRatio = 0.5f;
                                    if (maxNote != minNote)
                                        pitchRatio = (float)(noteNum - minNote) / (float)noteDiff;
                                        
                                    float yPos = marginY + (1.0f - pitchRatio) * (drawHeight - 3.0f);
                                    
                                    int velocity = ev->message.getVelocity();
                                    float velRatio = velocity / 127.0f;
                                    float alpha = 0.25f + 0.6f * velRatio; // scale preview note opacity
                                    
                                    g.setColour (juce::Colour (0xffe2e2e6).withAlpha (alpha));
                                    g.fillRect (xStart, yPos, juce::jmax (2.0f, xEnd - xStart), 3.0f);
                                }
                            }
                        }
                    }
                }
            }
            
            // Draw a subtle "MIDI" text label in a corner so the clip type remains identifiable
            g.setColour (juce::Colour (0xccffffff));
            g.setFont (juce::Font (10.0f, juce::Font::bold));
            g.drawText ("MIDI", getLocalBounds().reduced (6), juce::Justification::topLeft);
        }

        if (item->getMuted())
        {
            g.setColour (juce::Colour (0xffea4335));
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.drawText ("[MUTED]", getLocalBounds().reduced (5), juce::Justification::bottomLeft);
        }

        g.restoreState();
    }
    
    if (arrangement->selectedItem == item)
    {
        g.setColour (juce::Colour (0xfffbc02d));
        g.drawRoundedRectangle (getLocalBounds().toFloat(), 4.0f, 1.5f);
    }
}

void ItemComponent::mouseDown (const juce::MouseEvent& e)
{ 
    arrangement->selectedItem = this->item;
    arrangement->selectedTrackIdx = currentTrackIdx;
    arrangement->repaint();
    
    hasSavedUndoForCurrentDrag = false;

    auto tool = arrangement->getCurrentTool();
    if (tool == ArrangementComponent::EditTool::Split || e.mods.isAltDown())
    {
        arrangement->saveUndoState();
        arrangement->splitItem (this, e.position.x);
        return;
    }
    else if (tool == ArrangementComponent::EditTool::Eraser)
    {
        arrangement->saveUndoState();
        arrangement->deleteItem (this);
        return;
    }
    else if (tool == ArrangementComponent::EditTool::Mute)
    {
        arrangement->saveUndoState();
        item->setMuted (!item->getMuted());
        arrangement->repaint();
        return;
    }

    if (e.position.x <= 5.0f)
        dragMode = DragMode::TrimLeft;
    else if (e.position.x >= getWidth() - 5.0f)
        dragMode = DragMode::TrimRight;
    else
        dragMode = DragMode::Move;
        
    dragger.startDraggingComponent (this, e); 
}

MainComponent::MainComponent() : mixerResizer (*this)
{
    addChildComponent (mixerResizer);
    juce::LookAndFeel::setDefaultLookAndFeel (&modernLookAndFeel);
    setSize (1000, 700);

    // Setup Top Bar
    addAndMakeVisible (topBar);
    topBar.addAndMakeVisible (addTrackButton);
    topBar.addAndMakeVisible (addMidiTrackButton);
    topBar.addAndMakeVisible (playButton);
    topBar.addAndMakeVisible (pauseButton);
    topBar.addAndMakeVisible (stopButton);
    topBar.addAndMakeVisible (recordButton);
    topBar.addAndMakeVisible (loopButton);
    topBar.addAndMakeVisible (autoFadeButton);
    topBar.addAndMakeVisible (loadWavButton);
    topBar.addAndMakeVisible (loadVstButton);
    topBar.addAndMakeVisible (spliceButton);
    topBar.addAndMakeVisible (addMidiButton);
    topBar.addAndMakeVisible (showMidiEditorButton);

    playButton.setClickingTogglesState (false);
    pauseButton.setClickingTogglesState (false);
    stopButton.setClickingTogglesState (false);
    
    recordButton.setClickingTogglesState (true);
    
    loopButton.setClickingTogglesState (true);
    loopButton.onClick = [this] { audioEngine.setLooping(loopButton.getToggleState()); };

    // Metronome button
    topBar.addAndMakeVisible (metronomeButton);
    metronomeButton.setClickingTogglesState (true);
    metronomeButton.onClick = [this] {
        audioEngine.setMetronomeEnabled (metronomeButton.getToggleState());
    };

    // Timecode Display
    topBar.addAndMakeVisible (timecodeDisplay);

    // Wire up undo manager and tool callbacks
    undoManager.onStateRestored = [this] {
        bpmSlider.setValue (audioEngine.getBpm(), juce::dontSendNotification);
        
        int snapId = 1;
        auto snap = audioEngine.getSnapDivision();
        if (snap == AudioEngine::SnapDivision::Bar) snapId = 2;
        else if (snap == AudioEngine::SnapDivision::HalfBar) snapId = 3;
        else if (snap == AudioEngine::SnapDivision::Beat) snapId = 4;
        else if (snap == AudioEngine::SnapDivision::HalfBeat) snapId = 5;
        else if (snap == AudioEngine::SnapDivision::QuarterBeat) snapId = 6;
        else if (snap == AudioEngine::SnapDivision::EighthBeat) snapId = 7;
        snapComboBox.setSelectedId (snapId, juce::dontSendNotification);

        updateTracksUI();
        repaint();
    };

    arrangementView.onSaveUndoStateRequested = [this] {
        undoManager.saveUndoState();
    };

    // Add and setup tool/undo buttons
    topBar.addAndMakeVisible (selectToolBtn);
    topBar.addAndMakeVisible (splitToolBtn);
    topBar.addAndMakeVisible (eraserToolBtn);
    topBar.addAndMakeVisible (muteToolBtn);
    topBar.addAndMakeVisible (undoBtn);
    topBar.addAndMakeVisible (redoBtn);

    selectToolBtn.setRadioGroupId (1001);
    splitToolBtn.setRadioGroupId (1001);
    eraserToolBtn.setRadioGroupId (1001);
    muteToolBtn.setRadioGroupId (1001);

    selectToolBtn.setClickingTogglesState (true);
    splitToolBtn.setClickingTogglesState (true);
    eraserToolBtn.setClickingTogglesState (true);
    muteToolBtn.setClickingTogglesState (true);

    selectToolBtn.setToggleState (true, juce::dontSendNotification);

    selectToolBtn.onClick = [this] {
        arrangementView.setCurrentTool (ArrangementComponent::EditTool::Select);
    };
    splitToolBtn.onClick = [this] {
        arrangementView.setCurrentTool (ArrangementComponent::EditTool::Split);
    };
    eraserToolBtn.onClick = [this] {
        arrangementView.setCurrentTool (ArrangementComponent::EditTool::Eraser);
    };
    muteToolBtn.onClick = [this] {
        arrangementView.setCurrentTool (ArrangementComponent::EditTool::Mute);
    };

    undoBtn.onClick = [this] { undo(); };
    redoBtn.onClick = [this] { redo(); };

    // BPM Label
    topBar.addAndMakeVisible (bpmLabel);
    bpmLabel.setText ("BPM:", juce::dontSendNotification);
    bpmLabel.setFont (juce::Font (12.0f, juce::Font::bold));
    bpmLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    bpmLabel.setJustificationType (juce::Justification::centredRight);

    // BPM Slider
    topBar.addAndMakeVisible (bpmSlider);
    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 45, 18);
    bpmSlider.setRange (20.0, 300.0, 1.0);
    bpmSlider.setValue (audioEngine.getBpm());
    bpmSlider.onValueChange = [this] {
        updateBpmGlobally (bpmSlider.getValue());
    };

    // Snap Label
    topBar.addAndMakeVisible (snapLabel);
    snapLabel.setText ("SNAP:", juce::dontSendNotification);
    snapLabel.setFont (juce::Font (12.0f, juce::Font::bold));
    snapLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    snapLabel.setJustificationType (juce::Justification::centredRight);

    // Snap ComboBox
    topBar.addAndMakeVisible (snapComboBox);
    snapComboBox.addItem ("Off", 1);
    snapComboBox.addItem ("1 Bar", 2);
    snapComboBox.addItem ("1/2 Bar", 3);
    snapComboBox.addItem ("1 Beat", 4);
    snapComboBox.addItem ("1/2 Beat", 5);
    snapComboBox.addItem ("1/4 Beat", 6);
    snapComboBox.addItem ("1/8 Beat", 7);
    snapComboBox.setSelectedId (1, juce::dontSendNotification);
    
    snapComboBox.onChange = [this] {
        int id = snapComboBox.getSelectedId();
        if (id == 1) audioEngine.setSnapDivision (AudioEngine::SnapDivision::None);
        else if (id == 2) audioEngine.setSnapDivision (AudioEngine::SnapDivision::Bar);
        else if (id == 3) audioEngine.setSnapDivision (AudioEngine::SnapDivision::HalfBar);
        else if (id == 4) audioEngine.setSnapDivision (AudioEngine::SnapDivision::Beat);
        else if (id == 5) audioEngine.setSnapDivision (AudioEngine::SnapDivision::HalfBeat);
        else if (id == 6) audioEngine.setSnapDivision (AudioEngine::SnapDivision::QuarterBeat);
        else if (id == 7) audioEngine.setSnapDivision (AudioEngine::SnapDivision::EighthBeat);
    };

    addTrackButton.onClick = [this] {
        undoManager.saveUndoState();
        audioEngine.addTrack(Track::Type::Audio);
        updateTracksUI();
        repaint();
    };

    addMidiTrackButton.onClick = [this] {
        juce::PopupMenu m;
        m.addItem (1, "Omni (All Channels)");
        m.addSeparator();
        for (int ch = 1; ch <= 16; ++ch)
        {
            m.addItem (ch + 1, "Channel " + juce::String (ch));
        }
        
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addMidiTrackButton),
            [this](int result) {
                if (result > 0)
                {
                    int chosenChannel = result - 1; // 0 for Omni, 1-16 for specific channels
                    undoManager.saveUndoState();
                    audioEngine.addTrack(Track::Type::Midi, chosenChannel);
                    updateTracksUI();
                    repaint();
                }
            });
    };

    playButton.onClick = [this] { 
        audioEngine.setPlaying(true); 
        arrangementView.updateItems();
    };
    pauseButton.onClick = [this] { 
        audioEngine.setPlaying(false);
        arrangementView.updateItems();
        repaint();
    };
    stopButton.onClick = [this] { 
        audioEngine.setPlaying(false);
        audioEngine.setPlayPosition(0.0);
        arrangementView.updateItems();
        repaint();
    };
    recordButton.onClick = [this] {
        audioEngine.setRecording(recordButton.getToggleState());
        if (!recordButton.getToggleState())
        {
            arrangementView.updateItems();
            repaint();
        }
    };

    loadWavButton.onClick = [this] { loadWav(); };
    loadVstButton.onClick = [this] { loadVst(); };
    spliceButton.onClick = [this] {
        undoManager.saveUndoState();
        arrangementView.spliceAtPlayhead();
    };

    addMidiButton.onClick = [this] {
        if (audioEngine.getNumTracks() > 0)
        {
            undoManager.saveUndoState();
            auto item = std::make_unique<Item>();
            item->setType(Item::Type::Midi);
            item->setStartTime(audioEngine.getPlayPosition());
            item->setLength(2.0); // 2 seconds long
            audioEngine.getTrack(0)->addItem(std::move(item));
            arrangementView.updateItems();
            repaint();
        }
    };

    showMidiEditorButton.onClick = [this] {
        Item* firstMidiItem = nullptr;
        for (int i = 0; i < audioEngine.getNumTracks(); ++i)
        {
            for (auto& item : audioEngine.getTrack(i)->getItems())
            {
                if (item && item->getType() == Item::Type::Midi)
                {
                    firstMidiItem = item.get();
                    break;
                }
            }
            if (firstMidiItem) break;
        }
        arrangementView.openPianoRoll(firstMidiItem);
    };

    arrangementView.onPianoRollRequested = [this](Item* item) {
        auto* w = new PianoRollWindow (audioEngine);
        w->setItem(item);
        w->onItemsChanged = [this]() {
            arrangementView.updateItems();
            repaint();
        };
        w->setVisible(true);
        w->toFront(true);
    };

    arrangementView.onAudioClipEditorRequested = [this](Item* item) {
        auto* w = new AudioClipEditorWindow();
        w->setItem (item);
        w->onBufferModified = [this]() {
            arrangementView.updateItems();
            repaint();
        };
        w->setVisible (true);
        w->toFront (true);
    };

    arrangementView.onTrackStatusChanged = [this]() {
        updateTracksUI();
    };

    autoFadeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff242428));
    autoFadeButton.onClick = [this] {
        undoManager.saveUndoState();
        for (int i = 0; i < audioEngine.getNumTracks(); ++i)
        {
            auto* track = audioEngine.getTrack(i);
            for (auto& item : track->getItems())
            {
                if (item && item->getType() == Item::Type::Audio)
                {
                    item->setFadeIn (0.01);  // 10ms attack
                    item->setFadeOut (0.01); // 10ms release
                }
            }
        }
        arrangementView.updateItems();
        repaint();
    };

    // Setup Views
    menuBar.setModel (this);
    addAndMakeVisible (menuBar);
    addAndMakeVisible (arrangementView);
    
    mixerViewport.setViewedComponent (&mixerView, false);
    mixerViewport.setScrollBarsShown (true, true, true, true);
    
    if (isMixerDocked)
    {
        addAndMakeVisible (mixerViewport);
    }
    else
    {
        mixerWindow = std::make_unique<MixerWindow>("Layerhost Mixer", &mixerViewport, this);
    }

    scanSystemPlugins();
    audioEngine.initialise();
    std::cout << "Updating tracks UI..." << std::endl;
    updateTracksUI();
    std::cout << "Starting timer..." << std::endl;
    startTimerHz (60);
    std::cout << "MainComponent initialised!" << std::endl;
}

MainComponent::~MainComponent()
{
    mixerWindow = nullptr; // Delete window before clearing look and feel
    menuBar.setModel (nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::leftKey)
    {
        double bpm = audioEngine.getBpm();
        double tickLengthSecs = 60.0 / (bpm * 960.0);
        double currentPos = audioEngine.getPlayPosition();
        audioEngine.setPlayPosition (juce::jmax (0.0, currentPos - tickLengthSecs));
        repaint();
        return true;
    }
    if (key == juce::KeyPress::rightKey)
    {
        double bpm = audioEngine.getBpm();
        double tickLengthSecs = 60.0 / (bpm * 960.0);
        double currentPos = audioEngine.getPlayPosition();
        audioEngine.setPlayPosition (currentPos + tickLengthSecs);
        repaint();
        return true;
    }
    if (key == juce::KeyPress::spaceKey)
    {
        audioEngine.setPlaying (!audioEngine.getPlaying());
        arrangementView.updateItems();
        repaint();
        return true;
    }

    if (key.getKeyCode() == 'x' || key.getKeyCode() == 'X')
    {
        setMixerVisible (!isMixerVisible);
        return true;
    }

    if (key.getModifiers().isCommandDown() && (key.getKeyCode() == 'z' || key.getKeyCode() == 'Z'))
    {
        if (key.getModifiers().isShiftDown())
            redo();
        else
            undo();
        return true;
    }
    if (key.getModifiers().isCommandDown() && (key.getKeyCode() == 'y' || key.getKeyCode() == 'Y'))
    {
        redo();
        return true;
    }

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (arrangementView.selectedItem != nullptr)
        {
            juce::AlertWindow::showOkCancelBox (juce::AlertWindow::WarningIcon, "Delete Item",
                "Are you sure you want to delete this item?", "Yes", "No", nullptr,
                juce::ModalCallbackFunction::create ([this] (int result) {
                    if (result != 0) // 1 == Ok
                    {
                        if (arrangementView.selectedTrackIdx >= 0 && arrangementView.selectedTrackIdx < audioEngine.getNumTracks())
                        {
                            undoManager.saveUndoState();
                            audioEngine.getTrack(arrangementView.selectedTrackIdx)->extractItem (arrangementView.selectedItem);
                            arrangementView.selectedItem = nullptr;
                            arrangementView.updateItems();
                            repaint();
                        }
                    }
                }));
            return true;
        }
        else if (arrangementView.selectedTrackIdx >= 0 && arrangementView.selectedTrackIdx < audioEngine.getNumTracks())
        {
            juce::AlertWindow::showOkCancelBox (juce::AlertWindow::WarningIcon, "Delete Track",
                "Are you sure you want to delete this track?", "Yes", "No", nullptr,
                juce::ModalCallbackFunction::create ([this] (int result) {
                    if (result != 0) // 1 == Ok
                    {
                        undoManager.saveUndoState();
                        audioEngine.removeTrack(arrangementView.selectedTrackIdx);
                        arrangementView.selectedTrackIdx = -1;
                        arrangementView.selectedItem = nullptr;
                        arrangementView.updateItems();
                        updateTracksUI();
                        repaint();
                    }
                }));
            return true;
        }
    }
    return false;
}

void MainComponent::updateTracksUI()
{
    mixerSliders.clear();
    mixerPanSliders.clear();
    mixerMuteButtons.clear();
    mixerSoloButtons.clear();
    mixerArmButtons.clear();
    mixerMonitorButtons.clear();
    mixerRoutingComboBoxes.clear();
    mixerInputComboBoxes.clear();
    mixerLabels.clear();
    mixerMeters.clear();
    mixerFxSlots.clear();
    mixerFxRemoveButtons.clear();

    for (int i = 0; i <= audioEngine.getNumTracks(); ++i)
    {
        bool isMaster = (i == audioEngine.getNumTracks());
        auto* track = isMaster ? audioEngine.getMasterTrack() : audioEngine.getTrack(i);
        if (!track) continue;

        // Pan
        auto* pan = mixerPanSliders.add (new juce::Slider());
        pan->setSliderStyle (juce::Slider::LinearHorizontal);
        pan->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        pan->setRange (-1.0, 1.0, 0.01);
        pan->setValue (track->getPan());
        pan->onValueChange = [track, pan] { track->setPan ((float)pan->getValue()); };
        mixerView.addAndMakeVisible (pan);
        
        // Mute
        auto* mute = mixerMuteButtons.add (new juce::TextButton ("M"));
        mute->setClickingTogglesState (true);
        mute->setToggleState (track->getMute(), juce::dontSendNotification);
        mute->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffea4335));
        mute->onClick = [track, mute] { track->setMute (mute->getToggleState()); };
        mixerView.addAndMakeVisible (mute);
        
        // Solo
        auto* solo = mixerSoloButtons.add (new juce::TextButton ("S"));
        solo->setClickingTogglesState (true);
        solo->setToggleState (track->getSolo(), juce::dontSendNotification);
        solo->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xfffbc02d));
        solo->onClick = [track, solo] { track->setSolo (solo->getToggleState()); };
        mixerView.addAndMakeVisible (solo);

        // Arm
        auto* arm = mixerArmButtons.add (new juce::TextButton ("A"));
        arm->setClickingTogglesState (true);
        arm->setToggleState (track->getArmed(), juce::dontSendNotification);
        arm->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffea4335));
        arm->onClick = [this, track, arm] { 
            track->setArmed (arm->getToggleState()); 
            audioEngine.updateRouting();
        };
        if (isMaster) arm->setEnabled(false);
        mixerView.addAndMakeVisible (arm);

        // Monitor
        auto* mon = mixerMonitorButtons.add (new juce::TextButton ("Mon"));
        mon->setClickingTogglesState (true);
        mon->setToggleState (track->getMonitor(), juce::dontSendNotification);
        mon->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff00b4d8));
        mon->onClick = [this, track, mon] {
            track->setMonitor (mon->getToggleState());
            audioEngine.updateRouting();
        };
        if (isMaster) mon->setEnabled(false);
        mixerView.addAndMakeVisible (mon);

        // Input
        auto* inputCb = mixerInputComboBoxes.add (new juce::ComboBox());
        if (isMaster)
        {
            inputCb->addItem ("Stereo In", 1);
            inputCb->setSelectedId (1, juce::dontSendNotification);
            inputCb->setEnabled (false);
        }
        else if (track->getTrackType() == Track::Type::Midi)
        {
            inputCb->addItem ("MIDI Omni", 1);
            for (int ch = 1; ch <= 16; ++ch)
            {
                inputCb->addItem ("MIDI Ch " + juce::String (ch), ch + 1);
            }
            inputCb->setSelectedId (track->getMidiChannel() + 1, juce::dontSendNotification);
            inputCb->onChange = [this, track, inputCb] {
                track->setMidiChannel (inputCb->getSelectedId() - 1);
                
                // Keep track name synchronized
                juce::String nameBase = track->getName();
                int idx = nameBase.indexOf (" (");
                if (idx != -1)
                    nameBase = nameBase.substring (0, idx);
                juce::String chStr = (track->getMidiChannel() == 0) ? "Omni" : ("Ch " + juce::String (track->getMidiChannel()));
                track->setName (nameBase + " (" + chStr + ")");
                
                updateTracksUI();
                repaint();
            };
        }
        else
        {
            inputCb->addItem ("Stereo In", 1);
            inputCb->addItem ("Input 1", 2);
            inputCb->addItem ("Input 2", 3);
            inputCb->setSelectedId (track->getInputChannel() == -1 ? 1 : track->getInputChannel() + 2, juce::dontSendNotification);
            inputCb->onChange = [this, track, inputCb] {
                track->setInputChannel (inputCb->getSelectedId() - 2);
                audioEngine.updateRouting();
            };
        }
        mixerView.addAndMakeVisible (inputCb);

        // Routing
        auto* routing = mixerRoutingComboBoxes.add (new juce::ComboBox());
        routing->addItem ("Master", 1);
        for (int j = 0; j < audioEngine.getNumTracks(); ++j)
        {
            if (!isMaster && i != j) routing->addItem (audioEngine.getTrack(j)->getName(), j + 2);
        }
        routing->setSelectedId (track->getRouting() == -1 ? 1 : track->getRouting() + 2, juce::dontSendNotification);
        if (isMaster) routing->setEnabled(false);
        else
        {
            routing->onChange = [this, track, routing] {
                track->setRouting(routing->getSelectedId() - 2);
                audioEngine.updateRouting();
            };
        }
        mixerView.addAndMakeVisible(routing);

        auto* slider = mixerSliders.add (new juce::Slider());
        slider->setSliderStyle (juce::Slider::LinearVertical);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 20);
        slider->setRange (0.0, 2.0, 0.01);
        slider->setValue (track->getVolume());
        
        slider->onValueChange = [track, slider] { track->setVolume ((float)slider->getValue()); };
        
        mixerView.addAndMakeVisible (slider);

        auto* meter = mixerMeters.add (new VuMeterComponent());
        mixerView.addAndMakeVisible (meter);

        for (int j = 0; j < 8; ++j)
        {
            auto* plugin = track->getPlugin(j);
            auto* fxBtn = mixerFxSlots.add (new juce::TextButton());
            fxBtn->setButtonText(plugin != nullptr ? plugin->getName() : "+");
            
            fxBtn->setColour(juce::TextButton::buttonColourId, juce::Colour (0xff242428));
            fxBtn->setColour(juce::TextButton::textColourOffId, juce::Colour (0xffe2e2e6));
            
            fxBtn->onClick = [this, fxBtn, j, plugin, isMaster, i]() {
                if (plugin != nullptr) {
                    new PluginWindow(plugin);
                } else {
                    int tIdx = isMaster ? audioEngine.getNumTracks() : i;
                    
                    juce::PopupMenu m;
                    m.addItem(1, "Browse File...");
                    m.addSeparator();
                    
                    for (int k = 0; k < systemPluginFiles.size(); ++k)
                    {
                        m.addItem(k + 2, systemPluginFiles[k].getFileNameWithoutExtension());
                    }
                    
                    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent(fxBtn),
                        [this, tIdx, j](int result) {
                            if (result == 1)
                            {
                                loadVstIntoSlot(tIdx, j);
                            }
                            else if (result > 1)
                            {
                                loadPluginFileIntoSlot(systemPluginFiles[result - 2], tIdx, j);
                            }
                        });
                }
            };
            mixerView.addAndMakeVisible(fxBtn);

            auto* rmBtn = mixerFxRemoveButtons.add (new juce::TextButton("X"));
            rmBtn->setColour(juce::TextButton::buttonColourId, juce::Colour (0xffd90429));
            rmBtn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            rmBtn->setVisible(plugin != nullptr);
            rmBtn->onClick = [this, track, j]() {
                track->setPlugin(j, nullptr);
                updateTracksUI();
            };
            mixerView.addAndMakeVisible(rmBtn);
        }

        auto* label = mixerLabels.add (new juce::Label (track->getName(), track->getName()));
        label->setJustificationType (juce::Justification::centred);
        if (isMaster) label->setEditable(false, false, false);
        else
        {
            label->setEditable (false, true, false);
            label->onTextChange = [this, track, label] { 
                track->setName (label->getText()); 
                arrangementView.repaint(); 
                updateTracksUI(); // Update routing dropdowns
            };
        }
        mixerView.addAndMakeVisible (label);
    }
    
    resized();
    arrangementView.updateTrackHeaders();
}

void MainComponent::scanSystemPlugins()
{
    systemPluginFiles.clear();
    
    juce::Array<juce::File> searchPaths = {
        juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".vst3"),
        juce::File("/usr/lib/vst3"),
        juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".clap"),
        juce::File("/usr/lib/clap")
    };
    
    for (const auto& dir : searchPaths)
    {
        if (dir.isDirectory())
        {
            juce::Array<juce::File> filesVst3;
            dir.findChildFiles(filesVst3, juce::File::findFilesAndDirectories, true, "*.vst3");
            for (const auto& f : filesVst3) systemPluginFiles.add(f);

            juce::Array<juce::File> filesClap;
            dir.findChildFiles(filesClap, juce::File::findFilesAndDirectories, true, "*.clap");
            for (const auto& f : filesClap) systemPluginFiles.add(f);
            
            juce::Array<juce::File> filesSo;
            dir.findChildFiles(filesSo, juce::File::findFilesAndDirectories, true, "*.so");
            for (const auto& f : filesSo) systemPluginFiles.add(f);
        }
    }
}

void MainComponent::loadWav(bool atPlayhead)
{
    if (audioEngine.getNumTracks() == 0) return;
    
    fileChooser = std::make_unique<juce::FileChooser>("Select WAV file", juce::File{}, "*.wav");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, atPlayhead] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                auto item = std::make_unique<Item>();
                if (item->loadFile (file, audioEngine.getFormatManager(), audioEngine.getCurrentSampleRate()))
                {
                    double startTime = atPlayhead ? audioEngine.getPlayPosition() : 0.0;
                    item->setStartTime (startTime);
                    audioEngine.getTrack(0)->addItem (std::move (item));
                    arrangementView.updateItems();
                    repaint();
                }
            }
        });
}

void MainComponent::loadWavToSelectedTrack(bool atPlayhead)
{
    if (audioEngine.getNumTracks() == 0) return;
    
    int trackIdx = arrangementView.selectedTrackIdx;
    if (trackIdx < 0 || trackIdx >= audioEngine.getNumTracks())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, 
            "Load WAV", 
            "Please select a track first by clicking on its header.");
        return;
    }

    fileChooser = std::make_unique<juce::FileChooser>("Select WAV file", juce::File{}, "*.wav");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, trackIdx, atPlayhead] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                auto item = std::make_unique<Item>();
                if (item->loadFile (file, audioEngine.getFormatManager(), audioEngine.getCurrentSampleRate()))
                {
                    double startTime = atPlayhead ? audioEngine.getPlayPosition() : 0.0;
                    item->setStartTime (startTime);
                    
                    auto* track = audioEngine.getTrack (trackIdx);
                    if (track != nullptr)
                    {
                        track->addItem (std::move (item));
                        arrangementView.updateItems();
                        repaint();
                    }
                }
            }
        });
}

void MainComponent::loadVst()
{
    loadVstIntoSlot(0, 0); // default to track 0 slot 0 for backwards compatibility
}

void MainComponent::loadVstIntoSlot(int trackIdx, int slotIdx)
{
    if (audioEngine.getNumTracks() == 0 && trackIdx != audioEngine.getNumTracks()) return;

    fileChooser = std::make_unique<juce::FileChooser>("Select Plugin", juce::File{}, "*.vst3;*.clap");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | 
                              juce::FileBrowserComponent::canSelectFiles | 
                              juce::FileBrowserComponent::canSelectDirectories,
        [this, trackIdx, slotIdx] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            loadPluginFileIntoSlot(file, trackIdx, slotIdx);
        });
}

void MainComponent::loadPluginFileIntoSlot(const juce::File& file, int trackIdx, int slotIdx)
{
    std::cout << "Attempting to load plugin from path: " << file.getFullPathName() << std::endl;
    
    if (file.existsAsFile() || file.isDirectory())
    {
        juce::String formatName = file.hasFileExtension(".clap") ? "CLAP" : "VST3";
        juce::AudioPluginFormat* format = nullptr;
        for (int i = 0; i < audioEngine.getPluginFormatManager().getNumFormats(); ++i)
        {
            if (auto* f = audioEngine.getPluginFormatManager().getFormat(i))
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
            format->findAllTypesForFile (typesFound, file.getFullPathName());
            
            if (typesFound.size() > 0)
            {
                auto* audioDevice = audioEngine.getDeviceManager().getCurrentAudioDevice();
                double sampleRate = audioDevice ? audioDevice->getCurrentSampleRate() : 44100.0;
                int bufferSize = audioDevice ? audioDevice->getDefaultBufferSize() : 512;

                juce::String error;
                auto plugin = audioEngine.getPluginFormatManager().createPluginInstance (
                    *typesFound[0], sampleRate, bufferSize, error);

                if (plugin != nullptr)
                {
                    auto* p = plugin.get();
                    bool isMaster = (trackIdx == audioEngine.getNumTracks());
                    auto* track = isMaster ? audioEngine.getMasterTrack() : audioEngine.getTrack(trackIdx);
                    if (track)
                    {
                        track->setPlugin (slotIdx, std::move (plugin), sampleRate, bufferSize);
                        updateTracksUI();
                        new PluginWindow (p);
                    }
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Plugin Load Error",
                                                           "Failed to load plugin: " + error);
                }
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Plugin Error",
                                                       "No plugins found in file: " + file.getFileName());
            }
        }
    }
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121214));

    auto tbBounds = topBar.getBounds();
    g.setColour (juce::Colour (0xff1c1c20));
    g.fillRect (tbBounds);

    g.setColour (juce::Colour (0xff2a2a30));
    g.drawHorizontalLine (tbBounds.getBottom() - 1, 0.0f, (float)getWidth());
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    menuBar.setBounds(bounds.removeFromTop(24));
    topBar.setBounds (bounds.removeFromTop (50));
    if (isMixerDocked && isMixerVisible)
    {
        auto mixerBounds = bounds.removeFromBottom (dockedMixerHeight);
        auto resizerBounds = mixerBounds.removeFromTop (6);
        
        mixerResizer.setBounds (resizerBounds);
        mixerResizer.setVisible (true);
        
        mixerViewport.setBounds (mixerBounds);
        mixerViewport.setVisible (true);
    }
    else
    {
        mixerResizer.setVisible (false);
        if (isMixerDocked)
            mixerViewport.setVisible (false);
    }
    arrangementView.setBounds (bounds);

    // Layout Top Bar (Transport Only)
    auto tb = topBar.getLocalBounds().reduced (10);
    playButton.setBounds (tb.removeFromLeft (40));
    tb.removeFromLeft (5);
    pauseButton.setBounds (tb.removeFromLeft (40));
    tb.removeFromLeft (5);
    stopButton.setBounds (tb.removeFromLeft (40));
    tb.removeFromLeft (5);
    recordButton.setBounds (tb.removeFromLeft (40));
    tb.removeFromLeft (5);
    loopButton.setBounds (tb.removeFromLeft (40));
    tb.removeFromLeft (20);
    autoFadeButton.setBounds (tb.removeFromLeft (100));
    tb.removeFromLeft (20);
    metronomeButton.setBounds (tb.removeFromLeft (40));
    tb.removeFromLeft (15);
    timecodeDisplay.setBounds (tb.removeFromLeft (180));
    tb.removeFromLeft (15);
    
    selectToolBtn.setBounds (tb.removeFromLeft (40));
    tb.removeFromLeft (5);
    splitToolBtn.setBounds (tb.removeFromLeft (40));
    tb.removeFromLeft (5);
    eraserToolBtn.setBounds (tb.removeFromLeft (40));
    tb.removeFromLeft (5);
    muteToolBtn.setBounds (tb.removeFromLeft (40));
    tb.removeFromLeft (15);
    
    undoBtn.setBounds (tb.removeFromLeft (40));
    tb.removeFromLeft (5);
    redoBtn.setBounds (tb.removeFromLeft (40));
    tb.removeFromLeft (15);

    bpmLabel.setBounds (tb.removeFromLeft (40));
    bpmSlider.setBounds (tb.removeFromLeft (120));
    tb.removeFromLeft (15);
    snapLabel.setBounds (tb.removeFromLeft (45));
    snapComboBox.setBounds (tb.removeFromLeft (90));
    
    // Hide other buttons
    loadWavButton.setVisible(false);
    loadVstButton.setVisible(false);
    spliceButton.setVisible(false);
    addMidiButton.setVisible(false);
    showMidiEditorButton.setVisible(false);
    addTrackButton.setVisible(false);
    addMidiTrackButton.setVisible(false);

    layoutMixer();
}

void MainComponent::layoutMixer()
{
    if (isMixerDocked)
    {
        if (!isMixerVisible) return;
    }
    else
    {
        if (!mixerWindow || !mixerWindow->isVisible()) return;
    }
    
    // Layout Mixer
    int totalMixerWidth = 80 + mixerSliders.size() * 105;
    mixerView.setSize (totalMixerWidth, mixerViewport.getHeight() - 16);
    
    auto mb = mixerView.getLocalBounds().reduced (10);
    mb.removeFromLeft(80); // Space for MIXER label
    
    int stripWidth = 100;
    
    // Far left: Master Track (which is the last element in the arrays)
    if (!mixerSliders.isEmpty())
    {
        int masterIdx = mixerSliders.size() - 1;
        auto strip = mb.removeFromLeft (stripWidth);
        
        mixerLabels[masterIdx]->setBounds (strip.removeFromBottom (20));
        mixerRoutingComboBoxes[masterIdx]->setBounds (strip.removeFromBottom (24).reduced(2));
        mixerInputComboBoxes[masterIdx]->setBounds (strip.removeFromBottom (24).reduced(2));
        mixerPanSliders[masterIdx]->setBounds (strip.removeFromTop (20).reduced(5, 2));
        
        auto msStrip = strip.removeFromTop (20);
        int btnW = stripWidth / 4;
        mixerMuteButtons[masterIdx]->setBounds (msStrip.removeFromLeft (btnW).reduced(2));
        mixerSoloButtons[masterIdx]->setBounds (msStrip.removeFromLeft (btnW).reduced(2));
        mixerArmButtons[masterIdx]->setBounds (msStrip.removeFromLeft (btnW).reduced(2));
        mixerMonitorButtons[masterIdx]->setBounds (msStrip.reduced(2));
        
        for (int j = 0; j < 8; ++j)
        {
            auto slotBounds = strip.removeFromTop(18).reduced(2, 1);
            if (auto* btn = mixerFxSlots[masterIdx * 8 + j])
            {
                if (mixerFxRemoveButtons[masterIdx * 8 + j]->isVisible())
                {
                    auto rmBounds = slotBounds.removeFromRight(16);
                    mixerFxRemoveButtons[masterIdx * 8 + j]->setBounds(rmBounds);
                    slotBounds.removeFromRight(2);
                }
                btn->setBounds (slotBounds);
            }
        }
        
        auto meterStrip = strip.removeFromRight (15);
        strip.removeFromRight (5);
        mixerSliders[masterIdx]->setBounds (strip.reduced (5, 0));
        mixerMeters[masterIdx]->setBounds (meterStrip.withTrimmedBottom(25).withTrimmedTop(5));
        
        mb.removeFromLeft (5); // Gap after master
    }
    
    // Then rest of tracks
    for (int i = 0; i < mixerSliders.size() - 1; ++i)
    {
        auto strip = mb.removeFromLeft (stripWidth);
        mixerLabels[i]->setBounds (strip.removeFromBottom (20));
        mixerRoutingComboBoxes[i]->setBounds (strip.removeFromBottom (24).reduced(2));
        mixerInputComboBoxes[i]->setBounds (strip.removeFromBottom (24).reduced(2));
        
        // Pan
        mixerPanSliders[i]->setBounds (strip.removeFromTop (20).reduced(5, 2));
        
        // Mute / Solo / Arm / Mon
        auto msStrip = strip.removeFromTop (20);
        int btnW = stripWidth / 4;
        mixerMuteButtons[i]->setBounds (msStrip.removeFromLeft (btnW).reduced(2));
        mixerSoloButtons[i]->setBounds (msStrip.removeFromLeft (btnW).reduced(2));
        mixerArmButtons[i]->setBounds (msStrip.removeFromLeft (btnW).reduced(2));
        mixerMonitorButtons[i]->setBounds (msStrip.reduced(2));
        
        // FX Slots
        for (int j = 0; j < 8; ++j)
        {
            auto slotBounds = strip.removeFromTop(18).reduced(2, 1);
            if (auto* btn = mixerFxSlots[i * 8 + j])
            {
                if (mixerFxRemoveButtons[i * 8 + j]->isVisible())
                {
                    auto rmBounds = slotBounds.removeFromRight(16);
                    mixerFxRemoveButtons[i * 8 + j]->setBounds(rmBounds);
                    slotBounds.removeFromRight(2); // padding
                }
                btn->setBounds (slotBounds);
            }
        }
        
        auto meterStrip = strip.removeFromRight (15);
        strip.removeFromRight (5); // padding between slider and meter
        
        mixerSliders[i]->setBounds (strip.reduced (5, 0));
        
        // Account for slider text box below when placing meter
        mixerMeters[i]->setBounds (meterStrip.withTrimmedBottom(25).withTrimmedTop(5));
        
        mb.removeFromLeft (5); // gap
    }
}

void MainComponent::timerCallback()
{
    for (int i = 0; i < mixerMeters.size(); ++i)
    {
        bool isMaster = (i == audioEngine.getNumTracks());
        auto* track = isMaster ? audioEngine.getMasterTrack() : audioEngine.getTrack(i);
        if (track)
        {
            mixerMeters[i]->setLevel (track->getPeakLevel());
            mixerMeters[i]->repaint();
            
            if (i < mixerSliders.size() && mixerSliders[i] != nullptr)
                mixerSliders[i]->setValue (track->getVolume(), juce::dontSendNotification);
            if (i < mixerPanSliders.size() && mixerPanSliders[i] != nullptr)
                mixerPanSliders[i]->setValue (track->getPan(), juce::dontSendNotification);
        }
    }
    
    if (audioEngine.getPlaying() || audioEngine.getRecording())
    {
        arrangementView.updateHeaderStates();
        arrangementView.repaint();
    }
}

juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "Track", "Media", "View", "Transport", "Options" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int topLevelMenuIndex, const juce::String& menuName)
{
    juce::PopupMenu menu;
    if (topLevelMenuIndex == 0) // File
    {
        menu.addItem (14, "New Project");
        menu.addSeparator();
        menu.addItem (2, "Load Plugin...");
        menu.addSeparator();
        menu.addItem (15, "Save Project...");
        menu.addItem (16, "Open Project...");
    }
    else if (topLevelMenuIndex == 1) // Edit
    {
        menu.addItem (3, "Splice at Playhead");
        menu.addItem (13, "Auto-Fade All Audio Items");
    }
    else if (topLevelMenuIndex == 2) // Track
    {
        menu.addItem (4, "Add Audio Track");
        menu.addItem (11, "Add MIDI Track");
        menu.addItem (5, "Add MIDI Item");
    }
    else if (topLevelMenuIndex == 3) // Media
    {
        menu.addItem (1, "Load WAV...");
        menu.addItem (19, "Load WAV at Playhead...");
        menu.addSeparator();
        menu.addItem (20, "Load WAV to Selected Track...");
        menu.addItem (21, "Load WAV to Selected Track at Playhead...");
    }
    else if (topLevelMenuIndex == 4) // View
    {
        menu.addItem (6, "Show MIDI Editor");
        menu.addSeparator();
        menu.addItem (17, "Show Mixer (X)", true, isMixerVisible);
        menu.addItem (18, "Dock Mixer at Bottom", true, isMixerDocked);
    }
    else if (topLevelMenuIndex == 5) // Transport
    {
        menu.addItem (7, "Play");
        menu.addItem (8, "Stop");
        menu.addItem (9, "Record");
        menu.addItem (10, "Toggle Loop");
    }
    else if (topLevelMenuIndex == 6) // Options
    {
        menu.addItem (12, "Settings...");
    }
    return menu;
}

void MainComponent::menuItemSelected (int menuItemID, int topLevelMenuIndex)
{
    if (menuItemID == 1) loadWav(false);
    else if (menuItemID == 19) loadWav(true);
    else if (menuItemID == 20) loadWavToSelectedTrack(false);
    else if (menuItemID == 21) loadWavToSelectedTrack(true);
    else if (menuItemID == 2) loadVst();
    else if (menuItemID == 3) spliceButton.triggerClick();
    else if (menuItemID == 4) addTrackButton.triggerClick();
    else if (menuItemID == 11) addMidiTrackButton.triggerClick();
    else if (menuItemID == 5) addMidiButton.triggerClick();
    else if (menuItemID == 6) showMidiEditorButton.triggerClick();
    else if (menuItemID == 7) playButton.triggerClick();
    else if (menuItemID == 8) stopButton.triggerClick();
    else if (menuItemID == 9) recordButton.triggerClick();
    else if (menuItemID == 10) loopButton.triggerClick();
    else if (menuItemID == 12) openSettingsWindow();
    else if (menuItemID == 13) autoFadeButton.triggerClick();
    else if (menuItemID == 14) newProject();
    else if (menuItemID == 15) saveProject();
    else if (menuItemID == 16) openProject();
    else if (menuItemID == 17) setMixerVisible (!isMixerVisible);
    else if (menuItemID == 18) setMixerDocked (!isMixerDocked);
}

void MainComponent::openSettingsWindow()
{
    auto* deviceSelector = new juce::AudioDeviceSelectorComponent(
        audioEngine.getDeviceManager(),
        0, 2, 0, 2, true, true, true, false);

    deviceSelector->setSize (500, 400);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (deviceSelector);
    options.dialogTitle = "Audio/MIDI Settings";
    options.dialogBackgroundColour = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;

    options.launchAsync();
}

class NewProjectDialog : public juce::Component
{
public:
    NewProjectDialog (std::function<void (juce::String name, double bpm, double sampleRate, int numAudioTracks, int numMidiTracks)> onCreateCallback)
        : onCreate (onCreateCallback)
    {
        addAndMakeVisible (nameLabel);
        nameLabel.setText ("Project Name:", juce::dontSendNotification);
        addAndMakeVisible (nameEditor);
        nameEditor.setText ("Untitled Project");
        
        addAndMakeVisible (bpmLabel);
        bpmLabel.setText ("Tempo (BPM):", juce::dontSendNotification);
        addAndMakeVisible (bpmSlider);
        bpmSlider.setRange (40.0, 240.0, 1.0);
        bpmSlider.setValue (120.0);
        bpmSlider.setSliderStyle (juce::Slider::LinearBar);
        
        addAndMakeVisible (srLabel);
        srLabel.setText ("Sample Rate:", juce::dontSendNotification);
        addAndMakeVisible (srCombo);
        srCombo.addItem ("44100 Hz", 1);
        srCombo.addItem ("48000 Hz", 2);
        srCombo.addItem ("96000 Hz", 3);
        srCombo.setSelectedId (2); // 48000 Hz
        
        addAndMakeVisible (audioTracksLabel);
        audioTracksLabel.setText ("Audio Tracks:", juce::dontSendNotification);
        addAndMakeVisible (audioTracksSlider);
        audioTracksSlider.setRange (0, 1000, 1);
        audioTracksSlider.setValue (2);
        audioTracksSlider.setSliderStyle (juce::Slider::IncDecButtons);
        
        addAndMakeVisible (midiTracksLabel);
        midiTracksLabel.setText ("MIDI Tracks:", juce::dontSendNotification);
        addAndMakeVisible (midiTracksSlider);
        midiTracksSlider.setRange (0, 1000, 1);
        midiTracksSlider.setValue (2);
        midiTracksSlider.setSliderStyle (juce::Slider::IncDecButtons);
        
        addAndMakeVisible (createButton);
        createButton.onClick = [this] {
            double sr = 48000.0;
            if (srCombo.getSelectedId() == 1) sr = 44100.0;
            else if (srCombo.getSelectedId() == 3) sr = 96000.0;
            
            onCreate (nameEditor.getText(), bpmSlider.getValue(), sr, (int)audioTracksSlider.getValue(), (int)midiTracksSlider.getValue());
            
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState (1);
        };
        
        addAndMakeVisible (cancelButton);
        cancelButton.onClick = [this] {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState (0);
        };
        
        setSize (400, 300);
    }
    
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1c1c1f));
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced (20);
        
        int rowHeight = 24;
        int gap = 14;
        
        auto getRow = [&] (juce::Label& lbl, juce::Component& comp, int y) {
            lbl.setBounds (bounds.getX(), y, 120, rowHeight);
            comp.setBounds (bounds.getX() + 130, y, bounds.getWidth() - 130, rowHeight);
        };
        
        int y = bounds.getY();
        getRow (nameLabel, nameEditor, y);
        y += rowHeight + gap;
        
        getRow (bpmLabel, bpmSlider, y);
        y += rowHeight + gap;
        
        getRow (srLabel, srCombo, y);
        y += rowHeight + gap;
        
        getRow (audioTracksLabel, audioTracksSlider, y);
        y += rowHeight + gap;
        
        getRow (midiTracksLabel, midiTracksSlider, y);
        y += rowHeight + gap;
        
        createButton.setBounds (bounds.getX() + 80, y + 10, 100, 28);
        cancelButton.setBounds (bounds.getX() + 200, y + 10, 100, 28);
    }
    
private:
    juce::Label nameLabel, bpmLabel, srLabel, audioTracksLabel, midiTracksLabel;
    juce::TextEditor nameEditor;
    juce::Slider bpmSlider;
    juce::ComboBox srCombo;
    juce::Slider audioTracksSlider, midiTracksSlider;
    juce::TextButton createButton { "Create" }, cancelButton { "Cancel" };
    
    std::function<void (juce::String name, double bpm, double sampleRate, int numAudioTracks, int numMidiTracks)> onCreate;
};

void MainComponent::newProject()
{
    auto* dialog = new NewProjectDialog ([this] (juce::String name, double bpm, double sampleRate, int numAudioTracks, int numMidiTracks) {
        audioEngine.setPlaying (false);
        while (audioEngine.getNumTracks() > 0)
            audioEngine.removeTrack (0);
            
        // Configure sample rate if device supports it
        auto* device = audioEngine.getDeviceManager().getCurrentAudioDevice();
        if (device != nullptr)
        {
            auto setup = audioEngine.getDeviceManager().getAudioDeviceSetup();
            setup.sampleRate = sampleRate;
            audioEngine.getDeviceManager().setAudioDeviceSetup (setup, true);
        }
        
        audioEngine.setMetronomeEnabled (false);
        audioEngine.setSnapDivision (AudioEngine::SnapDivision::None);
        updateBpmGlobally (bpm);
        snapComboBox.setSelectedId (1, juce::dontSendNotification);
        
        // Add starting audio and MIDI tracks
        for (int i = 0; i < numAudioTracks; ++i)
            audioEngine.addTrack (Track::Type::Audio);
        for (int i = 0; i < numMidiTracks; ++i)
            audioEngine.addTrack (Track::Type::Midi);
            
        undoManager.clear();
        updateTracksUI();
        repaint();
    });
    
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (dialog);
    options.dialogTitle = "New Project";
    options.dialogBackgroundColour = juce::Colour (0xff1c1c1f);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    
    options.launchAsync();
}

void MainComponent::saveProject()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Save Project", juce::File{}, "*.lhproj");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File{})
            {
                if (auto xml = audioEngine.saveProjectToXml())
                {
                    xml->writeTo (file);
                }
            }
        });
}

void MainComponent::openProject()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Open Project", juce::File{}, "*.lhproj");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File{} && file.existsAsFile())
            {
                auto xml = juce::parseXML (file);
                if (xml != nullptr)
                {
                    if (audioEngine.loadProjectFromXml (*xml))
                    {
                        updateBpmGlobally (audioEngine.getBpm());
                        
                        int snapId = 1;
                        auto snap = audioEngine.getSnapDivision();
                        if (snap == AudioEngine::SnapDivision::Bar) snapId = 2;
                        else if (snap == AudioEngine::SnapDivision::HalfBar) snapId = 3;
                        else if (snap == AudioEngine::SnapDivision::Beat) snapId = 4;
                        else if (snap == AudioEngine::SnapDivision::HalfBeat) snapId = 5;
                        else if (snap == AudioEngine::SnapDivision::QuarterBeat) snapId = 6;
                        else if (snap == AudioEngine::SnapDivision::EighthBeat) snapId = 7;
                        snapComboBox.setSelectedId (snapId, juce::dontSendNotification);
                        
                        undoManager.clear();
                        updateTracksUI();
                        repaint();
                    }
                }
            }
        });
}

void MainComponent::undo()
{
    undoManager.undo();
}

void MainComponent::redo()
{
    undoManager.redo();
}

void MainComponent::setMixerDocked (bool docked)
{
    if (isMixerDocked == docked)
        return;
        
    isMixerDocked = docked;
    
    if (isMixerDocked)
    {
        if (mixerWindow != nullptr)
        {
            mixerWindow->setVisible (false);
            mixerWindow = nullptr;
        }
        
        if (isMixerVisible)
        {
            addAndMakeVisible (mixerViewport);
        }
    }
    else
    {
        removeChildComponent (&mixerViewport);
        
        if (isMixerVisible)
        {
            mixerWindow = std::make_unique<MixerWindow>("Layerhost Mixer", &mixerViewport, this);
            mixerWindow->setVisible (true);
        }
    }
    
    resized();
    layoutMixer();
}

void MainComponent::setMixerVisible (bool visible)
{
    if (isMixerVisible == visible)
        return;
        
    isMixerVisible = visible;
    
    if (isMixerDocked)
    {
        mixerViewport.setVisible (isMixerVisible);
    }
    else
    {
        if (isMixerVisible)
        {
            if (mixerWindow == nullptr)
                mixerWindow = std::make_unique<MixerWindow>("Layerhost Mixer", &mixerViewport, this);
            mixerWindow->setVisible (true);
        }
        else
        {
            if (mixerWindow != nullptr)
                mixerWindow->setVisible (false);
        }
    }
    
    resized();
    layoutMixer();
}

void MainComponent::updateBpmGlobally (double newBpm)
{
    audioEngine.setBpm (newBpm);
    bpmSlider.setValue (newBpm, juce::dontSendNotification);
    arrangementView.repaint();
    PianoRollWindow::updateAllOpenWindows();
    repaint();
}

MainComponent::MixerResizer::MixerResizer (MainComponent& owner) : mainComp (owner)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void MainComponent::MixerResizer::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1c20));
    g.setColour (juce::Colour (0xff2c2c34));
    g.drawHorizontalLine (0, 0.0f, (float)getWidth());
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float)getWidth());
    
    g.setColour (juce::Colour (0xff8e8e93).withAlpha (0.4f));
    int cy = getHeight() / 2;
    int cx = getWidth() / 2;
    g.drawHorizontalLine (cy, (float)cx - 15.0f, (float)cx + 15.0f);
}

void MainComponent::MixerResizer::mouseDown (const juce::MouseEvent& e)
{
    initialHeight = mainComp.dockedMixerHeight;
}

void MainComponent::MixerResizer::mouseDrag (const juce::MouseEvent& e)
{
    int deltaY = e.getDistanceFromDragStartY();
    int newHeight = initialHeight - deltaY;
    int maxHeight = (int)(mainComp.getHeight() * 0.7f);
    mainComp.dockedMixerHeight = juce::jlimit (100, juce::jmax (100, maxHeight), newHeight);
    mainComp.resized();
}

