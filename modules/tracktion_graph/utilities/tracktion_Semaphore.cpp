/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "../3rd_party/concurrentqueue.h"
#include "../3rd_party/lightweightsemaphore.h"

namespace tracktion { inline namespace graph
{

//==============================================================================
//==============================================================================
Semaphore::Semaphore (int initialCount)
{
    pimpl = std::make_unique<moodycamel::details::Semaphore> (initialCount);
}

Semaphore::~Semaphore()
{
}

bool Semaphore::wait()
{
    return pimpl->wait();
}

bool Semaphore::try_wait()
{
    return pimpl->try_wait();
}

bool Semaphore::timed_wait (std::uint64_t usecs)
{
    return pimpl->timed_wait (usecs);
}

void Semaphore::signal (int count)
{
    return pimpl->signal (count);
}


//==============================================================================
//==============================================================================
LightweightSemaphore::LightweightSemaphore (int initialCount)
{
    pimpl = std::make_unique<moodycamel::LightweightSemaphore> (initialCount);
}

LightweightSemaphore::~LightweightSemaphore()
{
}

bool LightweightSemaphore::wait()
{
    return pimpl->wait();
}

bool LightweightSemaphore::try_wait()
{
    return pimpl->tryWait();
}

bool LightweightSemaphore::timed_wait (std::uint64_t usecs)
{
    return pimpl->wait ((std::int64_t) usecs);
}

void LightweightSemaphore::signal (int count)
{
    return pimpl->signal (count);
}

}} // namespace tracktion_engine
