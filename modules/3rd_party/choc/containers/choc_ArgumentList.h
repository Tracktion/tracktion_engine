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

#ifndef CHOC_ARGUMENT_LIST_HEADER_INCLUDED
#define CHOC_ARGUMENT_LIST_HEADER_INCLUDED

#include <filesystem>
#include <vector>
#include <optional>
#include "../platform/choc_Platform.h"
#include "../text/choc_StringUtilities.h"

namespace choc
{

/**
    A no-frills helper class for manipulating a list of command-line
    arguments.

    Create one of these from your `argv`/`argc` and it lets you check
    for flags being in the list, remove items as you use them, and
    automatically check file paths to see if they exist.

    People have approached the task of parsing arguments in lots of
    different ways, and there are some fancy libraries that use a
    declarative pattern to let you specify your program's options,
    and doing things like automatic printing of help etc. This one
    isn't trying to do anything like that - it's just a container
    for your list of args, with methods to make it easy to do many
    common operations with them.
*/
struct ArgumentList
{
    ArgumentList() = default;
    ArgumentList (int argc, const char* const* argv);

    /// Returns the number of tokens in the list
    size_t size() const;
    /// Returns true if the list isn't empty
    bool empty() const;
    /// Returns a token from the list by index (or an empty string if out of bounds)
    std::string operator[] (size_t index) const;

    /// Returns true if the list contains the given argument flag. This
    /// is smart enough to see that an token "--foo=bar" contains the
    /// flag "--foo".
    bool contains (std::string_view flag) const;

    /// Returns the index of the token which this flag refers to
    int indexOf (std::string_view flag) const;

    /// If the list contains this flag, removes it and returns true.
    bool removeIfFound (std::string_view flag);

    /// If the given flag isn't found, this throws an exception with a
    /// message saying that it was required.
    void throwIfNotFound (std::string_view flag) const;

    /// Removes a token by index
    void removeIndex (int index);

    /// Looks for a given flag, and if found, returns its value The value
    /// for a flag could be part of the same token using an '=', e.g. "--foo=bar",
    /// or the token that follows it, e.g. "--foo bar". If not found, it returns
    /// an empty optional.
    std::optional<std::string> getValueFor (std::string_view flagToFind, bool remove);

    /// This is similar to getValueFor(), but if found, it removes the token
    /// from the list.
    std::optional<std::string> removeValueFor (std::string_view flagToFind);

    /// This is similar to the version of removeValueFor() that returns an optional,
    /// but this takes a default value to return in the case of it not being found.
    std::string removeValueFor (std::string_view flagToFind, std::string defaultValue);

    /// This acts like removeValueFor(), but parses the result as an integer.
    template <typename IntType>
    std::optional<IntType> removeIntValue (std::string_view flagToFind);

    /// This acts like removeIntValue(), but takes a default value to return in
    /// the case of the flag not being found.
    template <typename IntType>
    IntType removeIntValue (std::string_view flagToFind, IntType defaultValue);

    /// This acts like getValueFor() to look up a value, but treats it as a filename
    /// and checks for its existence - if the value is missing or the file doesn't
    /// exist, this will throw an exception.
    std::filesystem::path getExistingFile (std::string_view flag, bool remove);

    /// This acts like getExistingFile() but also removes the token from the list
    /// if found.
    std::filesystem::path removeExistingFile (std::string_view flag);

    /// This acts like getExistingFile, but doesn't throw an exception if the flag
    /// is missing. It does still throw an exception if a file is specified but
    /// doesn't exist.
    std::optional<std::filesystem::path> getExistingFileIfPresent (std::string_view flag, bool remove);

    /// This is like getExistingFileIfPresent() but also removes the token from the list.
    std::optional<std::filesystem::path> removeExistingFileIfPresent (std::string_view flag);

    /// This is like getExistingFile(), but expects the path to be a folder.
    std::filesystem::path getExistingFolder (std::string_view flag, bool remove);

    /// This is like getExistingFolder() but also removes the token from the list.
    std::optional<std::filesystem::path> removeExistingFolderIfPresent (std::string_view flag);

    /// Returns a vector of all the tokens in the form of paths.
    std::vector<std::filesystem::path> getAllAsFiles();

    /// Returns a vector of all the tokens in the form of paths, and checks each one
    /// to make sure it exists, throwing an exception if any don't exist.
    std::vector<std::filesystem::path> getAllAsExistingFiles();

    /// The executable path, taken from argv[0].
    std::string executableName;

    /// The list of tokens.
    std::vector<std::string> tokens;

private:
    //==============================================================================
    static bool isOption (std::string_view s)            { return isSingleDashOption (s) || isDoubleDashOption (s); }
    static bool isSingleDashOption (std::string_view s)  { return s.length() > 1 && s[0] == '-' && s[1] != '-'; }
    static bool isDoubleDashOption (std::string_view s)  { return s.length() > 2 && s[0] == '-' && s[1] == '-' && s[2] != '-'; }

    static std::string getLongOptionType (std::string_view);
    static std::string getLongOptionValue (std::string_view);
    static std::filesystem::path getAbsolutePath (const std::filesystem::path&);
    static std::filesystem::path throwIfNonexistent (const std::filesystem::path&);
    static std::filesystem::path throwIfNotAFolder (const std::filesystem::path&);
};






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

inline ArgumentList::ArgumentList (int argc, const char* const* argv)
{
    if (argv == nullptr || argc == 0)
        return;

    executableName = argv[0];

    for (int i = 1; i < argc; ++i)
        tokens.emplace_back (argv[i]);

   #if CHOC_OSX
    if (auto index = indexOf ("-NSDocumentRevisionsDebugMode"); index >= 0)
    {
        removeIndex (index);
        removeIndex (index);
    }
   #endif
}

inline size_t ArgumentList::size() const
{
    return tokens.size();
}

inline bool ArgumentList::empty() const
{
    return tokens.empty();
}

inline std::string ArgumentList::operator[] (size_t index) const
{
    if (index < tokens.size())
        return tokens[index];

    return {};
}

inline bool ArgumentList::contains (std::string_view arg) const
{
    return indexOf (arg) >= 0;
}

inline bool ArgumentList::removeIfFound (std::string_view arg)
{
    if (auto i = indexOf (arg); i >= 0)
    {
        removeIndex (i);
        return true;
    }

    return false;
}

inline int ArgumentList::indexOf (std::string_view arg) const
{
    CHOC_ASSERT (! choc::text::trim (arg).empty());
    bool isDoubleDash = isDoubleDashOption (arg);

    for (size_t i = 0; i < tokens.size(); ++i)
        if (tokens[i] == arg || (isDoubleDash && getLongOptionType (tokens[i]) == arg))
            return static_cast<int> (i);

    return -1;
}

inline void ArgumentList::removeIndex (int index)
{
    if (index >= 0 && index < static_cast<int> (tokens.size()))
        tokens.erase (tokens.begin() + index);
}

inline void ArgumentList::throwIfNotFound (std::string_view arg) const
{
    if (! contains (arg))
        throw std::runtime_error ("Expected argument: '" + std::string (arg) + "'");
}

inline std::optional<std::string> ArgumentList::getValueFor (std::string_view argToFind, bool remove)
{
    bool isDoubleDash = isDoubleDashOption (argToFind);
    bool isSingleDash = isSingleDashOption (argToFind);

    CHOC_ASSERT (isDoubleDash || isSingleDash); // the arg you pass in needs to be a "--option" or "-option"

    if (auto i = indexOf (argToFind); i >= 0)
    {
        auto index = static_cast<size_t> (i);

        if (isSingleDash)
        {
            if (index + 1 < tokens.size() && ! isOption (tokens[index + 1]))
            {
                auto value = tokens[index + 1];

                if (remove)
                {
                    removeIndex (i);
                    removeIndex (i);
                }

                return value;
            }

            if (remove)
                removeIndex (i);

            return {};
        }

        if (isDoubleDash)
        {
            auto value = getLongOptionValue (tokens[index]);

            if (remove)
                removeIndex (i);

            return value;
        }
    }

    return {};
}

inline std::optional<std::string> ArgumentList::removeValueFor (std::string_view argToFind)
{
    return getValueFor (argToFind, true);
}

inline std::string ArgumentList::removeValueFor (std::string_view argToFind, std::string defaultValue)
{
    if (auto v = getValueFor (argToFind, true))
        return *v;

    return defaultValue;
}

template <typename IntType>
std::optional<IntType> ArgumentList::removeIntValue (std::string_view argToFind)
{
    if (auto v = removeValueFor (argToFind))
        return static_cast<IntType> (std::stoll (*v));

    return {};
}

template <typename IntType>
IntType ArgumentList::removeIntValue (std::string_view argToFind, IntType defaultValue)
{
    if (auto v = removeValueFor (argToFind))
        return static_cast<IntType> (std::stoll (*v));

    return defaultValue;
}

inline std::filesystem::path ArgumentList::getExistingFile (std::string_view arg, bool remove)
{
    auto path = getValueFor (arg, remove);

    if (! path)
    {
        throwIfNotFound (arg);
        throw std::runtime_error ("Expected a filename after '" + std::string (arg) + "'");
    }

    return getAbsolutePath (*path);
}

inline std::filesystem::path ArgumentList::removeExistingFile (std::string_view arg)
{
    return getExistingFile (arg, true);
}

inline std::optional<std::filesystem::path> ArgumentList::getExistingFileIfPresent (std::string_view arg, bool remove)
{
    if (contains (arg))
        return getExistingFile (arg, remove);

    return {};
}

inline std::optional<std::filesystem::path> ArgumentList::removeExistingFileIfPresent (std::string_view arg)
{
    return getExistingFileIfPresent (arg, true);
}

inline std::filesystem::path ArgumentList::getExistingFolder (std::string_view arg, bool remove)
{
    return throwIfNotAFolder (getExistingFile (arg, remove));
}

inline std::optional<std::filesystem::path> ArgumentList::removeExistingFolderIfPresent (std::string_view arg)
{
    if (contains (arg))
        return getExistingFolder (arg, true);

    return {};
}

inline std::vector<std::filesystem::path> ArgumentList::getAllAsFiles()
{
    std::vector<std::filesystem::path> result;

    for (auto& arg : tokens)
        if (! isOption (arg))
            result.push_back (getAbsolutePath (arg));

    return result;
}

inline std::vector<std::filesystem::path> ArgumentList::getAllAsExistingFiles()
{
    auto files = getAllAsFiles();

    for (auto& f : files)
        throwIfNonexistent (f);

    return files;
}

inline std::filesystem::path ArgumentList::getAbsolutePath (const std::filesystem::path& path)
{
    auto s = path.string();

    if (s.length() >= 2 && s[0] == '"' && s.back() == '"')
        return getAbsolutePath (s.substr (1, s.length() - 2));

    return path.is_absolute() ? path : std::filesystem::current_path() / path;
}

inline std::filesystem::path ArgumentList::throwIfNonexistent (const std::filesystem::path& path)
{
    if (! exists (path))
        throw std::runtime_error ("File does not exist: " + path.string());

    return path;
}

inline std::filesystem::path ArgumentList::throwIfNotAFolder (const std::filesystem::path& path)
{
    throwIfNonexistent (path);

    if (! is_directory (path))
        throw std::runtime_error (path.string() + " is not a folder");

    return path;
}

inline std::string ArgumentList::getLongOptionType (std::string_view s)
{
    if (! isDoubleDashOption (s))
        return {};

    if (auto equals = s.find ("="); equals != std::string_view::npos)
        return std::string (choc::text::trim (s.substr (0, equals)));

    return std::string (s);
}

inline std::string ArgumentList::getLongOptionValue (std::string_view s)
{
    if (! isDoubleDashOption (s))
        return {};

    if (auto equals = s.find ("="); equals != std::string_view::npos)
        return std::string (choc::text::trim (s.substr (equals + 1)));

    return std::string (s);
}

} // namespace choc

#endif // CHOC_ARGUMENT_LIST_HEADER_INCLUDED