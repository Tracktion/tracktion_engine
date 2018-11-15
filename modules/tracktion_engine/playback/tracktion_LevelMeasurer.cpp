/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


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
LevelMeasurer::Client::Client() {}

void LevelMeasurer::Client::reset() noexcept
{
    for (auto& l : audioLevels)
        l = {};

    for (auto& o : overload)
        o = false;

    midiLevels = {};
    clearOverload = true;
}

bool LevelMeasurer::Client::getAndClearOverload() noexcept
{
    auto result = clearOverload;
    clearOverload = false;
    return result;
}

DbTimePair LevelMeasurer::Client::getAndClearMidiLevel() noexcept
{
    auto result = midiLevels;
    midiLevels.dB = -100.0f;
    return result;
}

DbTimePair LevelMeasurer::Client::getAndClearAudioLevel (int chan) noexcept
{
    jassert (chan >= 0 && chan < maxNumChannels);
    auto result = audioLevels[chan];
    audioLevels[chan].dB = -100.0f;
    return result;
}

//==============================================================================
void LevelMeasurer::processBuffer (juce::AudioBuffer<float>& buffer, int start, int numSamples)
{
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

            for (auto* c : clients)
            {
                if (newDB >= c->audioLevels[i].dB)
                {
                    c->audioLevels[i].dB = newDB;
                    c->audioLevels[i].time = now;
                }

                if (overloaded)
                    c->overload[i] = true;

                c->numChannelsUsed = numChans;
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

            for (auto* c : clients)
            {
                if (newDB >= c->audioLevels[i].dB)
                {
                    c->audioLevels[i].dB = newDB;
                    c->audioLevels[i].time = now;
                }

                if (overloaded)
                    c->overload[i] = true;

                c->numChannelsUsed = numChans;
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

        for (auto* c : clients)
        {
            if (sumDB >= c->audioLevels[0].dB)
            {
                c->audioLevels[0].dB   = sumDB;
                c->audioLevels[0].time = now;
            }

            if (diffDB >= c->audioLevels[1].dB)
            {
                c->audioLevels[1].dB   = diffDB;
                c->audioLevels[1].time = now;
            }

            if (sum  > 0.999f) c->overload[0] = true;
            if (diff > 0.999f) c->overload[1] = true;

            c->numChannelsUsed = 2;
        }
    }
}

void LevelMeasurer::processMidi (MidiMessageArray& midiBuffer, const float*)
{
    if (clients.isEmpty() || ! showMidi)
        return;

    float max = 0.0f;

    for (auto& m : midiBuffer)
        if (m.isNoteOn())
            max = jmax (max, m.getFloatVelocity());

    auto now = Time::getApproximateMillisecondCounter();

    for (auto c : clients)
    {
        auto db = gainToDb (max);

        if (db > c->midiLevels.dB)
        {
            c->midiLevels.dB = db;
            c->midiLevels.time = now;
        }
    }
}

void LevelMeasurer::processMidiLevel (float level)
{
    if (clients.isEmpty() || ! showMidi)
        return;

    auto now = Time::getApproximateMillisecondCounter();

    for (auto c : clients)
    {
        auto db = gainToDb (level);

        if (db > c->midiLevels.dB)
        {
            c->midiLevels.dB = db;
            c->midiLevels.time = now;
        }
    }
}

void LevelMeasurer::clearOverload()
{
    for (auto c : clients)
        c->clearOverload = true;
}

void LevelMeasurer::clear()
{
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
    jassert (! clients.contains (&c));
    clients.add (&c);
}

void LevelMeasurer::removeClient (Client& c)
{
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
LevelMeasuringAudioNode::LevelMeasuringAudioNode (SharedLevelMeasurer::Ptr lm, AudioNode* input)
   : SingleInputAudioNode (input), levelMeasurer (lm)
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
