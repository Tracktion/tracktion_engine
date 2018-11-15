/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


// how much extra time to give a track before it gets cut off - to allow for plugins
// that ring on.
static const double decayTimeAllowance = 5.0;
static const int secondsPerGroup = 8;

static inline int timeToGroupIndex (double t) noexcept
{
    return ((int) t) / secondsPerGroup;
}

//==============================================================================
struct CombiningAudioNode::TimedAudioNode
{
    TimedAudioNode (EditTimeRange t, AudioNode* n)  : time (t), node (n)
    {
    }

    EditTimeRange time;
    const ScopedPointer<AudioNode> node;
    int lastBufferSize = 0;

    void render (const AudioRenderContext& rc, EditTimeRange editTime) const
    {
        AudioRenderContext context (rc);

        if (editTime.getEnd() > time.end)
        {
            auto newLength = time.end - editTime.getStart();
            context.streamTime.end = context.streamTime.start + newLength;
            context.bufferNumSamples = jmax (0, (int) (context.bufferNumSamples * newLength
                                                        / rc.streamTime.getLength()));
        }

        auto amountToSkip = time.start - editTime.getStart();

        if (amountToSkip > 0)
        {
            auto samplesToSkip = jmin (context.bufferNumSamples,
                                       (int) (context.bufferNumSamples * amountToSkip
                                                / context.streamTime.getLength()));

            context.bufferStartSample += samplesToSkip;
            context.bufferNumSamples -= samplesToSkip;

            context.midiBufferOffset += amountToSkip;
            context.streamTime.start = context.streamTime.getStart() + amountToSkip;
            jassert (context.streamTime.start < context.streamTime.end);
        }

        if (context.bufferNumSamples > 0 || rc.bufferForMidiMessages != nullptr)
            node->renderAdding (context);
    }

    JUCE_DECLARE_NON_COPYABLE (TimedAudioNode)
};

//==============================================================================
CombiningAudioNode::CombiningAudioNode() {}
CombiningAudioNode::~CombiningAudioNode() {}

void CombiningAudioNode::addInput (EditTimeRange time, AudioNode* inputNode)
{
    if (inputNode == nullptr)
        return;

    if (time.isEmpty())
    {
        ScopedPointer<AudioNode> an (inputNode);
        return;
    }

    AudioNodeProperties info;
    info.hasAudio = false;
    info.hasMidi = false;
    info.numberOfChannels = 0;

    inputNode->getAudioNodeProperties (info);

    hasAudio |= info.hasAudio;
    hasMidi |= info.hasMidi;

    maxNumberOfChannels = jmax (maxNumberOfChannels, info.numberOfChannels);

    int i;
    for (i = 0; i < inputs.size(); ++i)
        if (inputs.getUnchecked(i)->time.start >= time.getStart())
            break;

    auto tan = inputs.insert (i, new TimedAudioNode (time, inputNode));

    jassert (time.end < Edit::maximumLength);

    // add the node to any groups it's near to.
    auto start = jmax (0, timeToGroupIndex (time.start - (secondsPerGroup / 2 + 2)));
    auto end   = jmax (0, timeToGroupIndex (time.end   + (secondsPerGroup / 2 + 2)));

    while (groups.size() <= end)
        groups.add (new Array<TimedAudioNode*>());

    for (i = start; i <= end; ++i)
    {
        auto g = groups.getUnchecked(i);

        int j;
        for (j = 0; j < g->size(); ++j)
            if (g->getUnchecked(j)->time.start >= time.start)
                break;

        jassert (tan != nullptr);
        g->insert (j, tan);
    }
}

void CombiningAudioNode::clear()
{
    inputs.clearQuick (true);
    groups.clearQuick (true);
    hasAudio = false;
    hasMidi = false;
}

void CombiningAudioNode::getAudioNodeProperties (AudioNodeProperties& info)
{
    info.hasAudio = hasAudio;
    info.hasMidi = hasMidi;
    info.numberOfChannels = maxNumberOfChannels;
}

void CombiningAudioNode::visitNodes (const VisitorFn& v)
{
    v (*this);

    for (auto* n : inputs)
        n->node->visitNodes (v);
}

bool CombiningAudioNode::purgeSubNodes (bool keepAudio, bool keepMidi)
{
    for (int i = inputs.size(); --i >= 0;)
    {
        auto input = inputs.getUnchecked(i);

        if (! input->node->purgeSubNodes (keepAudio, keepMidi))
        {
            for (int j = groups.size(); --j >= 0;)
                groups.getUnchecked(j)->removeAllInstancesOf (input);

            inputs.remove (i);
        }
    }

    return ! inputs.isEmpty();
}

void CombiningAudioNode::prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info)
{
    for (auto* n : inputs)
    {
        auto info2 = info;
        info2.startTime -= n->time.start;
        n->node->prepareAudioNodeToPlay (info2);
    }
}

bool CombiningAudioNode::isReadyToRender()
{
    for (auto* n : inputs)
        if (! n->node->isReadyToRender())
            return false;

    return true;
}

void CombiningAudioNode::releaseAudioNodeResources()
{
    for (auto* n : inputs)
        n->node->releaseAudioNodeResources();
}

void CombiningAudioNode::renderOver (const AudioRenderContext& rc)
{
    callRenderAdding (rc);
}

void CombiningAudioNode::renderAdding (const AudioRenderContext& rc)
{
    rc.sanityCheck();

    if (hasAudio || hasMidi)
        invokeSplitRender (rc, *this);
}

void CombiningAudioNode::renderSection (const AudioRenderContext& rc, EditTimeRange editTime)
{
    if (auto g = groups[timeToGroupIndex (editTime.getStart())])
    {
        for (auto tan : *g)
        {
            if (tan->time.end > editTime.getStart())
            {
                if (tan->time.start >= editTime.getEnd())
                    break;

                tan->render (rc, editTime);
            }
        }
    }
}

void CombiningAudioNode::prepareForNextBlock (const AudioRenderContext& rc)
{
    SCOPED_REALTIME_CHECK

    auto time = rc.getEditTime().editRange1.getStart();
    prefetchGroup (rc, time);

    if (rc.playhead.isLooping() && time > rc.playhead.getLoopTimes().end - 4.0)
        prefetchGroup (rc, rc.playhead.getLoopTimes().start);
}

void CombiningAudioNode::prefetchGroup (const AudioRenderContext& rc, const double time)
{
    if (auto g = groups[timeToGroupIndex (time)])
        for (auto tan : *g)
            tan->node->prepareForNextBlock (rc);
}
