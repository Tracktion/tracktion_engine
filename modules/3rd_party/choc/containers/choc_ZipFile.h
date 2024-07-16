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

#ifndef CHOC_ZIPFILE_HEADER_INCLUDED
#define CHOC_ZIPFILE_HEADER_INCLUDED

#include <chrono>
#include <iostream>
#include <vector>
#include "choc_zlib.h"
#include "../memory/choc_Endianness.h"
#include "../text/choc_Files.h"


namespace choc::zip
{

/**
    A class for handling the .zip file format.

    You can create one of these objects, giving it a stream from which it
    can read a zip file, and it'll let you read and retrieve the files in it.
*/
struct ZipFile
{
    ZipFile (std::shared_ptr<std::istream> zipFileData);
    ~ZipFile();

    ZipFile (const ZipFile&) = delete;
    ZipFile (ZipFile&&) = default;
    ZipFile& operator= (ZipFile&&) = default;
    ZipFile& operator= (const ZipFile&) = delete;

    /// This class represents one of the file entries in the zip,
    /// providing metadata and the ability to create a stream to read it.
    struct Item
    {
        std::string filename;

        uint64_t uncompressedSize = 0;
        uint64_t compressedSize = 0;
        uint64_t fileStartOffset = 0;
        uint32_t date = 0;
        uint32_t time = 0;
        uint32_t attributeFlags = 0;
        bool isCompressed = false;

        /// Returns file attribute bits from the zip entry
        uint32_t getFileType() const;
        bool isFolder() const;
        bool isSymLink() const;

        std::filesystem::file_time_type getFileTime() const;

        /// Returns a stream that can be used to read the decompressed
        /// content for this file.
        std::shared_ptr<std::istream> createReader() const;

        /// Attempts to uncompress this entry to a file of the appropriate name
        /// and sub-path within the top-level folder specified.
        /// Returns true if successful, and throws errors if things go wrong.
        bool uncompressToFile (const std::filesystem::path& targetFolder,
                               bool overwriteExistingFile,
                               bool setFileTime) const;

    private:
        struct ZipStream;
        friend struct ZipFile;
        Item (ZipFile&, const char*, uint32_t);
        ZipFile* owner;
    };

    //==============================================================================
    /// A list of the entries that were found in this zip container.
    std::vector<Item> items;

    /// Attempts to uncompress all the entries to a given folder. Returns true
    /// if they were all successful, and throws errors if things go wrong.
    bool uncompressToFolder (const std::filesystem::path& targetFolder,
                             bool overwriteExistingFiles,
                             bool setFileTimes) const;

private:
    //==============================================================================
    std::shared_ptr<std::istream> source;
    int64_t fileSize = 0;
    uint32_t directoryStart = 0;
    uint32_t numEntries = 0;

    bool scanForDirectory();
    void readDirectoryEntries();
    void readChunk (void* dest, int64_t pos, size_t size);
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

inline ZipFile::ZipFile (std::shared_ptr<std::istream> s) : source (std::move (s))
{
    CHOC_ASSERT (source != nullptr);
    source->exceptions (std::istream::failbit);

    try
    {
        if (scanForDirectory())
            readDirectoryEntries();
    }
    catch (...)
    {
        items.clear();
    }
}

inline ZipFile::~ZipFile()
{
    items.clear();
    source.reset();
}

inline void ZipFile::readChunk (void* dest, int64_t pos, size_t size)
{
    source->seekg (pos, std::ios_base::beg);
    source->read (static_cast<std::istream::char_type*> (dest), static_cast<std::streamsize> (size));
}

inline bool ZipFile::scanForDirectory()
{
    source->seekg (0, std::ios_base::end);
    fileSize = static_cast<int64_t> (source->tellg());

    const auto scanAreaStart = std::max ((int64_t) 0, fileSize - static_cast<int64_t> (1048576));
    const auto scanAreaEnd = fileSize;
    constexpr int64_t chunkSize = 512;
    auto pos = scanAreaEnd - chunkSize;

    for (;;)
    {
        if (pos < scanAreaStart)
            pos = scanAreaStart;

        auto len = std::min (chunkSize, scanAreaEnd - pos);

        char buffer[chunkSize];
        readChunk (buffer, pos, static_cast<size_t> (len));

        for (int32_t i = chunkSize - 4; i >= 0; --i)
        {
            if (choc::memory::readLittleEndian<uint32_t> (buffer + i) == 0x06054b50)
            {
                readChunk (buffer, pos + i, 22);
                numEntries = choc::memory::readLittleEndian<uint16_t> (buffer + 10);
                directoryStart = choc::memory::readLittleEndian<uint32_t> (buffer + 16);

                if (directoryStart >= 4)
                {
                    readChunk (buffer, directoryStart - 4, 8);

                    if (choc::memory::readLittleEndian<uint32_t> (buffer + 4) != 0x02014b50
                            && choc::memory::readLittleEndian<uint32_t> (buffer) == 0x02014b50)
                        directoryStart -= 4;
                }

                return true;
            }
        }

        if (pos <= scanAreaStart)
            break;

        pos -= chunkSize - 4;
    }

    return false;
}

inline void ZipFile::readDirectoryEntries()
{
    auto directorySize = static_cast<size_t> (fileSize - directoryStart);
    std::vector<char> directoryData;
    directoryData.resize (directorySize);

    readChunk (directoryData.data(), directoryStart, directorySize);

    size_t entryPos = 0;
    items.reserve (numEntries);

    for (size_t i = 0; i < numEntries; ++i)
    {
        if (entryPos + 46 > directorySize)
            break;

        auto entryData = directoryData.data() + entryPos;
        auto filenameLength = choc::memory::readLittleEndian<uint16_t> (entryData + 28);
        auto entrySize = 46u + filenameLength
                           + choc::memory::readLittleEndian<uint16_t> (entryData + 30)
                           + choc::memory::readLittleEndian<uint16_t> (entryData + 32);

        if (entryPos + entrySize > directorySize)
            break;

        items.push_back ({ *this, entryData, filenameLength });
        entryPos += entrySize;
    }
}

inline bool ZipFile::uncompressToFolder (const std::filesystem::path& targetFolder,
                                         bool overwriteExistingFiles, bool setFileTimes) const
{
    for (auto& item : items)
        if (! item.uncompressToFile (targetFolder, overwriteExistingFiles, setFileTimes))
            return false;

    return true;
}

inline std::filesystem::file_time_type ZipFile::Item::getFileTime() const
{
    std::tm tm {};
    tm.tm_year  = (int) (date >> 9);
    tm.tm_mon   = (int) (((date >> 5) & 15u) - 1u);
    tm.tm_mday  = date & 31u;
    tm.tm_hour  = (int) (time >> 11);
    tm.tm_min   = (time >> 5) & 63u;
    tm.tm_sec   = (int) ((time & 31u) * 2u);
    tm.tm_isdst = 0;

    auto systemTime = std::chrono::system_clock::from_time_t (std::mktime (&tm));
    return std::filesystem::file_time_type (systemTime.time_since_epoch());
}

inline ZipFile::Item::Item (ZipFile& f, const char* entryData, uint32_t filenameLength)
    : owner (std::addressof (f))
{
    isCompressed     = choc::memory::readLittleEndian<uint16_t> (entryData + 10) != 0;
    time             = choc::memory::readLittleEndian<uint16_t> (entryData + 12);
    date             = choc::memory::readLittleEndian<uint16_t> (entryData + 14);
    compressedSize   = choc::memory::readLittleEndian<uint32_t> (entryData + 20);
    uncompressedSize = choc::memory::readLittleEndian<uint32_t> (entryData + 24);
    attributeFlags   = choc::memory::readLittleEndian<uint32_t> (entryData + 38);
    fileStartOffset  = choc::memory::readLittleEndian<uint32_t> (entryData + 42);
    filename         = std::string (entryData + 46, entryData + 46 + filenameLength);
}

inline uint32_t ZipFile::Item::getFileType() const    { return (attributeFlags >> 28) & 0x0f; }
inline bool ZipFile::Item::isSymLink() const          { return getFileType() == 10; }

inline bool ZipFile::Item::isFolder() const
{
    return ! filename.empty() && (filename.back() == '/' || filename.back() == '\\');
}

struct ZipFile::Item::ZipStream  : public  std::istream,
                                   private std::streambuf
{
    ZipStream (const Item& i)
        : std::istream (this),
          fileStream (i.owner->source),
          compressedSize (static_cast<int64_t> (i.compressedSize)),
          fileStartOffset (static_cast<int64_t> (i.fileStartOffset))
    {
        char entry[30];
        i.owner->readChunk (entry, fileStartOffset, 30);

        if (choc::memory::readLittleEndian<uint32_t> (entry) == 0x04034b50u)
            headerSize = 30u + choc::memory::readLittleEndian<uint16_t> (entry + 26)
                             + choc::memory::readLittleEndian<uint16_t> (entry + 28);

        seekpos (0, std::ios_base::in);
    }

    std::streambuf::pos_type seekoff (std::streambuf::off_type off, std::ios_base::seekdir dir, std::ios_base::openmode mode) override
    {
        int64_t newPos = off;

        if (dir == std::ios_base::cur)
            newPos += position;
        else if (dir == std::ios_base::end)
            newPos = compressedSize - off;

        return seekpos (static_cast<std::streambuf::pos_type> (newPos), mode);
    }

    std::streambuf::pos_type seekpos (std::streambuf::pos_type newPos, std::ios_base::openmode) override
    {
        if (newPos > compressedSize)
            return std::streambuf::pos_type (std::streambuf::off_type (-1));

        position = newPos;
        return newPos;
    }

    std::streamsize xsgetn (std::streambuf::char_type* dest, std::streamsize size) override
    {
        if (headerSize <= 0 || position < 0 || position >= compressedSize)
            return 0;

        if (position + size > compressedSize)
            size = compressedSize - position;

        if (size == 0)
            return 0;

        fileStream->seekg (fileStartOffset + headerSize + position);
        fileStream->read (dest, size);
        position += size;
        return size;
    }

private:
    std::shared_ptr<std::istream> fileStream;
    const int64_t compressedSize, fileStartOffset;
    int64_t position = 0;
    uint32_t headerSize = 0;
};

inline std::shared_ptr<std::istream> ZipFile::Item::createReader() const
{
    auto zs = std::make_shared<ZipStream> (*this);

    if (isCompressed)
        return std::make_shared<zlib::InflaterStream> (zs, zlib::InflaterStream::FormatType::deflate);

    return zs;
}

inline bool ZipFile::Item::uncompressToFile (const std::filesystem::path& targetFolder,
                                             bool overwriteExistingFile, bool setFileTime) const
{
    if (filename.empty())
        return true;

    auto targetFile = targetFolder / filename;

    if (isFolder())
    {
        if (! (create_directories (targetFile) || is_directory (targetFile)))
            throw std::runtime_error ("Failed to create folder: " + targetFile.string());

        return true;
    }

    if (isSymLink())
        throw std::runtime_error ("Failed to uncompress " + targetFile.string() + ": file was a symbolic link");

    auto reader = createReader();

    if (reader == nullptr)
        return false;

    if (! overwriteExistingFile && exists (targetFile))
        return true;

    choc::file::replaceFileWithContent (targetFile, *reader);

    if (setFileTime)
        last_write_time (targetFile, getFileTime());

    return true;
}

} // namespace choc::zip


#endif // CHOC_ZIPFILE_HEADER_INCLUDED
