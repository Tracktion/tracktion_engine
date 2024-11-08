/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

namespace IDs
{
    #define DECLARE_ID(name)  const juce::Identifier name (#name);

    DECLARE_ID (safeRecording)
    DECLARE_ID (discardRecordings)
    DECLARE_ID (clearDevices)
    DECLARE_ID (justSendMMCIfEnabled)
    DECLARE_ID (canSendMMCStop)
    DECLARE_ID (allowRecordingIfNoInputsArmed)
    DECLARE_ID (clearDevicesOnStop)
    DECLARE_ID (updatingFromPlayHead)
    DECLARE_ID (scrubInterval)

    DECLARE_ID (userDragging)
    DECLARE_ID (lastUserDragTime)
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
    inline TimePosition snapTime (TransportControl& tc, TimePosition t, bool invertSnap)
    {
        return (tc.snapToTimecode ^ invertSnap) ? tc.getSnapType().roundTimeNearest (t, tc.edit.tempoSequence)
                                                : t;
    }

    inline TimePosition snapTimeUp (TransportControl& tc, TimePosition t, bool invertSnap)
    {
        return (tc.snapToTimecode ^ invertSnap) ? tc.getSnapType().roundTimeUp (t, tc.edit.tempoSequence)
                                                : t;
    }

    inline TimePosition snapTimeDown (TransportControl& tc, TimePosition t, bool invertSnap)
    {
        return (tc.snapToTimecode ^ invertSnap) ? tc.getSnapType().roundTimeDown (t, tc.edit.tempoSequence)
                                                : t;
    }

    inline void resyncLauncherClips (TransportControl& tc, std::optional<SyncPoint> startPoint, bool syncToStartOfBar)
    {
        auto epc = tc.getCurrentPlaybackContext();

        if (! epc)
            return;

        auto& edit = tc.edit;
        juce::Array<Clip*> launchedClips;

        edit.clipSlotCache.visitItems ([&] (auto cs)
        {
            if (auto c = cs->getClip())
            {
                if (auto lh = c->getLaunchHandle())
                {
                    if (lh->getQueuedStatus() == LaunchHandle::QueueState::stopQueued)
                        return;

                    if (lh->getPlayingStatus() == LaunchHandle::PlayState::playing)
                        launchedClips.add (c);
                }
            }
        });

        if (launchedClips.isEmpty())
            return;

        // If clips are starting in the future, stop them now
        if (startPoint)
        {
            for (auto c : launchedClips)
                c->getLaunchHandle()->stop ({});

            epc->blockUntilSyncPointChange();
        }

        const auto currentPoint = epc->getSyncPoint();

        if (! currentPoint)
            return;

        if (! startPoint)
            startPoint = currentPoint;

        auto& ts = edit.tempoSequence;
        const auto currentBeat = ts.toBeats (tc.getPosition());

        MonotonicBeat startSyncBeat;

        if (syncToStartOfBar)
        {
            const auto currentBarsBeats = ts.toBarsAndBeats (tc.getPosition());
            const auto barStartBeat = ts.toBeats (tempo::BarsAndBeats { .bars = currentBarsBeats.bars });
            const auto launchBeatDiff = currentBeat - barStartBeat;
            startSyncBeat = MonotonicBeat { currentPoint->monotonicBeat.v - launchBeatDiff };
        }
        else
        {
            const auto launchBeatDiff = currentPoint->beat - startPoint->beat;
            startSyncBeat = MonotonicBeat { currentPoint->monotonicBeat.v - launchBeatDiff };
        }

        for (auto c : launchedClips)
        {
            auto lh = c->getLaunchHandle();
            assert (lh);
            lh->play (startSyncBeat);
        }
    }
}


//==============================================================================
/**
    Represents the state of an Edit's transport.
*/
struct TransportControl::TransportState : private juce::ValueTree::Listener
{
    TransportState (TransportControl& tc, juce::ValueTree transportStateToUse)
        : state (transportStateToUse), transport (tc)
    {
        juce::UndoManager* um = nullptr;

        playing.referTo (transientState, IDs::playing, um);
        recording.referTo (transientState, IDs::recording, um);
        safeRecording.referTo (transientState, IDs::safeRecording, um);

        discardRecordings.referTo (transientState, IDs::discardRecordings, um);
        clearDevices.referTo (transientState, IDs::clearDevices, um);
        justSendMMCIfEnabled.referTo (transientState, IDs::justSendMMCIfEnabled, um);
        canSendMMCStop.referTo (transientState, IDs::canSendMMCStop, um);
        allowRecordingIfNoInputsArmed.referTo (transientState, IDs::allowRecordingIfNoInputsArmed, um);
        clearDevicesOnStop.referTo (transientState, IDs::clearDevicesOnStop, um);
        updatingFromPlayHead.referTo (transientState, IDs::updatingFromPlayHead, um);

        startTime.referTo (transientState, IDs::startTime, um);
        endTime.referTo (transientState, IDs::endTime, um);
        userDragging.referTo (transientState, IDs::userDragging, um);
        lastUserDragTime.referTo (transientState, IDs::lastUserDragTime, um);
        reallocationInhibitors.referTo (transientState, IDs::reallocationInhibitors, um);
        playbackContextAllocation.referTo (transientState, IDs::playbackContextAllocation, um);

        rewindButtonDown.referTo (transientState, IDs::rewindButtonDown, um);
        fastForwardButtonDown.referTo (transientState, IDs::fastForwardButtonDown, um);
        nudgeLeftCount.referTo (transientState, IDs::nudgeLeftCount, um);
        nudgeRightCount.referTo (transientState, IDs::nudgeRightCount, um);

        videoPosition.referTo (transientState, IDs::videoPosition, um);
        forceVideoJump.referTo (transientState, IDs::forceVideoJump, um);

        // CachedValues need to be set so they aren't using their default values
        // to avoid spurious listener callbacks
        playing = playing.get();
        recording = recording.get();
        safeRecording = safeRecording.get();

        state.addListener (this);
        transientState.addListener (this);
    }

    /** Destructor. */
    ~TransportState() override
    {
        jassert (reallocationInhibitors == 0);
    }

    /** Updates the current video position, calling any listeners. */
    void setVideoPosition (TimePosition time, bool forceJump)
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
               bool canSendMMCStop_)
    {
        discardRecordings = discardRecordings_;
        clearDevices = clearDevices_;
        canSendMMCStop = canSendMMCStop_;
        playing = false;
    }

    void updatePositionFromPlayhead (TimePosition newPosition)
    {
        updatingFromPlayHead = true;
        state.setProperty (IDs::position, newPosition.inSeconds(), nullptr);
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
    juce::CachedValue<bool> playing, recording, safeRecording;
    juce::CachedValue<bool> discardRecordings, clearDevices, justSendMMCIfEnabled, canSendMMCStop,
                            allowRecordingIfNoInputsArmed, clearDevicesOnStop;
    juce::CachedValue<bool> userDragging, forceVideoJump, rewindButtonDown, fastForwardButtonDown, updatingFromPlayHead;
    juce::CachedValue<juce::int64> lastUserDragTime;
    juce::CachedValue<TimePosition> startTime, endTime;
    juce::CachedValue<TimePosition> videoPosition;
    juce::CachedValue<int> reallocationInhibitors, playbackContextAllocation, nudgeLeftCount, nudgeRightCount;

    juce::ValueTree state, transientState { IDs::TRANSPORT };
    TransportControl& transport;

private:
    bool isInsideRecordingCallback = false;

    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i) override
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
                // This recursion check is to avoid the call to performRecord stopping
                // playback which in turn stops recording as it is trying to be started
                if (isInsideRecordingCallback)
                    return;

                recording.forceUpdateOfCachedValue();

                if (recording)
                {
                    juce::ScopedValueSetter<bool> svs (isInsideRecordingCallback, true);

                    if (auto res = transport.performRecord())
                    {
                        recording = true;
                        transport.listeners.call (&TransportControl::Listener::recordingStarted, res->first, res->second);
                    }
                    else
                    {
                        recording = false;
                    }
                }
                else
                {
                    transport.performStopRecording();
                }
            }
            else if (i == IDs::playbackContextAllocation)
            {
                transport.listeners.call (&TransportControl::Listener::playbackContextChanged);
            }
            else if (i == IDs::videoPosition)
            {
                videoPosition.forceUpdateOfCachedValue();
                transport.listeners.call (&TransportControl::Listener::setVideoPosition, videoPosition.get(), forceVideoJump);
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
};

//==============================================================================
struct TransportControl::SectionPlayer  : private Timer
{
    SectionPlayer (TransportControl& tc, TimeRange sectionToPlay)
        : transport (tc), section (sectionToPlay),
          wasLooping (tc.looping)
    {
        jassert (! sectionToPlay.isEmpty());
        transport.setPosition (sectionToPlay.getStart());
        transport.looping = false;
        transport.play (false);

        startTimerHz (25);
    }

    ~SectionPlayer() override
    {
        if (wasLooping)
            transport.looping = true;
    }

    TransportControl& transport;
    const TimeRange section;
    const bool wasLooping;

    void timerCallback() override
    {
        if (transport.getPosition() > section.getEnd())
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

    ~FileFlushTimer() override
    {
        stopTimer();
    }

    void timerCallback() override
    {
        if (owner.edit.isLoading())
            return;

        bool active = juce::Process::isForegroundProcess();

        if (active && forcePurge)
        {
            hasBeenDeactivated = true;
            active = false;
        }

        auto canPurge = [this]
        {
            if (owner.isPlaying() || owner.isRecording())
                return false;

            return SmartThumbnail::areThumbnailsFullyLoaded (owner.engine);
        };

        if (active != hasBeenDeactivated
            && canPurge())
        {
            hasBeenDeactivated = active;

            if (! active)
            {
                if (! forcePurge)
                    owner.engine.getAudioFileManager().releaseAllFiles();

                TemporaryFileManager::purgeOrphanFreezeAndProxyFiles (owner.edit);
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
        lastClickTime = juce::Time::getCurrentTime();

        if (b != isDown)
        {
            isDown = b;

            if (b)
            {
                firstPress = true;
                buttonDownTime = juce::Time::getCurrentTime();
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
    juce::Time buttonDownTime, lastClickTime;

    void timerCallback() override
    {
        auto now = juce::Time::getCurrentTime();
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
            if ((juce::Time::getCurrentTime() - buttonDownTime).inSeconds() < 0.5)
            {
                if (firstPress)
                {
                    firstPress = false;

                    auto t = owner.getPosition();

                    if (isRewind)
                        t = TransportHelpers::snapTimeDown (owner, t - 1.0e-5s, false);
                    else
                        t = TransportHelpers::snapTimeUp (owner, t + 1.0e-5s, false);

                    owner.setPosition (std::max (0_tp, t));
                }

                return;
            }
        }

        secs *= accel;
        accel = std::min (accel + 0.1, 6.0);

        scrub (owner, secs * 10.0);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ButtonRepeater)
};

//==============================================================================
struct TransportControl::PlayHeadWrapper
{
    PlayHeadWrapper (TransportControl& t)
        : transport (t)
    {}

    tracktion::graph::PlayHead* getNodePlayHead() const
    {
        return transport.playbackContext ? transport.playbackContext->getNodePlayHead()
                                         : nullptr;
    }

    double getSampleRate() const
    {
        return transport.playbackContext ? transport.playbackContext->getSampleRate()
                                         : 44100.0;
    }

    void play()
    {
        if (auto ph = getNodePlayHead())
            ph->play();
    }

    void play (TimeRange timeRange, bool looped)
    {
        if (auto ph = getNodePlayHead())
            ph->play (tracktion::toSamples (timeRange, getSampleRate()), looped);
    }

    void setRollInToLoop (TimePosition prerollStartTime)
    {
        if (auto ph = getNodePlayHead())
            ph->setRollInToLoop (tracktion::toSamples (prerollStartTime, getSampleRate()));
    }

    void stop()
    {
        if (auto ph = getNodePlayHead())
            ph->stop();
    }

    bool isPlaying() const
    {
        if (transport.playbackContext && transport.playbackContext->isPlayPending())
            return true;

        if (auto ph = getNodePlayHead())
            return ph->isPlaying();

        return false;
    }

    /** Returns the transport position to show in the UI, taking in to account any latency. */
    TimePosition getLiveTransportPosition() const
    {
        if (getNodePlayHead() != nullptr && transport.playbackContext != nullptr && transport.playbackContext->isPlaybackGraphAllocated())
            return transport.playbackContext->getAudibleTimelineTime();

        return getPosition();
    }

    TimePosition getPosition() const
    {
        if (auto ph = getNodePlayHead())
            return TimePosition::fromSamples (ph->getPosition(), getSampleRate());

        return {};
    }

    TimePosition getUnloopedPosition() const
    {
        if (auto ph = getNodePlayHead())
            return TimePosition::fromSamples (ph->getUnloopedPosition(), getSampleRate());

        return {};
    }

    void setPosition (TimePosition newPos)
    {
        if (getNodePlayHead() != nullptr)
            transport.playbackContext->postPosition (newPos);
    }

    void postPlay()
    {
        if (getNodePlayHead() != nullptr)
            transport.playbackContext->postPlay();
    }

    void isPlayPending()
    {
        if (getNodePlayHead() != nullptr)
            transport.playbackContext->isPlayPending();
    }

    bool isLooping() const
    {
        if (auto ph = getNodePlayHead())
            return ph->isLooping();

        return false;
    }

    TimeRange getLoopTimes() const
    {
        if (auto ph = getNodePlayHead())
            return tracktion::timeRangeFromSamples (ph->getLoopRange(), getSampleRate());

        return {};
    }

    void setLoopTimes (bool loop, TimeRange newRange)
    {
        if (auto ph = getNodePlayHead())
            ph->setLoopRange (loop, tracktion::toSamples (newRange, getSampleRate()));
    }

    void setUserIsDragging (bool isDragging)
    {
         if (auto ph = getNodePlayHead())
             ph->setUserIsDragging (isDragging);
    }

private:
    TransportControl& transport;
};


//==============================================================================
static int numScreenSaverDefeaters = 0;

struct TransportControl::ScreenSaverDefeater
{
    ScreenSaverDefeater()
    {
        if (juce::Desktop::getInstance().isHeadless())
            return;

        TRACKTION_ASSERT_MESSAGE_THREAD
        ++numScreenSaverDefeaters;
        juce::Desktop::setScreenSaverEnabled (numScreenSaverDefeaters == 0);
    }

    ~ScreenSaverDefeater()
    {
        if (juce::Desktop::getInstance().isHeadless())
            return;

        TRACKTION_ASSERT_MESSAGE_THREAD
        --numScreenSaverDefeaters;
        jassert (numScreenSaverDefeaters >= 0);
        juce::Desktop::setScreenSaverEnabled (numScreenSaverDefeaters == 0);
    }
};

//==============================================================================
static juce::Array<TransportControl*, juce::CriticalSection> activeTransportControls;

//==============================================================================
TransportControl::TransportControl (Edit& ed, const juce::ValueTree& v)
    : engine (ed.engine), edit (ed), state (v)
{
    jassert (state.hasType (IDs::TRANSPORT));
    juce::UndoManager* um = nullptr;
    startPosition.referTo (state, IDs::start, um);
    position.referTo (state, IDs::position, um);
    loopPoint1.referTo (state, IDs::loopPoint1, um);
    loopPoint2.referTo (state, IDs::loopPoint2, um);
    snapToTimecode.referTo (state, IDs::snapToTimecode, um, true);
    looping.referTo (state, IDs::looping, um);
    scrubInterval.referTo (state, IDs::scrubInterval, um, 0.1s);

    playHeadWrapper = std::make_unique<PlayHeadWrapper> (*this);
    transportState = std::make_unique<TransportState> (*this, state);

    rwRepeater = std::make_unique<ButtonRepeater> (*this, true);
    ffRepeater = std::make_unique<ButtonRepeater> (*this, false);

    fileFlushTimer = std::make_unique<FileFlushTimer> (*this);

    activeTransportControls.add (this);
    startTimerHz (50);
}

TransportControl::~TransportControl()
{
    stopTimer();

    activeTransportControls.removeAllInstancesOf (this);
    fileFlushTimer = nullptr;

    CRASH_TRACER
    stop (false, true);
}

//==============================================================================
juce::Array<TransportControl*> TransportControl::getAllActiveTransports (Engine& engine)
{
    juce::Array<TransportControl*> controls;

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

std::vector<std::unique_ptr<TransportControl::ScopedContextAllocator>> TransportControl::restartAllTransports (Engine& engine, bool clearDevices)
{
    std::vector<std::unique_ptr<ScopedContextAllocator>> restartHandles;

    for (auto tc : getAllActiveTransports (engine))
    {
        std::unique_ptr<ScopedPlaybackRestarter> spr;

        if (clearDevices)
        {
            restartHandles.push_back (std::make_unique<ScopedContextAllocator> (*tc));
            tc->stop (false, true);
            tc->freePlaybackContext();
        }
        else
        {
            tc->stopIfRecording();
        }

        tc->edit.restartPlayback();
    }

    return restartHandles;
}

void TransportControl::callRecordingAboutToStartListeners (InputDeviceInstance& in, EditItemID targetID)
{
    listeners.call (&Listener::recordingAboutToStart, in, targetID);
}

void TransportControl::callRecordingAboutToStopListeners (InputDeviceInstance& in, EditItemID targetID)
{
    recordingIsStoppingFlag = true;
    listeners.call (&Listener::recordingAboutToStop, in, targetID);
}

void TransportControl::callRecordingFinishedListeners (InputDeviceInstance& in, EditItemID targetID, Clip::Array recordedClips)
{
    recordingIsStoppingFlag = false;
    listeners.call (&Listener::recordingFinished, in, targetID, recordedClips);
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

bool TransportControl::isAllowedToReallocate() const noexcept
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
    inhibitors = std::max (0, inhibitors - 1);
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

    const auto start = position.get();

    if (playbackContext == nullptr)
    {
        playbackContext = std::make_unique<EditPlaybackContext> (*this);
        playbackContext->createPlayAudioNodes (start);
        transportState->playbackContextAllocation = transportState->playbackContextAllocation + 1;
    }

    if (alwaysReallocate)
        playbackContext->createPlayAudioNodes (start);
    else
        playbackContext->createPlayAudioNodesIfNeeded (start);
}

void TransportControl::freePlaybackContext()
{
    playbackContext.reset();
    clearPlayingFlags();
    transportState->playbackContextAllocation = std::max (0, transportState->playbackContextAllocation - 1);
}

void TransportControl::triggerClearDevicesOnStop()
{
    transportState->clearDevicesOnStop = true;

    if (isPlaying() || edit.isRendering())
        return;

    stop (false, true);
    ensureContextAllocated();
}

void TransportControl::forceOrphanFreezeAndProxyFilesPurge()
{
    fileFlushTimer->forcePurge = true;
}

//==============================================================================
void TransportControl::play (bool justSendMMCIfEnabled)
{
    transportState->play (justSendMMCIfEnabled);
}

void TransportControl::playFromStart (bool justSendMMCIfEnabled)
{
    setPosition (startPosition);
    TransportHelpers::resyncLauncherClips (*this, {}, true);
    play (justSendMMCIfEnabled);
}

void TransportControl::playSectionAndReset (TimeRange rangeToPlay)
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
                             bool canSendMMCStop)
{
    transportState->stop (discardRecordings,
                          clearDevices,
                          canSendMMCStop);
}

void TransportControl::stopIfRecording()
{
    if (isRecording())
        stop (false, false);
}

void TransportControl::stopRecording (bool discardRecordings)
{
    if (! isRecording())
        return;

    transportState->discardRecordings = discardRecordings;
    transportState->recording = false;
}

juce::Result TransportControl::applyRetrospectiveRecord (bool armedOnly)
{
    if (static_cast<int> (engine.getPropertyStorage().getProperty (SettingID::retrospectiveRecord, 30)) == 0)
        return juce::Result::fail (TRANS("Retrospective record is currently disabled"));

    if (playbackContext)
    {
        juce::Array<Clip*> clips;
        return playbackContext->applyRetrospectiveRecord (&clips, armedOnly);
    }

    return juce::Result::fail (TRANS("No active audio devices"));
}

juce::Array<juce::File> TransportControl::getRetrospectiveRecordAsAudioFiles()
{
    if (static_cast<int> (engine.getPropertyStorage().getProperty (SettingID::retrospectiveRecord, 30)) == 0)
        return {};

    if (playbackContext)
    {
        juce::Array<juce::File> files;
        juce::Array<Clip*> clips;
        playbackContext->applyRetrospectiveRecord (&clips);

        if (clips.size() > 0)
        {
            for (auto c : clips)
            {
                if (auto ac = dynamic_cast<WaveAudioClip*> (c))
                {
                    auto f = ac->getOriginalFile();
                    files.add (f);
                }
                else if (auto mc = dynamic_cast<MidiClip*> (c))
                {
                    auto clipPos = mc->getPosition();

                    juce::Array<Clip*> clipsToRender;
                    clipsToRender.add (mc);

                    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory);

                    auto f = dir.getNonexistentChildFile (juce::File::createLegalFileName (mc->getName()), ".wav");

                    juce::BigInteger tracksToDo;
                    int idx = 0;

                    for (auto t : getAllTracks (edit))
                    {
                        if (mc->getTrack() == t)
                            tracksToDo.setBit (idx);

                        idx++;
                    }

                    Renderer::renderToFile (TRANS("Render Clip"), f, edit, clipPos.time,
                                            tracksToDo, true, false, clipsToRender, true);

                    files.add (f);
                }

                c->removeFromParent();
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

            auto barsBeats = tempoSequence.toBarsAndBeats (targetContext->isLooping()
                                                            ? targetContext->getLoopTimes().getStart()
                                                            : position);

            auto previousBarTime = tempoSequence.toTime ({ barsBeats.bars, {} });

            auto syncInterval = isPreview ? targetContext->getLoopTimes().getLength()
                                          : TimeDuration::fromSeconds ((60.0 / tempo.getBpm() * timeSig.numerator));

            playbackContext->syncToContext (targetContext, previousBarTime, syncInterval);
        }
    }
}

bool TransportControl::isPlaying() const                { return transportState->playing; }
bool TransportControl::isRecording() const              { return transportState->recording; }
bool TransportControl::isSafeRecording() const          { return isRecording() && transportState->safeRecording; }
bool TransportControl::isStopping() const               { return isStopInProgress; }
bool TransportControl::isRecordingStopping() const      { return recordingIsStoppingFlag; }


TimePosition TransportControl::getTimeWhenStarted() const   { return transportState->startTime.get(); }

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

    if (isPlaying() && playHeadWrapper->getPosition() >= Edit::getMaximumEditEnd())
    {
        stop (false, false);
        position = Edit::getMaximumEditEnd();
        return;
    }

    if (! playHeadWrapper->isPlaying())
    {
        if (isRecording())
        {
            stop (false, false);
            return;
        }

        if (isPlaying())
            stop (false, false);
    }
    else if (! isPlaying())
    {
        // Playhead is playing but transport state is stopped so start playing
        play (false);
    }

    // Update the transport state from the playhead if we have one
    if (isPlayContextActive()
         && (! transportState->userDragging)
         && juce::Time::getMillisecondCounter() - transportState->lastUserDragTime > 200)
    {
        // Only update if we're not looping or we're playing as otherwise the transport
        // position will jump and be stuck at the loop in position.
        // The other way to fix this mught be to change the play head to only snap the position on play start..
        if (! looping || isPlaying())
        {
            const auto currentTime = playHeadWrapper->getLiveTransportPosition();
            transportState->setVideoPosition (currentTime, false);
            transportState->updatePositionFromPlayhead (currentTime);
        }
    }

    // Periodically update the loop times from the transport state
    if (--loopUpdateCounter == 0)
    {
        loopUpdateCounter = 10;

        if (looping)
        {
            auto lr = getLoopRange();
            lr = lr.withEnd (std::max (lr.getEnd(), lr.getStart() + 0.001s));
            playHeadWrapper->setLoopTimes (true, lr);
        }
        else
        {
            playHeadWrapper->setLoopTimes (false, {});
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
TimePosition TransportControl::getPosition() const
{
    return position.get();
}

void TransportControl::setPosition (TimePosition t)
{
    // This drag time update is here to avoid the transport position being updated
    // from the playhead before the position has a chance to be dispatched by it
    transportState->lastUserDragTime = juce::Time::getMillisecondCounter();
    position = t;
}

void TransportControl::setPosition (TimePosition timeToMoveTo, TimePosition timeToPerformJump)
{
    if (auto epc = getCurrentPlaybackContext())
        epc->postPosition (timeToMoveTo, timeToPerformJump);

    sendChangeMessage();
}

void TransportControl::setUserDragging (bool b)
{
    CRASH_TRACER

    if (playbackContext != nullptr)
        playHeadWrapper->setUserIsDragging (b);

    if (b != transportState->userDragging)
    {
        if (transportState->userDragging && isPlaying())
        {
            edit.getAutomationRecordManager().punchOut (false);

            if (playbackContext != nullptr)
                playHeadWrapper->setPosition (position);
        }

        transportState->userDragging = b;

        if (b)
            transportState->lastUserDragTime = juce::Time::getMillisecondCounter();
    }
}

bool TransportControl::isUserDragging() const noexcept
{
    return transportState->userDragging;
}

bool TransportControl::isPositionUpdatingFromPlayhead() const
{
    return transportState->updatingFromPlayHead;
}

//==============================================================================
void TransportControl::setLoopIn (TimePosition t)
{
    setLoopPoint1 (std::max (std::max (loopPoint1.get(), loopPoint2.get()), std::max (TimePosition(), t)));
    setLoopPoint2 (std::max (TimePosition(), t));
}

void TransportControl::setLoopOut (TimePosition t)
{
    setLoopPoint1 (std::min (std::min (loopPoint1.get(), loopPoint2.get()), std::max (TimePosition(), t)));
    setLoopPoint2 (std::max (TimePosition(), t));
}

void TransportControl::setLoopPoint1 (TimePosition t)
{
    loopPoint1 = juce::jlimit (0_tp, toPosition (edit.getLength() + Edit::getMaximumLength() * 0.75), t);
}

void TransportControl::setLoopPoint2 (TimePosition t)
{
    loopPoint2 = juce::jlimit (0_tp, toPosition (edit.getLength() + Edit::getMaximumLength() * 0.75), t);
}

void TransportControl::setLoopRange (TimeRange times)
{
    auto maxEndTime = toPosition (edit.getLength() + Edit::getMaximumLength() * 0.75);

    loopPoint1 = juce::jlimit (0_tp, maxEndTime, times.getStart());
    loopPoint2 = juce::jlimit (0_tp, maxEndTime, times.getEnd());
}

TimeRange TransportControl::getLoopRange() const noexcept
{
    return TimeRange::between (loopPoint1, loopPoint2);
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
                transportState->setVideoPosition (getPosition(), true);
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

void TransportControl::sendMMC (const juce::MidiMessage& mmc)
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

void TransportControl::sendMMCCommand (juce::MidiMessage::MidiMachineControlCommand command)
{
    sendMMC (juce::MidiMessage::midiMachineControlCommand (command));
}

inline bool anyEnabledMidiOutDevicesSendingMMC (DeviceManager& dm)
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
        sendMMCCommand (juce::MidiMessage::mmc_play);

        if (edit.isTimecodeSyncEnabled())
            return true;
    }

    return false;
}

bool TransportControl::sendMMCStartRecord()
{
    if (anyEnabledMidiOutDevicesSendingMMC (engine.getDeviceManager()))
    {
        sendMMCCommand (juce::MidiMessage::mmc_recordStart);

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
            const auto cursorPos = position.get();
            const auto loopRange = getLoopRange();

            if (cursorPos < loopRange.getStart()
                || cursorPos > loopRange.getEnd() - 0.1s)
            {
                position = loopRange.getStart();
            }

            transportState->startTime = loopRange.getStart();
            transportState->endTime   = loopRange.getEnd();

            if (transportState->endTime < transportState->startTime + 0.01s)
            {
                engine.getUIBehaviour().showWarningMessage (TRANS("Can't play in loop mode unless the in/out markers are further apart"));
                return;
            }
        }
        else
        {
            transportState->startTime = position.get();
            transportState->endTime   = Edit::getMaximumEditEnd();
        }

        if (edit.getAbletonLink().isConnected())
        {
            const double barLength = edit.tempoSequence.getTimeSig(0)->numerator;
            const double beatsUntilNextLinkCycle = edit.getAbletonLink().getBeatsUntilNextCycle (barLength);

            const double cyclePos = std::fmod (transportState->startTime.get().inSeconds(), barLength);
            const double nextLinkCycle = edit.tempoSequence.toTime (BeatPosition::fromBeats (beatsUntilNextLinkCycle)).inSeconds();

            transportState->startTime = TimePosition::fromSeconds ((transportState->startTime.get().inSeconds() - cyclePos) + (barLength - nextLinkCycle));
        }

        transportState->recording = false;
        transportState->safeRecording = false;
        playingFlag = std::make_unique<PlayingFlag> (engine);

        ensureContextAllocated();

        if (playbackContext)
        {
            playHeadWrapper->play ({ transportState->startTime, transportState->endTime }, looping);

            // Post the position change to be dispatched otherwise what we're effectively doing is setting
            // the position for "this" block and it will get incremented the next block, actually starting
            // playback 1 block from the start
            playHeadWrapper->setPosition (position);
        }
        else
        {
            clearPlayingFlags();
        }

        edit.setClickTrackRange ({});
    }
}

std::optional<std::pair<SyncPoint, std::optional<TimeRange>>> TransportControl::performRecord()
{
    if (! edit.shouldPlay())
        return std::nullopt;

    CRASH_TRACER
    sectionPlayer.reset();
    std::optional<SyncPoint> punchInPoint;
    std::optional<TimeRange> punchRange;

    if (! transportState->userDragging)
    {
        if (transportState->justSendMMCIfEnabled && sendMMCStartRecord())
            return std::nullopt;

        if (transportState->allowRecordingIfNoInputsArmed || areAnyInputsRecording())
        {
            // If we're already playing, just start the armed inputs recording and enable the click track
            if (isPlaying())
            {
                assert (playbackContext);

                punchInPoint = playbackContext->getSyncPoint();

                if (edit.recordingPunchInOut)
                    punchRange = getLoopRange();

                const auto currentPos = playbackContext->getPosition();
                const auto punchInTime = edit.recordingPunchInOut ? getLoopRange().getStart() : currentPos;

                playbackContext->prepareForRecording (currentPos, punchInTime);
                transportState->safeRecording = engine.getPropertyStorage().getProperty (SettingID::safeRecord, false);

                sendChangeMessage();
            }
            else
            {
                const auto loopRange = getLoopRange();
                transportState->startTime   = position.get();
                transportState->endTime     = Edit::getMaximumEditEnd();

                if (looping)
                {
                    if (loopRange.getLength() < 2s)
                    {
                        engine.getUIBehaviour().showWarningMessage (TRANS("To record in loop mode, the length of loop must be greater than 2 seconds."));
                        return std::nullopt;
                    }

                    if (edit.recordingPunchInOut)
                    {
                        engine.getUIBehaviour().showWarningMessage (TRANS("Recording can be done in either loop mode or punch in/out mode, but not both at the same time!"));
                        return std::nullopt;
                    }

                    transportState->startTime = loopRange.getStart();
                }
                else if (edit.recordingPunchInOut)
                {
                    if ((loopRange.getEnd() + 0.1s) <= transportState->startTime)
                        transportState->startTime = (loopRange.getStart() - 1.0s);
                }
                else
                {
                    if (abs (transportState->startTime) < 0.005s)
                        transportState->startTime = 0s;
                }

                auto prerollStart = transportState->startTime.get();
                double numCountInBeats = edit.getNumCountInBeats();
                const auto& ts = edit.tempoSequence;

                if (numCountInBeats > 0)
                {
                    auto currentBeat = ts.toBeats (transportState->startTime);
                    prerollStart = ts.toTime (currentBeat - BeatDuration::fromBeats (numCountInBeats + 0.5));
                    // N.B. this +0.5 beats here specifies the behaviour further down when setting the click range.
                    // If this changes, that will also need to change.
                }

                if (edit.getAbletonLink().isConnected())
                {
                    double barLength = ts.getTimeSig (0)->numerator;
                    double beatsUntilNextLinkCycle = edit.getAbletonLink().getBeatsUntilNextCycle (barLength);

                    if (numCountInBeats > 0)
                        beatsUntilNextLinkCycle -= 0.5;

                    prerollStart = prerollStart - toDuration (ts.toTime (BeatPosition::fromBeats (beatsUntilNextLinkCycle)));
                }

                playingFlag = std::make_unique<PlayingFlag> (engine);
                transportState->safeRecording = engine.getPropertyStorage().getProperty (SettingID::safeRecord, false);

                edit.updateMidiTimecodeDevices();

                ensureContextAllocated();

                if (playbackContext)
                {
                    if (edit.getNumCountInBeats() > 0)
                        playHeadWrapper->setLoopTimes (true, { transportState->startTime.get(), Edit::getMaximumEditEnd() });

                    if (looping)
                    {
                        // The order of this is critical as the audio thread might jump in and reset the
                        // roll-in-to-loop status of the loop-range is not set first
                        auto lr = getLoopRange();
                        lr = lr.withEnd (std::max (lr.getEnd(), lr.getStart() + 0.001s));
                        playHeadWrapper->setLoopTimes (true, lr);
                        playHeadWrapper->setRollInToLoop (prerollStart);
                    }
                    else
                    {
                        // Set the playhead loop times before preparing the context as this will be used by
                        // the RecordingContext to initialise itself
                        playHeadWrapper->setLoopTimes (false, { prerollStart, transportState->endTime.get() });
                    }

                    playHeadWrapper->setPosition (prerollStart);
                    position = prerollStart;

                    // Prepare the recordings after the playhead has been setup to avoid synchronisation problems
                    {
                        playbackContext->blockUntilSyncPointChange();

                        if (edit.recordingPunchInOut)
                            punchRange = getLoopRange();

                        const auto currentSyncPoint = playbackContext->getSyncPoint();
                        const auto punchInTime = transportState->startTime.get();
                        const auto punchInBeat = ts.toBeats (punchInTime);
                        const auto timeNow = currentSyncPoint->time;
                        const auto beatNow = ts.toBeats (timeNow);
                        const auto beatsUntilPunchIn = punchInBeat - beatNow;
                        const auto samplesUntilPunchIn = toSamples (punchInTime - timeNow, playbackContext->getSampleRate());
                        punchInPoint = SyncPoint { .referenceSamplePosition = currentSyncPoint->referenceSamplePosition + samplesUntilPunchIn,
                                                   .monotonicBeat = { currentSyncPoint->monotonicBeat.v + beatsUntilPunchIn },
                                                   .unloopedTime = punchInTime,
                                                   .time = transportState->startTime.get(),
                                                   .beat = ts.toBeats (transportState->startTime.get()) } ;
                        jassert (juce::approximatelyEqual (currentSyncPoint->time.inSeconds(), currentSyncPoint->unloopedTime.inSeconds()));

                        TransportHelpers::resyncLauncherClips (*this, punchInPoint, false);
                    }

                    playbackContext->prepareForRecording (prerollStart, transportState->startTime.get());

                    if (edit.getNumCountInBeats() > 0)
                    {
                        // As the pre-roll will be "num count in beats - 0.5" we have to add that back on before our calculation
                        // We also roll back 0.5 beats the end time to avoid hearing a block that starts directly or just before a beat
                        const auto clickStartBeat = ts.toBeats (prerollStart);
                        const auto clickEndBeat = ts.toBeats (transportState->startTime.get());

                        edit.setClickTrackRange (ts.toTime ({ BeatPosition::fromBeats (std::ceil (clickStartBeat.inBeats() + 0.5)),
                                                              BeatPosition::fromBeats (std::ceil (clickEndBeat.inBeats())) - 0.5_bd }));
                    }
                    else
                    {
                        edit.setClickTrackRange ({});
                    }

                    playHeadWrapper->setPosition (prerollStart);
                    playHeadWrapper->postPlay();
                    transportState->playing = true; // N.B. set these after the devices have been rebuilt and the playingFlag has been set
                    screenSaverDefeater = std::make_unique<ScreenSaverDefeater>();
                }
            }
        }
        else
        {
            engine.getUIBehaviour().showWarningMessage (
                TRANS("Recording is only possible  when at least one active input device is assigned to a track"));

            return std::nullopt;
        }
    }

    if (! transportState->justSendMMCIfEnabled)
        sendMMCCommand (juce::MidiMessage::mmc_recordStart);

    if (transportState->safeRecording)
        engine.getUIBehaviour().showSafeRecordDialog (*this);

    assert (punchInPoint);
    return std::make_pair (*punchInPoint, punchRange);
}

std::optional<SyncPoint> TransportControl::performStopRecording()
{
    if (! playbackContext)
        return std::nullopt;

    // This "! isRecording()" is backwards as  it's in response to the recording state being turned off
    // This is messy and will be cleaned up soon
    if (! isRecording() || tracktion::isRecording (*playbackContext))
    {
        CRASH_TRACER

        const bool discardRecordings = transportState->discardRecordings;
        const auto syncPoint = playbackContext->getSyncPoint();
        assert (syncPoint);
        playbackContext->stopRecording (syncPoint->unloopedTime, discardRecordings)
            .map_error ([this] (auto err) { engine.getUIBehaviour().showWarningAlert (TRANS("Recording"), err); });

        sendChangeMessage();
        listeners.call (&TransportControl::Listener::recordingStopped, *syncPoint, discardRecordings);

        return syncPoint;
    }

    return std::nullopt;
}

void TransportControl::performStop()
{
    CRASH_TRACER

    const juce::ScopedValueSetter<bool> svs (isStopInProgress, true);
    screenSaverDefeater.reset();
    sectionPlayer.reset();

    engine.getUIBehaviour().hideSafeRecordDialog (*this);

    if (playbackContext == nullptr)
    {
        jassert (! (isPlaying() || isRecording()));
        clearPlayingFlags();
        return;
    }

    if (! juce::Component::isMouseButtonDownAnywhere())
        setUserDragging (false); // in case it gets stuck

    if (isRecording() || tracktion::isRecording (*playbackContext))
    {
        CRASH_TRACER

        // grab this before stopping the playhead
        auto recEndTime = playHeadWrapper->getUnloopedPosition();
        auto recEndPos  = playHeadWrapper->getPosition();

        clearPlayingFlags();
        playHeadWrapper->stop();
        auto syncPoint = playbackContext->getSyncPoint();
        assert (syncPoint);
        playbackContext->stopRecording (recEndTime, transportState->discardRecordings)
            .map_error ([this] (auto err) { engine.getUIBehaviour().showWarningAlert (TRANS("Recording"), err); });

        position = transportState->discardRecordings ? transportState->startTime.get()
                                                     : (looping ? recEndPos
                                                                : recEndTime);

        listeners.call (&TransportControl::Listener::recordingStopped, *syncPoint, transportState->discardRecordings);
    }
    else
    {
        if (transportState->discardRecordings)
            engine.getUIBehaviour().showWarningMessage (TRANS("Can only abort a recording when something's actually recording."));

        clearPlayingFlags();
        playHeadWrapper->stop();
    }

    if (transportState->clearDevices || ! edit.playInStopEnabled || transportState->clearDevicesOnStop)
        releaseAudioNodes();
    else
        ensureContextAllocated();

    transportState->clearDevicesOnStop = false;

    if (transportState->canSendMMCStop)
        sendMMCCommand (juce::MidiMessage::mmc_stop);
}

void TransportControl::performPositionChange()
{
    CRASH_TRACER

    sectionPlayer.reset();
    edit.getAutomationRecordManager().punchOut (false);

    if (isRecording())
        stop (false, false);

    auto newPos = TimePosition::fromSeconds (static_cast<double> (state[IDs::position]));

    if (isPlaying() && looping)
    {
        auto range = getLoopRange();
        newPos = juce::jlimit (range.getStart(), range.getEnd(), newPos);
    }
    else
    {
        const auto minStartTime = edit.tempoSequence.toTime (-BeatPosition::fromBeats (edit.getNumCountInBeats())) - 0.5s;
        newPos = juce::jlimit (minStartTime, Edit::getMaximumEditEnd(), newPos);
    }

    if (playbackContext != nullptr)
        playHeadWrapper->setPosition (newPos);

    position = newPos;

    yieldGUIThread();

    if (! transportState->userDragging)
        transportState->lastUserDragTime = juce::Time::getMillisecondCounter();

    transportState->setVideoPosition (newPos, true);

    // MMC
    const double nudge = 0.05 / 96000.0;
    const double mmcTime = std::max (0_tp, newPos + edit.getTimecodeOffset()).inSeconds() + nudge;
    const int framesPerSecond = edit.getTimecodeFormat().getFPS();
    const int frames  = ((int) (mmcTime * framesPerSecond)) % framesPerSecond;
    const int hours   = (int) (mmcTime * (1.0 / 3600.0));
    const int minutes = (((int) mmcTime) / 60) % 60;
    const int seconds = (((int) mmcTime) % 60);

    sendMMC (juce::MidiMessage::midiMachineControlGoto (hours, minutes, seconds, frames));
}

void TransportControl::performRewindButtonChanged()
{
    const bool isDown = transportState->rewindButtonDown;
    rwRepeater->setDown (isDown);

    if (isDown)
        sendMMCCommand (juce::MidiMessage::mmc_rewind);
    else
        sendMMCCommand (isPlaying() ? juce::MidiMessage::mmc_play
                                    : juce::MidiMessage::mmc_stop);
}

void TransportControl::performFastForwardButtonChanged()
{
    const bool isDown = transportState->fastForwardButtonDown;
    ffRepeater->setDown (isDown);

    if (isDown)
        sendMMCCommand (juce::MidiMessage::mmc_fastforward);
    else
        sendMMCCommand (isPlaying() ? juce::MidiMessage::mmc_play
                                    : juce::MidiMessage::mmc_stop);
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
static TimeRange getLimitsOfSelectedClips (Edit& edit, const SelectableList& items)
{
    auto range = getTimeRangeForSelectedItems (items);

    if (range.isEmpty())
        return { {}, edit.getLength() };

    return range;
}

void toStart (TransportControl& tc, const SelectableList& items)
{
    auto selectionStart = getLimitsOfSelectedClips (tc.edit, items).getStart();
    tc.setPosition (tc.getPosition() < selectionStart + 0.001s ? 0_tp : selectionStart);
}

void toEnd (TransportControl& tc, const SelectableList& items)
{
    auto selectionEnd = getLimitsOfSelectedClips (tc.edit, items).getEnd();
    tc.setPosition (tc.getPosition() > selectionEnd - 0.001s ? toPosition (tc.edit.getLength()) : selectionEnd);
}

void tabBack (TransportControl& tc)     { tc.setPosition (tc.edit.getPreviousTimeOfInterest (tc.getPosition() - 0.001s)); }
void tabForward (TransportControl& tc)  { tc.setPosition (tc.edit.getNextTimeOfInterest     (tc.getPosition() + 0.001s)); }

void markIn (TransportControl& tc)      { tc.setLoopIn  (tc.getPosition()); }
void markOut (TransportControl& tc)     { tc.setLoopOut (tc.getPosition()); }

void scrub (TransportControl& tc, double units)
{
    CRASH_TRACER
    const auto unitSize = tc.scrubInterval.get();
    auto timeToMove = unitSize * units;
    auto t = tc.getPosition() + timeToMove;

    if (tc.snapToTimecode)
    {
        if (timeToMove > 0s)
            t = TransportHelpers::snapTimeUp (tc, t, false);
        else
            t = TransportHelpers::snapTimeDown (tc, t, false);
    }

    if (tc.isUserDragging() && tc.engine.getPropertyStorage().getProperty (SettingID::snapCursor, false))
        t = TransportHelpers::snapTimeDown (tc, t, false);

    tc.setPosition (std::max (0_tp, t));
}

void freePlaybackContextIfNotRecording (TransportControl& tc)
{
    if (tc.isPlayContextActive() && ! tc.isRecording())
        tc.freePlaybackContext();
}

}} // namespace tracktion { inline namespace engine
