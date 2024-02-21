/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
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

struct SyncRange
{
    SyncPoint start, end;
};

MonotonicBeatRange getMonotonicBeatRange (const SyncRange&);
BeatRange getBeatRange (const SyncRange&);


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

inline MonotonicBeatRange getMonotonicBeatRange (const SyncRange& r)
{
    return { { r.start.monotonicBeat.v, r.end.monotonicBeat.v } };
}

inline BeatRange getBeatRange (const SyncRange& r)
{
    return { r.start.beat, r.end.beat };
}




}} // namespace tracktion { inline namespace engine
