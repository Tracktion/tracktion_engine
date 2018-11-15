/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** An AudioNode that produces a click track. */
class ClickNode  : public AudioNode
{
public:
    ClickNode (bool isMidi, Edit&, double endTime);
    ~ClickNode();

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClickNode)
};

} // namespace tracktion_engine
