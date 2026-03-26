/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

//==============================================================================
template <typename FloatType>
static void getSumAndDiff (const juce::AudioBuffer<FloatType>& buffer,
                           FloatType& sum, FloatType& diff,
                           int startIndex, int numSamples)
{
    if (buffer.getNumChannels() == 0)
    {
        sum = 0;
        diff = 0;
    }
    else
    {
        FloatType s  = 0;
        FloatType lo = (FloatType) 1;
        FloatType hi = 0;

        for (int i = buffer.getNumChannels(); --i >= 0;)
        {
            auto mag = buffer.getMagnitude (i, startIndex, numSamples);
            s += mag;
            lo = juce::jmin (lo, mag);
            hi = juce::jmax (hi, mag);
        }

        sum = s / static_cast<FloatType> (buffer.getNumChannels());
        diff = juce::jmax (FloatType(), hi - lo);
    }
}

//==============================================================================
LevelMeasurer::LevelMeasurer()
{
    clear();
}

LevelMeasurer::~LevelMeasurer()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
}

//==============================================================================
int LevelMeasurer::Client::getNumChannelsUsed() const noexcept
{
    return numChannelsUsed;
}

void LevelMeasurer::Client::reset() noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);
    for (auto& l : audioLevels)
        l = {};

    std::fill (overload.begin(), overload.end(), false);

    midiLevels = {};
    clearOverload = true;
}

bool LevelMeasurer::Client::getAndClearOverload() noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);
    auto result = clearOverload;
    clearOverload = false;
    return result;
}

bool LevelMeasurer::Client::getAndClearPeak() noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);
    auto result = clearPeak;
    clearPeak = false;
    return result;
}

DbTimePair LevelMeasurer::Client::getAndClearMidiLevel() noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);
    auto result = midiLevels;
    midiLevels.dB = -100.0f;
    return result;
}

DbTimePair LevelMeasurer::Client::getAndClearAudioLevel (int chan) noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);

    if (chan < 0 || chan >= (int) audioLevels.size())
        return {};

    auto result = audioLevels[(size_t) chan];
    audioLevels[(size_t) chan].dB = -100.0f;
    return result;
}

void LevelMeasurer::Client::setNumChannelsUsed (int numChannels) noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);
    ensureNumChannels (numChannels);
    numChannelsUsed = numChannels;
}

void LevelMeasurer::Client::setOverload (int channel, bool hasOverloaded) noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);
    ensureNumChannels (channel + 1);
    overload[(size_t) channel] = hasOverloaded;
}

void LevelMeasurer::Client::setClearOverload (bool clear) noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);
    clearOverload = clear;
}

void LevelMeasurer::Client::setClearPeak (bool clear) noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);
    clearPeak = clear;
}

void LevelMeasurer::Client::ensureNumChannels (int needed)
{
    if (needed > (int) audioLevels.size())
    {
        audioLevels.resize ((size_t) needed);
        overload.resize ((size_t) needed, false);
    }
}

void LevelMeasurer::Client::updateAudioLevel (int channel, DbTimePair newAudioLevel) noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);
    ensureNumChannels (channel + 1);

    if (newAudioLevel.dB >= audioLevels[(size_t) channel].dB)
        audioLevels[(size_t) channel] = newAudioLevel;
}

void LevelMeasurer::Client::updateMidiLevel (DbTimePair newMidiLevel) noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);

    if (newMidiLevel.dB >= midiLevels.dB)
        midiLevels = newMidiLevel;
}

void LevelMeasurer::Client::updateAllChannels (const ChannelLevel* levels, int numChannels) noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);
    ensureNumChannels (numChannels);
    numChannelsUsed = numChannels;

    for (int i = 0; i < numChannels; ++i)
    {
        if (levels[i].level.dB >= audioLevels[(size_t) i].dB)
            audioLevels[(size_t) i] = levels[i].level;

        if (levels[i].overloaded)
            overload[(size_t) i] = true;
    }
}


//==============================================================================
void LevelMeasurer::processBuffer (juce::AudioBuffer<float>& buffer, int start, int numSamples)
{
    const std::scoped_lock sl (clientsMutex);

    if (clients.isEmpty())
        return;

    auto numChans = buffer.getNumChannels();
    auto now = juce::Time::getApproximateMillisecondCounter();

    // Compute all channel levels first, then batch-update each client with a single lock
    int resultNumChans;

    if (mode == LevelMeasurer::sumDiffMode)
    {
        float sum, diff;
        getSumAndDiff (buffer, sum, diff, start, numSamples);

        resultNumChans = 2;

        if ((int) channelLevels.size() < resultNumChans)
            channelLevels.resize ((size_t) resultNumChans);

        channelLevels[0] = { { now, gainToDb (sum) },  sum  > 0.999f };
        channelLevels[1] = { { now, gainToDb (diff) }, diff > 0.999f };
    }
    else
    {
        resultNumChans = numChans;

        if ((int) channelLevels.size() < resultNumChans)
            channelLevels.resize ((size_t) resultNumChans);

        for (int i = 0; i < resultNumChans; ++i)
        {
            auto gain = (mode == LevelMeasurer::peakMode)
                            ? buffer.getMagnitude (i, start, numSamples)
                            : buffer.getRMSLevel (i, start, numSamples);

            channelLevels[(size_t) i] = { { now, gainToDb (gain) }, gain > 0.999f };
        }
    }

    numActiveChannels = resultNumChans;

    for (auto c : clients)
        c->updateAllChannels (channelLevels.data(), resultNumChans);
}

void LevelMeasurer::processMidi (MidiMessageArray& midiBuffer, const float*)
{
    const std::scoped_lock sl (clientsMutex);

    if (clients.isEmpty() || ! showMidi)
        return;

    float max = 0.0f;

    for (auto& m : midiBuffer)
        if (m.isNoteOn())
            max = juce::jmax (max, m.getFloatVelocity());

    auto now = juce::Time::getApproximateMillisecondCounter();

    for (auto c : clients)
        c->updateMidiLevel ({ now, gainToDb (max) });
}

void LevelMeasurer::processMidiLevel (float level)
{
    const std::scoped_lock sl (clientsMutex);

    if (clients.isEmpty() || ! showMidi)
        return;

    auto now = juce::Time::getApproximateMillisecondCounter();

    for (auto c : clients)
        c->updateMidiLevel ({ now, gainToDb (level) });
}

void LevelMeasurer::clearOverload()
{
    const std::scoped_lock sl (clientsMutex);

    for (auto c : clients)
        c->setClearOverload (true);
}

void LevelMeasurer::clearPeak()
{
    const std::scoped_lock sl (clientsMutex);

    for (auto c : clients)
        c->setClearPeak (true);
}

void LevelMeasurer::clear()
{
    const std::scoped_lock sl (clientsMutex);

    for (auto c : clients)
        c->reset();

    levelCacheL = -100.0f;
    levelCacheR = -100.0f;
    numActiveChannels = 1;
}

void LevelMeasurer::setMode (LevelMeasurer::Mode m)
{
    clear();
    mode = m;
}

void LevelMeasurer::addClient (Client& c)
{
    const std::scoped_lock sl (clientsMutex);
    jassert (! clients.contains (&c));
    clients.add (&c);
}

void LevelMeasurer::removeClient (Client& c)
{
    const std::scoped_lock sl (clientsMutex);
    clients.removeFirstMatchingValue (&c);
}

void LevelMeasurer::setShowMidi (bool show)
{
    showMidi = show;
}

//==============================================================================
void SharedLevelMeasurer::startNextBlock (double streamTime)
{
    juce::SpinLock::ScopedLockType lock (spinLock);

    if (streamTime != lastStreamTime)
    {
        lastStreamTime = streamTime;

        processBuffer (sumBuffer, 0, sumBuffer.getNumSamples());
        sumBuffer.clear();
    }
}

void SharedLevelMeasurer::setSize (int channels, int numSamples)
{
    juce::SpinLock::ScopedLockType lock (spinLock);

    if (channels > sumBuffer.getNumChannels() || numSamples > sumBuffer.getNumSamples())
        sumBuffer.setSize (channels, numSamples);
}

void SharedLevelMeasurer::addBuffer (const juce::AudioBuffer<float>& inBuffer, int startSample, int numSamples)
{
    setSize (2, numSamples);

    juce::SpinLock::ScopedLockType lock (spinLock);

    for (int i = 0; i < juce::jmin (sumBuffer.getNumChannels(), inBuffer.getNumChannels()); ++i)
        sumBuffer.addFrom (i, 0, inBuffer, i, startSample, numSamples);
}

} // namespace tracktion::inline engine
