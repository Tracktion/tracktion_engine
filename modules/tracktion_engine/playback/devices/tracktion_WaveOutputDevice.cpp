/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


WaveOutputDevice::WaveOutputDevice (Engine& e, const String& name, const std::vector<ChannelIndex>& channels)
    : OutputDevice (e, TRANS("Wave Audio Output"), name),
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

String WaveOutputDevice::openDevice()
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
    XmlElement n ("SETTINGS");

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

    int ditherDepth = jlimit (16, 32, edit.engine.getDeviceManager().getBitDepth());

    ditherers[0].reset (ditherDepth);
    ditherers[1].reset (ditherDepth);
}

void WaveOutputDeviceInstance::fillNextAudioBlock (PlayHead& playhead, EditTimeRange streamTime,
                                                   float** allChannels, int numSamples)
{
    CRASH_TRACER
    const ScopedLock sl (audioNodeLock);

    WaveOutputDevice& wo = getWaveOutput();
    const auto& channels = wo.getChannels();

    const auto& channelSet = wo.getChannelSet();
    midiBuffer.clear();
    outputBuffer.setSize (jmax (2, channelSet.size()), numSamples);

    if (audioNode != nullptr)
    {
        AudioRenderContext rc (playhead, streamTime,
                               &outputBuffer, channelSet, 0, numSamples,
                               &midiBuffer, 0,
                               AudioRenderContext::contiguous, false);

        {
            SCOPED_REALTIME_CHECK_LONGER
            audioNode->prepareForNextBlock (rc);
        }

        SCOPED_REALTIME_CHECK_LONGER
        audioNode->renderOver (rc);

        if (wo.ditheringEnabled)
            for (int i = 0; i < outputBuffer.getNumChannels(); ++i)
                ditherers[i & 1].process (outputBuffer.getWritePointer (i), numSamples);

        jassert (outputBuffer.getNumSamples() == numSamples);
        outputBuffer.setSize (outputBuffer.getNumChannels(), numSamples, true); // (once had an unreproducible case where this buffer had been resized..)
        sanitiseValues (outputBuffer, 0, numSamples, 3.0f, 0.0f);
    }
    else
    {
        outputBuffer.clear();
    }

    SCOPED_REALTIME_CHECK

    // This may need to deal with folding surround to stereo etc. but at the moment this us unsupported
    if (edit.engine.getEngineBehaviour().isDescriptionOfWaveDevicesSupported())
    {
        jassert (outputBuffer.getNumChannels() == (int) channels.size());      //we can allow >= as well, but not <

        for (const auto& ci : channels)
        {
            const int srcIndex = channelSet.getChannelIndexForType (ci.channel);
            jassert (srcIndex != -1);

            if (srcIndex < outputBuffer.getNumChannels())
                FloatVectorOperations::add (allChannels[ci.indexInDevice], outputBuffer.getReadPointer (srcIndex), numSamples);
        }
    }
    else
    {
        float* left = nullptr;
        float* right = nullptr;

        auto findChannelIndex = [&channels] (juce::AudioChannelSet::ChannelType ct) -> int
        {
            for (const auto& ci : channels)
                if (ct == ci.channel)
                    return ci.indexInDevice;

            return -1;
        };

        const int chanL = findChannelIndex (juce::AudioChannelSet::left);
        const int chanR = findChannelIndex (juce::AudioChannelSet::right);

        if (chanL < 0)
        {
            jassert (chanR >= 0);
            left = allChannels[chanR];
        }
        else
        {
            left = allChannels[chanL];

            if (chanR >= 0)
                right = allChannels[chanR];
        }

        if (getWaveOutput().leftRightReversed)
            std::swap (left, right);

        if (right == nullptr)
        {
            if (left != nullptr)
            {
                if (outputBuffer.getNumChannels() > 1)
                {
                    // too many channels, so merge down the second into channel 0..
                    FloatVectorOperations::addWithMultiply (left, outputBuffer.getReadPointer (0), 0.5f, numSamples);
                    FloatVectorOperations::addWithMultiply (left, outputBuffer.getReadPointer (1), 0.5f, numSamples);
                }
                else
                {
                    FloatVectorOperations::add (left, outputBuffer.getReadPointer (0), numSamples);
                }
            }
        }
        else
        {
            if (left != nullptr)
                FloatVectorOperations::add (left,  outputBuffer.getReadPointer (0), numSamples);

            if (right != nullptr)
                FloatVectorOperations::add (right, outputBuffer.getReadPointer (jmin (outputBuffer.getNumChannels() - 1, 1)), numSamples);
        }
    }

    context.masterLevels.processBuffer (outputBuffer, 0, numSamples);
}

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
