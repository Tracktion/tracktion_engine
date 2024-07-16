/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#if TRACKTION_UNIT_TESTS && TRACKTION_UNIT_TESTS_ALGORITHM

namespace tracktion::inline core
{

//==============================================================================
//==============================================================================
class AlgorithmTests  : public juce::UnitTest
{
public:
    AlgorithmTests()
        : juce::UnitTest ("Algorithm", "tracktion_core")
    {}

    void runTest() override
    {
        beginTest ("stable_remove_duplicates");
        {
            int i1 = 1, i2 = 2, i3 = 3, i4 = 4;
            std::vector<int*> v;
            v.push_back (&i2);
            v.push_back (&i1);
            v.push_back (&i1);
            v.push_back (&i4);
            v.push_back (&i3);
            v.push_back (&i4);
            v.push_back (&i2);

            expectEquals (*v[0], 2);
            expectEquals (*v[1], 1);
            expectEquals (*v[2], 1);
            expectEquals (*v[3], 4);
            expectEquals (*v[4], 3);
            expectEquals (*v[5], 4);
            expectEquals (*v[6], 2);

            stable_remove_duplicates (v);
            expectEquals (*v[0], 2);
            expectEquals (*v[1], 1);
            expectEquals (*v[2], 4);
            expectEquals (*v[3], 3);
        }
    }
};

static AlgorithmTests algorithmTests;

}

#endif // TRACKTION_UNIT_TESTS && TRACKTION_UNIT_TESTS_ALGORITHM
