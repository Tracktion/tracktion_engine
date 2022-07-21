/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

struct AbletonLink::ImplBase  : public juce::Timer
{
    ImplBase (TransportControl& t)
        : transport (t)
    {}

    bool isConnected() const
    {
        return numPeers > 0;
    }

    void activateTimer (bool isActive)
    {
        if (isActive)
        {
            startTimer (12);
        }
        else
        {
            stopTimer();
            setSpeedCompensation (0.0);
        }
    }

    void syncronise (TimePosition pos)
    {
        if (! isConnected() || ! transport.isPlaying())
            return;

        // Get the current tempo info
        const auto& seq = transport.edit.tempoSequence.getInternalSequence();
        const auto localBarsAndBeatsPos = seq.toBarsAndBeats (pos);
        const auto numerator = static_cast<double> (localBarsAndBeatsPos.numerator);

        // Calculate any device latency and custom latency
        const double bps = seq.getBeatsPerSecondAt (pos).v;
        auto& dm = transport.engine.getDeviceManager();
        const auto outputLatencyBeats = ((dm.getBlockLength() * 2).inSeconds() + dm.getOutputLatencySeconds()) * bps;
        const auto customOffsetBeats = (customOffsetMs / 1000.0) * bps;

        // Calculate the bar phase from the transport position
        const auto outputLatencyPhase = outputLatencyBeats / numerator;
        const auto customOffsetPhase = customOffsetBeats / numerator;

        auto localPhase = (localBarsAndBeatsPos.beats.inBeats() / numerator)
                            - outputLatencyPhase + customOffsetPhase;
        localPhase = negativeAwareFmod (localPhase, 1.0);

        const auto linkPhase = getBarPhase (numerator) / numerator;
        linkBarPhase = linkPhase;

        // Find the phase offset between tracktion
        // If this is 0 this means perfect phase, > 0 means link is ahead, < 0 means tracktion is ahead
        auto offsetPhase = linkPhase - localPhase;
        chaseProportion = offsetPhase;

        const auto offsetBeats = offsetPhase * numerator;
        const auto timeNow = juce::Time::getMillisecondCounter();

        // If we're out of sync by more than a beat, re-sync by jumping
        if (std::abs (offsetBeats) >= (numerator / 2.0) && timeNow > inhibitTimer)
        {
            inhibitTimer = timeNow + 250;

            setSpeedCompensation (0.0);
            listeners.call (&Listener::linkRequestedPositionChange, BeatDuration::fromBeats (offsetBeats));
        }
        else
        {
            setSpeedCompensation (offsetPhase);
        }
    }

    void timerCallback() override
    {
    }

    void setTempoFromLink (double bpm)
    {
        Edit::WeakRef bailOut (&transport.edit);
        bpm = getTempoInRange (bpm);

        juce::MessageManager::callAsync ([this, bailOut, bpm]
        {
            if (bailOut != nullptr)
            {
                inhibitTimer = juce::Time::getMillisecondCounter() + 100;
                listeners.call (&Listener::linkRequestedTempoChange, bpm);
            }
        });
    }

    void setStartStopFromLink (bool isPlaying)
    {
        juce::MessageManager::callAsync ([this, bailOut = Edit::WeakRef (&transport.edit), isPlaying]
        {
            if (bailOut == nullptr)
                return;

            inhibitTimer = juce::Time::getMillisecondCounter() + 100;
            listeners.call (&Listener::linkRequestedStartStopChange, isPlaying);
        });
    }

    void callConnectionChanged()
    {
        Edit::WeakRef bailOut (&transport.edit);

        juce::MessageManager::callAsync ([this, bailOut]
        {
            if (bailOut != nullptr)
                listeners.call (&AbletonLink::Listener::linkConnectionChanged);
        });
    }

    double getBeatsUntilNextCycle (double quantum)
    {
        return quantum - getBarPhase (quantum);
    }

    void setCustomOffset (int offsetMs)
    {
        customOffsetMs = offsetMs;
    }

    void setTempoConstraint (juce::Range<double> minMaxTempo)
    {
        allowedTempos = minMaxTempo;

        if (isConnected())
            setTempoFromLink (getTempoFromLink());
    }

    double getTempoInRange (double bpm) const
    {
        if (bpm == 0.0)
            return 0;

        const auto isTooQuick = [topBpm = allowedTempos.getEnd()] (double b) { return b >= topBpm; };
        const bool wasTooQuick = isTooQuick (bpm); // Avoid infinite loops

        while (isTempoOutOfRange (bpm) && (wasTooQuick == isTooQuick (bpm)))
            bpm *= wasTooQuick ? 0.5 : 2.0;

        jassert (! isTempoOutOfRange (bpm)); // Tempo can't be halved/doubled to fit in the range

        return allowedTempos.clipValue (bpm);
    }

    bool isTempoOutOfRange (double bpm) const
    {
        return ! (allowedTempos.getStart() <= bpm && bpm <= allowedTempos.getEnd());
    }

    virtual bool isEnabled() const = 0;
    virtual void setEnabled (bool) = 0;
    virtual bool isPlaying() const = 0;
    virtual void enableStartStopSync (bool) = 0;
    virtual bool getStartStopSyncEnabledFromLink() const = 0;
    virtual void setStartStopToLink (bool) = 0;
    virtual void setTempoToLink (double bpm) = 0;
    virtual double getTempoFromLink() = 0;
    virtual double getBeatNow (double quantum) = 0;
    virtual double getBarPhase (double quantum) = 0;

    void addListener    (Listener* l) { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    static inline double negativeAwareFmod (double a, double b)
    {
        return a - b * std::floor (a / b);
    }

    void setSpeedCompensation (double phaseProportion)
    {
        if (auto epc = transport.getCurrentPlaybackContext())
        {
           #if TRACKTION_ENABLE_REALTIME_TIMESTRETCHING
            epc->setTempoAdjustment (phaseProportion * 10.0);
           #else
            const double speedComp = juce::jlimit (-10.0, 10.0, phaseProportion * 1000.0);
            epc->setSpeedCompensation (speedComp);
           #endif
        }
    }

    TransportControl& transport;
    juce::ListenerList<Listener> listeners;
    std::atomic<size_t> numPeers { 0 };
    std::atomic<int> customOffsetMs { 0 };
    uint32_t inhibitTimer = 0;
    std::atomic<double> linkBarPhase { 0.0 }, chaseProportion { 0.0 };

    juce::Range<double> allowedTempos { 0.0, 999.0 };
};

#if TRACKTION_ENABLE_ABLETON_LINK

#if (JUCE_WINDOWS || JUCE_MAC || JUCE_LINUX || JUCE_ANDROID)
    //==========================================================================
    struct LinkImpl  : public AbletonLink::ImplBase
    {
        LinkImpl (TransportControl& t)
            : AbletonLink::ImplBase (t)
        {
            link.setNumPeersCallback ([this] (std::size_t n) { numPeersCallback (n); });
            link.setTempoCallback ([this] (double bpm) { setTempoFromLink (bpm); });
            link.setStartStopCallback ([this] (bool isPlaying) { setStartStopFromLink (isPlaying); });
        }

        ~LinkImpl() override
        {
            link.setNumPeersCallback ([] (std::size_t) {});
            link.setTempoCallback ([] (double) {});
            link.setStartStopCallback ([] (bool) {});
        }

        bool isEnabled() const override
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            return link.isEnabled();
        }

        void setEnabled (bool isEnabled) override
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            link.enable (isEnabled);
            activateTimer (isEnabled);

            if (isEnabled)
                setTempoFromLink (link.captureAppSessionState().tempo());
        }

        bool isPlaying() const override
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            return link.captureAppSessionState().isPlaying();
        }

        void enableStartStopSync (bool enable) override
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            link.enableStartStopSync (enable);
        }

        bool getStartStopSyncEnabledFromLink() const override
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            return link.isStartStopSyncEnabled();
        }

        void numPeersCallback (std::size_t newNumPeers)
        {
            jassert (! juce::MessageManager::existsAndIsCurrentThread());
            numPeers = newNumPeers;
            callConnectionChanged();

            if (isConnected())
                setTempoFromLink (link.captureAudioSessionState().tempo());
        }

        void setStartStopToLink (bool isPlaying) override
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            auto state = link.captureAppSessionState();
            state.setIsPlaying (isPlaying, clock.micros());
            link.commitAppSessionState (state);
        }

        void setTempoToLink (double bpm) override
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            auto state = link.captureAppSessionState();
            state.setTempo (bpm, clock.micros());
            link.commitAppSessionState (state);
        }

        double getTempoFromLink() override
        {
            jassert (! juce::MessageManager::existsAndIsCurrentThread());
            return link.captureAudioSessionState().tempo();
        }

        double getBeatNow (double quantum) override
        {
            jassert (! juce::MessageManager::existsAndIsCurrentThread());
            return link.captureAudioSessionState().beatAtTime (clock.micros(), quantum);
        }

        double getBarPhase (double quantum) override
        {
            if (juce::MessageManager::existsAndIsCurrentThread())
                return link.captureAppSessionState().phaseAtTime (clock.micros(), quantum);

            return link.captureAudioSessionState().phaseAtTime (clock.micros(), quantum);
        }

        ableton::Link::Clock clock;
        ableton::Link link { 120 };
    };

#elif JUCE_IOS

    // To use Link on iOS you need to get access to the LinkKit repo from
    // Ableton, add its include folder to your header search paths, and link to
    // the libABLLink.a static library.
    #include "ABLLink.h"

    struct LinkImpl  : public AbletonLink::ImplBase
    {
        LinkImpl (TransportControl& t)
            : AbletonLink::ImplBase (t),
              link (ABLLinkNew (120))
        {
            ABLLinkSetSessionTempoCallback (link, tempoChangedCallback, this);
            ABLLinkSetIsConnectedCallback (link, isConnectedCallback, this);
            ABLLinkSetIsEnabledCallback (link, isEnabledCallback, this);
            ABLLinkSetStartStopCallback (link, startStopCallback, this);

            setEnabled (isActive);
        }

        ~LinkImpl() override
        {
             ABLLinkDelete (link);
        }

        bool isEnabled() const override
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            return ABLLinkIsEnabled (link) && isActive;
        }

        void setEnabled (bool isEnabled) override
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            isActive = isEnabled;
            ABLLinkSetActive (link, isEnabled);

            // We don't necessarily get an isConnectedCallback callback after
            // enabling, make sure everything is up to date.
            Edit::WeakRef bailOut (&transport.edit);

            Timer::callAfterDelay (500, [this, bailOut]
            {
                if (bailOut != nullptr)
                    isConnectedCallback (ABLLinkIsConnected (link), this);
            });
        }

        bool isPlaying() const override
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            return ABLLinkIsPlaying (ABLLinkCaptureAppSessionState (link));
        }

        void enableStartStopSync (bool) override
        {
            jassertfalse; // This is only settable via the system prefs
        }

        bool getStartStopSyncEnabledFromLink() const override
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            return ABLLinkIsStartStopSyncEnabled (link);
        }

        void setStartStopToLink (bool isPlaying) override
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            auto state = ABLLinkCaptureAppSessionState (link);
            ABLLinkSetIsPlaying (state, isPlaying, (std::uint64_t) juce::Time::getHighResolutionTicks());
            ABLLinkCommitAppSessionState (link, state);
        }

        void setTempoToLink (double bpm) override
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            auto state = ABLLinkCaptureAppSessionState (link);
            ABLLinkSetTempo (state, bpm, (std::uint64_t) juce::Time::getHighResolutionTicks());
            ABLLinkCommitAppSessionState (link, state);
        }

        double getTempoFromLink() override
        {
            jassert (! juce::MessageManager::existsAndIsCurrentThread());
            return ABLLinkGetTempo (ABLLinkCaptureAudioSessionState (link));
        }

        double getBeatNow (double quantum) override
        {
            jassert (! juce::MessageManager::existsAndIsCurrentThread());
            auto state = ABLLinkCaptureAudioSessionState (link);
            return ABLLinkBeatAtTime (state, (std::uint64_t) juce::Time::getHighResolutionTicks(), quantum);
        }

        double getBarPhase (double quantum) override
        {
            auto state = juce::MessageManager::existsAndIsCurrentThread()
                            ? ABLLinkCaptureAppSessionState (link)
                            : ABLLinkCaptureAudioSessionState (link);

            return ABLLinkPhaseAtTime (state, (std::uint64_t) juce::Time::getHighResolutionTicks(), quantum);
        }

        static void tempoChangedCallback (double bpm, void *context)
        {
            auto* thisPtr = static_cast<LinkImpl*> (context);
            thisPtr->setTempoFromLink (bpm);
        }

        static void isConnectedCallback (bool isConnected, void *context)
        {
            auto* thisPtr = static_cast<LinkImpl*> (context);

            thisPtr->numPeers = (size_t) (isConnected ? 1 : 0);
            thisPtr->activateTimer (isConnected);

            isEnabledCallback (isConnected, context);
        }

        static void isEnabledCallback (bool isEnabled, void *context)
        {
            auto* thisPtr = static_cast<LinkImpl*> (context);

            if (! isEnabled)
                thisPtr->numPeers = (size_t) 0;

            thisPtr->callConnectionChanged();

            if (isEnabled)
                broadcastTempo (thisPtr);
        }

        static void startStopCallback (bool isPlaying, void *context)
        {
            auto* thisPtr = static_cast<LinkImpl*> (context);
            thisPtr->setStartStopFromLink (isPlaying);
        }

        static void broadcastTempo (LinkImpl* context)
        {
            context->setTempoFromLink (context->getTempoFromLink());
        }

        ABLLinkRef link;
        static bool isActive; // Multiple edits may exist, Link is global
    };

    bool LinkImpl::isActive = true;

#endif
#endif // TRACKTION_ENABLE_ABLETON_LINK


//==============================================================================
AbletonLink::AbletonLink (TransportControl& t)
{
   #if TRACKTION_ENABLE_ABLETON_LINK
    implementation.reset (new LinkImpl (t));
   #endif

    juce::ignoreUnused (t);
}

AbletonLink::~AbletonLink() {}

#if TRACKTION_ENABLE_ABLETON_LINK && JUCE_IOS
 ABLLink* AbletonLink::getLinkInstanceForIOS()  { return static_cast<LinkImpl*>(implementation.get())->link; }
#endif

void AbletonLink::setEnabled (bool isEnabled)
{
    if (implementation != nullptr)
        implementation->setEnabled (isEnabled);
}

bool AbletonLink::isEnabled() const
{
    return implementation != nullptr && implementation->isEnabled();
}

bool AbletonLink::isConnected() const
{
    return implementation != nullptr && implementation->isConnected();
}

size_t AbletonLink::getNumPeers() const
{
    return implementation != nullptr ? implementation->numPeers.load() : 0;
}

bool AbletonLink::isPlaying() const
{
    return implementation != nullptr && implementation->isPlaying();
}

void AbletonLink::enableStartStopSync (bool enable)
{
    if (implementation != nullptr)
        implementation->enableStartStopSync (enable);
}

bool AbletonLink::isStartStopSyncEnabled() const
{
    return implementation != nullptr && implementation->getStartStopSyncEnabledFromLink();
}

double AbletonLink::getBeatsUntilNextCycle (double quantum) const
{
    if (implementation != nullptr)
        return implementation->getBeatsUntilNextCycle (quantum);

    return 0.0;
}

void AbletonLink::requestStartStopChange (bool isPlaying)
{
    if (implementation != nullptr)
        implementation->setStartStopToLink (isPlaying);
}

void AbletonLink::requestTempoChange (double newBpm)
{
    if (implementation != nullptr)
        implementation->setTempoToLink (newBpm);
}

void AbletonLink::setTempoConstraint (juce::Range<double> minMaxTempo)
{
    if (implementation != nullptr)
        implementation->setTempoConstraint (minMaxTempo);
}

double AbletonLink::getBarPhase() const
{
    return implementation != nullptr ? implementation->linkBarPhase.load() : 0.0;
}

double AbletonLink::getChaseProportion() const
{
    return implementation != nullptr ? implementation->chaseProportion.load() : 0.0;
}

double AbletonLink::getSessionTempo() const
{
    if (implementation != nullptr)
        return implementation->getTempoFromLink();

    return 120.0;
}

void AbletonLink::setCustomOffset (int offsetMs)
{
    if (implementation != nullptr)
        implementation->setCustomOffset (offsetMs);
}

void AbletonLink::syncronise (TimePosition pos)
{
    if (implementation != nullptr)
        implementation->syncronise (pos);
}

void AbletonLink::addListener (Listener* l)
{
    if (implementation != nullptr)
        implementation->addListener (l);
}

void AbletonLink::removeListener (Listener* l)
{
    if (implementation != nullptr)
        implementation->removeListener (l);
}

}} // namespace tracktion { inline namespace engine
