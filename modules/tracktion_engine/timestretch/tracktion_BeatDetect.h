/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

struct BeatDetect
{
    BeatDetect() {}

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

    void audioProcess (const float** inputs, int numChans)
    {
        double blockEnergy = 0;

        for (int chan = numChans; --chan >= 0;)
        {
            const float* const c = inputs[chan];

            for (int i = 0; i < blockSize; ++i)
                blockEnergy += c[i] * c[i];
        }

        pushEnergy (blockEnergy);
    }

    int getBlockSize()                      { return blockSize; }
    int getNumBeats()                       { return beatBlocks.size(); }
    juce::int64 getBeat (int idx) const     { return beatBlocks[idx] * blockSize; }

private:
    enum { historyLength = 43 };
    double energy [historyLength];
    int curBlock = 0;
    int lastBlock = -2;
    int blockSize = 0;
    juce::Array<int> beatBlocks;
    double sensitivity = 0;

    void pushEnergy (double e)
    {
        if (curBlock < historyLength)
        {
            energy [curBlock++] = e;

            if (curBlock == historyLength)
            {
                double ave = getAverageEnergy();

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
            beatBlocks.add(i);

        lastBlock = i;
    }

    double getAverageEnergy() const noexcept
    {
        double avEnergy = 0;

        for (int i = 0; i < historyLength; ++i)
            avEnergy += energy[i];

        return avEnergy / historyLength;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatDetect)
};

} // namespace tracktion_engine
