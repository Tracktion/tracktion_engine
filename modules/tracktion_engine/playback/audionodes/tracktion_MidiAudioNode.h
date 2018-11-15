/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** An AudioNode that plays MIDI data from a MidiMessageSequence,
    at a specific MIDI channel
*/
class MidiAudioNode   : public AudioNode
{
public:
    MidiAudioNode (juce::MidiMessageSequence sequence,
                   juce::Range<int> midiChannelNumbers,
                   EditTimeRange editSection,
                   juce::CachedValue<float>& volumeDb,
                   juce::CachedValue<bool>& mute,
                   Clip&, const MidiAudioNode* nodeToReplace);

    void renderSection (const AudioRenderContext&, EditTimeRange editTime);

    void getAudioNodeProperties (AudioNodeProperties&) override;
    void visitNodes (const VisitorFn&) override;
    bool purgeSubNodes (bool keepAudio, bool keepMidi) override;
    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo&) override;
    bool isReadyToRender() override;
    void releaseAudioNodeResources() override;
    void renderOver (const AudioRenderContext&) override;
    void renderAdding (const AudioRenderContext&) override;

    Clip& getClip() const noexcept                  { return *clip; }

private:
    juce::MidiMessageSequence ms;
    int currentIndex = 0;
    EditTimeRange editSection;
    juce::Range<int> channelNumbers;
    juce::CachedValue<float>& volumeDb;
    juce::CachedValue<bool>& mute;
    juce::ReferenceCountedObjectPtr<Clip> clip;
    MidiMessageArray::MPESourceID midiSourceID = MidiMessageArray::createUniqueMPESourceID();
    bool wasMute = false, shouldCreateMessagesForTime = false;

    //==============================================================================
    void createMessagesForTime (double time, MidiMessageArray&, double midiTimeOffset);
    void createNoteOffs (MidiMessageArray& destination, const juce::MidiMessageSequence& source,
                         double time, double midiTimeOffset);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiAudioNode)
};

//==============================================================================
/** Returns a MidiAudioNode if the AudioNode contains one for the given clip. */
MidiAudioNode* getClipIfPresentInNode (AudioNode*, Clip&);

} // namespace tracktion_engine
