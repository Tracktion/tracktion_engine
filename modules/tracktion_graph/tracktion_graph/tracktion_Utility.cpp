/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if GRAPH_UNIT_TESTS_SAMPLECONVERSION

namespace tracktion { inline namespace graph
{

class SampleConversionTests : public juce::UnitTest
{
public:
    SampleConversionTests()
        : juce::UnitTest ("SampleConversion", "tracktion_graph")
    {
    }
    
    void runTest() override
    {
        constexpr double sampleRate = 44100.0;
        using STR = juce::String;

        beginTest ("Sample to time and back");
        {
            for (int64_t i = 0; i < int64_t (sampleRate) * 2; ++i)
            {
                auto time = sampleToTime (i, sampleRate);
                auto sample = timeToSample (time, sampleRate);
                
                if (i != sample)
                    expect (false, juce::String ("Sample to time and back not equal s1=S1, t=TIME, s2=S2")
                                    .replace ("S1", STR (i)).replace ("TIME", STR (time)).replace ("S2", STR (sample)));
            }
        }

        beginTest ("Time to samples and back");
        {
            constexpr double startTime = 0.0;
            constexpr double endTime = 2.0;
            constexpr double minTimeDiff = sampleToTime (1, sampleRate) - sampleToTime (0, sampleRate);
            constexpr double increment = minTimeDiff - std::numeric_limits<double>::epsilon();

            for (double t = startTime; t < endTime; t += increment)
            {
                auto sample = timeToSample (t, sampleRate);
                auto time = sampleToTime (sample, sampleRate);
                
                if (! juce::isWithin (t, time, minTimeDiff))
                    expect (false, juce::String ("Time to sample and back not equal t1=TIME1, s=S1 t=TIME2")
                                    .replace ("TIME1", STR (t)).replace ("S1", STR (sample)).replace ("TIME2", STR (time)));
            }
        }
    }
};

static SampleConversionTests sampleConversionTests;

}}

#endif //GRAPH_UNIT_TESTS_SAMPLECONVERSION
