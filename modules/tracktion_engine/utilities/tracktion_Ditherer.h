/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/**
    An extremely naive ditherer.
    TODO: this could be done much better!
*/
struct Ditherer
{
    void reset (int numBits) noexcept
    {
        auto wordLen = std::pow (2.0f, (float)(numBits - 1));
        auto invWordLen = 1.0f / wordLen;
        amp = invWordLen / RAND_MAX;
        offset = invWordLen * 0.5f;
    }

    //==============================================================================
    void process (float* samps, int num) noexcept
    {
        while (--num >= 0)
        {
            random2 = random1;
            random1 = rand();

            auto in = *samps;

            // check for dodgy numbers coming in..
            if (in < -0.000001f || in > 0.000001f)
            {
                in += 0.5f * (s1 + s1 - s2);
                auto out = in + offset + amp * (float)(random1 - random2);
                *samps++ = out;
                s2 = s1;
                s1 = in - out;
            }
            else
            {
                *samps++ = in;
                in += 0.5f * (s1 + s1 - s2);
                auto out = in + offset + amp * (float)(random1 - random2);
                s2 = s1;
                s1 = in - out;
            }
        }
    }

    //==============================================================================
    int random1 = 0, random2 = 0;
    float amp = 0, offset = 0;
    float s1 = 0, s2 = 0;
};

} // namespace tracktion_engine
