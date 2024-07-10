/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
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
