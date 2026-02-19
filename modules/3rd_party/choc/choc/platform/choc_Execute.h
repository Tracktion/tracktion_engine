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

#ifndef CHOC_EXECUTE_HEADER_INCLUDED
#define CHOC_EXECUTE_HEADER_INCLUDED

#include <string>
#include <sstream>
#include <stdio.h>

namespace choc
{

/// The result of a call to the execute() function.
struct ProcessResult
{
    /// The process's output
    std::string output;

    /// The process's status code on exit. This is a standard unix-style
    /// exit code as returned, where 0 is success, and other values
    /// indicate failures.
    int statusCode = 0;
};

//==============================================================================
/// Executes a system command synchronously, returning when the spawned process
/// has terminated, and providing the command's output as a string, along with
/// the process's status code.
///
/// @param command The command to attempt to run.
/// @param alsoReadStdErr If set to true, the stderr stream will also be read and
/// returned as part of the output. If false, just stdout will be read.
///
/// This may throw an exception if something fails.
///
ProcessResult execute (std::string command,
                       bool alsoReadStdErr = false);





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


static std::string getErrnoString()
{
    char errorText[256] = {};

   #ifdef _WIN32
    strerror_s (errorText, sizeof(errorText) - 1, errno);
    return errorText;
   #else
    struct Helper
    {
        // overloads to handle two variants of strerror_r that return different things
        static const char* handleResult (int, const char* s)         { return s; }
        static const char* handleResult (const char* s, const char*) { return s; }
    };

    return Helper::handleResult (strerror_r (errno, errorText, sizeof(errorText)), errorText);
   #endif
}

inline ProcessResult execute (std::string command, bool alsoReadStdErr)
{
    if (alsoReadStdErr)
        command += " 2>&1";

   #ifdef _MSC_VER
    auto handle = ::_popen (command.c_str(), "r");
   #else
    auto handle = ::popen (command.c_str(), "r");
   #endif

    if (! handle)
        throw std::runtime_error ("choc::execute() failed: " + getErrnoString());

    std::ostringstream result;

    for (;;)
    {
        char buffer[1024];
        auto bytesRead = ::fread (buffer, 1, sizeof (buffer), handle);

        try
        {
            result.write (buffer, (std::streamsize) bytesRead);

            if (bytesRead < sizeof (buffer))
                break;
        }
        catch (...) { break; }
    }

   #ifdef _MSC_VER
    auto status = ::_pclose (handle);
   #else
    auto status = ::pclose (handle);
   #endif
    return { result.str(), status };
}

} // namespace choc

#endif  // CHOC_EXECUTE_HEADER_INCLUDED
