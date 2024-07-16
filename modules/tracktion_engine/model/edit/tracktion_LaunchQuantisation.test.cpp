/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_LAUNCH_QUANTISATION

#include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
class LaunchQuantisationTests : public juce::UnitTest
{
public:
    LaunchQuantisationTests()
        : juce::UnitTest ("LaunchQuantisation", "tracktion_engine")
    {
    }

    void runTest() override
    {
        auto& engine = *Engine::getEngines()[0];
        runBasicLaunchQuantisationTests (engine);
    }

private:
    void runBasicLaunchQuantisationTests (Engine& engine)
    {
        beginTest ("Type fractions");

        expectWithinAbsoluteError (toBarFraction (LaunchQType::none),           0.0,            0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::eightBars),      8.0,            0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::fourBars),       4.0,            0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::twoBars),        2.0,            0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::bar),            1.0,            0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::halfT),          0.3333333333,   0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::half),           0.5,            0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::halfD),          0.75,           0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::quarterT),       0.1666666667,   0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::quarter),        0.25,           0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::quarterD),       0.375,          0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::eighthT),        0.08333333333,  0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::eighth),         0.125,          0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::eighthD),        0.1875,         0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::sixteenthT),     0.04166666667,  0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::sixteenth),      0.0625,         0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::sixteenthD),     0.09375,        0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::thirtySecondT),  0.02083333333,  0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::thirtySecond),   0.03125,        0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::thirtySecondD),  0.046875,       0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::sixtyFourthT),   0.01041666667,  0.000001);
        expectWithinAbsoluteError (toBarFraction (LaunchQType::sixtyFourth),    0.015625,       0.000001);

        auto edit = test_utilities::createTestEdit (engine, 1);
        const auto& ts = edit->tempoSequence;

        auto expectNext = [this, &ts] (auto current, auto next, LaunchQType q)
                         {
                             const auto n = getNext (q, ts, current);
                             expectWithinAbsoluteError (n.inBeats(), next.inBeats(), 0.000001);
                         };

        beginTest ("Bar");
        {
            auto expectQ = [&] (auto c, auto n) { expectNext (c, n, LaunchQType::bar); };

            expectQ (0_bp, 0_bp);
            expectQ (1_bp, 4_bp);
            expectQ (2_bp, 4_bp);
            expectQ (3_bp, 4_bp);
            expectQ (4_bp, 4_bp);
            expectQ (5_bp, 8_bp);
            expectQ (6_bp, 8_bp);
            expectQ (7_bp, 8_bp);
            expectQ (8_bp, 8_bp);
            expectQ (9_bp, 12_bp);
        }

        beginTest ("Two Bars");
        {
            auto expectQ = [&] (auto c, auto n) { expectNext (c, n, LaunchQType::twoBars); };

            expectQ (0_bp, 0_bp);
            expectQ (1_bp, 8_bp);
            expectQ (2_bp, 8_bp);
            expectQ (3_bp, 8_bp);
            expectQ (4_bp, 8_bp);
            expectQ (5_bp, 8_bp);
            expectQ (6_bp, 8_bp);
            expectQ (7_bp, 8_bp);
            expectQ (8_bp, 8_bp);
            expectQ (9_bp, 16_bp);
        }

        beginTest ("Four Bars");
        {
            auto expectQ = [&] (auto c, auto n) { expectNext (c, n, LaunchQType::fourBars); };

            expectQ (0_bp, 0_bp);
            expectQ (1_bp, 16_bp);
            expectQ (8_bp, 16_bp);
            expectQ (15_bp, 16_bp);
            expectQ (16_bp, 16_bp);
            expectQ (17_bp, 32_bp);
            expectQ (32_bp, 32_bp);
        }

        beginTest ("Eight Bars");
        {
            auto expectQ = [&] (auto c, auto n) { expectNext (c, n, LaunchQType::eightBars); };

            expectQ (0_bp, 0_bp);
            expectQ (1_bp, 32_bp);
            expectQ (8_bp, 32_bp);
            expectQ (16_bp, 32_bp);
            expectQ (32_bp, 32_bp);
            expectQ (33_bp, 64_bp);
            expectQ (63_bp, 64_bp);
            expectQ (64_bp, 64_bp);
            expectQ (65_bp, 96_bp);
        }

        beginTest ("Half");
        {
            auto expectQ = [&] (auto c, auto n) { expectNext (c, n, LaunchQType::half); };

            expectQ (0_bp, 0_bp);
            expectQ (0.1_bp, 0.5_bp);
            expectQ (0.5_bp, 0.5_bp);

            expectQ (0.9_bp, 1_bp);
            expectQ (1.0_bp, 1_bp);

            expectQ (1.1_bp, 1.5_bp);
            expectQ (1.5_bp, 1.5_bp);

            expectQ (1.9_bp, 2_bp);
            expectQ (2.0_bp, 2_bp);
        }

        beginTest ("Quarter");
        {
            auto expectQ = [&] (auto c, auto n) { expectNext (c, n, LaunchQType::quarter); };

            expectQ (0_bp, 0_bp);
            expectQ (0.1_bp, 0.25_bp);
            expectQ (0.5_bp, 0.5_bp);
            expectQ (0.6_bp, 0.75_bp);

            expectQ (0.9_bp, 1_bp);
            expectQ (1.0_bp, 1_bp);

            expectQ (1.1_bp, 1.25_bp);
            expectQ (1.5_bp, 1.5_bp);

            expectQ (1.9_bp, 2_bp);
            expectQ (2.0_bp, 2_bp);
        }
    }
};

static LaunchQuantisationTests launchQuantisationTests;

}} // namespace tracktion { inline namespace engine

#endif
