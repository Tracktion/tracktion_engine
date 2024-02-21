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
