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

namespace tracktion { inline namespace graph
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
    /** Creates a semaphore with an initial count. */
    Semaphore (int initialCount = 0);

    /** Destructor. */
    ~Semaphore();

    /** Decrements the count by one and if the result of this goes below zero the
        call will block.
        @returns true if the decrement actually happened or false if there was a
        problem (like the maximum count has been exceeded)
    */
    bool wait();
    
    /** Attemps to decrement the count by one.
        If the decrement leaves the count above zero this will return true.
        If the decrement would leave the count below zero this will not block and
        return false.
    */
    bool try_wait();

    /** Performs a wait operation. If the wait would actually block, the block
        happens for the given period of time before returning or the semaphore is
        signalled, whichever happens first.
        @returns true if the decrement actually happened or false if the count
        would have gone below zero and this had to block.
    */
    bool timed_wait (std::uint64_t usecs);

    /** Increases the count by the ammount specified.
        If the count goes above zero, this will signal waiting threads, unblocking them.
    */
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
    A counting semaphore that spins on a atomic before waiting so will avoid
    system calls if wait periods are very short.
*/
class LightweightSemaphore
{
public:
    //==============================================================================
    /** Creates a semaphore with an initial count. */
    LightweightSemaphore (int initialCount = 0);

    /** Destructor. */
    ~LightweightSemaphore();

    /** Decrements the count by one and if the result of this goes below zero the
        call will block.
        @returns true if the decrement actually happened or false if there was a
        problem (like the maximum count has been exceeded)
    */
    bool wait();
    
    /** Attemps to decrement the count by one.
        If the decrement leaves the count above zero this will return true.
        If the decrement would leave the count below zero this will not block and
        return false.
    */
    bool try_wait();
    
    /** Performs a wait operation. If the wait would actually block, the block
        happens for the given period of time before returning or the semaphore is
        signalled, whichever happens first.
        @returns true if the decrement actually happened or false if the count
        would have gone below zero and this had to block.
    */
    bool timed_wait (std::uint64_t usecs);

    /** Increases the count by the ammount specified.
        If the count goes above zero, this will signal waiting threads, unblocking them.
    */
    void signal (int count = 1);

private:
    //==============================================================================
    std::unique_ptr<moodycamel::LightweightSemaphore> pimpl;
    
    LightweightSemaphore(const LightweightSemaphore&) = delete;
    LightweightSemaphore& operator= (const LightweightSemaphore& other) = delete;
};


}} // namespace tracktion_engine
