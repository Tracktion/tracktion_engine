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

LowPassPlugin::LowPassPlugin (PluginCreationInfo info) : Plugin (info)
{
    auto um = getUndoManager();

    frequencyValue.referTo (state, IDs::frequency, um, 4000.0f);
    modeValue.referTo (state, IDs::mode, um, 0);

    frequency = addParam ("frequency", TRANS("Frequency"), { 10.0f, 22000.0f },
                          [] (float value)              { return juce::String (juce::roundToInt (value)) + " Hz"; },
                          [] (const juce::String& s)    { return s.getFloatValue(); });

    mode = addParam("mode", TRANS("mode"), { 0, 1 },
        [](float value) { return juce::String(juce::roundToInt(value)); },
        [](const juce::String& s) { return s.getFloatValue(); });

    frequency->attachToCurrentValue (frequencyValue);
    mode->attachToCurrentValue(modeValue);
}

LowPassPlugin::~LowPassPlugin()
{
    notifyListenersOfDeletion();

    frequency->detachFromCurrentValue();
    mode->detachFromCurrentValue();
}

const char* LowPassPlugin::xmlTypeName = "lowpass";
// BEATCONNECT MODIFICATION START
const char* LowPassPlugin::uniqueId = "b82be959-2b55-43ec-8207-6b8b899dc0dc";
// BEATCONNECT MODIFICATION END

void LowPassPlugin::updateFilters()
{
    const float newFreq = frequency->getCurrentValue();
    const bool nowLowPass = mode->getCurrentValue() == 0;

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
