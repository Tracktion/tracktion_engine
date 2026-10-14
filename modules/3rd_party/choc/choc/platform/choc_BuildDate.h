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
#include <ctime>

namespace choc
{
    /// Returns the date and time at which this function was compiled, by
    /// parsing the __DATE__ and __TIME__ macros into a std::chrono timepoint.
    /// The compiler macros provide local time, so std::mktime is used to
    /// correctly convert to UTC.
    inline std::chrono::system_clock::time_point getBuildDate()
    {
        constexpr int mon = __DATE__[0] == 'J' ? (__DATE__[1] == 'a' ? 0
                                                : (__DATE__[2] == 'n' ? 5 : 6))
                          : __DATE__[0] == 'F' ? 1
                          : __DATE__[0] == 'M' ? (__DATE__[2] == 'r' ? 2 : 4)
                          : __DATE__[0] == 'A' ? (__DATE__[1] == 'p' ? 3 : 7)
                          : __DATE__[0] == 'S' ? 8
                          : __DATE__[0] == 'O' ? 9
                          : __DATE__[0] == 'N' ? 10
                          : __DATE__[0] == 'D' ? 11
                          : 0;

        constexpr int day = (__DATE__[4] == ' ') ? (__DATE__[5] - '0')
                                                 : (__DATE__[4] - '0') * 10
                                                     + (__DATE__[5] - '0');

        constexpr int year = (__DATE__[7] - '0') * 1000
                               + (__DATE__[8] - '0') * 100
                               + (__DATE__[9] - '0') * 10
                               + (__DATE__[10] - '0');

        constexpr int hrs = (__TIME__[0] - '0') * 10 + (__TIME__[1] - '0');
        constexpr int min = (__TIME__[3] - '0') * 10 + (__TIME__[4] - '0');
        constexpr int sec = (__TIME__[6] - '0') * 10 + (__TIME__[7] - '0');

        std::tm tm = {};
        tm.tm_year = year - 1900;
        tm.tm_mon  = mon;
        tm.tm_mday = day;
        tm.tm_hour = hrs;
        tm.tm_min  = min;
        tm.tm_sec  = sec;
        tm.tm_isdst = -1;

        return std::chrono::system_clock::from_time_t (std::mktime (&tm));
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
