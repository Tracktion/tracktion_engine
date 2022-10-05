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

juce::AudioPlayHead::CurrentPositionInfo getCurrentPositionInfo (Edit& edit)
{
    juce::AudioPlayHead::CurrentPositionInfo info;
    auto& transport = edit.getTransport();
    auto& tempoSequence = edit.tempoSequence;
    
    auto currentTime = transport.getPosition();
    
    if (auto epc = transport.getCurrentPlaybackContext())
        currentTime = epc->getPosition();
    
    auto position = createPosition (tempoSequence);
    position.set (currentTime);
    const auto timeSig = position.getTimeSignature();

    info.bpm = position.getTempo();
    info.timeSigNumerator = timeSig.numerator;
    info.timeSigDenominator = timeSig.denominator;
    info.timeInSeconds = position.getTime().inSeconds();
    info.editOriginTime = 0.0;
    info.ppqPosition = position.getPPQTime();
    info.ppqPositionOfLastBarStart = position.getPPQTimeOfBarStart();
    info.frameRate = juce::AudioPlayHead::fpsUnknown;
    info.isPlaying = transport.isPlaying();
    info.isRecording = transport.isRecording();
    info.isLooping = transport.looping;

    if (auto epc = transport.getCurrentPlaybackContext())
        info.isPlaying = epc->isPlaying();

    info.timeInSamples = (int64_t) std::floor ((info.timeInSeconds * edit.engine.getDeviceManager().getSampleRate()) + 0.5);

    const auto loopRange = transport.getLoopRange();
    position.set (loopRange.getStart());
    info.ppqLoopStart = position.getPPQTime();
    position.set (loopRange.getEnd());
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
    if (auto epc = transport.getCurrentPlaybackContext())
    {
        // N.B. Because we don't have full tempo sequence info from the host, we have
        // to assume that the tempo is constant and just sync to that
        // We could sync to a single bar by subtracting the ppqPositionOfLastBarStart from ppqPosition here
        const double beatsSinceStart = (info.ppqPosition * info.timeSigDenominator) / 4.0;
        const double secondsPerBeat = 240.0 / (info.bpm * info.timeSigDenominator);
        const TimeDuration timeOffset = TimeDuration::fromSeconds (beatsSinceStart * secondsPerBeat);
        
        const TimeDuration blockSizeInSeconds = TimeDuration::fromSeconds (edit.engine.getDeviceManager().getBlockSizeMs() / 1000.0);
        const TimePosition currentPositionInSeconds = epc->getPosition() + blockSizeInSeconds;
        // N.B we add the blockSizeInSeconds here as the playhead position will be at the end of the last block.
        // This avoids us re-syncing every block
        
        if (info.isPlaying)
        {
            if (TimeDuration::fromSeconds (std::abs ((currentPositionInSeconds - timeOffset).inSeconds())) > (blockSizeInSeconds / 2.0))
                epc->postPosition (toPosition (timeOffset));
            
            if (! epc->isPlaying())
                epc->play();
        }
        else
        {
            transport.setPosition (toPosition (timeOffset));
            
            if (epc->isPlaying())
                epc->stop();
        }
    }
    
    // Then the tempo info
    {
        auto position = createPosition (edit.tempoSequence);
        position.set (TimePosition::fromSeconds (info.timeInSeconds));
        const auto tempo = position.getTempo();
        const auto timeSig = position.getTimeSignature();

        const auto newBpm = info.bpm;
        const auto newNumerator = info.timeSigNumerator;
        const auto newDenominator = info.timeSigDenominator;

        if (tempo != newBpm
            || timeSig.numerator != newNumerator
            || timeSig.denominator != newDenominator)
        {
            juce::MessageManager::callAsync ([&edit, editRef = Edit::WeakRef (&edit),
                                              newBpm, newNumerator, newDenominator]
            {
                 if (! editRef)
                     return;

                 // N.B. This assumes only a simple tempo sequence with a single point
                 auto& tempoSequence = edit.tempoSequence;
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
        const auto pos = playhead.getPosition();
        const bool sucess = pos.hasValue();

        if (sucess)
        {
            positionInfo.resetToDefault();

            if (const auto sig = pos->getTimeSignature())
            {
                positionInfo.timeSigNumerator   = sig->numerator;
                positionInfo.timeSigDenominator = sig->denominator;
            }

            if (const auto loop = pos->getLoopPoints())
            {
                positionInfo.ppqLoopStart     = loop->ppqStart;
                positionInfo.ppqLoopEnd       = loop->ppqEnd;
            }

            if (const auto frame = pos->getFrameRate())
                positionInfo.frameRate = *frame;

            if (const auto timeInSeconds = pos->getTimeInSeconds())
                positionInfo.timeInSeconds = *timeInSeconds;

            if (const auto lastBarStartPpq = pos->getPpqPositionOfLastBarStart())
                positionInfo.ppqPositionOfLastBarStart = *lastBarStartPpq;

            if (const auto ppqPosition = pos->getPpqPosition())
                positionInfo.ppqPosition = *ppqPosition;

            if (const auto originTime = pos->getEditOriginTime())
                positionInfo.editOriginTime = *originTime;

            if (const auto bpm = pos->getBpm())
                positionInfo.bpm = *bpm;

            if (const auto timeInSamples = pos->getTimeInSamples())
                positionInfo.timeInSamples = *timeInSamples;

            positionInfo.isPlaying      = pos->getIsPlaying();
            positionInfo.isRecording    = pos->getIsRecording();
            positionInfo.isLooping      = pos->getIsLooping();
        }

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

}} // namespace tracktion { inline namespace engine
