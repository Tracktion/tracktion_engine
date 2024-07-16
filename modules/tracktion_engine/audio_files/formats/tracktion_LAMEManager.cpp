/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

#if JUCE_USE_LAME_AUDIO_FORMAT

//==============================================================================
juce::File LAMEManager::getLameEncoderExe()
{
   #if JUCE_WINDOWS
    auto defaultLame = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("lame.exe");
   #elif JUCE_MAC
    auto defaultLame = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("../Resources/lame");
   #else
    juce::File defaultLame ("/usr/bin/lame");
   #endif

    return defaultLame;
}

juce::File LAMEManager::getFFmpegExe()
{
   #if JUCE_WINDOWS
    auto defaultFFmpeg = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("ffmpeg.exe");
   #elif JUCE_MAC
    auto defaultFFmpeg = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("../Resources/ffmpeg");
   #else
    juce::File defaultFFmpeg ("/usr/bin/ffmpeg");
   #endif

    return defaultFFmpeg;
}

/** Adds lame encoders to the audio file format manager if necessary. */
void LAMEManager::registerAudioFormat (AudioFileFormatManager& affm)
{
    if (affm.getLameFormat() == nullptr && LAMEManager::lameIsAvailable())
    {
        TRACKTION_LOG ("LAME: using exe: " + LAMEManager::getLameEncoderExe().getFullPathName());

       #if TRACKTION_ENABLE_FFMPEG
        affm.addLameFormat (std::make_unique<FFmpegEncoderAudioFormat> (LAMEManager::getFFmpegExe()),
                            std::make_unique<FFmpegEncoderAudioFormat> (LAMEManager::getFFmpegExe()));
       #else
        affm.addLameFormat (std::make_unique<juce::LAMEEncoderAudioFormat> (LAMEManager::getLameEncoderExe()),
                            std::make_unique<juce::LAMEEncoderAudioFormat> (LAMEManager::getLameEncoderExe()));

       #endif
    }
}

bool LAMEManager::lameIsAvailable()
{
   #if TRACKTION_ENABLE_FFMPEG
    auto ffmpeg = getFFmpegExe();
    return ffmpeg.exists() && ffmpeg.getFileName().containsIgnoreCase ("ffmpeg");
   #else
    auto lameEnc = getLameEncoderExe();
    return lameEnc.exists() && lameEnc.getFileName().containsIgnoreCase ("lame");
   #endif
}

//==============================================================================
#else

juce::File LAMEManager::getLameEncoderExe() { return {}; }
void LAMEManager::registerAudioFormat (AudioFileFormatManager&) {}
bool LAMEManager::lameIsAvailable()         { return false; }

#endif

}} // namespace tracktion { inline namespace engine
