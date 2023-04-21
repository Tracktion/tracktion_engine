/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

struct CrashStackTracer::CrashTraceThreads
{
    CrashTraceThreads()
    {
        entries.ensureStorageAllocated (64);
    }

    inline void push (CrashStackTracer* c)
    {
        entries.add (c);
    }

    inline void pop (CrashStackTracer* c)
    {
        entries.removeFirstMatchingValue (c);
    }

    void dump() const
    {
      #if TRACKTION_LOG_ENABLED
        juce::Array<juce::Thread::ThreadID> threads;

        for (int i = entries.size(); --i >= 0;)
            threads.addIfNotAlreadyThere (entries.getUnchecked (i)->threadID);

        for (int j = 0; j < threads.size(); ++j)
        {
            TRACKTION_LOG ("Thread " + juce::String (j) + ":");

            auto thread = threads.getUnchecked(j);
            int n = 0;

            for (int i = entries.size(); --i >= 0;)
            {
                auto& s = *entries.getUnchecked (i);

                if (s.threadID == thread)
                {
                    if (s.pluginName != nullptr)
                        TRACKTION_LOG ("  ** Plugin crashed: " + juce::String (s.pluginName));

                    TRACKTION_LOG ("  " + juce::String (n++) + ": "
                                    + juce::File::createFileWithoutCheckingPath (s.file).getFileName()
                                    + ":" + juce::String (s.function) + ":" + juce::String (s.line));

                }
            }
        }
      #endif
    }

    void dump (juce::OutputStream& os, juce::Thread::ThreadID threadIDToDump) const
    {
        juce::Array<juce::Thread::ThreadID> threads;

        for (int i = entries.size(); --i >= 0;)
        {
            auto entryThreadID = entries.getUnchecked (i)->threadID;

            if (entryThreadID == threadIDToDump || entryThreadID == juce::Thread::ThreadID())
                threads.addIfNotAlreadyThere (entryThreadID);
        }

        for (int j = 0; j < threads.size(); ++j)
        {
            os.writeText ("Thread " + juce::String (j) + ":\n", false, false, nullptr);

            auto thread = threads.getUnchecked (j);
            int n = 0;

            for (int i = entries.size(); --i >= 0;)
            {
                auto& s = *entries.getUnchecked (i);

                if (s.threadID == thread)
                {
                    if (s.pluginName != nullptr)
                        os.writeText ("  ** Plugin crashed: " + juce::String (s.pluginName) + "\n", false, false, nullptr);

                    os.writeText ("  " + juce::String (n++) + ": "
                                   + juce::File::createFileWithoutCheckingPath (s.file).getFileName()
                                   + ":" + juce::String (s.function) + ":" + juce::String (s.line) + "\n", false, false, nullptr);

                }
            }
        }
    }

    juce::StringArray getCrashedPlugins() const
    {
        juce::StringArray plugins;

        for (auto s : entries)
            if (s->pluginName != nullptr)
                plugins.add (s->pluginName);

        return plugins;
    }

    juce::String getCrashedPlugin (juce::Thread::ThreadID thread)
    {
        for (int i = entries.size(); --i >= 0;)
        {
            auto& s = *entries.getUnchecked (i);

            if (s.threadID == thread)
                if (s.pluginName != nullptr)
                    return s.pluginName;
        }

        return {};
    }

    juce::String getCrashLocation (juce::Thread::ThreadID thread)
    {
        for (int i = entries.size(); --i >= 0;)
        {
            auto& s = *entries.getUnchecked (i);

            if (s.threadID == thread)
                return juce::File::createFileWithoutCheckingPath (s.file).getFileName()
                     + ":" + juce::String (s.function) + ":" + juce::String (s.line);
        }

        return "UnknownLocation";
    }

    juce::Array<CrashStackTracer*, juce::CriticalSection, 100> entries;
};

static CrashStackTracer::CrashTraceThreads crashStack;

CrashStackTracer::CrashStackTracer (const char* f, const char* fn, int l, const char* plugin)
    : file (f), function (fn), pluginName (plugin), line (l), threadID (juce::Thread::getCurrentThreadId())
{
    crashStack.push (this);

   #if ! TRACKTION_LOG_ENABLED
    juce::ignoreUnused (file, function, pluginName, line);
   #endif
}

CrashStackTracer::~CrashStackTracer()
{
    crashStack.pop (this);
}

juce::StringArray CrashStackTracer::getCrashedPlugins()
{
    return crashStack.getCrashedPlugins();
}

void CrashStackTracer::dump()
{
    TRACKTION_LOG ("Crashed");
    TRACKTION_LOG (juce::newLine);
    crashStack.dump();
}

void CrashStackTracer::dump (juce::OutputStream& os)
{
    dump (os, juce::Thread::ThreadID());
}

void CrashStackTracer::dump (juce::OutputStream& os, juce::Thread::ThreadID threadID)
{
    os.writeText ("Crashed", false, false, nullptr);
    os.writeText (juce::newLine, false, false, nullptr);
    crashStack.dump (os, threadID);
}

juce::String CrashStackTracer::getCrashedPlugin (juce::Thread::ThreadID threadID)
{
    return crashStack.getCrashedPlugin (threadID);
}

juce::String CrashStackTracer::getCrashLocation (juce::Thread::ThreadID threadID)
{
    return crashStack.getCrashLocation (threadID);
}

//==============================================================================
DeadMansPedalMessage::DeadMansPedalMessage (PropertyStorage& ps, const juce::String& message)
    : file (getDeadMansPedalFile (ps))
{
    file.replaceWithText (message);
}

DeadMansPedalMessage::~DeadMansPedalMessage()
{
    auto success = file.deleteFile();
    juce::ignoreUnused (success);
    jassert (success);
}

juce::String DeadMansPedalMessage::getAndClearLastMessage (PropertyStorage& propertyStorage)
{
    auto file = getDeadMansPedalFile (propertyStorage);
    auto s = file.loadFileAsString();

    if (s.isNotEmpty())
    {
        file.deleteFile();
        TRACKTION_LOG (s);
    }

    return s;
}

juce::File DeadMansPedalMessage::getDeadMansPedalFile (PropertyStorage& propertyStorage)
{
    auto folder = propertyStorage.getAppPrefsFolder();
    jassert (folder.exists() && folder.hasWriteAccess());
    return folder.getChildFile ("deadMansPedal");
}

}} // namespace tracktion { inline namespace engine
