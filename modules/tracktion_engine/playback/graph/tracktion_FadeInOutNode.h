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
//==============================================================================
/**
    A Node that fades in and out given time regions.
*/
class FadeInOutNode final : public Node,
                            public TracktionEngineNode
{
public:
    FadeInOutNode (std::unique_ptr<tracktion::graph::Node> input,
                   ProcessState&,
                   TimeRange fadeIn, TimeRange fadeOut,
                   AudioFadeCurve::Type fadeInType, AudioFadeCurve::Type fadeOutType,
                   bool clearSamplesOutsideFade);

    //==============================================================================
    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    std::unique_ptr<tracktion::graph::Node> input;
    TimeRange fadeIn, fadeOut;
    AudioFadeCurve::Type fadeInType, fadeOutType;
    bool clearExtraSamples = true;

    //==============================================================================
    bool renderingNeeded (TimeRange);
    void processSection (choc::buffer::ChannelArrayView<float>, TimeRange);
    static int timeToSample (int numSamples, TimeRange editTime, TimePosition);
};

}} // namespace tracktion { inline namespace engine
