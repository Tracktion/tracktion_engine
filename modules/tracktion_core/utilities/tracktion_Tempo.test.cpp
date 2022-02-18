/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS_TIME

#include "tracktion_Tempo.h"

namespace tracktion { inline namespace core
{

//==============================================================================
//==============================================================================
class TempoTests    : public juce::UnitTest
{
public:
    TempoTests()
        : juce::UnitTest ("Tempo", "tracktion_core")
    {
    }
    
    void runTest() override
    {
    }
};

static TempoTests tempoTests;

}} // namespace tracktion

#endif //TRACKTION_UNIT_TESTS_TIME
