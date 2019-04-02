/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
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

} // namespace tracktion_engine
