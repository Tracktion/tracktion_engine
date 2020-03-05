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

juce::AudioPlayHead::CurrentPositionInfo getCurrentPositionInfo (Edit& edit)
{
    juce::AudioPlayHead::CurrentPositionInfo info;
    auto& transport = edit.getTransport();
    auto& tempoSequence = edit.tempoSequence;
    
    double currentTime = transport.getCurrentPosition();
    
    if (auto playhead = transport.getCurrentPlayhead())
        currentTime = playhead->getPosition();
    
    TempoSequencePosition position (tempoSequence);
    position.setTime (currentTime);
    auto currentDetails = position.getCurrentTempo();

    info.bpm = currentDetails.bpm;
    info.timeSigNumerator = currentDetails.numerator;
    info.timeSigDenominator = currentDetails.denominator;
    info.timeInSeconds = position.getTime();
    info.editOriginTime = 0.0;
    info.ppqPosition = position.getPPQTime();
    info.ppqPositionOfLastBarStart = position.getPPQTimeOfBarStart();
    info.frameRate = AudioPlayHead::fpsUnknown;
    info.isPlaying = transport.isPlaying();
    info.isRecording = transport.isRecording();
    info.isLooping = transport.looping;

    if (auto playhead = transport.getCurrentPlayhead())
        info.isPlaying = playhead->isPlaying();

    info.timeInSamples = (int64) std::floor ((info.timeInSeconds * edit.engine.getDeviceManager().getSampleRate()) + 0.5);

    const auto loopRange = transport.getLoopRange();
    position.setTime (loopRange.getStart());
    info.ppqLoopStart = position.getPPQTime();
    position.setTime (loopRange.getEnd());
    info.ppqLoopEnd = position.getPPQTime();

    return info;
}

//==============================================================================
void synchroniseEditPosition (Edit& edit, const juce::AudioPlayHead::CurrentPositionInfo& info)
{
    if (info.bpm == 0.0)
        return;
    
    auto& transport = edit.getTransport();
    
    // First synchronise position
    if (auto playhead = transport.getCurrentPlayhead())
    {
        // N.B. Because we don't have full tempo sequence info from the host, we have
        // to assume that the tempo is constant and just sync to that
        // We could sync to a single bar by subtracting the ppqPositionOfLastBarStart from ppqPosition here
        const double beatsSinceStart = (info.ppqPosition * info.timeSigDenominator) / 4.0;
        const double secondsPerBeat = 240.0 / (info.bpm * info.timeSigDenominator);
        const double timeOffset = beatsSinceStart * secondsPerBeat;
        
        const double blockSizeInSeconds = edit.engine.getDeviceManager().getBlockSizeMs() / 1000.0;
        const double currentPositionInSeconds = playhead->getPosition() + blockSizeInSeconds;
        // N.B we add the blockSizeInSeconds here as the playhead position will be at the end of the last block.
        // This avoids us re-syncing every block
        
        if (info.isPlaying)
        {
            if (std::abs (timeOffset - currentPositionInSeconds) > (blockSizeInSeconds / 2.0))
                playhead->overridePosition (timeOffset);
            
            if (! playhead->isPlaying())
                playhead->play();
        }
        else
        {
            transport.setCurrentPosition (timeOffset);
            
            if (! playhead->isStopped())
                playhead->stop();
        }
    }
    
    // Then the tempo info
    {
        TempoSequencePosition position (edit.tempoSequence);
        position.setTime (info.timeInSeconds);
        const auto currentDetails = position.getCurrentTempo();
        const auto newBpm = info.bpm;
        const auto newNumerator = info.timeSigNumerator;
        const auto newDenominator = info.timeSigDenominator;

        if (currentDetails.bpm != newBpm
            || currentDetails.numerator != newNumerator
            || currentDetails.denominator != newDenominator)
        {
            MessageManager::callAsync ([editRef = WeakReference<Edit> (&edit),
                                        newBpm, newNumerator, newDenominator]
                                       {
                                            if (! editRef)
                                                return;
                                            
                                            // N.B. This assumes only a simple tempo sequence with a single point
                                            auto& tempoSequence = editRef->tempoSequence;
                                            auto tempoSetting = tempoSequence.getTempo (0);
                                            auto timeSigSetting = tempoSequence.getTimeSig (0);

                                            tempoSetting->setBpm (newBpm);
                                            timeSigSetting->numerator = newNumerator;
                                            timeSigSetting->denominator = newDenominator;
                                       });
        }
    }
}


//==============================================================================
//==============================================================================
ExternalPlayheadSynchroniser::ExternalPlayheadSynchroniser (Edit& e)
    : edit (e)
{
    positionInfo.resetToDefault();
}

bool ExternalPlayheadSynchroniser::synchronise (juce::AudioPlayHead& playhead)
{
    if (positionInfoLock.tryEnter())
    {
        const bool sucess = playhead.getCurrentPosition (positionInfo);
        positionInfoLock.exit();

        if (sucess)
        {
            // Synchronise the Edit's position and tempo info based on the host
            synchroniseEditPosition (edit, positionInfo);
            
            return true;
        }
    }
    
    return false;
}

bool ExternalPlayheadSynchroniser::synchronise (juce::AudioProcessor& processor)
{
    if (auto playhead = processor.getPlayHead())
        return synchronise (*playhead);

    return false;
}

juce::AudioPlayHead::CurrentPositionInfo ExternalPlayheadSynchroniser::getPositionInfo() const
{
    juce::SpinLock::ScopedLockType lock (positionInfoLock);
    return positionInfo;
}

} // namespace tracktion_engine
