/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

static bool isWorthConvertingToOgg (AudioFile& source, int quality)
{
    if (dynamic_cast<juce::OggVorbisAudioFormat*> (source.getFormat()) != nullptr)
    {
        auto estimatedQuality = juce::OggVorbisAudioFormat().estimateOggFileQuality (source.getFile());

        return estimatedQuality == 0
                || quality < estimatedQuality; // if they're asking for a higher quality than the current one, no point in converting
    }

    return true;
}

//==============================================================================
TracktionArchiveFile::IndexEntry::IndexEntry (juce::InputStream& in)
{
    offset = in.readInt();
    length = in.readInt();
    originalName = in.readString();
    storedName = in.readString();

    int numExtras = in.readShort();

    while (--numExtras >= 0 && ! in.isExhausted())
    {
        extraNames.add (in.readString());
        extraValues.add (in.readString());
    }

    DBG (originalName + " / " + storedName + "  length: " << length);
}

TracktionArchiveFile::IndexEntry::~IndexEntry() {}

void TracktionArchiveFile::IndexEntry::write (juce::OutputStream& out)
{
    out.writeInt (int (offset));
    out.writeInt (int (length));
    out.writeString (originalName);
    out.writeString (storedName);

    out.writeShort ((short) extraNames.size());

    for (int i = 0; i < extraNames.size(); ++i)
    {
        out.writeString (extraNames[i]);
        out.writeString (extraValues[i]);
    }
}


//==============================================================================
TracktionArchiveFile::TracktionArchiveFile (Engine& e, const juce::File& f)
    : engine (e), file (f)
{
    readIndex();
}

TracktionArchiveFile::~TracktionArchiveFile()
{
    flush();
}

int TracktionArchiveFile::getMagicNumber()
{
    return (int) juce::ByteOrder::littleEndianInt ("TKNA");
}

juce::File TracktionArchiveFile::getFile() const
{
    return file;
}

void TracktionArchiveFile::readIndex()
{
    valid = false;
    needToWriteIndex = false;
    entries.clear();

    juce::FileInputStream in (file);

    if (in.openedOk())
    {
        const int magic = in.readInt();

        if (magic == getMagicNumber())
        {
            indexOffset = in.readInt();
            jassert (indexOffset >= 8 && indexOffset < file.getSize());

            if (indexOffset >= 8 && indexOffset < file.getSize())
            {
                in.setPosition (indexOffset);
                int numEntries = in.readInt();

                while (--numEntries >= 0 && ! in.isExhausted())
                    entries.add (new IndexEntry (in));

                valid = true;
            }
        }
    }
}

void TracktionArchiveFile::flush()
{
    if (needToWriteIndex && valid)
    {
        juce::FileOutputStream out (file);

        if (out.openedOk())
        {
            out.setPosition (indexOffset);
            out.writeInt (entries.size());

            for (auto e : entries)
                e->write (out);

            out.setPosition (4);
            out.writeInt (int (indexOffset));
        }

        needToWriteIndex = false;
    }
}

bool TracktionArchiveFile::isValidArchive() const
{
    return valid;
}

int TracktionArchiveFile::getNumFiles() const
{
    return entries.size();
}

juce::String TracktionArchiveFile::getOriginalFileName (int index) const
{
    if (auto i = entries[index])
        return i->originalName.fromLastOccurrenceOf ("/", false, false);

    return {};
}

int TracktionArchiveFile::indexOfFile (const juce::String& name) const
{
    for (int i = entries.size(); --i >= 0;)
        if (getOriginalFileName(i).equalsIgnoreCase (name))
            return i;

    return -1;
}

std::unique_ptr<juce::InputStream> TracktionArchiveFile::createStoredInputStream (int index) const
{
    if (entries[index] != nullptr)
        if (auto f = file.createInputStream())
            return std::make_unique<juce::SubregionStream> (f.release(),
                                                            entries[index]->offset,
                                                            entries[index]->length,
                                                            true);

    return {};
}

bool TracktionArchiveFile::extractFile (int index, const juce::File& destDirectory,
                                        juce::File& fileCreated, bool askBeforeOverwriting)
{
    if (! destDirectory.createDirectory())
        return false;

    auto destFile = destDirectory.getChildFile (getOriginalFileName (index));
    fileCreated = destFile;

    if (askBeforeOverwriting && destFile.existsAsFile())
    {
        auto r = engine.getUIBehaviour()
                    .showYesNoCancelAlertBox (TRANS("Unpacking archive"),
                                              TRANS("The file XZZX already exists - do you want to overwrite it?")
                                                .replace ("XZZX", destFile.getFullPathName()),
                                              TRANS("Overwrite"),
                                              TRANS("Leave existing"));

        if (r == 1)  return true;
        if (r == 0)  return false;
    }

    if (destFile.isDirectory()
         || ! destFile.hasWriteAccess()
         || ! destFile.deleteFile()
         || entries[index] == nullptr)
        return false;

    auto source = createStoredInputStream (index);

    if (source == nullptr)
        return false;

    auto storedName = entries[index]->storedName;

    if (storedName != entries[index]->originalName)
    {
        if (storedName.endsWithIgnoreCase (".flac"))
            return AudioFileUtils::readFromFormat<juce::FlacAudioFormat> (engine, *source, destFile);

        if (storedName.endsWithIgnoreCase (".ogg"))
            return AudioFileUtils::readFromFormat<juce::OggVorbisAudioFormat> (engine, *source, destFile);

        if (storedName.endsWithIgnoreCase (".gz"))
            source = std::make_unique<juce::GZIPDecompressorInputStream> (source.release(), true);
        else
            jassertfalse;
    }

    {
        juce::FileOutputStream out (destFile);

        if (! out.openedOk())
            return false;

        out.writeFromInputStream (*source, -1);
    }

    return true;
}

bool TracktionArchiveFile::extractAll (const juce::File& destDirectory,
                                       juce::Array<juce::File>& filesCreated)
{
    if (! destDirectory.createDirectory())
        return false;

    for (int i = 0; i < entries.size(); ++i)
    {
        juce::File fileCreated;

        if (! extractFile (i, destDirectory, fileCreated, false))
            return false;

        filesCreated.add (fileCreated);
    }

    return true;
}

//==============================================================================
class ExtractionTask  : public ThreadPoolJobWithProgress
{
public:
    ExtractionTask (TracktionArchiveFile& archive_, const juce::File& destDir_,
                    bool warnAboutOverwrite_,
                    juce::Array<juce::File>& filesCreated_,
                    bool& wasAborted_)
        : ThreadPoolJobWithProgress (TRANS("Unpacking archive") + "..."),
          archive (archive_), destDir (destDir_),
          wasAborted (wasAborted_),
          warnAboutOverwrite (warnAboutOverwrite_),
          filesCreated (filesCreated_)
    {
        wasAborted = false;
    }

    JobStatus runJob()
    {
        CRASH_TRACER
        juce::FloatVectorOperations::disableDenormalisedNumberSupport();

        if (! destDir.createDirectory())
            return jobHasFinished;

        for (int i = 0; i < archive.getNumFiles(); ++i)
        {
            if (shouldExit())
            {
                wasAborted = true;

                for (auto& f : filesCreated)
                    f.deleteFile();

                break;
            }

            progress = i / (float) archive.getNumFiles();

            juce::File fileCreated;

            if (! archive.extractFile (i, destDir, fileCreated, warnAboutOverwrite))
                return jobHasFinished;

            if (fileCreated.exists())
                filesCreated.add (fileCreated);
        }

        ok = true;
        return jobHasFinished;
    }

    float getCurrentTaskProgress()
    {
        return progress;
    }

    TracktionArchiveFile& archive;
    juce::File destDir;
    bool ok = false;
    bool& wasAborted;
    float progress = 0;
    bool warnAboutOverwrite = false;
    juce::Array<juce::File>& filesCreated;
};

//==============================================================================
bool TracktionArchiveFile::extractAllAsTask (const juce::File& destDirectory, bool warnAboutOverwrite,
                                             juce::Array<juce::File>& filesCreated, bool& wasAborted)
{
    ExtractionTask task (*this, destDirectory, warnAboutOverwrite, filesCreated, wasAborted);

    engine.getUIBehaviour().runTaskWithProgressBar (task);

    return task.ok;
}

bool TracktionArchiveFile::addFile (const juce::File& f, const juce::File& rootDirectory,
                                    CompressionType compression)
{
    juce::String name;

    if (f.isAChildOf (rootDirectory))
        name = f.getRelativePathFrom (rootDirectory)
                .replaceCharacter ('\\', '/');
    else
        name = f.getFileName();

    return addFile (f, name, compression);
}

bool TracktionArchiveFile::addFile (const juce::File& f, const juce::String& filenameToUse,
                                    CompressionType compression)
{
    // don't risk using ogg or flac on small audio files
    if (compression != CompressionType::none && f.getSize() <= 16 * 1024)
        compression = CompressionType::zip;

    juce::FileInputStream in (f);
    bool ok = false;

    if (in.openedOk())
    {
        juce::FileOutputStream out (file);

        if (out.openedOk())
        {
            if (! valid)
            {
                out.setPosition (0);
                out.writeInt (getMagicNumber());
                out.writeInt (int (indexOffset));
                valid = true;
            }

            auto initialPosition = out.getPosition();

            out.setPosition (indexOffset);
            jassert (indexOffset < 2147483648);

            if (indexOffset >= 2147483648)
            {
                TRACKTION_LOG_ERROR ("Archive too large when archiving file: " + f.getFileName());
                return false;
            }

            auto filenameRoot = filenameToUse.substring (0, filenameToUse.lastIndexOfChar ('.'));

            std::unique_ptr<IndexEntry> entry (new IndexEntry());
            entry->offset = indexOffset;
            entry->length = 0;
            entry->originalName = filenameToUse;
            entry->storedName = filenameToUse;

            switch (compression)
            {
                case CompressionType::none:
                {
                    out.writeFromInputStream (in, -1);
                    break;
                }

                case CompressionType::zip:
                {
                    entry->storedName = filenameRoot + ".gz";

                    juce::GZIPCompressorOutputStream deflater (&out, 9, false);
                    deflater.writeFromInputStream (in, -1);
                    break;
                }

                case CompressionType::lossless:
                {
                    AudioFile af (engine, f);

                    if (af.isOggFile() || af.isMp3File() || af.isFlacFile())
                    {
                        out.writeFromInputStream (in, -1); // no point re-compressing these
                    }
                    else
                    {
                        if (af.getBitsPerSample() > 24)
                        {
                            // FLAC can't do higher than 24 bits so just have to zip it instead..
                            entry->storedName = filenameRoot + ".gz";

                            juce::GZIPCompressorOutputStream deflater (&out, 9, false);
                            deflater.writeFromInputStream (in, -1);
                        }
                        else
                        {
                            entry->storedName = filenameRoot + ".flac";

                            if (! AudioFileUtils::convertToFormat<juce::FlacAudioFormat> (engine, f, out, 0,
                                                                                          juce::StringPairArray()))
                            {
                                needToWriteIndex = true;
                                TRACKTION_LOG_ERROR ("Failed to add file to archive flac: " + f.getFileName());
                                return false;
                            }
                        }
                    }

                    break;
                }

                case CompressionType::lossyGoodQuality:
                case CompressionType::lossyMediumQuality:
                case CompressionType::lossyLowQuality:
                {
                    entry->storedName = filenameRoot + ".ogg";
                    entry->originalName = entry->storedName;  // oggs get extracted as oggs, not named back to how they were

                    auto quality = getOggQuality (compression);
                    AudioFile af (engine, f);

                    if (! isWorthConvertingToOgg (af, quality))
                    {
                        juce::FileInputStream fin (af.getFile());

                        if (! fin.openedOk())
                        {
                            needToWriteIndex = true;
                            TRACKTION_LOG_ERROR ("Failed to add file to archive: " + f.getFileName());
                            return false;
                        }

                        out.writeFromInputStream (fin, -1);
                    }
                    else if (! AudioFileUtils::convertToFormat<juce::OggVorbisAudioFormat> (engine, f, out, quality,
                                                                                            juce::StringPairArray()))
                    {
                        needToWriteIndex = true;
                        TRACKTION_LOG_ERROR ("Failed to add file to archive ogg: " + f.getFileName());
                        return false;
                    }

                    break;
                }

                default:
                {
                    TRACKTION_LOG_ERROR ("Unknown compression type when archiving file: " + f.getFileName());
                    jassertfalse;
                    break;
                }
            }

            out.flush();

            jassert (out.getPosition() > indexOffset);

            entry->length = std::max (juce::int64(), out.getPosition() - indexOffset);

            jassert (indexOffset + entry->length < 2147483648);

            if (indexOffset + entry->length >= 2147483648)
            {
                out.setPosition (initialPosition);
                out.truncate();
                TRACKTION_LOG_ERROR ("Archive too large when archiving file: " + f.getFileName());
                return false;
            }

            indexOffset += entry->length;
            needToWriteIndex = true;
            ok = true;

            entries.add (entry.release());
        }
    }

    return ok;
}

void TracktionArchiveFile::addFileInfo (const juce::String& filename,
                                        const juce::String& itemName,
                                        const juce::String& itemValue)
{
    auto i = indexOfFile (filename);

    if (auto entry = entries[i])
    {
        entry->extraNames.add (itemName);
        entry->extraValues.add (itemValue);
        needToWriteIndex = true;
    }
}

int TracktionArchiveFile::getOggQuality (CompressionType c)
{
    auto numOptions = juce::OggVorbisAudioFormat().getQualityOptions().size();

    if (c == CompressionType::lossyGoodQuality)     return numOptions - 1;
    if (c == CompressionType::lossyMediumQuality)   return numOptions / 2;

    return numOptions / 5;
}

}} // namespace tracktion { inline namespace engine
