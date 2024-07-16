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

//==============================================================================
/**
    Manages the LAME location property and an AudioFormat if the LAME encoder is
    provided.
*/
class LAMEManager
{
public:
    //==============================================================================
    /** Returns the current LAME file. */
    static juce::File getLameEncoderExe();

    /** Returns the current FFmpeg file. */
    static juce::File getFFmpegExe();

    /** Add the LAMEAudioFormat to the AudioFileFormatManager */
    static void registerAudioFormat (AudioFileFormatManager&);

    /** Returns true if a valid LAME/FFmpeg file is found. */
    static bool lameIsAvailable();

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LAMEManager)
};

}} // namespace tracktion { inline namespace engine
