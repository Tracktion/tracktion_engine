/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

struct AbletonLink::ImplBase  : public juce::Timer
{
    ImplBase (TransportControl& t) : transport (t)
    {
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

    void timerCallback() override
    {
        if (! transport.isPlaying() || ! isConnected)
            return;

        auto& deviceManager = transport.engine.getDeviceManager();

        const double bps = transport.edit.tempoSequence.getBeatsPerSecondAt (getCurrentPositionSeconds());
        const double linkBps = getTempoFromLink() / 60.0;

        const double currentPosBeats = transport.edit.tempoSequence.timeToBeats (getCurrentPositionSeconds());
        const double outputLatencyBeats = deviceManager.getOutputLatencySeconds() * bps;
        const double customOffsetBeats = (customOffsetMs / 1000.0) * bps;

        const double quantum = transport.edit.tempoSequence.getTimeSig(0)->numerator;
        const double tempoDivisor = isTempoOutOfRange (linkBps * 60.0) ? (bps / linkBps) : 1.0;

        const double linkPhase = getBarPhase (quantum);
        const double localPhase = negativeAwareFmod (currentPosBeats, quantum * tempoDivisor) / tempoDivisor;

        double offset = (linkPhase - localPhase) - (outputLatencyBeats + customOffsetBeats);

        if (std::abs (offset) > (quantum * 0.5))
            offset = offset > 0 ? offset - quantum : quantum + offset;

        const auto timeNow = juce::Time::getMillisecondCounter();

        if (std::abs (offset) >= 0.25 && timeNow > inhibitTimer)
        {
            inhibitTimer = timeNow + 250;

            listeners.call (&Listener::linkRequestedPositionChange, offset * tempoDivisor);
        }
        else
        {
            const double speedComp = juce::jlimit (-10.0, 10.0, offset * 10.0);
            setSpeedCompensation (speedComp);
        }
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

    double getCurrentPositionSeconds() const
    {
        if (auto epc = transport.getCurrentPlaybackContext())
            return epc->getPosition();

        return transport.getCurrentPosition();
    }

    void setCustomOffset (int offsetMs)
    {
        customOffsetMs = offsetMs;
    }

    void setTempoConstraint (juce::Range<double> minMaxTempo)
    {
        allowedTempos = minMaxTempo;

        if (isConnected)
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
        return ! allowedTempos.contains (bpm);
    }

    virtual bool isEnabled() const = 0;
    virtual void setEnabled (bool) = 0;
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

    void setSpeedCompensation (double speedComp)
    {
         if (auto epc = transport.getCurrentPlaybackContext())
             epc->setSpeedCompensation (speedComp);
    }

    TransportControl& transport;
    juce::ListenerList<Listener> listeners;
    bool isConnected = false;
    int customOffsetMs = 0;
    uint32_t inhibitTimer = 0;

    juce::Range<double> allowedTempos { 0.0, 999.0 };
};

#if TRACKTION_ENABLE_ABLETON_LINK

#if (JUCE_WINDOWS || JUCE_MAC || JUCE_LINUX || JUCE_ANDROID)

    #if JUCE_MAC
     #define LINK_PLATFORM_MACOSX  1
    #endif

    #if JUCE_WINDOWS
     #define LINK_PLATFORM_WINDOWS 1
    #endif

    #if JUCE_LINUX || JUCE_ANDROID
     #define LINK_PLATFORM_LINUX 1
    #endif

    //==========================================================================
    // To use Link on desktop and Android, grab the open source repo from github
    // and add these folders to your project's header search paths:
    // [relative path from project]/AbletonLink/include
    // [relative path from project]/AbletonLink/modules/asio-standalone/asio/include
    //
    // If you're building on Android, you will also need the ifaddrs header and source
    // from here: https://github.com/libpd/abl_link/tree/master/external/android-ifaddrs
    // Add the folder they are in to your header search paths also.

    } // namespace tracktion_engine

    #if JUCE_CLANG // TODO: Ignore conversion errors on Windows too
     #pragma clang diagnostic push
     #pragma clang diagnostic ignored "-Wconversion"
    #endif

    #if JUCE_ANDROID
     #include <ifaddrs.h>
    #endif

    #include <ableton/Link.hpp>
    #include <ableton/link/HostTimeFilter.hpp>

    #if JUCE_ANDROID
     #include <ifaddrs.cpp>
    #endif

    #if JUCE_CLANG
     #pragma clang diagnostic pop
    #endif

    namespace tracktion_engine
    {

    //==========================================================================
    struct LinkImpl  : public AbletonLink::ImplBase
    {
        LinkImpl (TransportControl& t)  : AbletonLink::ImplBase (t)
        {
            setupCallbacks();
        }

        void setupCallbacks()
        {
            link.setNumPeersCallback ([this] (std::size_t n) { isConnectedCallback (n); });
            link.setTempoCallback ([this] (double bpm) { setTempoFromLink (bpm); });
        }

        bool isEnabled() const override
        {
            return link.isEnabled();
        }

        void setEnabled (bool isEnabled) override
        {
            link.enable (isEnabled);
            activateTimer (isEnabled);

            if (isEnabled)
                setTempoFromLink (link.captureAppTimeline().tempo());
        }

        void isConnectedCallback (std::size_t numPeers)
        {
            isConnected = numPeers > 0;
            callConnectionChanged();

            if (isConnected)
                setTempoFromLink (link.captureAppTimeline().tempo());
        }

        void setTempoToLink (double bpm) override
        {
            auto timeline = link.captureAppTimeline();
            timeline.setTempo (bpm, clock.micros());
            link.commitAppTimeline (timeline);
        }

        double getTempoFromLink() override
        {
            return link.captureAppTimeline().tempo();
        }

        double getBeatNow (double quantum) override
        {
            return link.captureAppTimeline().beatAtTime (clock.micros(), quantum);
        }

        double getBarPhase (double quantum) override
        {
            return link.captureAppTimeline().phaseAtTime (clock.micros(), quantum);
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
            setupCallbacks();
            setEnabled (isActive);
        };

        ~LinkImpl()
        {
             ABLLinkDelete (link);
        }

        void setupCallbacks()
        {
            ABLLinkSetSessionTempoCallback (link, tempoChangedCallback, this);
            ABLLinkSetIsConnectedCallback (link, isConnectedCallback, this);
            ABLLinkSetIsEnabledCallback (link, isEnabledCallback, this);
        }

        bool isEnabled() const override
        {
            return ABLLinkIsEnabled (link) && isActive;
        }

        void setEnabled (bool isEnabled) override
        {
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

        void setTempoToLink (double bpm) override
        {
            auto timeline = ABLLinkCaptureAppTimeline (link);

            ABLLinkSetTempo (timeline, bpm, (uint64) juce::Time::getHighResolutionTicks());
            ABLLinkCommitAppTimeline (link, timeline);
        }

        double getTempoFromLink() override
        {
            return ABLLinkGetTempo (ABLLinkCaptureAppTimeline (link));
        }

        double getBeatNow (double quantum) override
        {
            auto timeline = ABLLinkCaptureAppTimeline (link);
            return ABLLinkBeatAtTime (timeline, (uint64) juce::Time::getHighResolutionTicks(), quantum);
        }

        double getBarPhase (double quantum) override
        {
            auto timeline = ABLLinkCaptureAppTimeline (link);
            return ABLLinkPhaseAtTime (timeline, (uint64) juce::Time::getHighResolutionTicks(), quantum);
        }

        static void tempoChangedCallback (double bpm, void *context)
        {
            auto* thisPtr = static_cast<LinkImpl*> (context);
            thisPtr->setTempoFromLink (bpm);
        }

        static void isConnectedCallback (bool isConnected, void *context)
        {
            auto* thisPtr = static_cast<LinkImpl*> (context);

            thisPtr->isConnected = isConnected;
            thisPtr->activateTimer (isConnected);

            isEnabledCallback (isConnected, context);
        }

        static void isEnabledCallback (bool isEnabled, void *context)
        {
            auto* thisPtr = static_cast<LinkImpl*> (context);

            if (! isEnabled)
                thisPtr->isConnected = false;

            thisPtr->callConnectionChanged();

            if (isEnabled)
                broadcastTempo (thisPtr);
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
    return implementation != nullptr && implementation->isConnected;
}

double AbletonLink::getBeatsUntilNextCycle (double quantum) const
{
    if (implementation != nullptr)
        return implementation->getBeatsUntilNextCycle (quantum);

    return 0.0;
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

}
