/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include <chrono>

namespace tracktion_graph
{

struct TimelineClock
{
    typedef std::chrono::duration<double, std::chrono::seconds::period> duration;
    typedef duration::rep                                               rep;
    typedef duration::period                                            period;
    typedef std::chrono::time_point<TimelineClock>                      time_point;
    static const bool is_steady =                                       false;

    static time_point now() noexcept
    {
        return {};
    }
};

using TimelinePoint = std::chrono::time_point<TimelineClock>;
using Duration = TimelinePoint::duration;

} // namespace tracktion_graph
