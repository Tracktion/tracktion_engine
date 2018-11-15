/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** An AudioNode that handles muting/soloing of its input node, according to
    the audibility of a track.
*/
class TrackMutingAudioNode  : public SingleInputAudioNode
{
public:
    TrackMutingAudioNode (Track& t, AudioNode* f, bool muteForInputsWhenRecording)
        : SingleInputAudioNode (f), edit (t.edit), track (&t)
    {
        callInputWhileMuted = t.processAudioNodesWhileMuted();
        processMidiWhileMuted = track->state.getProperty (IDs::processMidiWhenMuted, false);

        if (muteForInputsWhenRecording)
            for (InputDeviceInstance* in : edit.getAllInputDevices())
                if (in->isRecordingActive() && in->getTargetTrack() == &t)
                    inputDevicesToMuteFor.add (in);

        wasBeingPlayed = t.shouldBePlayed();
    }

    TrackMutingAudioNode (Edit& e, AudioNode* f)
        : SingleInputAudioNode (f), edit (e)
    {
        wasBeingPlayed = ! edit.areAnyTracksSolo();
    }

    //==============================================================================
    void renderOver (const AudioRenderContext& rc) override
    {
        const bool isPlayingNow = isBeingPlayed();

        if (wasJustMuted (isPlayingNow))
        {

            // send midi off events if we don't want to process midi while muted
            sendAllNotesOffIfDesired (rc);

            input->renderOver (rc);

            rampOver (rc, 1.0f, 0.0f);

            if (! processMidiWhileMuted)
            {
                rc.clearMidiBuffer();
                sendAllNotesOffIfDesired (rc);
            }
        }
        else if (wasJustUnMuted (isPlayingNow))
        {
            input->renderOver (rc);

            rampOver (rc, 0.0f, 1.0f);
        }
        else if (wasBeingPlayed)
        {
            input->renderOver (rc);
        }
        else if (callInputWhileMuted || processMidiWhileMuted)
        {
            input->renderOver (rc);

            if (processMidiWhileMuted)
                rc.clearAudioBuffer();
            else
                rc.clearAll();
        }
        else
        {
            rc.clearAll();
        }

        updateLastMutedState (isPlayingNow);
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        const bool isPlayingNow = isBeingPlayed();

        if (wasJustMuted (isPlayingNow))
        {
            // just been muted, so send some midi off events
            sendAllNotesOffIfDesired (rc);

            rampAdd (rc, 1.0f, 0.0f);
        }
        else if (wasJustUnMuted (isPlayingNow))
        {
            rampAdd (rc, 0.0f, 1.0f);
        }
        else if (wasBeingPlayed)
        {
            input->renderAdding (rc);
        }
        else if (callInputWhileMuted || processMidiWhileMuted)
        {
            callRenderOver (rc);
        }

        updateLastMutedState (isPlayingNow);
    }

private:
    Edit& edit;
    juce::ReferenceCountedObjectPtr<Track> track;
    bool wasBeingPlayed = false;
    bool callInputWhileMuted = false;
    bool processMidiWhileMuted = false;
    juce::Array<InputDeviceInstance*> inputDevicesToMuteFor;

    bool isBeingPlayed() const
    {
        bool playing = track != nullptr ? track->shouldBePlayed() : ! edit.areAnyTracksSolo();

        if (! playing)
            return false;

        for (int i = inputDevicesToMuteFor.size(); --i >= 0;)
            if (inputDevicesToMuteFor.getUnchecked (i)->shouldTrackContentsBeMuted())
                return false;

        return true;
    }

    bool wasJustMuted (bool isPlayingNow) const
    {
        return wasBeingPlayed && ! isPlayingNow;
    }

    bool wasJustUnMuted (bool isPlayingNow) const
    {
        return ! wasBeingPlayed && isPlayingNow;
    }

    void updateLastMutedState (bool isPlayingNow)
    {
        wasBeingPlayed = isPlayingNow;
    }

    void rampOver (const AudioRenderContext& rc, float start, float end)
    {
        if (rc.destBuffer != nullptr)
            rc.destBuffer->applyGainRamp (rc.bufferStartSample, rc.bufferNumSamples, start, end);
    }

    void rampAdd (const AudioRenderContext& rc, float start, float end)
    {
        if (rc.destBuffer != nullptr)
        {
            AudioRenderContext scratchContext (rc);
            AudioScratchBuffer scratchBuffer (rc.destBuffer->getNumChannels(), rc.bufferNumSamples);
            scratchContext.destBuffer = &scratchBuffer.buffer;
            input->renderAdding (scratchContext);

            for (int c = 0; c < rc.destBuffer->getNumChannels(); ++c)
                rc.destBuffer->addFromWithRamp (c, rc.bufferStartSample,
                                                scratchBuffer.buffer.getReadPointer (c),
                                                rc.bufferNumSamples, start, end);
        }
        else
        {
            // in case midi messages are being sent to our input nodes
            input->renderAdding (rc);
        }
    }

    void sendAllNotesOffIfDesired (const AudioRenderContext& rc)
    {
        if (! processMidiWhileMuted && rc.bufferForMidiMessages != nullptr)
            rc.bufferForMidiMessages->isAllNotesOff = true;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackMutingAudioNode)
};

} // namespace tracktion_engine
