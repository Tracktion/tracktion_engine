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

        sum = s / buffer.getNumChannels();
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
void LevelMeasurer::Client::reset() noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);
    for (auto& l : audioLevels)
        l = {};

    for (auto& o : overload)
        o = false;

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
    jassert (chan >= 0 && chan < maxNumChannels);
    auto result = audioLevels[chan];
    audioLevels[chan].dB = -100.0f;
    return result;
}

void LevelMeasurer::Client::setNumChannelsUsed (int numChannels) noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);
    numChannelsUsed = numChannels;
}

void LevelMeasurer::Client::setOverload (int channel, bool hasOverloaded) noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);
    overload[channel] = hasOverloaded;
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

void LevelMeasurer::Client::updateAudioLevel (int channel, DbTimePair newAudioLevel) noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);

    if (newAudioLevel.dB >= audioLevels[channel].dB)
        audioLevels[channel] = newAudioLevel;
}

void LevelMeasurer::Client::updateMidiLevel (DbTimePair newMidiLevel) noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);
    
    if (midiLevels.dB >= midiLevels.dB)
        midiLevels = newMidiLevel;
}


//==============================================================================
void LevelMeasurer::processBuffer (juce::AudioBuffer<float>& buffer, int start, int numSamples)
{
    const juce::ScopedLock sl (clientsMutex);

    if (clients.isEmpty())
        return;

    auto numChans = std::min ((int) Client::maxNumChannels, buffer.getNumChannels());
    auto now = juce::Time::getApproximateMillisecondCounter();

    if (mode == LevelMeasurer::peakMode)
    {
        // peak mode
        for (int i = numChans; --i >= 0;)
        {
            auto gain = buffer.getMagnitude (i, start, numSamples);
            bool overloaded = gain > 0.999f;
            auto newDB = gainToDb (gain);

            for (auto c : clients)
            {
                c->updateAudioLevel (i, { now, newDB });

                if (overloaded)
                    c->setOverload (i, true);

                c->setNumChannelsUsed (numChans);
            }
        }
    }
    else if (mode == LevelMeasurer::RMSMode)
    {
        // rms mode
        for (int i = numChans; --i >= 0;)
        {
            auto gain = buffer.getRMSLevel (i, start, numSamples);
            bool overloaded = gain > 0.999f;
            auto newDB = gainToDb (gain);

            for (auto c : clients)
            {
                c->updateAudioLevel (i, { now, newDB });

                if (overloaded)
                    c->setOverload (i, true);

                c->setNumChannelsUsed (numChans);
            }
        }
    }
    else
    {
        // sum + diff
        float sum, diff;
        getSumAndDiff (buffer, sum, diff, start, numSamples);

        auto sumDB  = gainToDb (sum);
        auto diffDB = gainToDb (diff);

        for (auto c : clients)
        {
            c->updateAudioLevel (0, { now, sumDB });
            c->updateAudioLevel (1, { now, diffDB });

            if (sum  > 0.999f) c->setOverload (0, true);
            if (diff > 0.999f) c->setOverload (1, true);

            c->setNumChannelsUsed (2);
        }
    }
}

void LevelMeasurer::processMidi (MidiMessageArray& midiBuffer, const float*)
{
    const juce::ScopedLock sl (clientsMutex);

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
    const juce::ScopedLock sl (clientsMutex);

    if (clients.isEmpty() || ! showMidi)
        return;

    auto now = juce::Time::getApproximateMillisecondCounter();

    for (auto c : clients)
        c->updateMidiLevel ({ now, gainToDb (level) });
}

void LevelMeasurer::clearOverload()
{
    const juce::ScopedLock sl (clientsMutex);

    for (auto c : clients)
        c->setClearOverload (true);
}

void LevelMeasurer::clearPeak()
{
    const juce::ScopedLock sl (clientsMutex);

    for (auto c : clients)
        c->setClearPeak (true);
}

void LevelMeasurer::clear()
{
    const juce::ScopedLock sl (clientsMutex);

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
    const juce::ScopedLock sl (clientsMutex);
    jassert (! clients.contains (&c));
    clients.add (&c);
}

void LevelMeasurer::removeClient (Client& c)
{
    const juce::ScopedLock sl (clientsMutex);
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

}} // namespace tracktion { inline namespace engine
