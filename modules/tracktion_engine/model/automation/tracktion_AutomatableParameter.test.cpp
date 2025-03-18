/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_BENCHMARKS && ENGINE_BENCHMARKS_AUTOMATIONITERATOR

namespace tracktion::inline engine
{

//==============================================================================
//==============================================================================
class AutomationIteratorBenchmarks  : public juce::UnitTest
{
public:
    AutomationIteratorBenchmarks()
        : juce::UnitTest ("AutomationIterator", "tracktion_benchmarks")
    {}

    void runTest() override
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
        auto r = getRandom();
        constexpr auto end = 180_tp;

        AutomationCurve straightCurve (*edit, AutomationCurve::TimeBase::time);
        AutomationCurve curvedCurve (*edit, AutomationCurve::TimeBase::time);
        AutomationCurve extremeCurvedCurve (*edit, AutomationCurve::TimeBase::time);
        AutomationCurve mixedCurve (*edit, AutomationCurve::TimeBase::time);

        auto nextCurveVal           = [&r] { return r.nextFloat() - 0.5f; };
        auto nextExtremeCurveVal    = [&r] { return r.nextFloat() * 0.5f + 0.5f * (r.nextBool() ? 1.0f : -1.0f); };

        auto iterate = [end] (auto& iterator, auto& pb)
        {
            [[maybe_unused]] volatile float val = 0.0f;

            for (auto t = 0_tp; t < end; t = t + 3ms)
            {
                ScopedMeasurement sm (pb.benchmark);
                iterator.setPosition (t);
                val = iterator.getCurrentValue();
            }
        };

        auto randomAccess = [&r, end] (auto& iterator, auto& pb)
        {
            [[maybe_unused]] volatile float val = 0.0f;

            for (int i = 0; i < 10'000; ++i)
            {
                auto t = end * r.nextDouble();

                ScopedMeasurement sm (pb.benchmark);
                iterator.setPosition (t);
                val = iterator.getCurrentValue();
            }
        };

        beginTest ("Benchmark: AutomationIterator");
        {
            {
                for (int i = 0; i < 90; ++i)
                {
                    auto t = (end / 90) * i;
                    auto v = r.nextFloat();
                    straightCurve.addPoint (t, v, 0.0, nullptr);
                }

                {
                    auto iter = [&]
                    {
                        ScopedBenchmark sb (getDescription ("Create time-based 3min curve with 90 points, c=0, accurate"));
                        return AutomationIterator (*edit, straightCurve);
                    }();

                    {
                        PublishingBenchmark pb (getDescription ("Iterate time-based 3min curve with 90 points 3ms interval, c=0, accurate"));
                        iterate (iter, pb);
                    }

                    {
                        PublishingBenchmark pb (getDescription ("Random access time-based 3min curve with 90 points 10'000 positions, c=0, accurate"));
                        randomAccess (iter, pb);
                    }
                }
            }

            {
                for (int i = 0; i < 90; ++i)
                {
                    auto t = (end / 90) * i;
                    auto v = r.nextFloat();
                    auto c = nextCurveVal();
                    curvedCurve.addPoint (t, v, c, nullptr);
                }

                {
                    auto iter = [&]
                    {
                        ScopedBenchmark sb (getDescription ("Create time-based 3min curve with 90 points, c=0.5, accurate"));
                        return AutomationIterator (*edit, curvedCurve);
                    }();

                    {
                        PublishingBenchmark pb (getDescription ("Iterate time-based 3min curve with 90 points 3ms interval, c=0.5, accurate"));
                        iterate (iter, pb);
                    }

                    {
                        PublishingBenchmark pb (getDescription ("Random access time-based 3min curve with 90 points 10'000 positions, c=0.5, accurate"));
                        randomAccess (iter, pb);
                    }
                }
            }

            {
                for (int i = 0; i < 90; ++i)
                {
                    auto t = (end / 90) * i;
                    auto v = r.nextFloat();
                    auto c = nextExtremeCurveVal();
                    extremeCurvedCurve.addPoint (t, v, c, nullptr);
                }

                {
                    auto iter = [&]
                    {
                        ScopedBenchmark sb (getDescription ("Create time-based 3min curve with 90 points, c=1.0, accurate"));
                        return AutomationIterator (*edit, extremeCurvedCurve);
                    }();

                    {
                        PublishingBenchmark pb (getDescription ("Iterate time-based 3min curve with 90 points 3ms interval, c=1.0, accurate"));
                        iterate (iter, pb);
                    }

                    {
                        PublishingBenchmark pb (getDescription ("Random access time-based 3min curve with 90 points 10'000 positions, c=1.0, accurate"));
                        randomAccess (iter, pb);
                    }
                }
            }

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

                {
                    auto iter = [&]
                    {
                        ScopedBenchmark sb (getDescription ("Create time-based 3min curve with 90 points, c=mixed, accurate"));
                        return AutomationIterator (*edit, mixedCurve);
                    }();

                    {
                        PublishingBenchmark pb (getDescription ("Iterate time-based 3min curve with 90 points 3ms interval, c=mixed, accurate"));
                        iterate (iter, pb);
                    }

                    {
                        PublishingBenchmark pb (getDescription ("Random access time-based 3min curve with 90 points 10'000 positions, c=mixed, accurate"));
                        randomAccess (iter, pb);
                    }
                }
            }
        }
    }

private:
    BenchmarkDescription getDescription (std::string bmName)
    {
        const auto bmCategory = (getName() + "/" + getCategory()).toStdString();
        const auto bmDescription = bmName;

        return { std::hash<std::string>{} (bmName + bmCategory + bmDescription),
                 bmCategory, bmName, bmDescription };
    }
};

static AutomationIteratorBenchmarks automationIteratorBenchmarks;

} // namespace tracktion::inline engine

#endif //TRACKTION_BENCHMARKS && ENGINE_BENCHMARKS_AUTOMATIONITERATOR
