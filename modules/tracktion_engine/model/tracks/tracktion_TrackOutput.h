/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/**
    Represents the destination output device(s) for a track.
*/
class TrackOutput   : private juce::ValueTree::Listener
{
public:
    //==============================================================================
    TrackOutput (AudioTrack&);
    ~TrackOutput();

    //==============================================================================
    void initialise();
    void flushStateToValueTree();

    /** called as a sanity-check after edit has created all the tracks */
    void updateOutput();

    //==============================================================================
    /** if compareDefaultDevices is true, then this returns true if the device name
        is 'default ..' and the actual device is named, or vice-versa
    */
    bool outputsToDevice (const juce::String& deviceName, bool compareDefaultDevices) const;

    /** description of where it's going. */
    juce::String getOutputName() const;

    /** includes the dest track's name, if relevant */
    juce::String getDescriptiveOutputName() const;

    /** if the track's being routed into another one, this returns it */
    AudioTrack* getDestinationTrack() const;

    /** True if this track's direct destination is the one given */
    bool outputsToDestTrack (AudioTrack&) const;

    /** true if any downstream tracks match this one */
    bool feedsInto (const AudioTrack* possibleDestTrack) const;

    /** finds the output device.

        If the track feeds into another track, this will optionally recurse into the
        dest track to find the device.
    */
    OutputDevice* getOutputDevice (bool traceThroughDestTracks) const;

    //==============================================================================
    void setOutputByName (const juce::String& name);
    void setOutputToTrack (AudioTrack*);
    void setOutputToDefaultDevice (bool isMidi);

    static void getPossibleOutputDeviceNames (const juce::Array<AudioTrack*>& tracks,
                                              juce::StringArray& s, juce::StringArray& a,
                                              juce::BigInteger& hasAudio,
                                              juce::BigInteger& hasMidi);

    static void getPossibleOutputNames (const juce::Array<AudioTrack*>& tracks,
                                        juce::StringArray& s, juce::StringArray& a,
                                        juce::BigInteger& hasAudio,
                                        juce::BigInteger& hasMidi);

    //==============================================================================
    bool canPlayAudio() const;
    /** (also true for virtual devices with midi synths) */
    bool canPlayMidi() const;

    /** false if not possible */
    bool injectLiveMidiMessage (const juce::MidiMessage&);

    juce::ValueTree state;

private:
    AudioTrack& owner;
    juce::CachedValue<juce::String> outputDevice;
    EditItemID destTrackID;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override {}
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackOutput)
};

} // namespace tracktion_engine
