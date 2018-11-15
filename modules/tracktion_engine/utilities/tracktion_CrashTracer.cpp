/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


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
        Array<Thread::ThreadID> threads;

        for (int i = entries.size(); --i >= 0;)
            threads.addIfNotAlreadyThere (entries.getUnchecked (i)->threadID);

        for (int j = 0; j < threads.size(); ++j)
        {
            TRACKTION_LOG ("Thread " + String (j) + ":");

            auto thread = threads.getUnchecked(j);
            int n = 0;

            for (int i = entries.size(); --i >= 0;)
            {
                auto& s = *entries.getUnchecked (i);

                if (s.threadID == thread)
                {
                    if (s.pluginName != nullptr)
                        TRACKTION_LOG ("  ** Plugin crashed: " + String (s.pluginName));

                    TRACKTION_LOG ("  " + String (n++) + ": "
                                    + File::createFileWithoutCheckingPath (s.file).getFileName()
                                    + ":" + String (s.function) + ":" + String (s.line));

                }
            }
        }
      #endif
    }

    void dump (OutputStream& os) const
    {
        Array<Thread::ThreadID> threads;

        for (int i = entries.size(); --i >= 0;)
            threads.addIfNotAlreadyThere (entries.getUnchecked (i)->threadID);

        for (int j = 0; j < threads.size(); ++j)
        {
            os.writeText ("Thread " + String (j) + ":\n", false, false, nullptr);

            auto thread = threads.getUnchecked(j);
            int n = 0;

            for (int i = entries.size(); --i >= 0;)
            {
                auto& s = *entries.getUnchecked (i);

                if (s.threadID == thread)
                {
                    if (s.pluginName != nullptr)
                        os.writeText ("  ** Plugin crashed: " + String (s.pluginName) + "\n", false, false, nullptr);

                    os.writeText ("  " + String (n++) + ": "
                                   + File::createFileWithoutCheckingPath (s.file).getFileName()
                                   + ":" + String (s.function) + ":" + String (s.line) + "\n", false, false, nullptr);

                }
            }
        }
    }

    StringArray getCrashedPlugins() const
    {
        StringArray plugins;

        for (auto s : entries)
            if (s->pluginName != nullptr)
                plugins.add (s->pluginName);

        return plugins;
    }

    String getCrashedPlugin (Thread::ThreadID thread)
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

    String getCrashLocation (Thread::ThreadID thread)
    {
        for (int i = entries.size(); --i >= 0;)
        {
            auto& s = *entries.getUnchecked (i);

            if (s.threadID == thread)
                return File::createFileWithoutCheckingPath (s.file).getFileName()
                     + ":" + String (s.function) + ":" + String (s.line);
        }

        return "UnknownLocation";
    }

    Array<CrashStackTracer*, CriticalSection, 100> entries;
};

static CrashStackTracer::CrashTraceThreads crashStack;

CrashStackTracer::CrashStackTracer (const char* f, const char* fn, int l, const char* plugin)
    : file (f), function (fn), pluginName (plugin), line (l), threadID (Thread::getCurrentThreadId())
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

StringArray CrashStackTracer::getCrashedPlugins()
{
    return crashStack.getCrashedPlugins();
}

void CrashStackTracer::dump()
{
    TRACKTION_LOG ("Crashed");
    TRACKTION_LOG (newLine);
    crashStack.dump();
}

void CrashStackTracer::dump (OutputStream& os)
{
    os.writeText ("Crashed", false, false, nullptr);
    os.writeText (newLine, false, false, nullptr);
    crashStack.dump (os);
}

String CrashStackTracer::getCrashedPlugin (juce::Thread::ThreadID threadID)
{
    return crashStack.getCrashedPlugin (threadID);
}

String CrashStackTracer::getCrashLocation (Thread::ThreadID threadID)
{
    return crashStack.getCrashLocation (threadID);
}

//==============================================================================
DeadMansPedalMessage::DeadMansPedalMessage (PropertyStorage& ps, const String& message)
    : file (getDeadMansPedalFile (ps))
{
    file.replaceWithText (message);
}

DeadMansPedalMessage::~DeadMansPedalMessage()
{
    auto success = file.deleteFile();
    ignoreUnused (success);
    jassert (success);
}

String DeadMansPedalMessage::getAndClearLastMessage (PropertyStorage& propertyStorage)
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

File DeadMansPedalMessage::getDeadMansPedalFile (PropertyStorage& propertyStorage)
{
    auto folder = propertyStorage.getAppPrefsFolder();
    jassert (folder.exists() && folder.hasWriteAccess());
    return folder.getChildFile ("deadMansPedal");
}
