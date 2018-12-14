/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

//==============================================================================
AirWindowCallback::AirWindowCallback (AirWindowsPlugin& o)
    : owner (o)
{
}

double AirWindowCallback::getSampleRate()
{
    return owner.sampleRate;
}
    
//==============================================================================
AirWindowsPlugin::AirWindowsPlugin (PluginCreationInfo info, std::unique_ptr<AirWindowsBase> base)
    : Plugin (info), callback (*this), impl (std::move (base))
{
    for (int i = 0; i < impl->getNumParameters(); i++)
    {
        char paramName[kVstMaxParamStrLen];
        impl->getParameterName (i, paramName);
        
        auto param = addParam ("param" + String (i), paramName,
                              { 0.0, 1.0 },
                              [] (float value)      {
                                  return String (value);
                              },
                              [] (const String& s)  {
                                  return s.getFloatValue();
                              });
        
        parameters.add (param);
    }
}
    
AirWindowsPlugin::~AirWindowsPlugin()
{
}
    
int AirWindowsPlugin::getNumOutputChannelsGivenInputs (int)
{
    jassert (impl->getNumOutputs() < 8);
    return impl->getNumOutputs();
}

void AirWindowsPlugin::initialise (const PlaybackInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
}

void AirWindowsPlugin::deinitialise()
{
}

void AirWindowsPlugin::applyToBuffer (const AudioRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    SCOPED_REALTIME_CHECK
    
    fc.setMaxNumChannels (impl->getNumOutputs());
    
    float* chanData[8];
    for (int i = 0; i < impl->getNumOutputs(); i++)
        chanData[i] = fc.destBuffer->getWritePointer (i, fc.bufferStartSample);
    
    for (int i = 0; i < impl->getNumParameters(); i++)
        impl->setParameter (i, parameters[i]->getCurrentValue());
    
    impl->processReplacing (chanData, chanData, fc.bufferNumSamples);
    
    zeroDenormalisedValuesIfNeeded (*fc.destBuffer);
}

void AirWindowsPlugin::restorePluginStateFromValueTree (const ValueTree& v)
{
}
    
//==============================================================================
const char* AirWindowsDeEss::xmlTypeName = "airwindows_deess";
    
AirWindowsDeEss::AirWindowsDeEss (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::deess::DeEss> (&callback))
{
}

}
