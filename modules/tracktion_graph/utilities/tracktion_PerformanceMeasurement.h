/*==============================================================================

  Copyright 2016 by Tracktion Corporation.
  For more information visit www.tracktion.com

 ==============================================================================*/

#pragma once

#include <string>
#include <chrono>
#include <iostream>

#ifdef __APPLE__
 #include <sys/kdebug_signpost.h>
#endif

#include "../../tracktion_core/utilities/tracktion_CPU.h"

namespace tracktion { inline namespace graph
{

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
    PerformanceMeasurement (const std::string& counterName,
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
        std::string toString() const;

        void addResult (double secondsElapsed, uint64_t cyclesElapsed) noexcept;

        std::string name;

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

    /** Returns a copy of the current stats. */
    Statistics getStatistics() const;

    /** Returns a copy of the current stats, and resets the internal counter. */
    Statistics getStatisticsAndReset();

private:
    //==============================================================================
    Statistics stats;
    int64_t runsPerPrint;
    bool printOnDestruction;
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    uint64_t startCycles;
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

inline PerformanceMeasurement::PerformanceMeasurement (const std::string& name, int runsPerPrintout, bool shouldPrintOnDestruction)
    : runsPerPrint (runsPerPrintout), printOnDestruction (shouldPrintOnDestruction)
{
    stats.name = name;
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
        const auto delta = cyclesElapsed - meanCycles;
        meanCycles += delta / (double) numRuns;
        const double delta2 = cyclesElapsed - meanCycles;
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

inline std::string PerformanceMeasurement::Statistics::toString() const
{
    auto timeToString = [] (double secs)
    {
        return std::to_string ((int64_t) (secs * (secs < 0.01 ? 1000000.0 : 1000.0) + 0.5))
                + (secs < 0.01 ? " us" : " ms");
    };

    std::string s = "Performance count for \"" + name + "\" over " + std::to_string (numRuns) + " run(s)\n"
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

inline void PerformanceMeasurement::start() noexcept
{
    startTime = std::chrono::high_resolution_clock::now();
    startCycles = rdtsc();
}

inline bool PerformanceMeasurement::stop()
{
    const auto elapsedSeconds = std::chrono::high_resolution_clock::now() - startTime;
    const auto elapsedCycles = rdtsc() - startCycles;
    stats.addResult (std::chrono::duration<double, std::ratio<1, 1>> (elapsedSeconds).count(),
                     elapsedCycles);

    if (runsPerPrint < 0)
        return false;
    
    if (stats.numRuns < runsPerPrint)
        return false;

    printStatistics();
    return true;
}

inline void PerformanceMeasurement::printStatistics()
{
    const auto desc = getStatisticsAndReset().toString();
    std::cout << (desc);
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

}}
