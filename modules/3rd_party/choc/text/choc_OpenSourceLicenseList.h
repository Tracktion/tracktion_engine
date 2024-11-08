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

#ifndef CHOC_LICENSETEXTCOLLECTOR_HEADER_INCLUDED
#define CHOC_LICENSETEXTCOLLECTOR_HEADER_INCLUDED

#include "choc_StringUtilities.h"

namespace choc::text
{

//==============================================================================
/*
    If you're using CHOC and/or other open-source libraries in your product,
    then you're legally obliged to comply with their license conditions, and
    these usually require you to reproduce their copyright notices somewhere
    in your app.

    To make this boring task easier, this class takes care of collecting together
    all the text from many licenses in one place, and also makes sure that
    only the licenses from code that is used by your app will be added to
    the list (so for example, choc contains some Windows-only licenses which
    won't appear in the list when building for other platforms).

    To use it to get the complete set of CHOC licenses, simply make sure you
    include this header *before* all the choc headers that you use. All the
    choc files that embed 3rd-party code will make sure that the appropriate
    license text is added to the global OpenSourceLicenseList object.

    Then, in your app, you can just print the string returned by
    `OpenSourceLicenseList::getAllLicenseText()`, and it should contain all
    the license text that you need.

    If you have code containing your own license text that you want to
    add to the list, simply make sure that your code includes this header,
    and then use the CHOC_REGISTER_OPEN_SOURCE_LICENCE macro to declare it.
    You can find examples of how to use this macro scattered around the
    choc codebase.
*/
struct OpenSourceLicenseList
{
    OpenSourceLicenseList();

    /// Returns a string containing all the licenses.
    static std::string getAllLicenseText();

    /// Adds a license to the list (this will de-dupe any identical
    /// licenses that are added more than once).
    static void addLicense (std::string text);

    /// Returns the global instance of this class which is used to
    /// collect all the licenses into one place.
    static OpenSourceLicenseList& getInstance();

    /// The licenses are simply kept in a vector.
    std::vector<std::string> licenses;
};

/// This macro declares a static object whose constructor will automatically
/// add a license to the global list. This means that you can place this macro
/// anywhere in your code and it'll be automatically registered at startup.
/// The libraryName must be a C++ identifier that's used to distinguish
/// different licenses
#define CHOC_REGISTER_OPEN_SOURCE_LICENCE(libraryName, licenseText) \
    struct LicenseAdder_ ## libraryName \
    { \
        LicenseAdder_ ## libraryName() { choc::text::OpenSourceLicenseList::addLicense (licenseText); } \
    }; \
    static inline LicenseAdder_ ## libraryName staticLicenseAdder_ ## libraryName;

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

static inline void addLicenseToList (std::vector<std::string>& list, std::string&& text)
{
    list.push_back (choc::text::trim (std::move (text)));

    // sort alphbetically because otherwise the results will differ
    // depending on link order
    std::sort (list.begin(), list.end());
    list.erase (std::unique (list.begin(), list.end()), list.end());
}

inline OpenSourceLicenseList::OpenSourceLicenseList()
{
    // First, add our own license..
    addLicenseToList (licenses, R"(
==============================================================================
CHOC is (C)2022 Julian Storer/Tracktion Corporation, and may be used under
the terms of the ISC license:

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
)");
}

inline OpenSourceLicenseList& OpenSourceLicenseList::getInstance()
{
    static OpenSourceLicenseList instance;
    return instance;
}

inline std::string OpenSourceLicenseList::getAllLicenseText()
{
    return choc::text::joinStrings (getInstance().licenses, "\n\n") + "\n\n";
}

inline void OpenSourceLicenseList::addLicense (std::string text)
{
    addLicenseToList (getInstance().licenses, std::move (text));
}

} // namespace choc::text

#endif // CHOC_LICENSETEXTCOLLECTOR_HEADER_INCLUDED
