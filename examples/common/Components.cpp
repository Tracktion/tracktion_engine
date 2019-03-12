/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

#pragma once

namespace te = tracktion_engine;

//==============================================================================
ClipComponent::ClipComponent (EditViewState& evs, te::Clip::Ptr c)
    : editViewState (evs), clip (c)
{
}

void ClipComponent::paint (Graphics& g)
{
    g.fillAll (clip->getColour().withAlpha (0.5f));
    g.setColour (Colours::black);
    g.drawRect (getLocalBounds());
    
    if (editViewState.selectionManager.isSelected (clip.get()))
    {
        g.setColour (Colours::red);
        g.drawRect (getLocalBounds(), 2);
    }
}

void ClipComponent::mouseDown (const MouseEvent&)
{
    editViewState.selectionManager.selectOnly (clip.get());
}

//==============================================================================
AudioClipComponent::AudioClipComponent (EditViewState& evs, te::Clip::Ptr c)
    : ClipComponent (evs, c)
{
    updateThumbnail();
}

void AudioClipComponent::paint (Graphics& g)
{
    ClipComponent::paint (g);
    
    if (editViewState.drawWaveforms && thumbnail != nullptr)
        drawWaveform (g, *getWaveAudioClip(), *thumbnail, Colours::black.withAlpha (0.5f),
                      0, getWidth(), 0, getHeight(), 0, false);
}

void AudioClipComponent::drawWaveform (Graphics& g, te::AudioClipBase& c, te::SmartThumbnail& thumb, Colour colour,
                                       int left, int right, int y, int h, int xOffset, bool drawLoopMarkers)
{
    auto getTimeRangeForDrawing = [this] (const int left, const int right) -> te::EditTimeRange
    {
        if (auto p = getParentComponent())
        {
            double t1 = editViewState.xToTime (left, p->getWidth());
            double t2 = editViewState.xToTime (right, p->getWidth());
            
            return { t1, t2 };
        }
        
        return {};
    };
    
    jassert (left <= right);
    const auto gain = c.getGain();
    const auto pan = thumb.getNumChannels() == 1 ? 0.0f : c.getPan();
    
    const float pv = pan * gain;
    const float gainL = (gain - pv);
    const float gainR = (gain + pv);
    
    const bool usesTimeStretchedProxy = c.usesTimeStretchedProxy();
    
    const auto clipPos = c.getPosition();
    auto offset = clipPos.getOffset();
    auto speedRatio = c.getSpeedRatio();
    
    g.setColour (colour);
    
    if (usesTimeStretchedProxy)
    {
        const Rectangle<int> area (left + xOffset, y, right - left, h);
        
        if (! thumb.isOutOfDate())
        {
            drawChannels (g, thumb, area, false,
                          getTimeRangeForDrawing (left, right),
                          c.isLeftChannelActive(), c.isRightChannelActive(),
                          gainL, gainR);
        }
    }
    else if (c.getLoopLength() == 0)
    {
        auto region = getTimeRangeForDrawing (left, right);
        
        auto t1 = (region.getStart() + offset) * speedRatio;
        auto t2 = (region.getEnd()   + offset) * speedRatio;
        
        drawChannels (g, thumb,
                      { left + xOffset, y, right - left, h },
                      false, { t1, t2 },
                      c.isLeftChannelActive(), c.isRightChannelActive(),
                      gainL, gainR);
    }
}

void AudioClipComponent::drawChannels (Graphics& g, te::SmartThumbnail& thumb, Rectangle<int> area, bool useHighRes,
                                       te::EditTimeRange time, bool useLeft, bool useRight,
                                       float leftGain, float rightGain)
{
    if (useLeft && useRight && thumb.getNumChannels() > 1)
    {
        thumb.drawChannel (g, area.removeFromTop (area.getHeight() / 2), useHighRes, time, 0, leftGain);
        thumb.drawChannel (g, area, useHighRes, time, 1, rightGain);
    }
    else if (useLeft)
    {
        thumb.drawChannel (g, area, useHighRes, time, 0, leftGain);
    }
    else if (useRight)
    {
        thumb.drawChannel (g, area, useHighRes, time, 1, rightGain);
    }
}

void AudioClipComponent::updateThumbnail()
{
    if (auto* wac = getWaveAudioClip())
    {
        te::AudioFile af (wac->getAudioFile());
        
        if (af.getFile().existsAsFile() || (! wac->usesSourceFile()))
        {
            if (af.isValid())
            {
                const te::AudioFile proxy ((wac->hasAnyTakes() && wac->isShowingTakes()) ? wac->getAudioFile() : wac->getPlaybackFile());
                
                if (thumbnail == nullptr)
                    thumbnail = std::make_unique<te::SmartThumbnail> (wac->edit.engine, proxy, *this, &wac->edit);
                else
                    thumbnail->setNewFile (proxy);
            }
            else
            {
                thumbnail = nullptr;
            }
        }
    }
}

//==============================================================================
RecordingClipComponent::RecordingClipComponent (te::Track::Ptr t, EditViewState& evs)
    : track (t), editViewState (evs)
{
    startTimerHz (10);
    initialiseThumbnailAndPunchTime();
}

void RecordingClipComponent::initialiseThumbnailAndPunchTime()
{
    if (auto at = dynamic_cast<te::AudioTrack*> (track.get()))
    {
        for (auto* idi : at->edit.getEditInputDevices().getDevicesForTargetTrack (*at))
        {
            punchInTime = idi->getPunchInTime();
            
            if (idi->getRecordingFile().exists())
                thumbnail = at->edit.engine.getRecordingThumbnailManager().getThumbnailFor (idi->getRecordingFile());
        }
    }
}

void RecordingClipComponent::paint (Graphics& g)
{
    g.fillAll (Colours::red.withAlpha (0.5f));
    g.setColour (Colours::black);
    g.drawRect (getLocalBounds());
    
    if (editViewState.drawWaveforms)
        drawThumbnail (g, Colours::black.withAlpha (0.5f));
}

void RecordingClipComponent::drawThumbnail (Graphics& g, Colour waveformColour) const
{
    if (thumbnail == nullptr)
        return;
    
    Rectangle<int> bounds;
    Range<double> times;
    getBoundsAndTime (bounds, times);
    auto w = bounds.getWidth();
    
    if (w > 0 && w < 10000)
    {
        g.setColour (waveformColour);
        thumbnail->thumb.drawChannels (g, bounds, w, times, 1.0f);
    }
}

bool RecordingClipComponent::getBoundsAndTime (Rectangle<int>& bounds, Range<double>& times) const
{
    auto editTimeToX = [this] (double t)
    {
        if (auto p = getParentComponent())
            return editViewState.timeToX (t, p->getWidth()) - getX();
        return 0;
    };
    
    auto xToEditTime = [this] (int x)
    {
        if (auto p = getParentComponent())
            return editViewState.xToTime (x + getX(), p->getWidth());
        return 0.0;
    };
    
    bool hasLooped = false;
    auto& edit = track->edit;
    
    if (auto* playhead = edit.getTransport().getCurrentPlayhead())
    {
        auto localBounds = getLocalBounds();
        
        auto timeStarted = thumbnail->punchInTime;
        auto unloopedPos = timeStarted + thumbnail->thumb.getTotalLength();
        
        auto t1 = timeStarted;
        auto t2 = unloopedPos;
        
        if (playhead->isLooping() && t2 >= playhead->getLoopTimes().end)
        {
            hasLooped = true;
            
            t1 = jmin (t1, playhead->getLoopTimes().start);
            t2 = playhead->getPosition();
            
            t1 = jmax (editViewState.viewX1.get(), t1);
            t2 = jmin (editViewState.viewX2.get(), t2);
        }
        else if (edit.recordingPunchInOut)
        {
            const double in  = thumbnail->punchInTime;
            const double out = edit.getTransport().getLoopRange().getEnd();
            
            t1 = jlimit (in, out, t1);
            t2 = jlimit (in, out, t2);
        }
        
        bounds = localBounds.withX (jmax (localBounds.getX(), editTimeToX (t1)))
                 .withRight (jmin (localBounds.getRight(), editTimeToX (t2)));
        
        auto loopRange = playhead->getLoopTimes();
        const double recordedTime = unloopedPos - playhead->getLoopTimes().start;
        const int numLoops = (int) (recordedTime / loopRange.getLength());
        
        const Range<double> editTimes (xToEditTime (bounds.getX()),
                                       xToEditTime (bounds.getRight()));
        
        times = (editTimes + (numLoops * loopRange.getLength())) - timeStarted;
    }
    
    return hasLooped;
}

void RecordingClipComponent::timerCallback()
{
    updatePosition();
}

void RecordingClipComponent::updatePosition()
{
    auto& edit = track->edit;
    
    if (auto playhead = edit.getTransport().getCurrentPlayhead())
    {
        double t1 = punchInTime >= 0 ? punchInTime : edit.getTransport().getTimeWhenStarted();
        double t2 = jmax (t1, playhead->getUnloopedPosition());
        
        if (playhead->isLooping())
        {
            auto loopTimes = playhead->getLoopTimes();
            
            if (t2 >= loopTimes.end)
            {
                t1 = jmin (t1, loopTimes.start);
                t2 = loopTimes.end;
            }
        }
        else if (edit.recordingPunchInOut)
        {
            auto mr = edit.getTransport().getLoopRange();
            auto in  = mr.getStart();
            auto out = mr.getEnd();
            
            t1 = jlimit (in, out, t1);
            t2 = jlimit (in, out, t2);
        }
        
        t1 = jmax (t1, editViewState.viewX1.get());
        t2 = jmin (t2, editViewState.viewX2.get());
    
        if (auto p = getParentComponent())
        {
            int x1 = editViewState.timeToX (t1, p->getWidth());
            int x2 = editViewState.timeToX (t2, p->getWidth());
            
            setBounds (x1, 0, x2 - x1, p->getHeight());
            return;
        }
    }
    
    setBounds ({});
}

//==============================================================================
TrackHeaderComponent::TrackHeaderComponent (EditViewState& evs, te::Track::Ptr t)
    : editViewState (evs), track (t)
{
    Helpers::addAndMakeVisible (*this, { &trackName, &armButton, &muteButton, &soloButton, &inputButton });
    
    armButton.setColour (TextButton::buttonOnColourId, Colours::red);
    muteButton.setColour (TextButton::buttonOnColourId, Colours::red);
    soloButton.setColour (TextButton::buttonOnColourId, Colours::green);

    trackName.setText (t->getName(), dontSendNotification);
    
    if (auto at = dynamic_cast<te::AudioTrack*> (track.get()))
    {
        inputButton.onClick = [this, at]
        {
            PopupMenu m;
            
            if (EngineHelpers::trackHasInput (*at))
            {
                bool ticked = EngineHelpers::isInputMonitoringEnabled (*at);
                m.addItem (1000, "Input Monitoring", true, ticked);
                m.addSeparator();
            }
            
            int id = 1;
            for (auto instance : at->edit.getAllInputDevices())
            {
                if (instance->getInputDevice().getDeviceType() == te::InputDevice::waveDevice)
                {
                    bool ticked = instance->getTargetTrack() == at;
                    m.addItem (id++, instance->getInputDevice().getName(), true, ticked);
                }
            }
            
            int res = m.show();

            if (res == 1000)
            {
                EngineHelpers::enableInputMonitoring (*at, ! EngineHelpers::isInputMonitoringEnabled (*at));
            }
            else if (res > 0)
            {
                id = 1;
                for (auto instance : at->edit.getAllInputDevices())
                {
                    if (instance->getInputDevice().getDeviceType() == te::InputDevice::waveDevice)
                    {
                        if (id == res)
                            instance->setTargetTrack (at, 0);
                        id++;
                    }
                }
            }
        };
        armButton.onClick = [this, at]
        {
            EngineHelpers::armTrack (*at, ! EngineHelpers::isTrackArmed (*at));
            armButton.setToggleState (EngineHelpers::isTrackArmed (*at), dontSendNotification);
        };
        muteButton.onClick = [at] { at->setMute (! at->isMuted (false)); };
        soloButton.onClick = [at] { at->setSolo (! at->isSolo (false)); };
        
        armButton.setToggleState (EngineHelpers::isTrackArmed (*at), dontSendNotification);
    }
    else
    {
        armButton.setVisible (false);
        muteButton.setVisible (false);
        soloButton.setVisible (false);
    }
    
    track->state.addListener (this);
    inputsState = track->edit.state.getChildWithName (te::IDs::INPUTDEVICES);
    inputsState.addListener (this);
    
    valueTreePropertyChanged (track->state, te::IDs::mute);
    valueTreePropertyChanged (track->state, te::IDs::solo);
    valueTreePropertyChanged (inputsState, te::IDs::targetIndex);
}

TrackHeaderComponent::~TrackHeaderComponent()
{
    track->state.removeListener (this);
}

void TrackHeaderComponent::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
{
    if (te::TrackList::isTrack (v))
    {
        if (i == te::IDs::mute)
            muteButton.setToggleState ((bool)v[i], dontSendNotification);
        else if (i == te::IDs::solo)
            soloButton.setToggleState ((bool)v[i], dontSendNotification);
    }
    else if (v.hasType (te::IDs::INPUTDEVICES) || v.hasType (te::IDs::INPUTDEVICE))
    {
        if (auto at = dynamic_cast<te::AudioTrack*> (track.get()))
        {
            armButton.setEnabled (EngineHelpers::trackHasInput (*at));
            armButton.setToggleState (EngineHelpers::isTrackArmed (*at), dontSendNotification);
        }
    }
}

void TrackHeaderComponent::paint (Graphics& g)
{
    g.setColour (Colours::grey);
    g.fillRect (getLocalBounds().withTrimmedRight (2));
    
    if (editViewState.selectionManager.isSelected (track.get()))
    {
        g.setColour (Colours::red);
        g.drawRect (getLocalBounds().withTrimmedRight (-4), 2);
    }
}

void TrackHeaderComponent::mouseDown (const MouseEvent&)
{
    editViewState.selectionManager.selectOnly (track.get());
}

void TrackHeaderComponent::resized()
{
    auto r = getLocalBounds().reduced (4);
    trackName.setBounds (r.removeFromTop (r.getHeight() / 2));
    
    int w = r.getHeight();
    inputButton.setBounds (r.removeFromLeft (w));
    r.removeFromLeft (2);
    armButton.setBounds (r.removeFromLeft (w));
    r.removeFromLeft (2);
    muteButton.setBounds (r.removeFromLeft (w));
    r.removeFromLeft (2);
    soloButton.setBounds (r.removeFromLeft (w));
    r.removeFromLeft (2);
}

//==============================================================================
TrackComponent::TrackComponent (EditViewState& evs, te::Track::Ptr t)
    : editViewState (evs), track (t)
{
    track->state.addListener (this);
    track->edit.getTransport().addChangeListener (this);
    
    markAndUpdate (updateClips);
}

TrackComponent::~TrackComponent()
{
    track->state.removeListener (this);
    track->edit.getTransport().removeChangeListener (this);
}

void TrackComponent::paint (Graphics& g)
{
    g.fillAll (Colours::grey);
    
    if (editViewState.selectionManager.isSelected (track.get()))
    {
        g.setColour (Colours::red);
        g.drawRect (getLocalBounds().withTrimmedLeft (-4), 2);
    }
}

void TrackComponent::mouseDown (const MouseEvent&)
{
    editViewState.selectionManager.selectOnly (track.get());
}

void TrackComponent::changeListenerCallback (ChangeBroadcaster*)
{
    markAndUpdate (updateRecordClips);
}

void TrackComponent::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
{
    if (te::Clip::isClipState (v))
    {
        if (i == te::IDs::start
            || i == te::IDs::length)
        {
            markAndUpdate (updatePositions);
        }
    }
}

void TrackComponent::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& c)
{
    if (te::Clip::isClipState (c))
        markAndUpdate (updateClips);
}

void TrackComponent::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& c, int)
{
    if (te::Clip::isClipState (c))
        markAndUpdate (updateClips);
}

void TrackComponent::valueTreeChildOrderChanged (juce::ValueTree& v, int a, int b)
{
    if (te::Clip::isClipState (v.getChild (a)))
        markAndUpdate (updatePositions);
    else if (te::Clip::isClipState (v.getChild (b)))
        markAndUpdate (updatePositions);
}

void TrackComponent::handleAsyncUpdate()
{
    if (compareAndReset (updateClips))
        buildClips();
    if (compareAndReset (updatePositions))
        resized();
    if (compareAndReset (updateRecordClips))
        buildRecordClips();
}

void TrackComponent::resized()
{
    for (auto cc : clips)
    {
        auto& c = cc->getClip();
        auto pos = c.getPosition();
        int x1 = editViewState.timeToX (pos.getStart(), getWidth());
        int x2 = editViewState.timeToX (pos.getEnd(), getWidth());
        
        cc->setBounds (x1, 0, x2 - x1, getHeight());
    }
}

void TrackComponent::buildClips()
{
    clips.clear();
    
    if (auto ct = dynamic_cast<te::ClipTrack*> (track.get()))
    {
        for (auto c : ct->getClips())
        {
            ClipComponent* cc = nullptr;
            
            if (dynamic_cast<te::WaveAudioClip*> (c))
                cc = new AudioClipComponent (editViewState, c);
            else
                cc = new ClipComponent (editViewState, c);
            
            clips.add (cc);
            addAndMakeVisible (cc);
        }
    }
    
    resized();
}

void TrackComponent::buildRecordClips()
{
    bool needed = false;
    
    if (track->edit.getTransport().isRecording())
    {
        for (auto in : track->edit.getAllInputDevices())
        {
            if (in->isRecordingActive() && track == in->getTargetTrack())
            {
                needed = true;
                break;
            }
        }
    }
    
    if (needed)
    {
        recordingClip = std::make_unique<RecordingClipComponent> (track, editViewState);
        addAndMakeVisible (*recordingClip);
    }
    else
    {
        recordingClip = nullptr;
    }
}

//==============================================================================
PlayheadComponent::PlayheadComponent (te::Edit& e , EditViewState& evs)
    : edit (e), editViewState (evs)
{
    startTimerHz (30);
    setMouseCursor (MouseCursor::LeftRightResizeCursor);
}

void PlayheadComponent::paint (Graphics& g)
{
    g.setColour (Colours::yellow);
    g.drawRect (xPosition, 0, 2, getHeight());
}

bool PlayheadComponent::hitTest (int x, int)
{
    if (std::abs (x - xPosition) <= 3)
        return true;
    
    return false;
}

void PlayheadComponent::mouseDown (const MouseEvent&)
{
    edit.getTransport().setUserDragging (true);
}

void PlayheadComponent::mouseUp (const MouseEvent&)
{
    edit.getTransport().setUserDragging (false);
}

void PlayheadComponent::mouseDrag (const MouseEvent& e)
{
    double t = editViewState.xToTime (e.x, getWidth());
    edit.getTransport().setCurrentPosition (t);
    timerCallback();
}

void PlayheadComponent::timerCallback()
{
    int newX = editViewState.timeToX (edit.getTransport().getCurrentPosition(), getWidth());
    if (newX != xPosition)
    {
        repaint (jmin (newX, xPosition) - 1, 0, jmax (newX, xPosition) - jmin (newX, xPosition) + 3, getHeight());
        xPosition = newX;
    }
}

//==============================================================================
EditComponent::EditComponent (te::Edit& e, te::SelectionManager& sm)
    : edit (e), editViewState (e, sm)
{
    edit.state.addListener (this);
    editViewState.selectionManager.addChangeListener (this);
    
    addAndMakeVisible (playhead);
    
    markAndUpdate (updateTracks);
}

EditComponent::~EditComponent()
{
    editViewState.selectionManager.removeChangeListener (this);
    edit.state.removeListener (this);
}

void EditComponent::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
{
    if (v.hasType (IDs::EDITVIEWSTATE))
    {
        if (i == IDs::viewX1
            || i == IDs::viewX2
            || i == IDs::viewY)
        {
            markAndUpdate (updateZoom);
        }
        else if (i == IDs::drawWaveforms)
        {
            repaint();
        }
    }
}

void EditComponent::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& c)
{
    if (te::TrackList::isTrack (c))
        markAndUpdate (updateTracks);
}

void EditComponent::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& c, int)
{
    if (te::TrackList::isTrack (c))
        markAndUpdate (updateTracks);
}

void EditComponent::valueTreeChildOrderChanged (juce::ValueTree& v, int a, int b)
{
    if (te::TrackList::isTrack (v.getChild (a)))
        markAndUpdate (updateTracks);
    else if (te::TrackList::isTrack (v.getChild (b)))
        markAndUpdate (updateTracks);
}

void EditComponent::handleAsyncUpdate()
{
    if (compareAndReset (updateTracks))
        buildTracks();
    else if (compareAndReset (updateZoom))
        resized();
}

void EditComponent::resized()
{
    jassert (headers.size() == tracks.size());
    
    const int trackHeight = 50, headerWidth = 150, trackGap = 2;
    
    playhead.setBounds (getLocalBounds().withTrimmedLeft (headerWidth));
    
    int y = editViewState.viewY;
    for (int i = 0; i < jmin (headers.size(), tracks.size()); i++)
    {
        auto h = headers[i];
        auto t = tracks[i];
        
        h->setBounds (0, y, headerWidth, trackHeight);
        t->setBounds (headerWidth, y, getWidth() - headerWidth, trackHeight);
        
        y += trackHeight + trackGap;
    }
    
    for (auto t : tracks)
        t->resized();
}

void EditComponent::buildTracks()
{
    tracks.clear();
    headers.clear();
    
    for (auto t : getAllTracks (edit))
    {
        TrackComponent* c = nullptr;
        
        if (t->isTempoTrack())
        {
            if (editViewState.showGlobalTrack)
                c = new TrackComponent (editViewState, t);
        }
        else if (t->isMarkerTrack())
        {
            if (editViewState.showMarkerTrack)
                c = new TrackComponent (editViewState, t);
        }
        else if (t->isChordTrack())
        {
            if (editViewState.showChordTrack)
                c = new TrackComponent (editViewState, t);
        }
        else
        {
            c = new TrackComponent (editViewState, t);
        }
        
        if (c != nullptr)
        {
            tracks.add (c);
            addAndMakeVisible (c);
            
            auto h = new TrackHeaderComponent (editViewState, t);
            headers.add (h);
            addAndMakeVisible (h);                        
        }
    }
    
    playhead.toFront (false);
    resized();
}


