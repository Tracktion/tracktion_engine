/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_LAUNCH_QUANTISATION

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"

namespace tracktion::inline engine {

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("LaunchQuantisation")
    {
        auto& engine = *Engine::getEngines()[0];

        // Type fractions
        CHECK (std::abs (toBarFraction (LaunchQType::none)           - 0.0)            <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::eightBars)      - 8.0)            <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::fourBars)       - 4.0)            <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::twoBars)        - 2.0)            <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::bar)            - 1.0)            <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::halfT)          - 0.3333333333)   <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::half)           - 0.5)            <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::halfD)          - 0.75)           <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::quarterT)       - 0.1666666667)   <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::quarter)        - 0.25)           <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::quarterD)       - 0.375)          <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::eighthT)        - 0.08333333333)  <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::eighth)         - 0.125)          <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::eighthD)        - 0.1875)         <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::sixteenthT)     - 0.04166666667)  <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::sixteenth)      - 0.0625)         <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::sixteenthD)     - 0.09375)        <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::thirtySecondT)  - 0.02083333333)  <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::thirtySecond)   - 0.03125)        <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::thirtySecondD)  - 0.046875)       <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::sixtyFourthT)   - 0.01041666667)  <= 0.000001);
        CHECK (std::abs (toBarFraction (LaunchQType::sixtyFourth)    - 0.015625)       <= 0.000001);

        auto edit = test_utilities::createTestEdit (engine, 1);
        const auto& ts = edit->tempoSequence;

        auto expectNext = [&ts] (auto current, auto next, LaunchQType q)
                         {
                             const auto n = getNext (q, ts, current);
                             CHECK (std::abs (n.inBeats() - next.inBeats()) <= 0.000001);
                         };

        // Eight Bars
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

        // Four Bars
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

        // Two Bars
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

        // Bar
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

        // Half
        {
            auto expectQ = [&] (auto c, auto n) { expectNext (c, n, LaunchQType::half); };

            expectQ (0_bp,      0_bp);

            expectQ (0.1_bp,    2_bp);
            expectQ (1_bp,      2_bp);
            expectQ (1.5_bp,    2_bp);
            expectQ (2_bp,      2_bp);

            expectQ (3_bp,      4_bp);
            expectQ (4_bp,      4_bp);

            expectQ (5_bp,      6_bp);
            expectQ (6_bp,      6_bp);

            expectQ (7_bp,      8_bp);
            expectQ (8_bp,      8_bp);

            expectQ (8.01_bp,   10_bp);
            expectQ (9_bp,      10_bp);
        }

        // Quarter
        {
            auto expectQ = [&] (auto c, auto n) { expectNext (c, n, LaunchQType::quarter); };

            expectQ (0_bp, 0_bp);

            expectQ (0.1_bp, 1_bp);
            expectQ (0.5_bp, 1_bp);
            expectQ (0.6_bp, 1_bp);
            expectQ (0.9_bp, 1_bp);
            expectQ (1.0_bp, 1_bp);

            expectQ (1.1_bp, 2_bp);
            expectQ (1.5_bp, 2_bp);
            expectQ (1.9_bp, 2_bp);
            expectQ (2.0_bp, 2_bp);

            expectQ (2.1_bp, 3_bp);
            expectQ (2.9_bp, 3_bp);

            expectQ (3.1_bp, 4_bp);
            expectQ (3.9_bp, 4_bp);
        }
    }
}

} // namespace tracktion::inline engine

#endif
