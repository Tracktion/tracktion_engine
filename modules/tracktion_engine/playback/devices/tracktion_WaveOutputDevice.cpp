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

WaveOutputDevice::WaveOutputDevice (Engine& e, const juce::String& deviceName, const std::vector<ChannelIndex>& channels)
    : OutputDevice (e, TRANS("Wave Audio Output"), deviceName),
      deviceChannels (channels),
      channelSet (createChannelSet (channels)),
      ditheringEnabled (false),
      leftRightReversed (false)
{
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
        engine.getDeviceManager().setWaveOutChannelsEnabled (deviceChannels, b);
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
    return deviceChannels.size() >= 1 ? deviceChannels[0].indexInDevice : -1;
}

int WaveOutputDevice::getRightChannel() const
{
    return deviceChannels.size() >= 2 ? deviceChannels[1].indexInDevice : -1;
}

bool WaveOutputDevice::isStereoPair() const
{
    return deviceChannels.size() == 2;
}

void WaveOutputDevice::setStereoPair (bool stereo)
{
    auto& dm = engine.getDeviceManager();
    
    if (deviceChannels.size() == 2)
        dm.setDeviceOutChannelStereo (std::max (getLeftChannel(), getRightChannel()), stereo);
    else if (deviceChannels.size() == 1)
        dm.setDeviceOutChannelStereo (getLeftChannel(), stereo);
}
    
}} // namespace tracktion { inline namespace engine
