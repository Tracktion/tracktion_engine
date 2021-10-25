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


namespace tracktion_engine
{

inline AudioNode* createTrackCompAudioNode (AudioNode* input, const Array<EditTimeRange>& muteTimes,
                                            const Array<EditTimeRange>& nonMuteTimes, double crossfadeTime)
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

inline AudioNode* createAudioNode (TrackCompManager::TrackComp& trackComp, Track& t, AudioNode* input)
{
    auto crossfadeTimeMs = trackComp.edit.engine.getPropertyStorage().getProperty (SettingID::compCrossfadeMs, 20.0);
    auto crossfadeTime = static_cast<double> (crossfadeTimeMs) / 1000.0;
    auto nonMuteTimes = trackComp.getNonMuteTimes (t, crossfadeTime);
    
    return createTrackCompAudioNode (input, TrackCompManager::TrackComp::getMuteTimes (nonMuteTimes),
                                     nonMuteTimes, crossfadeTime);
}


} // namespace tracktion_engine
