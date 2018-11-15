/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** An AudioNode mutes an input click audio node within a given range or if disabled in the Edit. */
class ClickMutingNode   : public SingleInputAudioNode
{
public:
    ClickMutingNode (ClickNode* clickNodeInput, Edit& e)
        : SingleInputAudioNode (clickNodeInput), clickNode (clickNodeInput), edit (e)
    {
    }

    void renderOver (const AudioRenderContext& rc) override
    {
        callRenderAdding (rc);
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        if (rc.playhead.isUserDragging() || ! rc.playhead.isPlaying())
            return;

        invokeSplitRender (rc, *this);
    }

    void renderSection (const AudioRenderContext& rc, EditTimeRange editTime)
    {
        if (isMutedAtTime (editTime.getStart()))
            return;

        clickNode->renderSection (rc, editTime);
    }

private:
    ClickNode* clickNode = nullptr;
    const Edit& edit;

    bool isMutedAtTime (double time) const
    {
        const bool clickEnabled = edit.clickTrackEnabled.get();

        if (clickEnabled && edit.clickTrackRecordingOnly)
            return ! edit.getTransport().isRecording();

        if (! clickEnabled)
            return ! edit.getClickTrackRange().contains (time);

        return ! clickEnabled;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClickMutingNode)
};

} // namespace tracktion_engine
