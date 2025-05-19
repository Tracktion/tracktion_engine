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

OutputDevice::OutputDevice (Engine& e, juce::String n, juce::String idToUse)
   : IODevice (e, std::move (n), std::move (idToUse))
{
}

OutputDevice::~OutputDevice()
{
}


//==============================================================================
OutputDeviceInstance::OutputDeviceInstance (OutputDevice& d, EditPlaybackContext& c)
    : owner (d), context (c), edit (c.edit)
{
}

OutputDeviceInstance::~OutputDeviceInstance()
{
}

}} // namespace tracktion { inline namespace engine
