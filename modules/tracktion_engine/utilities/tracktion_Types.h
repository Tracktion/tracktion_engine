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

//==========================================================================
//==========================================================================
struct MonotonicBeat { BeatPosition v; };

struct MonotonicBeatRange { BeatRange v; };


//==========================================================================
//==========================================================================
/**
    Holds a reference sample position and the Edit time and beat that it corresponds to.
*/
struct SyncPoint
{
    int64_t referenceSamplePosition = 0;    /**< The reference sample position */
    MonotonicBeat monotonicBeat;            /**< The unlooped, elapsed number of beats. */
    TimePosition unloopedTime;              /**< The Edit timeline time as if no looping had happened. */
    TimePosition time;                      /**< The Edit timeline time. */
    BeatPosition beat;                      /**< The Edit timeline beat. */
};

}} // namespace tracktion { inline namespace engine
