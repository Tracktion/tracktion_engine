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

struct BeatDetect
{
    BeatDetect() = default;

    void setSampleRate (double sampleRate)
    {
        blockSize = juce::roundToInt (sampleRate / 43.06640625);
    }

    void setSensitivity (double newSensitivity)
    {
        const double C_MIN = 0.8f;
        const double C_MAX = 4.0f;

        sensitivity = C_MIN + newSensitivity * (C_MAX - C_MIN);
    }

    template <typename Buffer>
    void audioProcess (const Buffer& buffer)
    {
        double blockEnergy = 0;
        auto size = buffer.getSize();

        for (decltype (size.numChannels) chan = 0; chan < size.numChannels; ++chan)
        {
            auto d = buffer.getIterator (chan);

            for (decltype (size.numFrames) i = 0; i < size.numFrames; ++i)
            {
                auto sample = static_cast<double> (*d);
                blockEnergy += sample * sample;
                ++d;
            }
        }

        pushEnergy (blockEnergy);
    }

    SampleCount getBlockSize() const                    { return blockSize; }
    const juce::Array<SampleCount>& getBeats() const    { return beatSamples; }

private:
    static constexpr int historyLength = 43;
    double energy[historyLength] = {};
    int curBlock = 0, lastBlock = -2;
    SampleCount blockSize = 0;
    juce::Array<SampleCount> beatSamples;
    double sensitivity = 0;

    void pushEnergy (double e)
    {
        if (curBlock < historyLength)
        {
            energy[curBlock++] = e;

            if (curBlock == historyLength)
            {
                auto ave = getAverageEnergy();

                for (int i = 0; i < historyLength; ++i)
                    if (energy[i] > sensitivity * ave)
                        addBlock(i);
            }
        }
        else
        {
            energy[curBlock % historyLength] = e;

            if (e > sensitivity * getAverageEnergy())
                addBlock(curBlock);

            curBlock++;
        }
    }

    void addBlock (int i)
    {
        if (i - 1 != lastBlock)
            beatSamples.add (i * blockSize);

        lastBlock = i;
    }

    double getAverageEnergy() const noexcept
    {
        double total = 0;

        for (auto e : energy)
            total += e;

        return total / historyLength;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatDetect)
};

}} // namespace tracktion { inline namespace engine
