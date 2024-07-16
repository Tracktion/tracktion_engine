/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion::inline core
{

/** Subtracts b from a without wrapping. */
constexpr int subtractNoWrap (int a, int b)
{
    if ((b > 0 && a < std::numeric_limits<int>::min() + b)
        || (b < 0 && a > std::numeric_limits<int>::max() + b))
    {
        // Overflow would occur
        return std::numeric_limits<int>::min();
    }

    return a - b;
}

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
static_assert(subtractNoWrap (std::numeric_limits<int>::min(), std::numeric_limits<int>::max()) == std::numeric_limits<int>::min());
static_assert(subtractNoWrap (std::numeric_limits<int>::min(), -(std::numeric_limits<int>::min() + 1)) == std::numeric_limits<int>::min());
static_assert(subtractNoWrap (std::numeric_limits<int>::min(), 0) == std::numeric_limits<int>::min());
static_assert(subtractNoWrap (0, std::numeric_limits<int>::max()) == std::numeric_limits<int>::min() + 1);
static_assert(subtractNoWrap (0, std::numeric_limits<int>::min() + 1) == std::numeric_limits<int>::max());
static_assert(subtractNoWrap (1024, -2147483648) == std::numeric_limits<int>::min());
static_assert(subtractNoWrap (1024, 1024) == 0);
static_assert(subtractNoWrap (0, 0) == 0);
static_assert(subtractNoWrap (-1'073'741'824, 1'073'741'824) == std::numeric_limits<int>::min());
}
