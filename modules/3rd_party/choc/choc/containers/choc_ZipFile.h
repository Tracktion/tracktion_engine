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
/**
    A class for creating .zip files.

    You can create one of these objects, giving it a stream to write to,
    and then add files to it. Calling flush() or deleting the ZipWriter
    will write the central directory and complete the archive.
*/
struct ZipWriter
{
    /// Compression level for files added to the archive.
    enum class CompressionLevel
    {
        uncompressed  = 0,    ///< No compression (store only)
        fastest       = 1,    ///< Fastest compression
        normal        = 6,    ///< Balanced compression/speed
        best          = 9,    ///< Maximum compression (slowest)
        defaultLevel  = -1    ///< Use default compression level
    };

    /// Creates a ZipWriter that will write to the given stream.
    ZipWriter (std::shared_ptr<std::ostream> outputStream);
    ~ZipWriter();

    ZipWriter (const ZipWriter&) = delete;
    ZipWriter (ZipWriter&&) = default;
    ZipWriter& operator= (ZipWriter&&) = default;
    ZipWriter& operator= (const ZipWriter&) = delete;

    /// Adds a file to the archive with the given path and content.
    void addFile (std::string_view path,
                  std::string_view content,
                  CompressionLevel compressionLevel = CompressionLevel::defaultLevel);


    /// Adds a file to the archive by reading from a stream.
    /// The stream will be read until EOF.
    void addFileFromStream (std::string_view path,
                            std::istream& source,
                            CompressionLevel compressionLevel = CompressionLevel::defaultLevel);

    /// Adds a folder to the archive. A trailing '/' will be added if not present.
    void addFolder (std::string_view path);


    /// Flushes the archive by writing the central directory - you can call
    // this manually, or let the destructor take care of it.
    void flush();

private:
    struct Impl;
    std::unique_ptr<ZipWriter::Impl> pimpl;
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


//==============================================================================
// ZipWriter implementation
//==============================================================================

struct ZipWriter::Impl
{
    struct FileEntry
    {
        std::string filename;
        uint64_t localHeaderOffset = 0;
        uint64_t compressedSize = 0;
        uint64_t uncompressedSize = 0;
        uint32_t crc32 = 0;
        uint16_t compressionMethod = 0;
        uint16_t modTime = 0;
        uint16_t modDate = 0;
    };

    std::shared_ptr<std::ostream> stream;
    std::vector<FileEntry> entries;
    int64_t centralDirectoryStart = -1;

    Impl (std::shared_ptr<std::ostream> s) : stream (std::move (s))
    {
        CHOC_ASSERT (stream != nullptr);
        stream->exceptions (std::ostream::failbit);
    }

    static void applyCurrentTime (FileEntry& entry)
    {
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t (now);
        std::tm tm {};

       #if defined(_WIN32) || defined(_WIN64)
        localtime_s (&tm, &timeT);
       #else
        localtime_r (&timeT, &tm);
       #endif

        entry.modDate = static_cast<uint16_t> (((tm.tm_year + 1900 - 1980) << 9)
                                                 | ((tm.tm_mon + 1) << 5)
                                                 | tm.tm_mday);

        entry.modTime = static_cast<uint16_t> ((tm.tm_hour << 11)
                                                | (tm.tm_min << 5)
                                                | (tm.tm_sec / 2));
    }

    void writeLocalFileHeader (const std::string& filename,
                               uint16_t compressionMethod,
                               uint16_t modTime,
                               uint16_t modDate,
                               uint32_t crc32,
                               uint64_t compressedSize,
                               uint64_t uncompressedSize)
    {
        writeUInt32 (0x04034b50); // Local file header signature
        writeUInt16 (20); // Version needed to extract (2.0)
        writeUInt16 (0); // General purpose bit flag
        writeUInt16 (compressionMethod);
        writeUInt16 (modTime);
        writeUInt16 (modDate);
        writeUInt32 (crc32);
        writeUInt32 (static_cast<uint32_t> (compressedSize));
        writeUInt32 (static_cast<uint32_t> (uncompressedSize));
        writeUInt16 (static_cast<uint16_t> (filename.length()));
        writeUInt16 (0); // Extra field length
        stream->write (filename.data(), static_cast<std::streamsize> (filename.length()));
    }

    void writeCentralDirectory()
    {
        if (centralDirectoryStart >= 0)
            return;

        auto centralDirOffset = static_cast<uint64_t> (stream->tellp());
        centralDirectoryStart = static_cast<int64_t> (centralDirOffset);

        for (const auto& entry : entries)
            writeCentralDirectoryHeader (entry);

        auto centralDirEnd = static_cast<uint64_t> (stream->tellp());
        auto centralDirSize = centralDirEnd - centralDirOffset;

        // Write end of central directory record
        writeEndOfCentralDirectory (centralDirOffset, centralDirSize);

        stream->flush();
    }

    void writeCentralDirectoryHeader (const FileEntry& entry)
    {
        writeUInt32 (0x02014b50); // Central directory file header signature
        writeUInt16 (0x031e); // Version made by (Unix)
        writeUInt16 (20); // Version needed to extract
        writeUInt16 (0); // General purpose bit flag
        writeUInt16 (entry.compressionMethod);
        writeUInt16 (entry.modTime);
        writeUInt16 (entry.modDate);
        writeUInt32 (entry.crc32);
        writeUInt32 (static_cast<uint32_t> (entry.compressedSize));
        writeUInt32 (static_cast<uint32_t> (entry.uncompressedSize));
        writeUInt16 (static_cast<uint16_t> (entry.filename.length()));
        writeUInt16 (0); // Extra field length
        writeUInt16 (0); // File comment length
        writeUInt16 (0); // Disk number start
        writeUInt16 (0); // Internal file attributes

        // External file attributes (Unix file permissions in upper 16 bits)
        // For files: 0644 (readable by all, writable by owner)
        // For folders: 0755 (readable/executable by all, writable by owner)
        writeUInt32 (entry.filename.empty() || entry.filename.back() != '/'
                        ? 0x81A40000   // Regular file: -rw-r--r--
                        : 0x41ED0000); // Directory: drwxr-xr-x

        writeUInt32 (static_cast<uint32_t> (entry.localHeaderOffset)); // Relative offset of local header
        stream->write (entry.filename.data(), static_cast<std::streamsize> (entry.filename.length()));
    }

    void writeEndOfCentralDirectory (uint64_t centralDirOffset, uint64_t centralDirSize)
    {
        writeUInt32 (0x06054b50); // End of central directory signature
        writeUInt16 (0); // Number of this disk
        writeUInt16 (0); // Disk where central directory starts
        writeUInt16 (static_cast<uint16_t> (entries.size())); // Number of central directory records on this disk
        writeUInt16 (static_cast<uint16_t> (entries.size())); // Total number of central directory records
        writeUInt32 (static_cast<uint32_t> (centralDirSize));
        writeUInt32 (static_cast<uint32_t> (centralDirOffset));
        writeUInt16 (0); // ZIP file comment length
    }

    void writeUInt16 (uint16_t value)
    {
        uint8_t buffer[2];
        choc::memory::writeLittleEndian (buffer, value);
        stream->write (reinterpret_cast<const char*> (buffer), 2);
    }

    void writeUInt32 (uint32_t value)
    {
        uint8_t buffer[4];
        choc::memory::writeLittleEndian (buffer, value);
        stream->write (reinterpret_cast<const char*> (buffer), 4);
    }

    void removeCentralDirectoryIfPresent()
    {
        if (centralDirectoryStart >= 0)
        {
            stream->seekp (centralDirectoryStart);
            centralDirectoryStart = -1;
        }
    }

    void addFileImpl (std::string_view path,
                      std::istream* sourceStream,
                      std::string_view content,
                      ZipWriter::CompressionLevel compressionLevel)
    {
        removeCentralDirectoryIfPresent();

        FileEntry entry;
        entry.filename = std::string (path);
        entry.localHeaderOffset = static_cast<uint64_t> (stream->tellp());

        applyCurrentTime (entry);

        auto level = static_cast<choc::zlib::DeflaterStream::CompressionLevel> (compressionLevel);
        bool useCompression = (level != 0);
        entry.compressionMethod = useCompression ? 8 : 0; // 8 = deflate, 0 = store

        // Write header with placeholder CRC32 and compressed size
        auto headerPos = stream->tellp();
        writeLocalFileHeader (entry.filename, entry.compressionMethod, entry.modTime,
                              entry.modDate, 0, 0, 0);

        auto dataStartPos = stream->tellp();

        if (useCompression)
        {
            choc::zlib::DeflaterStream deflater (stream, level, -15); // -15 = raw deflate
            writeContent (entry, deflater, sourceStream, content);
        }
        else
        {
            // Store uncompressed
            writeContent (entry, *stream, sourceStream, content);
        }

        stream->flush(); // Ensure all compressed data is written before measuring position
        auto dataEndPos = stream->tellp();
        entry.compressedSize = static_cast<uint64_t> (dataEndPos - dataStartPos);

        // Go back and update the header with the actual CRC32 and compressed size
        stream->seekp (headerPos);
        writeLocalFileHeader (entry.filename, entry.compressionMethod, entry.modTime,
                              entry.modDate, entry.crc32, entry.compressedSize, entry.uncompressedSize);
        stream->seekp (dataEndPos);

        entries.push_back (entry);
    }

    void addFolder (std::string_view path)
    {
        removeCentralDirectoryIfPresent();

        FileEntry entry;
        entry.filename = std::string (path);

        if (! entry.filename.empty() && entry.filename.back() != '/')
            entry.filename += '/';

        entry.localHeaderOffset = static_cast<uint64_t> (stream->tellp());
        entry.uncompressedSize = 0;
        entry.compressedSize = 0;
        entry.crc32 = 0;
        entry.compressionMethod = 0;

        applyCurrentTime (entry);

        writeLocalFileHeader (entry.filename, entry.compressionMethod, entry.modTime,
                              entry.modDate, entry.crc32, entry.compressedSize, entry.uncompressedSize);

        entries.push_back (entry);
    }

    static void writeContent (FileEntry& entry,
                              std::ostream& destStream,
                              std::istream* sourceStream,
                              std::string_view content)
    {
        if (sourceStream != nullptr)
        {
            constexpr size_t bufferSize = 8192;
            std::vector<char> buffer (bufferSize);

            for (;;)
            {
                try
                {
                    sourceStream->read (buffer.data(), static_cast<std::streamsize> (bufferSize));
                }
                catch (...) {}

                auto actuallyRead = static_cast<size_t> (sourceStream->gcount());

                if (actuallyRead == 0)
                    break;

                // Calculate CRC32 on uncompressed data
                entry.crc32 = static_cast<uint32_t> (choc::zlib::zlib::Checksum::crc32 (entry.crc32,
                                                                                        reinterpret_cast<const uint8_t*> (buffer.data()),
                                                                                        static_cast<unsigned> (actuallyRead)));
                entry.uncompressedSize += actuallyRead;
                destStream.write (buffer.data(), static_cast<std::streamsize> (actuallyRead));
            }
        }
        else
        {
            entry.uncompressedSize = content.size();
            destStream.write (content.data(), static_cast<std::streamsize> (content.size()));
            entry.crc32 = static_cast<uint32_t> (choc::zlib::zlib::Checksum::crc32 (0,
                                                                                    reinterpret_cast<const uint8_t*> (content.data()),
                                                                                    static_cast<unsigned> (content.size())));
        }
    }
};


inline ZipWriter::ZipWriter (std::shared_ptr<std::ostream> outputStream)
    : pimpl (std::make_unique<ZipWriter::Impl> (std::move (outputStream)))
{
}

inline ZipWriter::~ZipWriter()
{
    try
    {
        flush();
    }
    catch (...) {}
}

inline void ZipWriter::addFile (std::string_view path,
                                std::string_view content,
                                ZipWriter::CompressionLevel level)
{
    pimpl->addFileImpl (path, nullptr, content, level);
}

inline void ZipWriter::addFileFromStream (std::string_view path,
                                          std::istream& source,
                                          ZipWriter::CompressionLevel level)
{
    pimpl->addFileImpl (path, &source, {}, level);
}

inline void ZipWriter::addFolder (std::string_view path)
{
    pimpl->addFolder (path);
}

inline void ZipWriter::flush()
{
    pimpl->writeCentralDirectory();
}

} // namespace choc::zip


#endif // CHOC_ZIPFILE_HEADER_INCLUDED
