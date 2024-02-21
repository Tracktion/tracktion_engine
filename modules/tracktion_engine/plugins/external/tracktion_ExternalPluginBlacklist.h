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

/** Some plugins fail with certain calls to their parameters so we'll just avoid adding those to the list. */
static inline bool isParameterBlacklisted (ExternalPlugin& plugin, juce::AudioPluginInstance&, juce::AudioProcessorParameter& parameter)
{
    ignoreUnused (plugin);
    ignoreUnused (parameter);

   #if JUCE_WINDOWS
    if (plugin.isVST())
    {
        if (plugin.desc.fileOrIdentifier.endsWith ("Trigger_2.dll"))
            if (parameter.getParameterIndex() >= 117)
                return true;
    }
   #endif

    return false;
}

}} // namespace tracktion { inline namespace engine
