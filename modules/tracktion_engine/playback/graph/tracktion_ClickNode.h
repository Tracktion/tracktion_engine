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
/**
    Adds audio and MIDI clicks to the input buffers.
*/
class ClickNode final   : public tracktion_graph::Node
{
public:
    ClickNode (Edit&, int numAudioChannels, bool generateMidi, tracktion_graph::PlayHead&);

    std::vector<Node*> getDirectInputNodes() override;
    tracktion_graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    Edit& edit;
    tracktion_graph::PlayHead& playHead;
    ClickGenerator clickGenerator;
    const int numChannels;
    const bool generateMidi;
    double sampleRate = 44100.0;
};

} // namespace tracktion_engine
