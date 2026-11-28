/*==============================================================================

  Copyright 2016 by Tracktion Corporation.
  For more information visit www.tracktion.com

 ==============================================================================*/

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#ifdef __APPLE__
 #include <sys/kdebug_signpost.h>
 #include <os/signpost.h>
#endif

#include "../../tracktion_core/utilities/tracktion_CPU.h"

namespace tracktion::inline graph {

//==============================================================================
//==============================================================================
/**
    A macOS specific class to start/stop a signpost for use in Instruments.
*/
struct ScopedSignpost
{
    /** Starts a signpost with a given index. */
    ScopedSignpost (uint32_t signpostIndex)
        : index (signpostIndex)
    {
        (void) index;
       #ifdef __APPLE__
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wdeprecated"
        kdebug_signpost_start (index, 0, 0, 0, 0);
        #pragma clang diagnostic pop
       #endif
    }

    /** Stops the signpost previously started. */
    ~ScopedSignpost()
    {
       #ifdef __APPLE__
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wdeprecated"
        kdebug_signpost_end (index, 0, 0, 0, 0);
        #pragma clang diagnostic pop
       #endif
    }

private:
    //==============================================================================
    const uint32_t index;
};


#ifdef __APPLE__
//==============================================================================
//==============================================================================
struct NamedSignpost
{
    NamedSignpost (const char* nameToUse)
        : name (nameToUse)
    {
       #ifdef __APPLE__
        if (__builtin_available (macOS 10.14, *))
            os_signpost_interval_begin (getLog(), id, "", "%s", name);
       #endif
    }

    ~NamedSignpost()
    {
       #ifdef __APPLE__
        if (__builtin_available (macOS 10.14, *))
            os_signpost_interval_end (getLog(), id, "", "%s", name);
       #endif
    }

private:
    [[ maybe_unused ]] const char* name;
   #ifdef __APPLE__
    const os_signpost_id_t id  { generateID() };

    static os_log_t getLog()
    {
        static os_log_t log;

        if (__builtin_available (macOS 10.14, *))
            if (log == nullptr)
                log = os_log_create ("", OS_LOG_CATEGORY_POINTS_OF_INTEREST);

        return log;
    }

    static os_signpost_id_t generateID()
    {
        if (__builtin_available (macOS 10.14, *))
            return os_signpost_id_generate (getLog());

        return {};
    }
   #endif
};
#endif //__APPLE__


//==============================================================================
//==============================================================================
/** A timer for measuring performance of code.

    e.g. @code

        PerformanceMeasurement pc ("fish", 50);

        for (;;)
        {
            const ScopedPerformanceMeasurement (pc);
            doSomethingFishy();
        }
    @endcode

    In this example, the time of each period between calling start/stop will be
    measured and averaged over 50 runs, and the results logged to std::cout
    every 50 times round the loop.
*/
class PerformanceMeasurement
{
public:
    //==============================================================================
    /** Creates a PerformanceMeasurement object.

        @param counterName      the name used when printing out the statistics
        @param runsPerPrintout  the number of start/stop iterations before calling
                                printStatistics()
    */
    PerformanceMeasurement (std::string counterName,
                            int runsPerPrintout = 100,
                            bool printOnDestruction = true);

    /** Destructor. */
    ~PerformanceMeasurement();

    //==============================================================================
    /** Starts timing.
        @see stop
    */
    void start() noexcept;

    /** Stops timing and prints out the results.

        The number of iterations before doing a printout of the
        results is set in the constructor.

        @see start
    */
    bool stop();

    /** Dumps the current metrics to std::cout. */
    void printStatistics();

    /** Holds the current statistics. */
    struct Statistics
    {
        Statistics() noexcept = default;

        void clear() noexcept;
        double getVarianceSeconds() const;
        double getVarianceCycles() const;
        std::string toString (const std::string& name) const;

        void addResult (double secondsElapsed, uint64_t cyclesElapsed) noexcept;

        double meanSeconds      = 0.0;
        double m2Seconds        = 0.0;
        double maximumSeconds   = 0.0;
        double minimumSeconds   = 0.0;
        double totalSeconds     = 0.0;

        double meanCycles       = 0.0;
        double m2Cycles         = 0.0;
        uint64_t maximumCycles  = 0;
        uint64_t minimumCycles  = 0;
        uint64_t totalCycles    = 0;

        int64_t numRuns = 0;
    };

    /** Returns the name. */
    std::string getName() const;

    /** Returns a copy of the current stats. */
    Statistics getStatistics() const;

    /** Returns a copy of the current stats, and resets the internal counter. */
    Statistics getStatisticsAndReset();

    /** Returns the duration of the last start/stop pair. */
    double getLastDurationSeconds() const noexcept      { return lastDurationSeconds; }

private:
    //==============================================================================
    Statistics stats;
    double lastDurationSeconds = 0.0;
    std::string name;
    int64_t runsPerPrint;
    bool printOnDestruction;
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    uint64_t startCycles;
};

//==============================================================================
//==============================================================================
/**
    A fixed-size histogram of durations which can be used to find percentiles.

    The buckets are logarithmically spaced, each about 3% wide, so a percentile
    is accurate to about 3% of its value for durations from 1ns to 18 minutes.
    The minimum, maximum and mean are exact.

    Adding a result doesn't allocate or lock so can be done on a real-time thread.
*/
class DurationHistogram
{
public:
    /** Creates an empty histogram. */
    DurationHistogram() noexcept = default;

    /** Removes all the results. */
    void clear() noexcept;

    /** Adds a duration. [[ real_time_safe ]] */
    void addResult (double seconds) noexcept;

    /** Returns the number of results added. */
    int64_t getNumResults() const noexcept              { return numResults; }

    /** Returns the shortest duration added. */
    double getMinimumSeconds() const noexcept           { return minimumSeconds; }

    /** Returns the longest duration added. */
    double getMaximumSeconds() const noexcept           { return maximumSeconds; }

    /** Returns the mean of the durations added. */
    double getMeanSeconds() const noexcept              { return numResults > 0 ? totalSeconds / (double) numResults : 0.0; }

    /** Returns the duration which the given percentage (0-100) of the results
        were no longer than. E.g. 99.0 returns the 99th percentile.
    */
    double getPercentileSeconds (double percentile) const noexcept;

private:
    //==============================================================================
    static constexpr int subBucketBits = 5;
    static constexpr size_t numSubBuckets = size_t (1) << subBucketBits;
    static constexpr size_t numBuckets = 36 * numSubBuckets;

    std::array<uint32_t, numBuckets> buckets {};
    int64_t numResults = 0;
    double minimumSeconds = 0.0, maximumSeconds = 0.0, totalSeconds = 0.0;

    static size_t getBucketIndex (uint64_t nanoseconds) noexcept;
    static uint64_t getBucketEndNanoseconds (size_t index) noexcept;
};

//==============================================================================
/**
    Timings of a run of audio blocks, where each block is one process call of
    a whole graph.

    This holds the things you need to compare how reliably two configurations
    process a block: percentiles of the time taken and the number of blocks
    that took longer than the block's own duration.

    If the blocks were processed at a real-time cadence, it also holds how late
    the driving thread woke up for each block and how many blocks finished after
    the next block was due.
*/
struct BlockTimingStatistics
{
    /** Removes all the results. */
    void clear() noexcept;

    /** Adds the time taken to process a block. [[ real_time_safe ]]
        @param processingSeconds    The time taken to process the block
        @param periodSeconds        The duration of the block (numSamples / sampleRate)
    */
    void addBlock (double processingSeconds, double periodSeconds) noexcept;

    /** Returns a multi-line description of the timings. */
    std::string toString() const;

    /** Formats a duration as microseconds with one decimal place, without units. */
    static std::string toMicroseconds (double seconds);

    DurationHistogram processingTime;       /**< The time taken to process each block. */
    double blockPeriodSeconds = 0.0;        /**< The duration of a full block. */
    int64_t numWarmUpBlocks = 0;            /**< The number of blocks processed first and left out of these timings. */
    int64_t numBlocksOverPeriod = 0;        /**< The number of blocks that took longer to process than their duration. */

    // The following are only used if the blocks were processed at a real-time cadence
    bool isRealTime = false;                /**< Whether the blocks were processed at a real-time cadence. */
    DurationHistogram wakeUpLateness;       /**< How long after each block's deadline the driving thread started processing it. */
    int64_t numMissedDeadlines = 0;         /**< The number of blocks that finished after the next block was due to start. */
    std::string driverThreadDescription;    /**< The priority and timer the driving thread actually got. */
};

//==============================================================================
//==============================================================================
/**
    RAII wrapper to start and stop a performance measurement.
*/
class ScopedPerformanceMeasurement
{
public:
    /** Constructor.
        Starts a performance measurement.
    */
    ScopedPerformanceMeasurement (PerformanceMeasurement& pm)
        : performanceMeasurement (pm)
    {
        performanceMeasurement.start();
    }

    /** Destructor.
        Stops a performance measurement.
    */
    ~ScopedPerformanceMeasurement()
    {
        performanceMeasurement.stop();
    }

    PerformanceMeasurement& performanceMeasurement;
};


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

inline PerformanceMeasurement::PerformanceMeasurement (std::string name_, int runsPerPrintout, bool shouldPrintOnDestruction)
    : name (std::move (name_)), runsPerPrint (runsPerPrintout), printOnDestruction (shouldPrintOnDestruction)
{
}

inline PerformanceMeasurement::~PerformanceMeasurement()
{
    if (printOnDestruction && stats.numRuns > 0)
        printStatistics();
}

inline void PerformanceMeasurement::Statistics::clear() noexcept
{
    meanSeconds = m2Seconds = maximumSeconds = minimumSeconds = totalSeconds = 0;
    meanCycles = m2Cycles = 0.0;
    maximumCycles = minimumCycles = totalCycles = 0;
    numRuns = 0;
}

inline void PerformanceMeasurement::Statistics::addResult (double secondsElapsed, uint64_t cyclesElapsed) noexcept
{
    if (numRuns == 0)
    {
        maximumSeconds = secondsElapsed;
        minimumSeconds = secondsElapsed;

        maximumCycles = cyclesElapsed;
        minimumCycles = cyclesElapsed;
    }
    else
    {
        maximumSeconds = std::max (maximumSeconds, secondsElapsed);
        minimumSeconds = std::min (minimumSeconds, secondsElapsed);

        maximumCycles = std::max (maximumCycles, cyclesElapsed);
        minimumCycles = std::min (minimumCycles, cyclesElapsed);
    }

    ++numRuns;
    totalSeconds += secondsElapsed;
    totalCycles += cyclesElapsed;

    // Seconds
    {
        const double delta = secondsElapsed - meanSeconds;
        meanSeconds += delta / (double) numRuns;
        const double delta2 = secondsElapsed - meanSeconds;
        m2Seconds += delta * delta2;
    }

    // Cycles
    {
        const auto delta = (double) cyclesElapsed - meanCycles;
        meanCycles += delta / (double) numRuns;
        const double delta2 = (double) cyclesElapsed - meanCycles;
        m2Cycles += delta * delta2;
    }
}

inline double PerformanceMeasurement::Statistics::getVarianceSeconds() const
{
    return numRuns > 0 ? (m2Seconds / (double) numRuns) : 0.0;
}

inline double PerformanceMeasurement::Statistics::getVarianceCycles() const
{
    return numRuns > 0 ? (m2Cycles / (double) numRuns) : 0.0;
}

inline std::string PerformanceMeasurement::Statistics::toString (const std::string& perfName) const
{
    auto timeToString = [] (double secs)
    {
        return std::to_string ((int64_t) (secs * (secs < 0.01 ? 1000000.0 : 1000.0) + 0.5))
                + (secs < 0.01 ? " us" : " ms");
    };

    std::string s = "Performance count for \"" + perfName + "\" over " + std::to_string (numRuns) + " run(s)\n"
                    + "\t Seconds: Mean = " + timeToString (meanSeconds)
                    + ", min = "            + timeToString (minimumSeconds)
                    + ", max = "            + timeToString (maximumSeconds)
                    + ", SD = "             + timeToString (std::sqrt (getVarianceSeconds()))
                    + ", total = "          + timeToString (totalSeconds) + "\n"
                    + "\t Cycles: Mean = "  + std::to_string (meanCycles)
                    + ", min = "            + std::to_string (minimumCycles)
                    + ", max = "            + std::to_string (maximumCycles)
                    + ", SD = "             + std::to_string (std::sqrt (getVarianceCycles()))
                    + ", total = "          + std::to_string (totalCycles) + "\n";

    return s;
}

//==============================================================================
inline void DurationHistogram::clear() noexcept
{
    buckets.fill (0);
    numResults = 0;
    minimumSeconds = maximumSeconds = totalSeconds = 0.0;
}

inline void DurationHistogram::addResult (double seconds) noexcept
{
    seconds = std::clamp (seconds, 0.0, 1.0e9);

    if (numResults == 0)
    {
        minimumSeconds = seconds;
        maximumSeconds = seconds;
    }
    else
    {
        minimumSeconds = std::min (minimumSeconds, seconds);
        maximumSeconds = std::max (maximumSeconds, seconds);
    }

    ++numResults;
    totalSeconds += seconds;
    ++buckets[getBucketIndex (static_cast<uint64_t> (seconds * 1.0e9))];
}

inline double DurationHistogram::getPercentileSeconds (double percentile) const noexcept
{
    if (numResults == 0)
        return 0.0;

    const auto targetCount = std::clamp (static_cast<int64_t> (std::ceil ((percentile / 100.0) * (double) numResults)),
                                         int64_t (1), numResults);
    int64_t count = 0;

    for (size_t i = 0; i < numBuckets; ++i)
    {
        count += buckets[i];

        if (count >= targetCount)
            return std::clamp ((double) getBucketEndNanoseconds (i) * 1.0e-9, minimumSeconds, maximumSeconds);
    }

    return maximumSeconds;
}

inline size_t DurationHistogram::getBucketIndex (uint64_t nanoseconds) noexcept
{
    // The first buckets are 1ns wide, after that each power of two is split in to numSubBuckets
    if (nanoseconds < numSubBuckets)
        return static_cast<size_t> (nanoseconds);

    const auto exponent = static_cast<size_t> (std::bit_width (nanoseconds) - 1 - subBucketBits);
    const auto index = (exponent + 1) * numSubBuckets
                        + (static_cast<size_t> (nanoseconds >> exponent) - numSubBuckets);

    return std::min (index, numBuckets - 1);
}

inline uint64_t DurationHistogram::getBucketEndNanoseconds (size_t index) noexcept
{
    if (index < numSubBuckets)
        return index;

    const auto exponent = (index / numSubBuckets) - 1;
    const auto subBucket = index % numSubBuckets;

    return (static_cast<uint64_t> (numSubBuckets + subBucket + 1) << exponent) - 1;
}

//==============================================================================
inline void BlockTimingStatistics::clear() noexcept
{
    processingTime.clear();
    wakeUpLateness.clear();
    numBlocksOverPeriod = 0;
    numMissedDeadlines = 0;
}

inline void BlockTimingStatistics::addBlock (double processingSeconds, double periodSeconds) noexcept
{
    processingTime.addResult (processingSeconds);

    if (processingSeconds > periodSeconds)
        ++numBlocksOverPeriod;
}

inline std::string BlockTimingStatistics::toMicroseconds (double seconds)
{
    std::ostringstream text;
    text << std::fixed << std::setprecision (1) << (seconds * 1.0e6);
    return text.str();
}

inline std::string BlockTimingStatistics::toString() const
{
    const auto numBlocks = processingTime.getNumResults();

    auto countToString = [numBlocks] (int64_t count)
    {
        std::ostringstream text;
        text << count << " (" << std::fixed << std::setprecision (2)
             << (numBlocks > 0 ? 100.0 * (double) count / (double) numBlocks : 0.0) << "%)";
        return text.str();
    };

    auto s = std::string (isRealTime ? "Real-time block" : "Block") + " timings over " + std::to_string (numBlocks) + " block(s), "
                + std::to_string (numWarmUpBlocks) + " warm-up block(s) left out, block period = " + toMicroseconds (blockPeriodSeconds) + " us\n"
                + "\t Processing [us]: min = "     + toMicroseconds (processingTime.getMinimumSeconds())
                + ", mean = "                       + toMicroseconds (processingTime.getMeanSeconds())
                + ", p50 = "                        + toMicroseconds (processingTime.getPercentileSeconds (50.0))
                + ", p90 = "                        + toMicroseconds (processingTime.getPercentileSeconds (90.0))
                + ", p99 = "                        + toMicroseconds (processingTime.getPercentileSeconds (99.0))
                + ", p99.9 = "                      + toMicroseconds (processingTime.getPercentileSeconds (99.9))
                + ", max = "                        + toMicroseconds (processingTime.getMaximumSeconds()) + "\n"
                + "\t Blocks over the block period: " + countToString (numBlocksOverPeriod) + "\n";

    if (isRealTime)
        s += "\t Driver wake-up lateness [us]: mean = " + toMicroseconds (wakeUpLateness.getMeanSeconds())
                + ", p50 = "                        + toMicroseconds (wakeUpLateness.getPercentileSeconds (50.0))
                + ", p99 = "                        + toMicroseconds (wakeUpLateness.getPercentileSeconds (99.0))
                + ", max = "                        + toMicroseconds (wakeUpLateness.getMaximumSeconds()) + "\n"
                + "\t Missed deadlines: "          + countToString (numMissedDeadlines) + "\n"
                + "\t Driver thread: "             + driverThreadDescription + "\n";

    return s;
}

inline void PerformanceMeasurement::start() noexcept
{
    startTime = std::chrono::high_resolution_clock::now();
    startCycles = rdtsc();
}

inline bool PerformanceMeasurement::stop()
{
    const auto elapsedSeconds = std::chrono::high_resolution_clock::now() - startTime;
    const auto elapsedCycles = rdtsc() - startCycles;
    lastDurationSeconds = std::chrono::duration<double, std::ratio<1, 1>> (elapsedSeconds).count();
    stats.addResult (lastDurationSeconds, elapsedCycles);

    if (runsPerPrint < 0)
        return false;

    if (stats.numRuns < runsPerPrint)
        return false;

    printStatistics();
    return true;
}

inline void PerformanceMeasurement::printStatistics()
{
    const auto desc = getStatisticsAndReset().toString (name);
    std::cout << (desc);
}

inline std::string PerformanceMeasurement::getName() const
{
    return name;
}

inline PerformanceMeasurement::Statistics PerformanceMeasurement::getStatistics() const
{
    return stats;
}

inline PerformanceMeasurement::Statistics PerformanceMeasurement::getStatisticsAndReset()
{
    Statistics s (stats);
    stats.clear();

    return s;
}

}
