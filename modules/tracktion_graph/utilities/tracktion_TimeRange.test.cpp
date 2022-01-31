/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if GRAPH_UNIT_TESTS_TIME

#include "tracktion_TimeRange.h"

namespace tracktion_graph
{

//==============================================================================
//==============================================================================
class TimeRangeTests    : public juce::UnitTest
{
public:
    TimeRangeTests()
        : juce::UnitTest ("TimeRange", "tracktion_graph")
    {
    }
    
    void runTest() override
    {
        using namespace tracktion_graph;
        using namespace std::literals;
        
        beginTest ("TimeRange");
        {

        }

        beginTest ("BeatRange");
        {

        }
    }
};

static TimeRangeTests timeRangeTests;

} // namespace tracktion_graph

#endif //GRAPH_UNIT_TESTS_TIME
