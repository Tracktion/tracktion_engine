/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once


namespace tracktion::inline graph {

//==============================================================================
/**
    Available strategies for thread pools.
    This will determine how worker threads are handled when no Nodes are
    available for processing.
*/
enum class ThreadPoolStrategy
{
    conditionVariable,      /**< Uses CVs to pause threads. */
    realTime,               /**< Uses pause, yield and sleeps to suspend threads. */
    hybrid,                 /**< Uses a combination of the above, avoiding CVs on the audio thread. */
    semaphore,              /**< Uses a semaphore to suspend threads. */
    lightweightSemaphore,   /**< Uses a semaphore/spin mechanism to suspend threads.*/
    lightweightSemHybrid,   /**< Uses a combination of semaphores/spin and yields to suspend threads.*/

    // Experimental variants for comparing how threads are woken and suspended
    lightweightSemHybridWakeAll,    /**< lightweightSemHybrid, waking sleeping threads with a single call. */
    lightweightSemHybridCascade,    /**< lightweightSemHybrid, waking two threads which each wake two more. */
    lightweightSemHybridCapped,     /**< lightweightSemHybrid, without letting unused semaphore counts build up. */
    lightweightSemHybridNoSemSpin,  /**< lightweightSemHybrid, without the semaphore spinning before it sleeps. */
    lightweightSemaphoreNoSemSpin,  /**< lightweightSemaphore, without the semaphore spinning before it sleeps. */
    blockEvent,                     /**< Threads sleep on a block counter as soon as there are no Nodes queued. */
    blockEventHybrid,               /**< Threads spin while a block is being processed and sleep on a block counter between blocks. */
    blockEventSpin                  /**< Threads never sleep. For measuring the cost of waking threads only. */
};

/** Returns a function to create a ThreadPool for the given stategy. */
LockFreeMultiThreadedNodePlayer::ThreadPoolCreator getPoolCreatorFunction (ThreadPoolStrategy);

}
