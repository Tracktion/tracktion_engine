/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

Envelope::Envelope()
{
    calculateAttackTime();
    calculateDecayTime();
    calculateReleaseTime();
}
	
void Envelope::calculateAttackTime()
{
	float samples = float (sampleRate * attackTime);

    attackCoeff = std::exp (-std::log ((1.0f + attackTCO) / attackTCO) / float (samples));
	attackOffset = (1.0f + attackTCO) * (1.0f - attackCoeff);
}

void Envelope::calculateDecayTime()
{
	float samples = float (sampleRate * decayTime);
	
    decayCoeff = std::exp (-std::log ((1.0f + decayTCO) / decayTCO) / samples);
	decayOffset = (sustainLevel - decayTCO) * (1.0f - decayCoeff);
}

void Envelope::calculateReleaseTime()
{
    float samples = float (sampleRate * releaseTime);

    releaseCoeff = std::exp (-std::log ((1.0f + releaseTCO) / releaseTCO) / samples);
	releaseOffset = -releaseTCO * (1.0f - releaseCoeff);
}
    
}
