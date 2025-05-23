/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

LowPassPlugin::LowPassPlugin (PluginCreationInfo info) : Plugin (info)
{
    auto um = getUndoManager();

    frequencyValue.referTo (state, IDs::frequency, um, 4000.0f);
    mode.referTo (state, IDs::mode, um, "lowpass");

    frequency = addParam ("frequency", TRANS("Frequency"), { 10.0f, 22000.0f },
                          [] (float value)              { return juce::String (juce::roundToInt (value)) + " Hz"; },
                          [] (const juce::String& s)    { return s.getFloatValue(); });

    frequency->attachToCurrentValue (frequencyValue);
}

LowPassPlugin::~LowPassPlugin()
{
    notifyListenersOfDeletion();

    frequency->detachFromCurrentValue();
}

const char* LowPassPlugin::xmlTypeName = "lowpass";

void LowPassPlugin::updateFilters()
{
    const float newFreq = frequency->getCurrentValue();
    const bool nowLowPass = isLowPass();

    if (currentFilterFreq != newFreq || nowLowPass != isCurrentlyLowPass)
    {
        currentFilterFreq = newFreq;
        isCurrentlyLowPass = nowLowPass;

        auto c = nowLowPass ? juce::IIRCoefficients::makeLowPass  (sampleRate, newFreq)
                            : juce::IIRCoefficients::makeHighPass (sampleRate, newFreq);

        filter[0].setCoefficients (c);
        filter[1].setCoefficients (c);
    }
}

void LowPassPlugin::initialise (const PluginInitialisationInfo& info)
{
    sampleRate = info.sampleRate;

    for (int i = 0; i < numElementsInArray (filter); ++i)
        filter[i].reset();

    currentFilterFreq = 0;
    updateFilters();
}

void LowPassPlugin::deinitialise()
{
}

void LowPassPlugin::applyToBuffer (const PluginRenderContext& fc)
{
    if (fc.destBuffer != nullptr)
    {
        SCOPED_REALTIME_CHECK

        updateFilters();

        clearChannels (*fc.destBuffer, 2, -1, fc.bufferStartSample, fc.bufferNumSamples);

        for (int i = std::min (2, fc.destBuffer->getNumChannels()); --i >= 0;)
            filter[i].processSamples (fc.destBuffer->getWritePointer (i, fc.bufferStartSample), fc.bufferNumSamples);

        sanitiseValues (*fc.destBuffer, fc.bufferStartSample, fc.bufferNumSamples, 3.0f);
    }
}

}} // namespace tracktion { inline namespace engine
