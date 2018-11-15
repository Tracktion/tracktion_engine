/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace IDs
{
    #define DECLARE_ID(name)  const juce::Identifier name (#name);

    DECLARE_ID (safeRecording)
    DECLARE_ID (discardRecordings)
    DECLARE_ID (clearDevices)
    DECLARE_ID (justSendMMCIfEnabled)
    DECLARE_ID (canSendMMCStop)
    DECLARE_ID (invertReturnToStartPosSelection)
    DECLARE_ID (allowRecordingIfNoInputsArmed)
    DECLARE_ID (clearDevicesOnStop)
    DECLARE_ID (updatingFromPlayHead)
    DECLARE_ID (scrubInterval)

    DECLARE_ID (userDragging)
    DECLARE_ID (lastUserDragTime)
    DECLARE_ID (cursorPosAtPlayStart)
    DECLARE_ID (reallocationInhibitors)
    DECLARE_ID (playbackContextAllocation)

    DECLARE_ID (rewindButtonDown)
    DECLARE_ID (fastForwardButtonDown)
    DECLARE_ID (nudgeLeftCount)
    DECLARE_ID (nudgeRightCount)

    DECLARE_ID (videoPosition)
    DECLARE_ID (forceVideoJump)

    #undef DECLARE_ID
}

namespace TransportHelpers
{
    double snapTime (TransportControl& tc, double t, bool invertSnap)
    {
        return (tc.snapToTimecode ^ invertSnap) ? tc.getSnapType().roundTimeNearest (t, tc.edit.tempoSequence)
                                                : t;
    }

    double snapTimeUp (TransportControl& tc, double t, bool invertSnap)
    {
        return (tc.snapToTimecode ^ invertSnap) ? tc.getSnapType().roundTimeUp (t, tc.edit.tempoSequence)
                                                : t;
    }

    double snapTimeDown (TransportControl& tc, double t, bool invertSnap)
    {
        return (tc.snapToTimecode ^ invertSnap) ? tc.getSnapType().roundTimeDown (t, tc.edit.tempoSequence)
                                                : t;
    }
}


//==============================================================================
/**
    Represents the state of an Edit's transport.
*/
struct TransportControl::TransportState : private ValueTree::Listener
{
    TransportState (TransportControl& tc, juce::ValueTree transportState)
        : state (transportState), transport (tc)
    {
        UndoManager* um = nullptr;

        playing.referTo (transientState, IDs::playing, um);
        recording.referTo (transientState, IDs::recording, um);
        safeRecording.referTo (transientState, IDs::safeRecording, um);

        discardRecordings.referTo (transientState, IDs::discardRecordings, um);
        clearDevices.referTo (transientState, IDs::clearDevices, um);
        justSendMMCIfEnabled.referTo (transientState, IDs::justSendMMCIfEnabled, um);
        canSendMMCStop.referTo (transientState, IDs::canSendMMCStop, um);
        invertReturnToStartPosSelection.referTo (transientState, IDs::invertReturnToStartPosSelection, um);
        allowRecordingIfNoInputsArmed.referTo (transientState, IDs::allowRecordingIfNoInputsArmed, um);
        clearDevicesOnStop.referTo (transientState, IDs::clearDevicesOnStop, um);
        updatingFromPlayHead.referTo (transientState, IDs::updatingFromPlayHead, um);

        startTime.referTo (transientState, IDs::startTime, um);
        endTime.referTo (transientState, IDs::endTime, um);
        userDragging.referTo (transientState, IDs::userDragging, um);
        lastUserDragTime.referTo (transientState, IDs::lastUserDragTime, um);
        cursorPosAtPlayStart.referTo (transientState, IDs::cursorPosAtPlayStart, um, -1000);
        reallocationInhibitors.referTo (transientState, IDs::reallocationInhibitors, um);
        playbackContextAllocation.referTo (transientState, IDs::playbackContextAllocation, um);

        rewindButtonDown.referTo (transientState, IDs::rewindButtonDown, um);
        fastForwardButtonDown.referTo (transientState, IDs::fastForwardButtonDown, um);
        nudgeLeftCount.referTo (transientState, IDs::nudgeLeftCount, um);
        nudgeRightCount.referTo (transientState, IDs::nudgeRightCount, um);

        videoPosition.referTo (transientState, IDs::videoPosition, um);
        forceVideoJump.referTo (transientState, IDs::forceVideoJump, um);

        state.addListener (this);
        transientState.addListener (this);
    }

    /** Destructor. */
    ~TransportState()
    {
        jassert (reallocationInhibitors == 0);
    }

    /** Updates the current video position, calling any listeners. */
    void setVideoPosition (double time, bool forceJump)
    {
        forceVideoJump = forceJump;
        videoPosition = time;
    }

    //==============================================================================
    /** Start playback from the current transport position. */
    void play (bool justSendMMCIfEnabled_)
    {
        justSendMMCIfEnabled = justSendMMCIfEnabled_;
        playing = true;
    }

    /** Start recording. */
    void record (bool justSendMMCIfEnabled_, bool allowRecordingIfNoInputsArmed_)
    {
        justSendMMCIfEnabled = justSendMMCIfEnabled_;
        allowRecordingIfNoInputsArmed = allowRecordingIfNoInputsArmed_;
        recording = true;
    }

    /** Stop playback/recording. */
    void stop (bool discardRecordings_,
               bool clearDevices_,
               bool canSendMMCStop_,
               bool invertReturnToStartPosSelection_)
    {
        discardRecordings = discardRecordings_;
        clearDevices = clearDevices_;
        canSendMMCStop = canSendMMCStop_;
        invertReturnToStartPosSelection = invertReturnToStartPosSelection_;
        playing = false;
    }

    void updatePositionFromPlayhead (double newPosition)
    {
        updatingFromPlayHead = true;
        state.setProperty (IDs::position, newPosition, nullptr);
        updatingFromPlayHead = false;
    }

    void nudgeLeft()
    {
        nudgeLeftCount = ((nudgeLeftCount + 1) % 2);
    }

    void nudgeRight()
    {
        nudgeRightCount = ((nudgeRightCount + 1) % 2);
    }

    //==============================================================================
    CachedValue<bool> playing, recording, safeRecording;
    CachedValue<bool> discardRecordings, clearDevices, justSendMMCIfEnabled, canSendMMCStop,
                      invertReturnToStartPosSelection, allowRecordingIfNoInputsArmed, clearDevicesOnStop;
    CachedValue<bool> userDragging, lastUserDragTime, forceVideoJump, rewindButtonDown, fastForwardButtonDown, updatingFromPlayHead;
    CachedValue<double> startTime, endTime, cursorPosAtPlayStart, videoPosition;
    CachedValue<int> reallocationInhibitors, playbackContextAllocation, nudgeLeftCount, nudgeRightCount;

    ValueTree state, transientState { IDs::TRANSPORT };
    TransportControl& transport;

private:
    void valueTreePropertyChanged (ValueTree& v, const Identifier& i) override
    {
        if (v == state)
        {
            if (i == IDs::position)
            {
                if (! updatingFromPlayHead)
                    transport.performPositionChange();
            }
            else if (i == IDs::looping)
            {
                transport.stopIfRecording();

                auto& ecm = transport.engine.getExternalControllerManager();

                if (ecm.isAttachedToEdit (transport.edit))
                    ecm.loopChanged (state[IDs::looping]);
            }
            else if (i == IDs::snapToTimecode)
            {
                auto& ecm = transport.engine.getExternalControllerManager();

                if (ecm.isAttachedToEdit (transport.edit))
                    ecm.snapChanged (state[IDs::snapToTimecode]);
            }
        }
        else if (v == transientState)
        {
            if (i == IDs::playing)
            {
                playing.forceUpdateOfCachedValue();

                if (playing)
                    transport.performPlay();
                else
                    transport.performStop();

                transport.startedOrStopped();
            }
            else if (i == IDs::recording)
            {
                recording.forceUpdateOfCachedValue();

                if (recording)
                    recording = transport.performRecord();

                transport.startedOrStopped();
            }
            else if (i == IDs::playbackContextAllocation)
            {
                transport.listeners.call (&TransportControl::Listener::playbackContextChanged);
            }
            else if (i == IDs::videoPosition)
            {
                videoPosition.forceUpdateOfCachedValue();
                transport.listeners.call (&TransportControl::Listener::setVideoPosition, videoPosition, forceVideoJump);
            }
            else if (i == IDs::rewindButtonDown)
            {
                fastForwardButtonDown = false;
                rewindButtonDown.forceUpdateOfCachedValue();
                transport.performRewindButtonChanged();
            }
            else if (i == IDs::fastForwardButtonDown)
            {
                rewindButtonDown = false;
                fastForwardButtonDown.forceUpdateOfCachedValue();
                transport.performFastForwardButtonChanged();
            }
            else if (i == IDs::nudgeLeftCount)
            {
                transport.performNudgeLeft();
            }
            else if (i == IDs::nudgeRightCount)
            {
                transport.performNudgeRight();
            }
        }
    }

    void valueTreeChildAdded (ValueTree&, ValueTree&) override {}
    void valueTreeChildRemoved (ValueTree&, ValueTree&, int) override {}
    void valueTreeChildOrderChanged (ValueTree&, int, int) override {}
    void valueTreeParentChanged (ValueTree&) override {}
};

//==============================================================================
struct TransportControl::SectionPlayer  : private Timer
{
    SectionPlayer (TransportControl& tc, EditTimeRange sectionToPlay)
        : transport (tc), section (sectionToPlay),
          originalTransportTime (tc.getCurrentPosition()),
          wasLooping (tc.looping)
    {
        jassert (! sectionToPlay.isEmpty());
        transport.setCurrentPosition (sectionToPlay.getStart());
        transport.looping = false;
        transport.play (false);

        startTimerHz (25);
    }

    ~SectionPlayer()
    {
        if (wasLooping)
            transport.looping = true;
    }

    TransportControl& transport;
    const EditTimeRange section;
    const double originalTransportTime;
    const bool wasLooping;

    void timerCallback() override
    {
        if (transport.getCurrentPosition() > section.getEnd())
            transport.stop (false, false); // Will delete the SectionPlayer
    }
};

//==============================================================================
struct TransportControl::FileFlushTimer  : private Timer
{
    FileFlushTimer (TransportControl& tc)
        : owner (tc)
    {
        startTimer (500);
    }

    void timerCallback() override
    {
        if (owner.edit.isLoading())
            return;

        bool active = Process::isForegroundProcess();

        if (active && forcePurge)
        {
            hasBeenDeactivated = true;
            active = false;
        }

        if (active != hasBeenDeactivated
             && ! (owner.isPlaying() || owner.isRecording()))
        {
            hasBeenDeactivated = active;

            if (! active)
            {
                if (! forcePurge)
                    owner.engine.getAudioFileManager().releaseAllFiles();

                owner.edit.purgeOrphanFreezeAndProxyFiles();
                forcePurge = false;
            }
            else
            {
                owner.engine.getAudioFileManager().checkFilesForChanges();
            }
        }
    }

    TransportControl& owner;
    bool hasBeenDeactivated = false, forcePurge = false;
};

//==============================================================================
struct TransportControl::ButtonRepeater : private Timer
{
    ButtonRepeater (TransportControl& tc, bool isRW)
        : owner (tc), isRewind (isRW)
    {
    }

    void setDown (bool b)
    {
        accel = 1.0;
        lastClickTime = Time::getCurrentTime();

        if (b != isDown)
        {
            isDown = b;

            if (b)
            {
                firstPress = true;
                buttonDownTime = Time::getCurrentTime();
            }

            static int buttsDown = 0;

            if (b)
            {
                ++buttsDown;
                startTimer (20);
                timerCallback();
            }
            else
            {
                --buttsDown;
                stopTimer();
            }

            owner.setUserDragging (buttsDown > 0);
        }
    }

    void nudge()
    {
        setDown (true);
        timerCallback();
        setDown (false);
    }

private:
    TransportControl& owner;
    double accel = 1.0;
    bool isRewind, isDown = false, firstPress = false;
    Time buttonDownTime, lastClickTime;

    void timerCallback() override
    {
        auto now = Time::getCurrentTime();
        double secs = (now - lastClickTime).inSeconds();
        lastClickTime = now;

        if (isRewind)
        {
            // don't respond to both keys at once
            if (owner.ffRepeater->isDown)
                return;

            secs = -secs;
        }

        if (owner.snapToTimecode)
        {
            if ((Time::getCurrentTime() - buttonDownTime).inSeconds() < 0.5)
            {
                if (firstPress)
                {
                    firstPress = false;

                    double t = owner.position;

                    if (isRewind)
                        t = TransportHelpers::snapTimeDown (owner, t - 1.0e-5, false);
                    else
                        t = TransportHelpers::snapTimeUp (owner, t + 1.0e-5, false);

                    owner.setCurrentPosition (t);
                }

                return;
            }
        }

        secs *= accel;
        accel = jmin (accel + 0.1, 6.0);

        scrub (owner, secs * 10.0);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ButtonRepeater)
};

//==============================================================================
static Array<TransportControl*, CriticalSection> activeTransportControls;

//==============================================================================
TransportControl::TransportControl (Edit& ed, const juce::ValueTree& v)
    : engine (ed.engine), edit (ed), state (v)
{
    jassert (state.hasType (IDs::TRANSPORT));
    UndoManager* um = nullptr;
    position.referTo (state, IDs::position, um);
    loopPoint1.referTo (state, IDs::loopPoint1, um);
    loopPoint2.referTo (state, IDs::loopPoint2, um);
    snapToTimecode.referTo (state, IDs::snapToTimecode, um, true);
    looping.referTo (state, IDs::looping, um);
    scrubInterval.referTo (state, IDs::scrubInterval, um, 0.1);

    transportState = std::make_unique<TransportState> (*this, state);

    rwRepeater = std::make_unique<ButtonRepeater> (*this, true);
    ffRepeater = std::make_unique<ButtonRepeater> (*this, false);

    fileFlushTimer = std::make_unique<FileFlushTimer> (*this);

    activeTransportControls.add (this);
    startTimerHz (25);
}

TransportControl::~TransportControl()
{
    activeTransportControls.removeAllInstancesOf (this);
    fileFlushTimer = nullptr;

    CRASH_TRACER
    stop (false, true);
}

PlayHead* TransportControl::getCurrentPlayhead() const
{
    return playbackContext ? &playbackContext->playhead
                            : nullptr;
}

//==============================================================================
Array<TransportControl*> TransportControl::getAllActiveTransports (Engine& engine)
{
    Array<TransportControl*> controls;

    for (auto edit : engine.getActiveEdits().getEdits())
        controls.add (&edit->getTransport());

    return controls;
}

int TransportControl::getNumPlayingTransports (Engine& engine)
{
    return engine.getActiveEdits().numTransportsPlaying;
}

void TransportControl::stopAllTransports (Engine& engine, bool discardRecordings, bool clearDevices)
{
    for (auto tc : getAllActiveTransports (engine))
        tc->stop (discardRecordings, clearDevices);
}

void TransportControl::restartAllTransports (Engine& engine, bool clearDevices)
{
    for (auto tc : getAllActiveTransports (engine))
    {
        std::unique_ptr<ScopedPlaybackRestarter> spr;

        if (clearDevices)
        {
            spr = std::make_unique<ScopedPlaybackRestarter> (*tc);
            tc->stop (false, true);
        }
        else
        {
            tc->stopIfRecording();
        }

        tc->edit.restartPlayback();
    }
}

TransportControl::PlayingFlag::PlayingFlag (Engine& e) noexcept : engine (e)    { ++engine.getActiveEdits().numTransportsPlaying; }
TransportControl::PlayingFlag::~PlayingFlag() noexcept                          { --engine.getActiveEdits().numTransportsPlaying; }

//==============================================================================
void TransportControl::editHasChanged()
{
    if (transportState->reallocationInhibitors > 0)
    {
        isDelayedChangePending = true;
        return;
    }

    isDelayedChangePending = false;

    if (playbackContext == nullptr)
        return;

    ensureContextAllocated (true);
    engine.getExternalControllerManager().updateAllDevices();
}

int TransportControl::isAllowedToReallocate() const noexcept
{
    return transportState->reallocationInhibitors <= 0;
}

//==============================================================================
TransportControl::ReallocationInhibitor::ReallocationInhibitor (TransportControl& tc)
    : transport (tc)
{
    auto& inhibitors = transport.transportState->reallocationInhibitors;
    inhibitors = inhibitors + 1;
}

TransportControl::ReallocationInhibitor::~ReallocationInhibitor()
{
    auto& inhibitors = transport.transportState->reallocationInhibitors;
    jassert (inhibitors > 0);
    inhibitors = jmax (0, inhibitors - 1);
}


//==============================================================================
void TransportControl::releaseAudioNodes()
{
    if (playbackContext != nullptr)
        playbackContext->clearNodes();
}

void TransportControl::ensureContextAllocated (bool alwaysReallocate)
{
    if (! edit.shouldPlay())
        return;

    const double start = position;

    if (playbackContext == nullptr)
    {
        playbackContext = std::make_unique<EditPlaybackContext> (*this);
        playbackContext->createPlayAudioNodes (start);
        transportState->playbackContextAllocation = transportState->playbackContextAllocation + 1;
    }

    if (isPlaying() || isRecording() || edit.playInStopEnabled)
    {
        if (alwaysReallocate)
            playbackContext->createPlayAudioNodes (start);
        else
            playbackContext->createPlayAudioNodesIfNeeded (start);
    }
    else
        playbackContext->clearNodes();
}

void TransportControl::freePlaybackContext()
{
    playbackContext.reset();
    clearPlayingFlags();
    transportState->playbackContextAllocation = jmax (0, transportState->playbackContextAllocation - 1);
}

void TransportControl::triggerClearDevicesOnStop()
{
    transportState->clearDevicesOnStop = true;

    if (isPlaying())
        return;

    stop (false, true);
    ensureContextAllocated();
}

void TransportControl::forceOrphanFreezeAndProxyFilesPurge()
{
    fileFlushTimer->forcePurge = true;
}

//==============================================================================
static int numScreenSaverDefeaters = 0;

struct TransportControl::ScreenSaverDefeater
{
    ScreenSaverDefeater()
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        ++numScreenSaverDefeaters;
        Desktop::setScreenSaverEnabled (numScreenSaverDefeaters == 0);
    }

    ~ScreenSaverDefeater()
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        --numScreenSaverDefeaters;
        jassert (numScreenSaverDefeaters >= 0);
        Desktop::setScreenSaverEnabled (numScreenSaverDefeaters == 0);
    }
};

//==============================================================================
void TransportControl::play (bool justSendMMCIfEnabled)
{
    transportState->play (justSendMMCIfEnabled);
}

void TransportControl::playSectionAndReset (EditTimeRange rangeToPlay)
{
    CRASH_TRACER

    if (! isPlaying())
        sectionPlayer = std::make_unique<SectionPlayer> (*this, rangeToPlay);
}

void TransportControl::record (bool justSendMMCIfEnabled, bool allowRecordingIfNoInputsArmed)
{
    transportState->record (justSendMMCIfEnabled, allowRecordingIfNoInputsArmed);
}

void TransportControl::stop (bool discardRecordings,
                             bool clearDevices,
                             bool canSendMMCStop,
                             bool invertReturnToStartPosSelection)
{
    transportState->stop (discardRecordings,
                          clearDevices,
                          canSendMMCStop,
                          invertReturnToStartPosSelection);
}

void TransportControl::stopIfRecording()
{
    if (isRecording())
        stop (false, false);
}

Result TransportControl::applyRetrospectiveRecord()
{
    if (static_cast<int> (engine.getPropertyStorage().getProperty (SettingID::retrospectiveRecord, 30)) == 0)
        return Result::fail (TRANS("Retrospective record is currently disabled"));

    if (playbackContext)
        return playbackContext->applyRetrospectiveRecord();

    return Result::fail (TRANS("No active audio devices"));
}
    
Array<File> TransportControl::getRetrospectiveRecordAsAudioFiles()
{
    if (static_cast<int> (engine.getPropertyStorage().getProperty (SettingID::retrospectiveRecord, 30)) == 0)
        return {};
    
    if (playbackContext)
    {
        Array<File> files;
        Array<Clip*> clips;
        playbackContext->applyRetrospectiveRecord (&clips);
        
        if (clips.size() > 0)
        {
            for (auto c : clips)
            {
                if (auto ac = dynamic_cast<WaveAudioClip*> (c))
                {
                    File f = ac->getOriginalFile();
                    files.add (f);
                }
                else if (auto mc = dynamic_cast<MidiClip*> (c))
                {
                    auto clipPos = mc->getPosition();
                    
                    Array<Clip*> clipsToRender;
                    clipsToRender.add (mc);
                    
                    File dir = File::getSpecialLocation (File::tempDirectory);
                    
                    auto f = dir.getNonexistentChildFile (File::createLegalFileName (mc->getName()), ".wav");
                    
                    BigInteger tracksToDo;
                    int idx = 0;
                    for (auto t : getAllTracks (edit))
                    {
                        if (mc->getTrack() == t)
                            tracksToDo.setBit (idx);
                        idx++;
                    }
                    
                    Renderer::renderToFile (TRANS("Render Clip"), f, edit, clipPos.time,
                                            tracksToDo, true, clipsToRender, true);
                    
                    files.add (f);
                }
                c->removeFromParentTrack();
            }
            return files;
        }
    }
    
    return {};
}

void TransportControl::syncToEdit (Edit* editToSyncTo, bool isPreview)
{
    CRASH_TRACER

    if (playbackContext && editToSyncTo != nullptr)
    {
        if (auto targetContext = editToSyncTo->getTransport().getCurrentPlaybackContext())
        {
            auto& tempoSequence = editToSyncTo->tempoSequence;
            auto& tempo   = tempoSequence.getTempoAt (position);
            auto& timeSig = tempoSequence.getTimeSigAt (position);

            auto barsBeats = tempoSequence.timeToBarsBeats (targetContext->playhead.isLooping()
                                                              ? targetContext->playhead.getLoopTimes().start
                                                              : position);

            auto previousBarTime = tempoSequence.barsBeatsToTime ({ barsBeats.bars, 0.0 });

            auto syncInterval = isPreview ? targetContext->playhead.getLoopTimes().getLength()
                                          : (60.0 / tempo.getBpm() * timeSig.numerator);

            playbackContext->syncToContext (targetContext, previousBarTime, syncInterval);
        }
    }
}

bool TransportControl::isPlaying() const              { return transportState->playing; }
bool TransportControl::isRecording() const            { return transportState->recording; }
bool TransportControl::isSafeRecording() const        { return isRecording() && transportState->safeRecording; }

double TransportControl::getTimeWhenStarted() const   { return transportState->startTime; }

//==============================================================================
bool TransportControl::areAnyInputsRecording()
{
    for (auto in : edit.getAllInputDevices())
        if (in->isRecordingActive())
            return true;

    return false;
}

void TransportControl::clearPlayingFlags()
{
    transportState->playing = false;
    transportState->recording = false;
    transportState->safeRecording = false;
    playingFlag.reset();
}

//==============================================================================
void TransportControl::timerCallback()
{
    CRASH_TRACER

    if (playbackContext == nullptr)
        return;

    if (isDelayedChangePending)
        editHasChanged();

    if (isPlaying() && playbackContext->playhead.getPosition() >= Edit::maximumLength)
    {
        stop (false, false);
        position = double (Edit::maximumLength);
        return;
    }

    if (! playbackContext->playhead.isPlaying())
    {
        if (isRecording())
        {
            stop (false, false);
            return;
        }

        if (isPlaying())
        {
            clearPlayingFlags();
            startedOrStopped();
        }

        if ((! transportState->userDragging) && Time::getMillisecondCounter() - transportState->lastUserDragTime > 200)
            playbackContext->playhead.setPosition (position);
    }
    else
    {
        if ((! transportState->userDragging) && Time::getMillisecondCounter() - transportState->lastUserDragTime > 200)
        {
            const double currentTime = playbackContext->playhead.getPosition();
            transportState->setVideoPosition (currentTime, false);
            transportState->updatePositionFromPlayhead (currentTime);
        }

        if (--loopUpdateCounter == 0)
        {
            loopUpdateCounter = 10;

            if (looping)
            {
                auto lr = getLoopRange();
                lr.end = std::max (lr.end, lr.start + 0.001);
                playbackContext->playhead.setLoopTimes (true, lr);
            }
            else
            {
                playbackContext->playhead.setLoopTimes (false, {});
            }
        }
    }
}


//==============================================================================
void TransportControl::setRewindButtonDown (bool isDown)
{
    sectionPlayer.reset();
    transportState->rewindButtonDown = isDown;
}

void TransportControl::setFastForwardButtonDown (bool isDown)
{
    sectionPlayer.reset();
    transportState->fastForwardButtonDown = isDown;
}

void TransportControl::nudgeLeft()
{
    sectionPlayer.reset();
    transportState->nudgeLeft();
}

void TransportControl::nudgeRight()
{
    sectionPlayer.reset();
    transportState->nudgeRight();
}


//==============================================================================
double TransportControl::getCurrentPosition() const
{
    return position;
}

void TransportControl::setCurrentPosition (double newPos)
{
    CRASH_TRACER
    position = newPos;
}

void TransportControl::setUserDragging (bool b)
{
    CRASH_TRACER

    if (playbackContext != nullptr)
        playbackContext->playhead.setUserIsDragging (b);

    if (b != transportState->userDragging)
    {
        if (transportState->userDragging && isPlaying())
        {
            edit.getAutomationRecordManager().punchOut (false);

            if (playbackContext != nullptr)
                playbackContext->playhead.setPosition (position);
        }

        transportState->userDragging = b;

        if (b)
            transportState->lastUserDragTime = Time::getMillisecondCounter();
    }
}

bool TransportControl::isUserDragging() const noexcept
{
    return transportState->userDragging;
}

//==============================================================================
void TransportControl::setLoopIn (double t)
{
    setLoopPoint1 (jmax (loopPoint1.get(), loopPoint2.get(), jmax (0.0, t)));
    setLoopPoint2 (jmax (0.0, t));
}

void TransportControl::setLoopOut (double t)
{
    setLoopPoint1 (jmin (loopPoint1.get(), loopPoint2.get(), jmax (0.0, t)));
    setLoopPoint2 (jmax (0.0, t));
}

void TransportControl::setLoopPoint1 (double t)
{
    loopPoint1 = jlimit (0.0, edit.getLength() + Edit::maximumLength * 0.75, t);
}

void TransportControl::setLoopPoint2 (double t)
{
    loopPoint2 = jlimit (0.0, edit.getLength() + Edit::maximumLength * 0.75, t);
}

void TransportControl::setLoopRange (EditTimeRange times)
{
    auto maxEndTime = edit.getLength() + Edit::maximumLength * 0.75;

    loopPoint1 = jlimit (0.0, maxEndTime, times.getStart());
    loopPoint2 = jlimit (0.0, maxEndTime, times.getEnd());
}

EditTimeRange TransportControl::getLoopRange() const noexcept
{
    return EditTimeRange::between (loopPoint1, loopPoint2);
}

void TransportControl::setSnapType (TimecodeSnapType newSnapType)
{
    currentSnapType = newSnapType;
}


//==============================================================================
void TransportControl::startedOrStopped()
{
    if (lastPlayStatus != isPlaying() || lastRecordStatus != isRecording())
    {
        const bool wasRecording = lastRecordStatus;

        {
            CRASH_TRACER
            sendChangeMessage();

            lastPlayStatus = isPlaying();
            lastRecordStatus = isRecording();

            edit.sendStartStopMessageToPlugins();
        }

        {
            CRASH_TRACER
            if (isPlaying())
            {
                transportState->setVideoPosition (getCurrentPosition(), true);
                listeners.call (&Listener::startVideo);

                if (wasRecording)
                    listeners.call (&Listener::autoSaveNow);
            }
            else
            {
                listeners.call (&Listener::stopVideo);
            }

            listeners.call (&Listener::setAllLevelMetersActive, false);
            listeners.call (&Listener::setAllLevelMetersActive, true);
        }
    }
}

void TransportControl::sendMMC (const MidiMessage& mmc)
{
    CRASH_TRACER
    auto& dm = engine.getDeviceManager();

    for (int i = dm.getNumMidiOutDevices(); --i >= 0;)
    {
        if (auto* mo = dm.getMidiOutDevice (i))
        {
            if (mo->isEnabled() && mo->isSendingMMC())
            {
                mo->fireMessage (mmc);
                break;
            }
        }
    }
}

void TransportControl::sendMMCCommand (MidiMessage::MidiMachineControlCommand command)
{
    sendMMC (MidiMessage::midiMachineControlCommand (command));
}

bool anyEnabledMidiOutDevicesSendingMMC (DeviceManager& dm)
{
    for (int i = dm.getNumMidiOutDevices(); --i >= 0;)
        if (auto mo = dm.getMidiOutDevice (i))
            if (mo->isEnabled() && mo->isSendingMMC())
                return true;

    return false;
}

bool TransportControl::sendMMCStartPlay()
{
    if (anyEnabledMidiOutDevicesSendingMMC (engine.getDeviceManager()))
    {
        sendMMCCommand (MidiMessage::mmc_play);

        if (edit.isTimecodeSyncEnabled())
            return true;
    }

    return false;
}

bool TransportControl::sendMMCStartRecord()
{
    if (anyEnabledMidiOutDevicesSendingMMC (engine.getDeviceManager()))
    {
        sendMMCCommand (MidiMessage::mmc_recordStart);

        if (edit.isTimecodeSyncEnabled())
            return true;
    }

    return false;
}

//==============================================================================
void TransportControl::performPlay()
{
    CRASH_TRACER
    sectionPlayer.reset();

    if (! edit.shouldPlay())
        return;

    if (! playingFlag)
    {
        if (transportState->justSendMMCIfEnabled && sendMMCStartPlay())
            return;

        if (looping)
        {
            const double cursorPos = position;
            const auto loopRange = getLoopRange();

            if (cursorPos < loopRange.getStart()
                || cursorPos > loopRange.getEnd() - 0.1)
            {
                position = loopRange.getStart();
            }

            transportState->startTime = loopRange.getStart();
            transportState->endTime   = loopRange.getEnd();

            if (transportState->endTime < transportState->startTime + 0.01)
            {
                engine.getUIBehaviour().showWarningMessage (TRANS("Can't play in loop mode unless the in/out markers are further apart"));
                return;
            }
        }
        else
        {
            transportState->startTime = position.get();
            transportState->endTime   = double (Edit::maximumLength);
        }

        if (edit.getAbletonLink().isConnected())
        {
            double barLength = edit.tempoSequence.getTimeSig(0)->numerator;
            double beatsUntilNextLinkCycle = edit.getAbletonLink().getBeatsUntilNextCycle (barLength);

            double cyclePos = std::fmod (transportState->startTime, barLength);
            double nextLinkCycle = edit.tempoSequence.beatsToTime (beatsUntilNextLinkCycle);

            transportState->startTime = (transportState->startTime - cyclePos) + (barLength - nextLinkCycle);
        }

        transportState->recording = false;
        transportState->safeRecording = false;
        playingFlag = std::make_unique<PlayingFlag> (engine);

        if (transportState->cursorPosAtPlayStart < -100.0)
            transportState->cursorPosAtPlayStart = position.get();

        ensureContextAllocated();

        if (playbackContext)
        {
            playbackContext->playhead.play ({ transportState->startTime, transportState->endTime }, looping);

            if (looping)
            {
                playbackContext->playhead.setPosition (position);
            }
            else if (transportState->startTime < 0.2)
            {
                // if we're playing from near time = 0, roll back a fraction so we
                // don't miss the first block - this won't be noticable further along
                // in the edit.
                playbackContext->playhead.setPosition (transportState->startTime - 0.2);
            }
        }
        else
        {
            clearPlayingFlags();
        }

        edit.setClickTrackRange ({});
    }
}

bool TransportControl::performRecord()
{
    if (! edit.shouldPlay())
        return true;

    CRASH_TRACER
    sectionPlayer.reset();

    stop (false, false);

    if (! transportState->userDragging)
    {
        if (transportState->justSendMMCIfEnabled && sendMMCStartRecord())
            return true;

        if (transportState->allowRecordingIfNoInputsArmed || areAnyInputsRecording())
        {
            const auto loopRange = getLoopRange();
            transportState->startTime   = position.get();
            transportState->endTime     = double (Edit::maximumLength);

            if (looping)
            {
                if (loopRange.getLength() < 2.0)
                {
                    engine.getUIBehaviour().showWarningMessage (TRANS("To record in loop mode, the length of loop must be greater than 2 seconds."));
                    return false;
                }

                if (edit.recordingPunchInOut)
                {
                    engine.getUIBehaviour().showWarningMessage (TRANS("Recording can be done in either loop mode or punch in/out mode, but not both at the same time!"));
                    return false;
                }

                transportState->startTime = loopRange.getStart();
            }
            else if (edit.recordingPunchInOut)
            {
                if (loopRange.getEnd() + 0.1 <= transportState->startTime)
                    transportState->startTime = loopRange.getStart() - 1.0;
            }
            else
            {
                if (std::abs (transportState->startTime) < 0.005)
                    transportState->startTime = 0;
            }

            double prerollStart = transportState->startTime;
            double numCountInBeats = edit.getNumCountInBeats();

            if (numCountInBeats > 0)
            {
                double currentBeat = edit.tempoSequence.timeToBeats (transportState->startTime);
                prerollStart = edit.tempoSequence.beatsToTime (currentBeat - (numCountInBeats + 0.5));
            }

            if (edit.getAbletonLink().isConnected())
            {
                double barLength = edit.tempoSequence.getTimeSig (0)->numerator;
                double beatsUntilNextLinkCycle = edit.getAbletonLink().getBeatsUntilNextCycle (barLength);

                if (numCountInBeats > 0)
                    beatsUntilNextLinkCycle -= 0.5;

                prerollStart -= edit.tempoSequence.beatsToTime (beatsUntilNextLinkCycle);
            }

            transportState->cursorPosAtPlayStart = position.get();

            playingFlag = std::make_unique<PlayingFlag> (engine);
            transportState->safeRecording = engine.getPropertyStorage().getProperty (SettingID::safeRecord, false);

            edit.updateMidiTimecodeDevices();

            ensureContextAllocated();

            if (playbackContext)
            {
                if (edit.getNumCountInBeats() > 0)
                    playbackContext->playhead.setLoopTimes (true, { transportState->startTime, Edit::maximumLength });

                // if we're playing from near time = 0, roll back a fraction so we
                // don't miss the first block - this won't be noticable further along
                // in the edit.
                if (prerollStart < 0.2)
                    prerollStart -= 0.2;

                if (looping)
                    playbackContext->playhead.setRollInToLoop (prerollStart);

                // Set the playhead loop times before preparing the context as this will be used by
                // the RecordingContext to initialise itself
                playbackContext->playhead.setLoopTimes (false, { prerollStart, transportState->endTime });
                playbackContext->prepareForRecording (prerollStart, transportState->startTime);
                playbackContext->playhead.play ({ prerollStart, transportState->endTime }, false);
                playbackContext->playhead.setPosition (prerollStart);
                position = prerollStart;

                if (edit.getNumCountInBeats() > 0)
                    edit.setClickTrackRange ({ prerollStart, transportState->startTime });
                else
                    edit.setClickTrackRange ({});

                transportState->playing = true; // N.B. set these after the devices have been rebuilt and the playingFlag has been set
                screenSaverDefeater = std::make_unique<ScreenSaverDefeater>();
            }
        }
        else
        {
            engine.getUIBehaviour().showWarningMessage (
                TRANS("Recording is only possible  when at least one active input device is assigned to a track"));

            return false;
        }
    }

    if (! transportState->justSendMMCIfEnabled)
        sendMMCCommand (MidiMessage::mmc_recordStart);

    if (transportState->safeRecording)
        engine.getUIBehaviour().showSafeRecordDialog (*this);

    return true;
}

void TransportControl::performStop()
{
    CRASH_TRACER

    screenSaverDefeater.reset();
    sectionPlayer.reset();

    engine.getUIBehaviour().hideSafeRecordDialog (*this);

    if (playbackContext == nullptr)
    {
        jassert (! (isPlaying() || isRecording()));
        clearPlayingFlags();
        return;
    }

    if (! Component::isMouseButtonDownAnywhere())
        setUserDragging (false); // in case it gets stuck

    if (isRecording())
    {
        CRASH_TRACER

        // grab this before stopping the playhead
        auto recEndTime = playbackContext->playhead.getUnloopedPosition();
        auto recEndPos  = playbackContext->playhead.getPosition();

        clearPlayingFlags();
        playbackContext->playhead.stop();
        playbackContext->recordingFinished ({ transportState->startTime, recEndTime },
                                            transportState->discardRecordings);

        position = transportState->discardRecordings ? transportState->startTime
                                                     : (looping ? recEndPos
                                                                : recEndTime);
    }
    else
    {
        if (transportState->discardRecordings)
            engine.getUIBehaviour().showWarningMessage (TRANS("Can only abort a recording when something's actually recording."));

        clearPlayingFlags();
        playbackContext->playhead.stop();
    }

    if (transportState->clearDevices || ! edit.playInStopEnabled || transportState->clearDevicesOnStop)
        releaseAudioNodes();
    else
        ensureContextAllocated();

    transportState->clearDevicesOnStop = false;

    if ((transportState->invertReturnToStartPosSelection ^ bool (engine.getPropertyStorage().getProperty (SettingID::resetCursorOnStop, false)))
         && transportState->cursorPosAtPlayStart >= 0)
        setCurrentPosition (transportState->cursorPosAtPlayStart);

    if (transportState->canSendMMCStop)
        sendMMCCommand (MidiMessage::mmc_stop);
}

void TransportControl::performPositionChange()
{
    CRASH_TRACER

    sectionPlayer.reset();
    edit.getAutomationRecordManager().punchOut (false);

    if (isRecording())
        stop (false, false);

    double newPos = state[IDs::position];

    if (isPlaying() && looping)
    {
        auto range = getLoopRange();
        newPos = jlimit (range.start, range.end, newPos);
    }
    else
    {
        newPos = jlimit (0.0, Edit::maximumLength, newPos);
    }

    if (playbackContext != nullptr && isPlaying())
        playbackContext->playhead.setPosition (newPos);

    if (! isPlaying())
        transportState->cursorPosAtPlayStart = newPos;

    position = newPos;

    yieldGUIThread();

    if (! transportState->userDragging)
        transportState->lastUserDragTime = Time::getMillisecondCounter();

    transportState->setVideoPosition (newPos, true);

    // MMC
    const double nudge = 0.05 / 96000.0;
    const double mmcTime = jmax (0.0, newPos + edit.getTimecodeOffset()) + nudge;
    const int framesPerSecond = edit.getTimecodeFormat().getFPS();
    const int frames  = ((int) (mmcTime * framesPerSecond)) % framesPerSecond;
    const int hours   = (int) (mmcTime * (1.0 / 3600.0));
    const int minutes = (((int) mmcTime) / 60) % 60;
    const int seconds = (((int) mmcTime) % 60);

    sendMMC (MidiMessage::midiMachineControlGoto (hours, minutes, seconds, frames));
}

void TransportControl::performRewindButtonChanged()
{
    const bool isDown = transportState->rewindButtonDown;
    rwRepeater->setDown (isDown);

    if (isDown)
        sendMMCCommand (MidiMessage::mmc_rewind);
    else
        sendMMCCommand (isPlaying() ? MidiMessage::mmc_play
                        : MidiMessage::mmc_stop);
}

void TransportControl::performFastForwardButtonChanged()
{
    const bool isDown = transportState->fastForwardButtonDown;
    ffRepeater->setDown (isDown);

    if (isDown)
        sendMMCCommand (MidiMessage::mmc_fastforward);
    else
        sendMMCCommand (isPlaying() ? MidiMessage::mmc_play
                        : MidiMessage::mmc_stop);
}

void TransportControl::performNudgeLeft()
{
    rwRepeater->nudge();
}

void TransportControl::performNudgeRight()
{
    ffRepeater->nudge();
}


//==============================================================================
static EditTimeRange getLimitsOfSelectedClips (Edit& edit, const SelectableList& items)
{
    auto range = getTimeRangeForSelectedItems (items);

    if (range.isEmpty())
        return { 0.0, edit.getLength() };

    return range;
}

void toStart (TransportControl& tc, const SelectableList& items)
{
    auto selectionStart = getLimitsOfSelectedClips (tc.edit, items).getStart();
    tc.setCurrentPosition (tc.getCurrentPosition() < selectionStart + 0.001 ? 0.0 : selectionStart);
}

void toEnd (TransportControl& tc, const SelectableList& items)
{
    auto selectionEnd = getLimitsOfSelectedClips (tc.edit, items).getEnd();
    tc.setCurrentPosition (tc.getCurrentPosition() > selectionEnd - 0.001 ? tc.edit.getLength() : selectionEnd);
}

void tabBack (TransportControl& tc)     { tc.setCurrentPosition (tc.edit.getPreviousTimeOfInterest (tc.getCurrentPosition() - 0.001)); }
void tabForward (TransportControl& tc)  { tc.setCurrentPosition (tc.edit.getNextTimeOfInterest     (tc.getCurrentPosition() + 0.001)); }

void markIn (TransportControl& tc)      { tc.setLoopIn  (tc.getCurrentPosition()); }
void markOut (TransportControl& tc)     { tc.setLoopOut (tc.getCurrentPosition()); }

void scrub (TransportControl& tc, double units)
{
    CRASH_TRACER
    const double unitSize = tc.scrubInterval;
    double timeToMove = units * unitSize;
    double t = tc.getCurrentPosition() + timeToMove;

    if (tc.snapToTimecode)
    {
        if (timeToMove > 0)
            t = TransportHelpers::snapTimeUp (tc, t, false);
        else
            t = TransportHelpers::snapTimeDown (tc, t, false);
    }

    if (tc.isUserDragging() && tc.engine.getPropertyStorage().getProperty (SettingID::snapCursor, false))
        t = TransportHelpers::snapTimeDown (tc, t, false);

    tc.setCurrentPosition (t);
}

void freePlaybackContextIfNotRecording (TransportControl& tc)
{
    if (tc.isPlayContextActive() && ! tc.isRecording())
        tc.freePlaybackContext();
}
