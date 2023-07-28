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

#ifndef CHOC_FILE_UTILS_HEADER_INCLUDED
#define CHOC_FILE_UTILS_HEADER_INCLUDED

#include <fstream>
#include <functional>
#include <cwctype>
#include <stdexcept>
#include <random>
#include <filesystem>
#include "../text/choc_UTF8.h"

namespace choc::file
{

/// A file handling error, thrown by some of the functions in this namespace.
struct Error  : public std::runtime_error
{
    Error (const std::string& error) : std::runtime_error (error) {}
};

/// Attempts to read the entire contents of the given file into memory,
/// throwing an Error exception if anything goes wrong. The lambda argument will be
/// called with the amount of space needed as a uint64_t parameter, and it must
/// return a pointer to a sufficiently large block of memory to write the data into
/// (or nullptr if it can't provide a suitable buffer, in which case nothing will be done).
/// Returns the number of bytes successfully read.
template <typename GetDestBufferFn>
size_t readFileContent (const std::string& filename, GetDestBufferFn&&);

/// Attempts to load the contents of the given filename into a string,
/// throwing an Error exception if anything goes wrong.
std::string loadFileAsString (const std::string& filename);

/// Attempts to create or overwrite the specified file with some new data.
/// This will attempt to create and parent folders needed for the file, and will
/// throw an Error exception if something goes wrong.
void replaceFileWithContent (const std::filesystem::path& file,
                             std::string_view newContent);

//==============================================================================
/// A self-deleting temp file or folder.
struct TempFile
{
    TempFile();
    TempFile (TempFile&&) = default;
    TempFile& operator= (TempFile&&) = default;
    TempFile (const TempFile&) = delete;
    TempFile& operator= (const TempFile&) = delete;
    ~TempFile();

    /// Creates a temporary folder with the given name, which will be recursively deleted
    /// when this object is destroyed.
    TempFile (std::string_view subFolderName);

    /// Creates the specified folder in the system temporary directory, and sets the
    /// TempFile::file member to point to the given filename within it.
    /// The file (but not the folder) will be deleted when this object is destroyed.
    TempFile (std::string_view subFolderName, std::string_view filename);

    /// Creates the specified folder in the system temporary directory, and sets the
    /// TempFile::file member to be a randomly chosen filename within it, based on the
    /// supplied file root and suffix.
    /// The file (but not the folder) will be deleted when this object is destroyed.
    /// So for example TempFile ("foo", "bar", "txt") may create a file with a name such
    /// as "/tmp/foo/bar_12345.txt".
    TempFile (std::string_view subFolderName,
              std::string_view filenameRoot,
              std::string_view filenameSuffix);

    /// Creates a filename by adding a random integer suffix to a prefix string, and
    /// appending a file type suffix. E.g. createRandomFilename ("foo", "txt") may
    /// return something like "foo_12345.txt"
    static std::string createRandomFilename (std::string_view filenameRoot,
                                             std::string_view filenameSuffix);

    /// The temp file that has been selected. When the TempFile is destroyed, this
    /// file will be deleted (recursively if it is a folder).
    std::filesystem::path file;
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

template <typename GetDestBufferFn>
size_t readFileContent (const std::string& filename, GetDestBufferFn&& getBuffer)
{
    if (filename.empty())
        throw Error ("Illegal filename");

    try
    {
        std::ifstream stream;
        stream.exceptions (std::ofstream::failbit | std::ofstream::badbit);
        stream.open (filename, std::ios::binary | std::ios::ate);

        if (! stream.is_open())
            throw Error ("Failed to open file: " + filename);

        auto fileSize = stream.tellg();

        if (fileSize < 0)
            throw Error ("Failed to read from file: " + filename);

        if (fileSize == 0)
            return {};

        if (auto destBuffer = getBuffer (static_cast<uint64_t> (fileSize)))
        {
            stream.seekg (0);

            if (stream.read (static_cast<std::ifstream::char_type*> (destBuffer), static_cast<std::streamsize> (fileSize)))
                return static_cast<size_t> (fileSize);

            throw Error ("Failed to read from file: " + filename);
        }
    }
    catch (const std::ios_base::failure& e)
    {
        throw Error ("Failed to read from file: " + filename + ": " + e.what());
    }

    return 0;
}

inline std::string loadFileAsString (const std::string& filename)
{
    std::string result;

    readFileContent (filename, [&] (uint64_t size)
    {
        result.resize (static_cast<std::string::size_type> (size));
        return result.data();
    });

    return result;
}

inline void replaceFileWithContent (const std::filesystem::path& path, std::string_view newContent)
{
    try
    {
        try
        {
            if (path.has_parent_path())
                if (auto parent = path.parent_path(); ! exists (parent))
                    create_directories (parent);
        }
        catch (const std::ios_base::failure&) {}

        std::ofstream stream;
        stream.exceptions (std::ofstream::failbit | std::ofstream::badbit);
        stream.open (path.string(), std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
        stream.write (newContent.data(), static_cast<std::streamsize> (newContent.size()));
        return;
    }
    catch (const std::ios_base::failure& e)
    {
        throw Error ("Failed to write to file: " + path.string() + ": " + e.what());
    }

    throw Error ("Failed to open file: " + path.string());
}

//==============================================================================
inline TempFile::TempFile() = default;

inline TempFile::TempFile (std::string_view folder)
{
    file = std::filesystem::temp_directory_path();

    if (! folder.empty())
    {
        file = file / folder;
        create_directories (file);
    }
}

inline TempFile::TempFile (std::string_view folder, std::string_view filename)
  : TempFile (folder)
{
    file = file / filename;
}

inline TempFile::TempFile (std::string_view folder, std::string_view root, std::string_view suffix)
  : TempFile (folder, createRandomFilename (root, suffix))
{
}

inline std::string TempFile::createRandomFilename (std::string_view root, std::string_view suffix)
{
    std::random_device seed;
    std::mt19937 rng (seed());
    std::uniform_int_distribution<> dist (1, 99999999);
    auto randomID = std::to_string (static_cast<uint32_t> (dist (rng)));
    auto name = std::string (root) + "_" + randomID;

    if (! suffix.empty())
        name += (suffix[0] == '.' ? "" : ".") + std::string (suffix);

    return name;
}

inline TempFile::~TempFile()
{
    remove_all (file);
}

} // namespace choc::file

#endif // CHOC_FILE_UTILS_HEADER_INCLUDED
