/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

WaveOutputDevice::WaveOutputDevice (Engine& e, const WaveDeviceDescription& desc)
    : OutputDevice (e, desc.name, "waveout_" + juce::String::toHexString (desc.name.hashCode())),
      deviceDescription (desc),
      originalChannels (desc.channels),
      channelSet (desc.channels.toChannelSet()),
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

    applyChannelOrdering();
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
    if (deviceDescription.getNumChannels() < 2)
        return;

    if (leftRightReversed != shouldReverse)
    {
        leftRightReversed = shouldReverse;
        applyChannelOrdering();
        changed();
        saveProps();
    }
}

void WaveOutputDevice::applyChannelOrdering()
{
    if (leftRightReversed)
        deviceDescription.channels = originalChannels.reversed();
    else
        deviceDescription.channels = originalChannels;

    channelSet = deviceDescription.channels.toChannelSet();
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

void WaveOutputDevice::updateLevelMeasurer (float* const* outputChannelData, int totalNumOutputChannels, int numSamples)
{
    auto& channels = getChannels();
    auto numDeviceChannels = (int) channels.getNumChannels();

    if (numDeviceChannels <= 0)
        return;

    meterChannelPtrs.resize ((size_t) numDeviceChannels);

    for (int i = 0; i < numDeviceChannels; ++i)
    {
        auto idx = channels[(size_t) i].indexInDevice;

        if (idx < 0 || idx >= totalNumOutputChannels || outputChannelData[idx] == nullptr)
            return;

        meterChannelPtrs[(size_t) i] = outputChannelData[idx];
    }

    juce::AudioBuffer<float> buf (meterChannelPtrs.data(), numDeviceChannels, numSamples);
    levelMeasurer.processBuffer (buf, 0, numSamples);
}

void WaveOutputDevice::startTestTone()
{
    const auto sampleRate = engine.getDeviceManager().getSampleRate();

    if (sampleRate <= 0)
        return;

    testTonePendingSampleRate.store ((float) sampleRate, std::memory_order_relaxed);
    testTonePendingTotalSamples.store (juce::roundToInt (sampleRate), std::memory_order_relaxed);
    testToneStartRequested.store (true, std::memory_order_release);
}

void WaveOutputDevice::renderTestTone (float* const* outputChannelData, int totalNumOutputChannels, int numSamples)
{
    if (testToneStartRequested.exchange (false, std::memory_order_acquire))
    {
        testToneTotalSamples = testTonePendingTotalSamples.load (std::memory_order_relaxed);
        testToneSampleIndex = 0;
        testToneSine.resetPhase();
        testToneSine.setFrequency (440.0f, testTonePendingSampleRate.load (std::memory_order_relaxed));
    }

    if (testToneSampleIndex < 0)
        return;

    constexpr float amplitude = 0.5f;
    const int total = testToneTotalSamples;
    const int fadeInSamples = juce::jmax (1, total / 10);
    const int fadeOutStart  = total - juce::jmax (1, total / 4);

    auto& channels = getChannels();
    const auto numDeviceChannels = (int) channels.getNumChannels();

    const int samplesAvailable = juce::jmin (numSamples, total - testToneSampleIndex);

    for (int s = 0; s < samplesAvailable; ++s)
    {
        const int i = testToneSampleIndex + s;
        float env = 1.0f;

        if (i < fadeInSamples)
            env = (float) i / (float) fadeInSamples;
        else if (i >= fadeOutStart)
            env = (float) (total - i) / (float) (total - fadeOutStart);

        const float sample = amplitude * env * testToneSine.getSample();

        for (int c = 0; c < numDeviceChannels; ++c)
        {
            const auto idx = channels[(size_t) c].indexInDevice;

            if (idx >= 0 && idx < totalNumOutputChannels)
                if (auto* dest = outputChannelData[idx])
                    dest[s] += sample;
        }
    }

    testToneSampleIndex += samplesAvailable;

    if (testToneSampleIndex >= total)
        testToneSampleIndex = -1;
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
    const auto numChannels = (int) getWaveOutput().getChannels().getNumChannels();
    outputBuffer.setSize (numChannels, blockSize);

    int ditherDepth = juce::jlimit (16, 32, edit.engine.getDeviceManager().getBitDepth());

    ditherers.resize ((size_t) numChannels);

    for (auto& d : ditherers)
        d.reset (ditherDepth);
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
    auto currentNumChannels = deviceDescription.getNumChannels();
    auto options = dm.getPossibleChannelGroupsForDevice (*this, DeviceManager::maxNumChannelsPerDevice);
    auto totalOptions = (uint32_t) options.size();

    for (uint32_t num = 1; num <= totalOptions; ++num)
    {
        if (shouldIncludeChannelCount (num, totalOptions))
            m.addItem (options[(int) (num - 1)], true, num == currentNumChannels,
                       [this, &dm, num] { dm.setDeviceNumChannels (*this, num); });
    }

    if (totalOptions > 8)
    {
        m.addSeparator();
        m.addItem (TRANS("Custom channel count..."), [this, &dm, totalOptions]
                   { showCustomChannelCountDialog (dm, *this, totalOptions); });
    }

    if (includeSetAllChannelsOptions)
    {
        m.addSeparator();
        m.addItem ("Set all to mono channels", [&dm] { dm.setAllWaveOutputsToNumChannels (1); });
        m.addItem ("Set all to stereo pairs",  [&dm] { dm.setAllWaveOutputsToNumChannels (2); });
    }

    return m;
}

} // namespace tracktion::inline engine
