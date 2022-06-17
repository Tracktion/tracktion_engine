/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion { inline namespace engine
{

//==============================================================================
ExpEnvelope::ExpEnvelope()
{
    calculateAttackTime();
    calculateDecayTime();
    calculateReleaseTime();
}

void ExpEnvelope::calculateAttackTime()
{
    float samples = float (sampleRate * attackTime);

    attackCoeff = std::exp (-std::log ((1.0f + attackTCO) / attackTCO) / float (samples));
    attackOffset = (1.0f + attackTCO) * (1.0f - attackCoeff);
}

void ExpEnvelope::calculateDecayTime()
{
    float samples = float (sampleRate * decayTime);

    decayCoeff = std::exp (-std::log ((1.0f + decayTCO) / decayTCO) / samples);
    decayOffset = (sustainLevel - decayTCO) * (1.0f - decayCoeff);
}

void ExpEnvelope::calculateReleaseTime()
{
    float samples = float (sampleRate * releaseTime);

    releaseCoeff = std::exp (-std::log ((1.0f + releaseTCO) / releaseTCO) / samples);
    releaseOffset = -releaseTCO * (1.0f - releaseCoeff);
}

}} // namespace tracktion { inline namespace engine
