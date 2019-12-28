/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

//==============================================================================
template <typename FloatType>
static void getSumAndDiff (const AudioBuffer<FloatType>& buffer,
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
            lo = jmin (lo, mag);
            hi = jmax (hi, mag);
        }

        sum = s / buffer.getNumChannels();
        diff = jmax (FloatType(), hi - lo);
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

void LevelMeasurer::Client::updateAudioLevel (int channel, DbTimePair newAudioLevel) noexcept
{
    juce::SpinLock::ScopedLockType sl (mutex);

    if (newAudioLevel.dB >= audioLevels[channel].dB)
        audioLevels[channel] = newAudioLevel;
}

void LevelMeasurer::Client::updateMidiLevel (DbTimePair newMidiLevel) noexcept
{
    if (midiLevels.dB >= midiLevels.dB)
        midiLevels = newMidiLevel;
}


//==============================================================================
void LevelMeasurer::processBuffer (juce::AudioBuffer<float>& buffer, int start, int numSamples)
{
    const ScopedLock sl (clientsMutex);

    if (clients.isEmpty())
        return;

    float newLevel[Client::maxNumChannels] = {};
    auto numChans = jmin ((int) Client::maxNumChannels, buffer.getNumChannels());
    auto now = Time::getApproximateMillisecondCounter();

    if (mode == LevelMeasurer::peakMode)
    {
        // peak mode
        for (int i = numChans; --i >= 0;)
        {
            auto gain = buffer.getMagnitude (i, start, numSamples);
            newLevel[i] = gain;
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
            newLevel[i] = gain;
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
        newLevel[0] = sum;
        newLevel[1] = diff;

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
    const ScopedLock sl (clientsMutex);

    if (clients.isEmpty() || ! showMidi)
        return;

    float max = 0.0f;

    for (auto& m : midiBuffer)
        if (m.isNoteOn())
            max = jmax (max, m.getFloatVelocity());

    auto now = Time::getApproximateMillisecondCounter();

    for (auto c : clients)
        c->updateMidiLevel ({ now, gainToDb (max) });
}

void LevelMeasurer::processMidiLevel (float level)
{
    const ScopedLock sl (clientsMutex);

    if (clients.isEmpty() || ! showMidi)
        return;

    auto now = Time::getApproximateMillisecondCounter();

    for (auto c : clients)
        c->updateMidiLevel ({ now, gainToDb (level) });
}

void LevelMeasurer::clearOverload()
{
    const ScopedLock sl (clientsMutex);

    for (auto c : clients)
        c->setClearOverload (true);
}

void LevelMeasurer::clear()
{
    const ScopedLock sl (clientsMutex);

    for (auto c : clients)
        c->reset();

    levelCache = -100.0f;
    numActiveChannels = 1;
}

void LevelMeasurer::setMode (LevelMeasurer::Mode m)
{
    clear();
    mode = m;
}

void LevelMeasurer::addClient (Client& c)
{
    const ScopedLock sl (clientsMutex);
    jassert (! clients.contains (&c));
    clients.add (&c);
}

void LevelMeasurer::removeClient (Client& c)
{
    const ScopedLock sl (clientsMutex);
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

//==============================================================================
LevelMeasuringAudioNode::LevelMeasuringAudioNode (SharedLevelMeasurer::Ptr lm, AudioNode* source)
    : SingleInputAudioNode (source), levelMeasurer (lm)
{
}

void LevelMeasuringAudioNode::prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info)
{
    input->prepareAudioNodeToPlay (info);

    if (levelMeasurer != nullptr)
        levelMeasurer->setSize (2, info.blockSizeSamples);
}

void LevelMeasuringAudioNode::prepareForNextBlock (const AudioRenderContext& rc)
{
    input->prepareForNextBlock (rc);

    if (levelMeasurer != nullptr)
        levelMeasurer->startNextBlock (rc.streamTime.getStart());
}

void LevelMeasuringAudioNode::renderOver (const AudioRenderContext& rc)
{
    input->renderOver (rc);

    if (levelMeasurer != nullptr)
        levelMeasurer->addBuffer (*rc.destBuffer, rc.bufferStartSample, rc.bufferNumSamples);
}

void LevelMeasuringAudioNode::renderAdding (const AudioRenderContext& rc)
{
    callRenderOver (rc);
}

}
