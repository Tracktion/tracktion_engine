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

#ifndef CHOC_HIGH_RES_CLOCK_HEADER_INCLUDED
#define CHOC_HIGH_RES_CLOCK_HEADER_INCLUDED

#include <chrono>
#include <type_traits>

namespace choc
{
    /// If you want a high resolution clock, then std::chrono::high_resolution_clock
    /// would seem like the obvious choice.. however, apparently on some systems it
    /// can be non-steady, in which case it's best to fall back to steady_clock.
    /// This typedef just keeps all that tedious knowledge in one place.
    using HighResolutionSteadyClock = std::conditional<std::chrono::high_resolution_clock::is_steady,
                                                       std::chrono::high_resolution_clock,
                                                       std::chrono::steady_clock>::type;
}

#endif // CHOC_HIGH_RES_CLOCK_HEADER_INCLUDED
