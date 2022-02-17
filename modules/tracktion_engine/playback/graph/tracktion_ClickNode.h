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

//==============================================================================
namespace Click
{
    int getMidiClickNote (Engine&, bool big);
    juce::String getClickWaveFile (Engine&, bool big);
    void setMidiClickNote (Engine&, bool big, int noteNum);
    void setClickWaveFile (Engine&, bool big, const juce::String& filename);
}

//==============================================================================
/**
   Generates click audio and MIDI and adds them to the provided buffer.
 */
class ClickGenerator
{
public:
    //==============================================================================
    /** Creates a click generator for an Edit. */
    ClickGenerator (Edit&, bool isMidi, TimePosition endTime);

    /** Prepares a ClickGenerator to be played.
        Must be called before processBlock
    */
    void prepareToPlay (double sampleRate, TimePosition startTime);

    /** Adds clicks to a block of audio and MIDI for a given time range. */
    void processBlock (choc::buffer::ChannelArrayView<float>*, MidiMessageArray*, TimeRange);

private:
    const Edit& edit;
    bool midi = false;
    juce::Array<TimePosition> beatTimes;
    juce::BigInteger loudBeats;
    int currentBeat = 0;

    double sampleRate = 44100.0;
    juce::AudioBuffer<float> bigClick, littleClick;
    int bigClickMidiNote = 37, littleClickMidiNote = 76;

    //==============================================================================
    bool isMutedAtTime (TimePosition) const;
};


//==============================================================================
/**
    Adds audio and MIDI clicks to the input buffers.
*/
class ClickNode final   : public tracktion::graph::Node
{
public:
    ClickNode (Edit&, int numAudioChannels, bool generateMidi, tracktion::graph::PlayHead&);

    std::vector<Node*> getDirectInputNodes() override;
    tracktion::graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    Edit& edit;
    tracktion::graph::PlayHead& playHead;
    ClickGenerator clickGenerator;
    const int numChannels;
    const bool generateMidi;
    double sampleRate = 44100.0;
};

}} // namespace tracktion { inline namespace engine
