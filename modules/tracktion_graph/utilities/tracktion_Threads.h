/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
*/

#pragma once

namespace tracktion { inline namespace graph
{

/** Changes the thread's priority.

    May return false if for some reason the priority can't be changed.

    @param priority     the new priority, in the range 0 (lowest) to 10 (highest). A priority
                        of 5 is normal.
*/
bool setThreadPriority (std::thread&, int priority);

/** Tries to upgrade the current thread to realtime priority. */
bool tryToUpgradeCurrentThreadToRealtime (const juce::Thread::RealtimeOptions&);

}} // namespace tracktion_engine
