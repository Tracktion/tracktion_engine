//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Classy Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2022 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_BUILDDATE_HEADER_INCLUDED
#define CHOC_BUILDDATE_HEADER_INCLUDED

#include <chrono>

namespace choc
{
    /// Returns the date of the day on which this function was compiled, by
    /// parsing the __DATE__ macro into a std::chrono timepoint.
    constexpr std::chrono::system_clock::time_point getBuildDate()
    {
        using namespace std::chrono;

        constexpr month m (__DATE__[0] == 'J' ? (__DATE__[1] == 'a' ? 1u
                                              : (__DATE__[2] == 'n' ? 6u : 7u))
                         : __DATE__[0] == 'F' ? 2
                         : __DATE__[0] == 'M' ? (__DATE__[2] == 'r' ? 3u : 5u)
                         : __DATE__[0] == 'A' ? (__DATE__[1] == 'p' ? 4u : 8u)
                         : __DATE__[0] == 'S' ? 9u
                         : __DATE__[0] == 'O' ? 10u
                         : __DATE__[0] == 'N' ? 11u
                         : __DATE__[0] == 'D' ? 12u
                         : 0u);

        constexpr day d ((__DATE__[4] == ' ') ? static_cast<unsigned> (__DATE__[5] - '0')
                                              : static_cast<unsigned> ((__DATE__[4] - '0') * 10
                                                                         + (__DATE__[5] - '0')));

        constexpr year y ((__DATE__[7] - '0') * 1000
                            + (__DATE__[8] - '0') * 100
                            + (__DATE__[9] - '0') * 10
                            + (__DATE__[10] - '0'));

        constexpr auto count = sys_days (year_month_day (y, m, d));

        return time_point<system_clock, days> (count);
    }

    /// Returns the number of whole days that have elapsed since this
    /// function was compiled.
    /// @see getBuildDate()
    inline int getDaysSinceBuildDate()
    {
        auto buildDate = getBuildDate();
        auto now = std::chrono::system_clock::now();
        return static_cast<int> (std::chrono::duration_cast<std::chrono::days> (now - buildDate).count());
    }
}

#endif // CHOC_BUILDDATE_HEADER_INCLUDED
