/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

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

//==============================================================================
/** Holds the duration a benchmark took to run. */
struct BenchmarkResult
{
    BenchmarkDescription description;   /**< The BenchmarkDescription. */
    int64_t ticksStart = 0, ticksEnd = 0, ticksPerSecond = 0;
    juce::Time date { juce::Time::getCurrentTime() };
};

/** Returns the duration in seconds of a BenchmarkResult. */
inline double getDuration (const BenchmarkResult& r)
{
    return (r.ticksEnd - r.ticksStart) / static_cast<double> (r.ticksPerSecond);
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
        ticksStart = Time::getHighResolutionTicks();
    }
    
    /** Stops timing the benchmark. */
    void stop()
    {
        ticksEnd = Time::getHighResolutionTicks();
    }
    
    /** Returns the timing results. */
    BenchmarkResult getResult() const
    {
        return { description, ticksStart, ticksEnd, ticksPerSecond };
    }
    
private:
    BenchmarkDescription description;
    int64 ticksStart = 0, ticksEnd = 0;
    const int64 ticksPerSecond { Time::getHighResolutionTicksPerSecond() };
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
        ScopedBenchmark sb (getBenchmarkDescription ("Save random tree as XML"));
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


} // namespace tracktion_engine
