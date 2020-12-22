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

//==============================================================================
/**
   Generates click audio and MIDI and adds them to the provided buffer.
 */
class ClickGenerator
{
public:
    //==============================================================================
    /** Creates a click generator for an Edit. */
    ClickGenerator (Edit&, bool isMidi, double endTime);

    /** Prepares a ClickGenerator to be played.
        Must be called before processBlock
    */
    void prepareToPlay (double sampleRate, double startTime);

    /** Adds clicks to a block of audio and MIDI for a given time range. */
    void processBlock (choc::buffer::ChannelArrayView<float>*, MidiMessageArray*, EditTimeRange);

private:
    const Edit& edit;
    bool midi = false;
    juce::Array<double> beatTimes;
    juce::BigInteger loudBeats;
    int currentBeat = 0;

    double sampleRate = 44100.0;
    juce::AudioBuffer<float> bigClick, littleClick;
    int bigClickMidiNote = 37, littleClickMidiNote = 76;

    //==============================================================================
    bool isMutedAtTime (double time) const;
};


//==============================================================================
/** An AudioNode that produces a click track. */
class ClickAudioNode  : public AudioNode
{
public:
    ClickAudioNode (bool isMidi, Edit&, double endTime);
    ~ClickAudioNode() override;

    void getAudioNodeProperties (AudioNodeProperties&) override;
    void visitNodes (const VisitorFn&) override;

    bool purgeSubNodes (bool keepAudio, bool keepMidi) override;
    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo&) override;
    bool isReadyToRender() override;
    void releaseAudioNodeResources() override;
    void renderOver (const AudioRenderContext& rc) override;
    void renderAdding (const AudioRenderContext& rc) override;
    void renderSection (const AudioRenderContext&, EditTimeRange editTime);

    static int getMidiClickNote (Engine&, bool big);
    static juce::String getClickWaveFile (Engine&, bool big);
    static void setMidiClickNote (Engine&, bool big, int noteNum);
    static void setClickWaveFile (Engine&, bool big, const juce::String& filename);

private:
    const Edit& edit;
    bool midi = false;
    juce::Array<double> beatTimes;
    juce::BigInteger loudBeats;
    int currentBeat = 0;

    double sampleRate = 44100.0;
    juce::AudioBuffer<float> bigClick, littleClick;
    int bigClickMidiNote = 37, littleClickMidiNote = 76;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClickAudioNode)
};

} // namespace tracktion_engine
