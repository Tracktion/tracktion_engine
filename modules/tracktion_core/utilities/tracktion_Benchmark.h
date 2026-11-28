/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "../../tracktion_graph/utilities/tracktion_PerformanceMeasurement.h"

namespace tracktion::inline engine {

//==============================================================================
//==============================================================================
/** Describes a benchmark.
    These fields will be used to sort and group your benchmarks for comparison over time.
*/
struct BenchmarkDescription
{
    size_t hash = 0;            /**< A hash uniquely identifying this benchmark. */
    std::string category;       /**< A category for grouping. */
    std::string name;           /**< A human-readable name for the benchmark. */
    std::string description;    /**< An optional description that might include configs etc. */
    std::string platform { juce::SystemStats::getOperatingSystemName().toStdString() }; /**< A platform string. */
};

/** Creates a description by hashing the name and description fields. */
inline BenchmarkDescription createBenchmarkDescription (std::string category, std::string name, std::string description)
{
    return { std::hash<std::string>{} (name + description),
             category, name, description };
}

//==============================================================================
/** Holds the duration a benchmark took to run. */
struct BenchmarkResult
{
    BenchmarkDescription description;   /**< The BenchmarkDescription. */
    double totalSeconds = 0.0, meanSeconds = 0.0, minSeconds = 0.0, maxSeconds = 0.0, varianceSeconds = 0.0;
    uint64_t totalCycles = 0, meanCycles = 0, minCycles = 0, maxCycles = 0;
    double varianceCycles = 0.0;
    juce::Time date { juce::Time::getCurrentTime() };

    /** Timings of the individual blocks, if the benchmark processed a stream of them.
        N.B. These are only printed, they aren't published.
    */
    std::optional<tracktion::graph::BlockTimingStatistics> blockTimings = std::nullopt;
};

/** Creates a BenchmarkResult from a set of Statistics. */
inline BenchmarkResult createBenchmarkResult (BenchmarkDescription description,
                                              const tracktion::graph::PerformanceMeasurement::Statistics& stats)
{
    return { description,
             stats.totalSeconds, stats.meanSeconds, stats.minimumSeconds, stats.maximumSeconds, stats.getVarianceSeconds(),
             stats.totalCycles, (uint64_t) stats.meanCycles, stats.minimumCycles, stats.maximumCycles, stats.getVarianceCycles() };
}

/** Creates a BenchmarkResult from a set of Statistics and the timings of the blocks processed. */
inline BenchmarkResult createBenchmarkResult (BenchmarkDescription description,
                                              const tracktion::graph::PerformanceMeasurement::Statistics& stats,
                                              const tracktion::graph::BlockTimingStatistics& blockTimings)
{
    auto result = createBenchmarkResult (std::move (description), stats);
    result.blockTimings = blockTimings;
    return result;
}

/** Creates a table of the results which have block timings, grouped by benchmark name.

    Timings from different runs, especially on shared CI machines, aren't
    comparable so each row also has its mean and 99th percentile relative to the
    first row in its group with the same block period. That is the number to use
    to compare configurations.
*/
inline std::string createBlockTimingTable (const std::vector<BenchmarkResult>& results)
{
    using tracktion::graph::BlockTimingStatistics;
    std::vector<std::string> names;

    for (const auto& r : results)
        if (r.blockTimings && std::find (names.begin(), names.end(), r.description.name) == names.end())
            names.push_back (r.description.name);

    if (names.empty())
        return {};

    auto pad = [] (const std::string& text, size_t width)
    {
        return text.size() < width ? std::string (width - text.size(), ' ') + text : text;
    };

    auto us = [&pad] (double seconds)                       { return pad (BlockTimingStatistics::toMicroseconds (seconds), 10); };
    auto count = [&pad] (int64_t n)                         { return pad (std::to_string (n), 8); };
    auto ratio = [&pad] (double value, double reference)
    {
        return pad (juce::String (reference > 0.0 ? value / reference : 0.0, 2).toStdString(), 8);
    };

    std::string s = "Block timings in microseconds. \"x mean\" and \"x p99\" are relative to the first row with the same period in each group.\n"
                    "\"over\" is the number of blocks that took longer to process than the block period.\n";

    for (const auto& name : names)
    {
        bool hasRealTimeRows = false;
        std::vector<std::string> driverThreads;

        for (const auto& r : results)
            if (r.blockTimings && r.description.name == name)
                hasRealTimeRows = hasRealTimeRows || r.blockTimings->isRealTime;

        s += "\n" + name + "\n"
                + pad ("period", 10) + pad ("blocks", 8) + pad ("min", 10) + pad ("mean", 10) + pad ("p50", 10) + pad ("p90", 10)
                + pad ("p99", 10) + pad ("p99.9", 10) + pad ("max", 10) + pad ("x mean", 8) + pad ("x p99", 8) + pad ("over", 8);

        if (hasRealTimeRows)
            s += pad ("missed", 8) + pad ("late p50", 10) + pad ("late p99", 10) + pad ("late max", 10);

        s += "  description\n";

        for (const auto& r : results)
        {
            if (! r.blockTimings || r.description.name != name)
                continue;

            const auto& bt = *r.blockTimings;
            const auto& processing = bt.processingTime;

            // Find the first row with the same period to compare with
            const BlockTimingStatistics* reference = nullptr;

            for (const auto& other : results)
            {
                if (other.blockTimings && other.description.name == name
                    && std::abs (other.blockTimings->blockPeriodSeconds - bt.blockPeriodSeconds) < 1.0e-9)
                {
                    reference = &*other.blockTimings;
                    break;
                }
            }

            s += us (bt.blockPeriodSeconds) + count (processing.getNumResults())
                    + us (processing.getMinimumSeconds()) + us (processing.getMeanSeconds())
                    + us (processing.getPercentileSeconds (50.0)) + us (processing.getPercentileSeconds (90.0))
                    + us (processing.getPercentileSeconds (99.0)) + us (processing.getPercentileSeconds (99.9))
                    + us (processing.getMaximumSeconds())
                    + ratio (processing.getMeanSeconds(), reference->processingTime.getMeanSeconds())
                    + ratio (processing.getPercentileSeconds (99.0), reference->processingTime.getPercentileSeconds (99.0))
                    + count (bt.numBlocksOverPeriod);

            if (bt.isRealTime)
            {
                s += count (bt.numMissedDeadlines)
                        + us (bt.wakeUpLateness.getPercentileSeconds (50.0))
                        + us (bt.wakeUpLateness.getPercentileSeconds (99.0))
                        + us (bt.wakeUpLateness.getMaximumSeconds());

                if (std::find (driverThreads.begin(), driverThreads.end(), bt.driverThreadDescription) == driverThreads.end())
                    driverThreads.push_back (bt.driverThreadDescription);
            }
            else if (hasRealTimeRows)
            {
                s += std::string (38, ' ');
            }

            s += "  " + r.description.description + "\n";
        }

        if (hasRealTimeRows)
        {
            s += "\"missed\" is the number of blocks that finished after the next block was due, \"late\" is how long after its deadline the driver thread woke up.\n";

            for (const auto& driverThread : driverThreads)
                s += "Driver thread: " + driverThread + "\n";

            s += "N.B. There is no audio device so on macOS the threads are only in an audio workgroup if the driver thread says so.\n";
        }
    }

    return s;
}

//==============================================================================
/**
    An indiviual Benchmark.
    To masure a benchmark, simply create one of these with a valid description
    then before the code you are measuring call start and stop afterwards.
    Once you've done that, call getResult() to return the duration the benchmark took to run.

    To collect a set of BenchmarkResults see @BenchmarkList
*/
class Benchmark
{
public:
    /** Creates a Benchmark for a given BenchmarkDescription. */
    Benchmark (BenchmarkDescription desc)
        : description (std::move (desc))
    {
    }

    /** Starts timing the benchmark. */
    void start()
    {
        measurement.start();
    }

    /** Stops timing the benchmark. */
    void stop()
    {
        measurement.stop();
    }

    /** Returns the timing results. */
    BenchmarkResult getResult() const
    {
        return createBenchmarkResult (description, measurement.getStatistics());
    }

private:
    BenchmarkDescription description;
    tracktion::graph::PerformanceMeasurement measurement { {}, -1, false };
};

//==============================================================================
/**
    Contans a list of BenchmarkResult[s].
    For easy use, this can be used as a singleton so results can easily be added
    to it from anywhere in your code.

    Once you've finished benchmarking, you can get all the results to publish
    them to your database.
*/
class BenchmarkList
{
public:
    /** Constructs a BenchmarkList. @see getInstance */
    BenchmarkList() = default;

    /** Adds a result to the list. [[ thread_safe ]] */
    void addResult (BenchmarkResult r)
    {
        std::lock_guard lock (mutex);
        results.emplace_back (std::move (r));
    }

    /** Returns all the results. [[ thread_safe ]] */
    std::vector<BenchmarkResult> getResults() const
    {
        std::lock_guard lock (mutex);
        return results;
    }

    /** Removes all the current results. [[ thread_safe ]] */
    void clear()
    {
        std::lock_guard lock (mutex);
        return results.clear();
    }

    /** Gets the singleton instance. [[ thread_safe ]] */
    static BenchmarkList& getInstance()
    {
        static BenchmarkList list;
        return list;
    }

private:
    mutable std::mutex mutex;
    std::vector<BenchmarkResult> results;
};

//==============================================================================
/**
    Helper class for measuring a benchmark and adding it to the singleton BenchmarkList list.
    @code
    {
        ScopedBenchmark sb (createBenchmarkDescription ("Save random tree as XML"));
        doSomeLongCalculation();
    }

    publish (BenchmarkList::getInstance().getResults());
    @endcode
*/
struct ScopedBenchmark
{
    /** Constructs and starts a Benchmark. */
    ScopedBenchmark (BenchmarkDescription desc)
        : benchmark (std::move (desc))
    {
        benchmark.start();
    }

    /** Stops the Benchmark and adds the result to the BenchmarkList. */
    ~ScopedBenchmark()
    {
        benchmark.stop();
        BenchmarkList::getInstance().addResult (benchmark.getResult());
    }

private:
    Benchmark benchmark;
};

//==============================================================================
/**
    Similar to ScopedBenchmark except it doesn't start/stop the measurement so
    you can do this manually (or with a ScopedMeasurement) around a specific
    code section.
*/
struct PublishingBenchmark
{
    /** Constructs a Benchmark. */
    PublishingBenchmark (BenchmarkDescription desc)
        : benchmark (std::move (desc))
    {
    }

    /** Adds the result to the BenchmarkList. */
    ~PublishingBenchmark()
    {
        BenchmarkList::getInstance().addResult (benchmark.getResult());
    }

    Benchmark benchmark;
};

//==============================================================================
/**
    Helper class for starting/stopping a benchmark measurement.
*/
struct ScopedMeasurement
{
    /** Constructs and starts a Benchmark. */
    ScopedMeasurement (Benchmark& bm)
        : benchmark (bm)
    {
        benchmark.start();
    }

    /** Stops the Benchmark. */
    ~ScopedMeasurement()
    {
        benchmark.stop();
    }

private:
    Benchmark& benchmark;
};

} // namespace tracktion::inline engine
