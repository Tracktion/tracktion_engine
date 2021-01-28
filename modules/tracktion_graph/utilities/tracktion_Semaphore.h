/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace moodycamel
{
    class LightweightSemaphore;
    namespace details { class Semaphore; }
}

namespace tracktion_graph
{

//==============================================================================
//==============================================================================
/**
    A basic counting semaphore.
*/
class Semaphore
{
public:
    //==============================================================================
    Semaphore (int initialCount = 0);

    ~Semaphore();

    bool wait();
    
    bool try_wait();
    
    bool timed_wait (std::uint64_t usecs);

    void signal (int count = 1);

private:
    //==============================================================================
    std::unique_ptr<moodycamel::details::Semaphore> pimpl;
    
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator= (const Semaphore& other) = delete;
};


//==============================================================================
//==============================================================================
/**
    A counting semaphore that spins on a atomic before waiting.
*/
class LightweightSemaphore
{
public:
    //==============================================================================
    LightweightSemaphore (int initialCount = 0);

    ~LightweightSemaphore();

    bool wait();
    
    bool try_wait();
    
    bool timed_wait (std::uint64_t usecs);

    void signal (int count = 1);

private:
    //==============================================================================
    std::unique_ptr<moodycamel::LightweightSemaphore> pimpl;
    
    LightweightSemaphore(const LightweightSemaphore&) = delete;
    LightweightSemaphore& operator= (const LightweightSemaphore& other) = delete;
};


} // namespace tracktion_engine
