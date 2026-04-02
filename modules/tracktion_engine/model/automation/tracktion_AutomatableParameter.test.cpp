/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_BENCHMARKS && ENGINE_BENCHMARKS_AUTOMATIONITERATOR

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine
{

TEST_SUITE ("tracktion_engine")
{

TEST_CASE ("Benchmark: AutomationIterator")
{
    // Create an empty edit
    // Create a 3 min curve with a point every 2s
    // Create with:
    // - straight sections
    // - curves -0.5 - 0.5
    // - curves greater than +-0.5
    // - even mix
    // Benchmark construction time
    // Benchmark sequential iteration every 3ms
    // Benchmark 10'000 random accesses

    auto& engine = *tracktion::engine::Engine::getEngines()[0];
    auto edit = Edit::createSingleTrackEdit (engine);
    auto r = juce::Random::getSystemRandom();
    constexpr auto end = 180_tp;

    AutomationCurve straightCurve (*edit, AutomationCurve::TimeBase::time);
    AutomationCurve curvedCurve (*edit, AutomationCurve::TimeBase::time);
    AutomationCurve extremeCurvedCurve (*edit, AutomationCurve::TimeBase::time);
    AutomationCurve mixedCurve (*edit, AutomationCurve::TimeBase::time);

    auto nextCurveVal           = [&r] { return r.nextFloat() - 0.5f; };
    auto nextExtremeCurveVal    = [&r] { return r.nextFloat() * 0.5f + 0.5f * (r.nextBool() ? 1.0f : -1.0f); };

    auto iterate = [end] (auto& iterator)
    {
        [[maybe_unused]] volatile float val = 0.0f;

        for (auto t = 0_tp; t < end; t = t + 3ms)
        {
            iterator.setPosition (t);
            val = iterator.getCurrentValue();
        }
    };

    auto randomAccess = [&r, end] (auto& iterator)
    {
        [[maybe_unused]] volatile float val = 0.0f;

        for (int i = 0; i < 10'000; ++i)
        {
            auto t = end * r.nextDouble();
            iterator.setPosition (t);
            val = iterator.getCurrentValue();
        }
    };

    // Straight curve
    {
        for (int i = 0; i < 90; ++i)
        {
            auto t = (end / 90) * i;
            auto v = r.nextFloat();
            straightCurve.addPoint (t, v, 0.0, nullptr);
        }

        auto iter = AutomationIterator (*edit, straightCurve);
        iterate (iter);
        randomAccess (iter);
    }

    // Curved curve (c=0.5)
    {
        for (int i = 0; i < 90; ++i)
        {
            auto t = (end / 90) * i;
            auto v = r.nextFloat();
            auto c = nextCurveVal();
            curvedCurve.addPoint (t, v, c, nullptr);
        }

        auto iter = AutomationIterator (*edit, curvedCurve);
        iterate (iter);
        randomAccess (iter);
    }

    // Extreme curved curve (c=1.0)
    {
        for (int i = 0; i < 90; ++i)
        {
            auto t = (end / 90) * i;
            auto v = r.nextFloat();
            auto c = nextExtremeCurveVal();
            extremeCurvedCurve.addPoint (t, v, c, nullptr);
        }

        auto iter = AutomationIterator (*edit, extremeCurvedCurve);
        iterate (iter);
        randomAccess (iter);
    }

    // Mixed curve
    {
        for (int i = 0; i < 90; ++i)
        {
            auto t = (end / 90) * i;
            auto v = r.nextFloat();
            auto c = 0.0f;

            if (i % 3 == 1)
                c = nextCurveVal();
            else if (i % 3 == 2)
                c = nextExtremeCurveVal();

            mixedCurve.addPoint (t, v, c, nullptr);
        }

        auto iter = AutomationIterator (*edit, mixedCurve);
        iterate (iter);
        randomAccess (iter);
    }
}

} // TEST_SUITE

} // namespace tracktion::inline engine

#endif //TRACKTION_BENCHMARKS && ENGINE_BENCHMARKS_AUTOMATIONITERATOR
