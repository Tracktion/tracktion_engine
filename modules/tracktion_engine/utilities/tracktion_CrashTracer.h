/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

namespace tracktion_engine
{

/**
    Used by the CRASH_TRACER macros to help provide a useful crash log of the stack.
*/
struct CrashStackTracer
{
    CrashStackTracer (const char* file, const char* fn, int line, const char* pluginName);
    ~CrashStackTracer();

    static juce::StringArray getCrashedPlugins();
    static void dump();
    static void dump (juce::OutputStream& os);

    static juce::String getCrashedPlugin (juce::Thread::ThreadID);
    static juce::String getCrashLocation (juce::Thread::ThreadID);

    struct CrashTraceThreads;

private:
    const char* file;
    const char* function;
    const char* pluginName;
    int line;
    juce::Thread::ThreadID threadID;
};

/** This macro adds the current location to a stack which gets logged if a crash happens. */
#define CRASH_TRACER            const CrashStackTracer JUCE_JOIN_MACRO (__crashTrace, __LINE__) (__FILE__, __FUNCTION__, __LINE__, nullptr);

/** This macro adds the current location and the name of a plugin to a stack which gets logged if a crash happens. */
#define CRASH_TRACER_PLUGIN(p)  const CrashStackTracer JUCE_JOIN_MACRO (__crashTrace, __LINE__) (__FILE__, __FUNCTION__, __LINE__, p);


//==============================================================================
/**
    This RAII class saves a property which will be reported at startup as a failure
    if the app disappears before its destructor is called.
*/
struct DeadMansPedalMessage
{
    DeadMansPedalMessage (PropertyStorage&, const juce::String&);
    ~DeadMansPedalMessage();

    static juce::String getAndClearLastMessage (PropertyStorage&);

private:
    const juce::File file;
    static juce::File getDeadMansPedalFile (PropertyStorage&);
};

}
