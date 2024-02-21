/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
/**
    A Node that fades in and out given time regions.
*/
class FadeInOutNode final : public Node,
                            public TracktionEngineNode,
                            public DynamicallyOffsettableNodeBase
{
public:
    FadeInOutNode (std::unique_ptr<tracktion::graph::Node> input,
                   ProcessState&,
                   TimeRange fadeIn, TimeRange fadeOut,
                   AudioFadeCurve::Type fadeInType, AudioFadeCurve::Type fadeOutType,
                   bool clearSamplesOutsideFade);

    void setDynamicOffsetTime (TimeDuration) override;

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
    TimeDuration dynamicOffset;

    //==============================================================================
    bool renderingNeeded (TimeRange);
};

}} // namespace tracktion { inline namespace engine
