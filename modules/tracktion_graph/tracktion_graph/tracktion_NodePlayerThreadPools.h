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
    lightweightSemHybrid    /**< Uses a combination of semaphores/spin and yields to suspend threads.*/
};

/** Returns a function to create a ThreadPool for the given stategy. */
LockFreeMultiThreadedNodePlayer::ThreadPoolCreator getPoolCreatorFunction (ThreadPoolStrategy);

}}
