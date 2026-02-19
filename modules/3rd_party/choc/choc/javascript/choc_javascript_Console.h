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

#ifndef CHOC_JAVASCRIPT_CONSOLE_HEADER_INCLUDED
#define CHOC_JAVASCRIPT_CONSOLE_HEADER_INCLUDED

#include <iostream>
#include "choc_javascript.h"


namespace choc::javascript
{

enum class LoggingLevel
{
    log, info, warn, error, debug
};

//==============================================================================
/** This function will bind some standard console functions to a javascript
    context.

    Just call registerConsoleFunctions() on your choc::javascript::Context object
    and it will bind the appropriate functions for you to use.

    It provides a very stripped-down `console` object which has methods such as
    `log` and `debug` that will write to the functors you provide. This lets code
    call standard logging functions like `console.log()`.

    You can optionally provide your own functor to do the writing, or leave it
    empty to have the output written to std::cout and std::cerr.

    NB: this doesn't work with duktape: can't seem to get it to parse an object
        that has methods..
*/
void registerConsoleFunctions (choc::javascript::Context&,
                               std::function<void(std::string_view, LoggingLevel)> handleOutput = {});




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

inline void registerConsoleFunctions (choc::javascript::Context& context,
                                      std::function<void(std::string_view, LoggingLevel)> handleOutput)
{
    if (! handleOutput)
    {
        handleOutput = [] (std::string_view content, LoggingLevel level)
        {
            if (level == LoggingLevel::debug)
                std::cerr << content << std::endl;
            else
                std::cout << content << std::endl;
        };
    }

    context.registerFunction ("_choc_console_log",
                              [handleOutput] (ArgumentList args) mutable -> choc::value::Value
    {
        if (auto content = args[0])
        {
            auto level = LoggingLevel::log;

            switch (args.get<int> (1))
            {
                case 0: level = LoggingLevel::log; break;
                case 1: level = LoggingLevel::info; break;
                case 2: level = LoggingLevel::warn; break;
                case 3: level = LoggingLevel::error; break;
                case 4: level = LoggingLevel::debug; break;
            }

            handleOutput (content->isString() ? content->toString()
                                              : choc::json::toString (*content), level);
        }

        return {};
    });

    context.run (R"(
console = {
    log:   function()     { for (let a of arguments) _choc_console_log (a, 0); },
    info:  function()     { for (let a of arguments) _choc_console_log (a, 1); },
    warn:  function()     { for (let a of arguments) _choc_console_log (a, 2); },
    error: function()     { for (let a of arguments) _choc_console_log (a, 3); },
    debug: function()     { for (let a of arguments) _choc_console_log (a, 4); }
};
)");
}

} // namespace choc::javascript

#endif // CHOC_JAVASCRIPT_CONSOLE_HEADER_INCLUDED
