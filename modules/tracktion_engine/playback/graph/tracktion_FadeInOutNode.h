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
//==============================================================================
/**
    A Node that fades in and out given time regions.
*/
class FadeInOutNode final : public tracktion_graph::Node
{
public:
    FadeInOutNode (std::unique_ptr<tracktion_graph::Node> input,
                   tracktion_graph::PlayHeadState&,
                   EditTimeRange fadeIn, EditTimeRange fadeOut,
                   AudioFadeCurve::Type fadeInType, AudioFadeCurve::Type fadeOutType,
                   bool clearSamplesOutsideFade);

    //==============================================================================
    tracktion_graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    std::unique_ptr<tracktion_graph::Node> input;
    tracktion_graph::PlayHeadState& playHeadState;
    EditTimeRange fadeIn, fadeOut;
    AudioFadeCurve::Type fadeInType, fadeOutType;
    juce::Range<int64_t> fadeInSampleRange, fadeOutSampleRange;
    bool clearExtraSamples = true;

    //==============================================================================
    bool renderingNeeded (const juce::Range<int64_t>&) const;
};

} // namespace tracktion_engine
