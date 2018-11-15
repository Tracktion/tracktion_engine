/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


#if TRACKTION_UNIT_TESTS

class ConstrainedCachedValueTests   : public juce::UnitTest
{
public:
    ConstrainedCachedValueTests()
        : juce::UnitTest ("ConstrainedCachedValue ", "Tracktion") {}

    //==============================================================================
    void runTest() override
    {
        beginTest ("Odd/even tests");
        {
            juce::ValueTree v ("TREE");
            ConstrainedCachedValue<int> value;
            value.setConstrainer ([] (int v) { return v % 2 == 0 ? v : v + 1; });
            value.referTo (v, "value", nullptr);

            expectEquals (value.get(), 0);
            value = 1;
            expectEquals (value.get(), 2);
            value = 2;
            expectEquals (value.get(), 2);
            value = 17;
            expectEquals (value.get(), 18);
        }

        beginTest ("ceil/floor tests");
        {
            juce::ValueTree v ("TREE");
            ConstrainedCachedValue<float> value;
            bool useCeil = true;
            value.setConstrainer ([&useCeil] (float v) { return useCeil ? std::ceil (v) : std::floor (v); });
            value.referTo (v, "value", nullptr);

            expectWithinAbsoluteError (value.getDefault(), 0.0f, 0.00000000000001f);
            expectWithinAbsoluteError (value.get(), 0.0f, 0.00000000000001f);
            value = 1.1f;
            expectWithinAbsoluteError (value.get(), 2.0f, 0.00000000000001f);
            value = 2.7f;
            expectWithinAbsoluteError (value.get(), 3.0f, 0.00000000000001f);

            useCeil = false;
            value = 2.7f;
            expectWithinAbsoluteError (value.get(), 2.0f, 0.00000000000001f);
            value = 1.1f;
            expectWithinAbsoluteError (value.get(), 1.0f, 0.00000000000001f);
            value = -3.0f;
            expectWithinAbsoluteError (value.get(), -3.0f, 0.00000000000001f);

            value.referTo (v, "value", nullptr, 1.2f);
            expectWithinAbsoluteError (value.getDefault(), 1.0f, 0.00000000000001f);
            expectWithinAbsoluteError (value.get(), -3.0f, 0.00000000000001f);
        }
    }
};

static ConstrainedCachedValueTests constrainedCachedValueTests;

#endif // TRACKTION_UNIT_TESTS
