/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion::inline engine {

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

    /** Creates a FadeInOutNode for something positioned in beats, e.g. a beat-based clip.
        The fades are at its start and end wherever they are with the current tempo, so if
        the tempo changes they move with it. The fade lengths are still in time.
    */
    FadeInOutNode (std::unique_ptr<tracktion::graph::Node> input,
                   ProcessState&,
                   BeatRange position, TimeDuration fadeInLength, TimeDuration fadeOutLength,
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

    struct BeatPositionedFades
    {
        BeatRange position;
        TimeDuration fadeInLength, fadeOutLength;
        std::optional<size_t> tempoHash;    // Of the tempo map the fades were last worked out with
    };

    std::optional<BeatPositionedFades> beatPositionedFades;

    //==============================================================================
    bool renderingNeeded (TimeRange);
    void updateBeatPositionedFades();
};

} // namespace tracktion::inline engine
