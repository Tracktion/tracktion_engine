/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_ENABLE_ABLETON_LINK && JUCE_IOS
 struct ABLLink;
#endif

namespace tracktion { inline namespace engine
{

class AbletonLink
{
public:
    AbletonLink (TransportControl&);
    ~AbletonLink();

    /** On iOS you need this to instantiate an ABLLinkSettingsViewController. */
   #if TRACKTION_ENABLE_ABLETON_LINK && JUCE_IOS
    ABLLink* getLinkInstanceForIOS();
   #endif

    /** Enable Link. On platforms other than iOS, Link connects automatically
        once this is set. On iOS the user must additionally switch Link on using
        the ABLLinkSettingsViewController. Off by default on desktop, on by default on iOS.
    */
    void setEnabled (bool isEnabled);

    /** Is Link enabled? It may not be connected to any peers even if it is. */
    bool isEnabled() const;

    /** Is Link connected to any other peers? */
    bool isConnected() const;

    /** Returns the number of connected peers. */
    size_t getNumPeers() const;

    /** Returns whether the Link transport is currently playing. */
    bool isPlaying() const;

    /** Sets whether start/stop is syncronsied. */
    void enableStartStopSync (bool enable);

    /** Returns whether start/stop is syncronsied. If you're not connected, this may not make sense. */
    bool isStartStopSyncEnabled() const;

    /** Get the current session tempo. If you're not connected, this may not make sense. */
    double getSessionTempo() const;

    /** Number of beats until the next bar (quantum) - e.g. supply 4.0 for 4/4 cycles. */
    double getBeatsUntilNextCycle (double quantum) const;

    /** Use to offset the calculations by some amount (on top of device latency). */
    void setCustomOffset (int offsetMs);

    /** Once set, tempos chnages from link are transparently divided or multiplied
        by 2 to stay within this range (if possible). */
    void setTempoConstraint (juce::Range<double> minMaxTempo);

    /** Returns how far though the current bar the Link clock is. */
    double getBarPhase() const;

    /** Returns how much chasing playback is doing.
        0 means perfectly in sync
        1 means playback is speeding up as much as it can
       -1 means playback is slowing down as much as it can
    */
    double getChaseProportion() const;

    //==========================================================================
    void syncronise (TimePosition);

    //==========================================================================
    /** When Link is on, you shouldn't set the tempo directly.
        Instead add yourself as a listener, call requestTempoChange() and wait for
        a linkRequestedTempoChange() callback.

        If a large position change is required to get in sync with other clients,
        linkRequestedPositionChange is called and the listener user must perform it.
        Usually, however, this object can perform some nudging to keep in sync.
    */
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void linkRequestedStartStopChange (bool isPlaying) = 0;
        virtual void linkRequestedTempoChange (double newBpm) = 0;
        virtual void linkRequestedPositionChange (BeatDuration adjustment) = 0;
        virtual void linkConnectionChanged() = 0;
    };

    void requestStartStopChange (bool isPlaying);
    void requestTempoChange (double newBpm);
    void addListener (Listener*);
    void removeListener (Listener*);

private:

    struct ImplBase;
    friend struct LinkImpl;
    std::unique_ptr<ImplBase> implementation;
};

}} // namespace tracktion { inline namespace engine
