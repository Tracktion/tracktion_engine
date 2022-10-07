/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "tracktion_TimedMutingAudioNode.h"
#include "tracktion_FadeInOutAudioNode.h"


namespace tracktion { inline namespace engine
{

inline AudioNode* createTrackCompAudioNode (AudioNode* input,
                                            const juce::Array<legacy::EditTimeRange>& muteTimes,
                                            const juce::Array<legacy::EditTimeRange>& nonMuteTimes,
                                            double crossfadeTime)
{
    if (muteTimes.isEmpty())
        return input;

    input = new TimedMutingAudioNode (input, muteTimes);

    for (auto r : nonMuteTimes)
    {
        auto fadeIn = r.withLength (crossfadeTime) - 0.0001;
        auto fadeOut = fadeIn.movedToEndAt (r.getEnd() + 0.0001);

        if (! (fadeIn.isEmpty() && fadeOut.isEmpty()))
            input = new FadeInOutAudioNode (input, fadeIn, fadeOut,
                                            AudioFadeCurve::convex,
                                            AudioFadeCurve::convex, false);
    }

    return input;
}

inline AudioNode* createTrackCompAudioNode (AudioNode* input,
                                            const juce::Array<TimeRange>& muteTimes,
                                            const juce::Array<TimeRange>& nonMuteTimes,
                                            TimeDuration crossfadeTime)
{
    juce::Array<legacy::EditTimeRange> muteEditTimes;
    juce::Array<legacy::EditTimeRange> nonMuteEditTimes;

    for (auto t : muteTimes)
        muteEditTimes.add ({ t.getStart().inSeconds(), t.getEnd().inSeconds() });

    for (auto t : nonMuteTimes)
        nonMuteEditTimes.add ({ t.getStart().inSeconds(), t.getEnd().inSeconds() });

    return createTrackCompAudioNode (input,
                                     muteEditTimes,
                                     nonMuteEditTimes,
                                     crossfadeTime.inSeconds());
}

inline AudioNode* createAudioNode (TrackCompManager::TrackComp& trackComp, Track& t, AudioNode* input)
{
    auto crossfadeTimeMs = trackComp.edit.engine.getPropertyStorage().getProperty (SettingID::compCrossfadeMs, 20.0);
    auto crossfadeTime = TimeDuration::fromSeconds (static_cast<double> (crossfadeTimeMs) / 1000.0);
    auto nonMuteTimes = trackComp.getNonMuteTimes (t, crossfadeTime);
    
    return createTrackCompAudioNode (input, TrackCompManager::TrackComp::getMuteTimes (nonMuteTimes),
                                     nonMuteTimes, crossfadeTime);
}


}} // namespace tracktion { inline namespace engine
