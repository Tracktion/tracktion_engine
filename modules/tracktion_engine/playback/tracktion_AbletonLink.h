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

/**
    Manages an Ableton Link session connecting an Edit with a number of networked peers,
    syncronising their tempos, bar phases and start/stop statuses.
*/
class AbletonLink
{
public:
    /** Creates an Ableton Link connection for an Edit.
        Don't create one of these manually, get the one contained in the Edit
        with Edit::getAbletonLink().
    */
    AbletonLink (TransportControl&);

    /** Destructor. */
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
    /** Request the Link session to start or stop playing.
        This will in turn trigger a callback to linkRequestedStartStopChange
        which should be used to actually start the transport.
        [[ message_thread ]]
    */
    void requestStartStopChange (bool isPlaying);

    /** Request the Link session to change tempo.
        This will in turn trigger a callback to linkRequestedTempoChange
        which should be used to actually change the tempo.
        [[ message_thread ]]
    */
    void requestTempoChange (double newBpm);

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
        /** Destructor. */
        virtual ~Listener() = default;

        /** Called by link to request that tracktion starts or stops its tempo.
            [[ message_thread ]]
        */
        virtual void linkRequestedStartStopChange (bool isPlaying) = 0;

        /** Called by link to request that tracktion changes its tempo.
            [[ message_thread ]]
        */
        virtual void linkRequestedTempoChange (double newBpm) = 0;

        /** Called by Link when the number of peer connections changes.
            @see isConnected, getNumPeers
            [[ message_thread ]]
        */
        virtual void linkConnectionChanged() = 0;

        /** Should be implemented to perform a larger beat position change.
            N.B. This will be called on the audio thread and the beat adjustment
            should take place before the next audio block. This usually means
            posting a position change directly in to the EditPlaybackContext.
            Just make sure you don't block during this callback.
            [[ audio_thread ]]
        */
        virtual void linkRequestedPositionChange (BeatDuration adjustment) = 0;
    };

    /** Adds a listener. */
    void addListener (Listener*);

    /** Removes a previously added listener. */
    void removeListener (Listener*);

    //==========================================================================
    /** @internal */
    void syncronise (TimePosition);

private:
    struct ImplBase;
    friend struct LinkImpl;
    std::unique_ptr<ImplBase> implementation;
};

}} // namespace tracktion { inline namespace engine
