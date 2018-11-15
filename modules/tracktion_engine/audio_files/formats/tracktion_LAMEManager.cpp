/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


#if JUCE_USE_LAME_AUDIO_FORMAT

//==============================================================================
juce::File LAMEManager::getLameEncoderExe()
{
   #if JUCE_WINDOWS
    auto defaultLame = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("lame.exe");
   #elif JUCE_MAC
    auto defaultLame = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("lame");
   #else
    juce::File defaultLame ("/usr/bin/lame");
   #endif

    return defaultLame;
}

/** Adds lame encoders to the audio file format manager if necessary. */
void LAMEManager::registerAudioFormat (AudioFileFormatManager& affm)
{
    if (affm.getLameFormat() == nullptr && LAMEManager::lameIsAvailable())
    {
        TRACKTION_LOG ("LAME: using exe: " + LAMEManager::getLameEncoderExe().getFullPathName());

        affm.addLameFormat (std::make_unique<juce::LAMEEncoderAudioFormat> (LAMEManager::getLameEncoderExe()),
                            std::make_unique<juce::LAMEEncoderAudioFormat> (LAMEManager::getLameEncoderExe()));
    }
}

bool LAMEManager::lameIsAvailable()
{
    auto lameEnc = getLameEncoderExe();
    return lameEnc.exists() && lameEnc.getFileName().containsIgnoreCase ("lame");
}

//==============================================================================
#else

juce::File LAMEManager::getLameEncoderExe() { return {}; }
void LAMEManager::registerAudioFormat (AudioFileFormatManager&) {}
bool LAMEManager::lameIsAvailable()         { return false; }

#endif
