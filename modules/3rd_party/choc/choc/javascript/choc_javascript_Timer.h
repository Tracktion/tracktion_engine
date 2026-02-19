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

#ifndef CHOC_JAVASCRIPT_TIMER_HEADER_INCLUDED
#define CHOC_JAVASCRIPT_TIMER_HEADER_INCLUDED

#include <unordered_map>
#include <iostream>

#include "choc_javascript.h"
#include "../gui/choc_MessageLoop.h"


namespace choc::javascript
{

//==============================================================================
/** This function will bind some standard setInterval/setTimeout functions to
    a javascript engine.

    Just call registerTimerFunctions() on your choc::javascript::Context object
    and it will bind the appropriate functions for you to use.

    The timer implementation uses choc::messageloop::Timer, and the callbacks will
    be invoked on the message thread. So to work correctly, your app must be running
    a message loop, and you must be careful not to cause race conditions by calling
    into the same javascript context from other threads.

    The following functions are added, which should behave as you'd expect their
    standard javascript counterparts to work:

        setInterval (callback, milliseconds) // returns the new timer ID
        setTimeout (callback, milliseconds)  // returns the new timer ID
        clearInterval (timerID)
*/
void registerTimerFunctions (choc::javascript::Context&);





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

inline void registerTimerFunctions (Context& context)
{
    struct TimerList
    {
        TimerList (Context& c) : context (c) {}

        Context& context;
        std::unordered_map<int64_t, choc::messageloop::Timer> activeTimers;
        int64_t nextTimerID = 0;

        int64_t setTimeout (uint32_t interval)
        {
            auto timerID = ++nextTimerID;

            activeTimers[timerID] = choc::messageloop::Timer (interval, [this, timerID]
            {
                auto timeIDCopy = timerID; // local copy as this lambda will get deleted..
                activeTimers.erase (timeIDCopy);

                try
                {
                    context.invoke ("_choc_invokeTimeout", timeIDCopy);
                }
                catch (const choc::javascript::Error& e)
                {
                    std::cerr << e.what() << std::endl;
                }

                return false;
            });

            return timerID;
        }

        int64_t setInterval (uint32_t interval)
        {
            auto timerID = ++nextTimerID;

            activeTimers[timerID] = choc::messageloop::Timer (interval, [this, timerID]
            {
                try
                {
                    context.invoke ("_choc_invokeInterval", timerID);
                }
                catch (const choc::javascript::Error& e)
                {
                    std::cerr << e.what() << std::endl;
                }

                return true;
            });

            return timerID;
        }

        void clearInterval (int64_t timerID)
        {
            activeTimers.erase (timerID);
        }
    };

    auto timerList = std::make_shared<TimerList> (context);

    context.registerFunction ("_choc_setTimeout", [timerList] (ArgumentList args) -> choc::value::Value
    {
        if (auto interval = args.get<int> (0); interval >= 0)
            return choc::value::Value (timerList->setTimeout (static_cast<uint32_t> (interval)));

        return {};
    });

    context.registerFunction ("_choc_setInterval", [timerList] (ArgumentList args) -> choc::value::Value
    {
        if (auto interval = args.get<int> (0); interval >= 0)
            return choc::value::Value (timerList->setInterval (static_cast<uint32_t> (interval)));

        return {};
    });

    context.registerFunction ("_choc_clearInterval", [timerList] (ArgumentList args) -> choc::value::Value
    {
        if (auto timerID = args.get<int> (0))
            timerList->clearInterval (timerID);

        return {};
    });

    context.run (R"(
var _choc_activeTimers = {};

function _choc_invokeTimeout (timerID)
{
    var t = _choc_activeTimers[timerID];
    _choc_activeTimers[timerID] = undefined;

    if (t)
        t();
}

function _choc_invokeInterval (timerID)
{
    var t = _choc_activeTimers[timerID];

    if (t)
        t();
}

function setInterval (callback, milliseconds)
{
    var timerID = _choc_setInterval ((milliseconds >= 1 ? milliseconds : 1) | 0);

    if (timerID)
        _choc_activeTimers[timerID] = callback;

    return timerID;
}

function setTimeout (callback, milliseconds)
{
    var timerID = _choc_setTimeout ((milliseconds >= 1 ? milliseconds : 1) | 0);

    if (timerID)
        _choc_activeTimers[timerID] = callback;

    return timerID;
}

function clearInterval (timerID)
{
    _choc_activeTimers[timerID] = undefined;
    _choc_clearInterval (timerID | 0);
}
)");
}

} // namespace choc::javascript

#endif // CHOC_JAVASCRIPT_TIMER_HEADER_INCLUDED
