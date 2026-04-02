/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {
} // namespace tracktion::inline engine

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CONSTRAINED_CACHED_VALUE
#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
namespace tracktion::inline engine {

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("ConstrainedCachedValue")
    {
        SUBCASE ("Odd/even tests")
        {
            juce::ValueTree state ("TREE");
            ConstrainedCachedValue<int> value;
            value.setConstrainer ([] (int v) { return v % 2 == 0 ? v : v + 1; });
            value.referTo (state, "value", nullptr);

            CHECK_EQ (value.get(), 0);
            value = 1;
            CHECK_EQ (value.get(), 2);
            value = 2;
            CHECK_EQ (value.get(), 2);
            value = 17;
            CHECK_EQ (value.get(), 18);
        }

        SUBCASE ("ceil/floor tests")
        {
            juce::ValueTree state ("TREE");
            ConstrainedCachedValue<float> value;
            bool useCeil = true;
            value.setConstrainer ([&useCeil] (float v) { return useCeil ? std::ceil (v) : std::floor (v); });
            value.referTo (state, "value", nullptr);

            CHECK (std::abs (value.getDefault() - 0.0f) <= 0.00000000000001f);
            CHECK (std::abs (value.get() - 0.0f) <= 0.00000000000001f);
            value = 1.1f;
            CHECK (std::abs (value.get() - 2.0f) <= 0.00000000000001f);
            value = 2.7f;
            CHECK (std::abs (value.get() - 3.0f) <= 0.00000000000001f);

            useCeil = false;
            value = 2.7f;
            CHECK (std::abs (value.get() - 2.0f) <= 0.00000000000001f);
            value = 1.1f;
            CHECK (std::abs (value.get() - 1.0f) <= 0.00000000000001f);
            value = -3.0f;
            CHECK (std::abs (value.get() - (-3.0f)) <= 0.00000000000001f);

            value.referTo (state, "value", nullptr, 1.2f);
            CHECK (std::abs (value.getDefault() - 1.0f) <= 0.00000000000001f);
            CHECK (std::abs (value.get() - (-3.0f)) <= 0.00000000000001f);
        }
    }
}

} // namespace tracktion::inline engine
#endif // TRACKTION_UNIT_TESTS
