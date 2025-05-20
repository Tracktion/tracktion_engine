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

WaveOutputDevice::WaveOutputDevice (Engine& e, const WaveDeviceDescription& desc)
    : OutputDevice (e, desc.name, "waveout_" + juce::String::toHexString (desc.name.hashCode())),
      deviceDescription (desc),
      channelSet (createChannelSet (desc.channels)),
      ditheringEnabled (false),
      leftRightReversed (false)
{
    enabled = desc.enabled;
    loadProps();
}

WaveOutputDevice::~WaveOutputDevice()
{
    notifyListenersOfDeletion();
    saveProps();
}

void WaveOutputDevice::resetToDefault()
{
    engine.getPropertyStorage().removePropertyItem (SettingID::waveout, getName());
    loadProps();
}

void WaveOutputDevice::setEnabled (bool b)
{
    if (enabled != b)
    {
        enabled = b;
        changed();
        engine.getDeviceManager().setDeviceEnabled (*this, b);
        // do nothing now! this object is probably deleted..
    }
}

juce::String WaveOutputDevice::openDevice()
{
    return {};
}

void WaveOutputDevice::closeDevice()
{
}

//==============================================================================
void WaveOutputDevice::loadProps()
{
    ditheringEnabled = false;
    leftRightReversed = false;

    if (auto n = engine.getPropertyStorage().getXmlPropertyItem (SettingID::waveout, getName()))
    {
        ditheringEnabled = n->getBoolAttribute ("dithering", ditheringEnabled);
        leftRightReversed = n->getBoolAttribute ("reversed", leftRightReversed);
    }
}

void WaveOutputDevice::saveProps()
{
    juce::XmlElement n ("SETTINGS");

    n.setAttribute ("dithering", ditheringEnabled);
    n.setAttribute ("reversed", leftRightReversed);

    engine.getPropertyStorage().setXmlPropertyItem (SettingID::waveout, getName(), n);
}

void WaveOutputDevice::reverseChannels (bool shouldReverse)
{
    jassert (isStereoPair());

    if (leftRightReversed != shouldReverse)
    {
        leftRightReversed = shouldReverse;
        changed();
        saveProps();
    }
}

void WaveOutputDevice::setDithered (bool dither)
{
    if (ditheringEnabled != dither)
    {
        ditheringEnabled = dither;
        changed();
        saveProps();
    }
}

//==============================================================================
WaveOutputDeviceInstance* WaveOutputDevice::createInstance (EditPlaybackContext& c)
{
    return new WaveOutputDeviceInstance (*this, c);
}

//==============================================================================
WaveOutputDeviceInstance::WaveOutputDeviceInstance (WaveOutputDevice& d, EditPlaybackContext& c)
    : OutputDeviceInstance (d, c),
      outputBuffer (1, 128)
{
}

void WaveOutputDeviceInstance::prepareToPlay (double, int blockSize)
{
    outputBuffer.setSize (2, blockSize);

    int ditherDepth = juce::jlimit (16, 32, edit.engine.getDeviceManager().getBitDepth());

    ditherers[0].reset (ditherDepth);
    ditherers[1].reset (ditherDepth);
}

//==============================================================================
int WaveOutputDevice::getLeftChannel() const
{
    return deviceDescription.getNumChannels() >= 1 ? deviceDescription.channels[0].indexInDevice : -1;
}

int WaveOutputDevice::getRightChannel() const
{
    return deviceDescription.getNumChannels() >= 2 ? deviceDescription.channels[1].indexInDevice : -1;
}

bool WaveOutputDevice::isStereoPair() const
{
    return deviceDescription.getNumChannels() == 2;
}

void WaveOutputDevice::setStereoPair (bool stereo)
{
    engine.getDeviceManager().setDeviceNumChannels (*this, stereo ? 2 : 1);
}

juce::PopupMenu WaveOutputDevice::createChannelGroupMenu (bool includeSetAllChannelsOptions)
{
    juce::PopupMenu m;
    auto& dm = engine.getDeviceManager();
    uint32_t channelNum = 0;
    auto currentNumChannels = deviceDescription.getNumChannels();

    for (auto& option : dm.getPossibleChannelGroupsForDevice (*this, DeviceManager::maxNumChannelsPerDevice))
    {
        auto num = ++channelNum;
        m.addItem (option, true, num == currentNumChannels, [this, &dm, num] { dm.setDeviceNumChannels (*this, num); });
    }

    if (includeSetAllChannelsOptions)
    {
        m.addSeparator();
        m.addItem ("Set all to mono channels", [&dm] { dm.setAllWaveOutputsToNumChannels (1); });
        m.addItem ("Set all to stereo pairs",  [&dm] { dm.setAllWaveOutputsToNumChannels (2); });
    }

    return m;
}

}} // namespace tracktion { inline namespace engine
