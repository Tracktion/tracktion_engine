/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine
{
    std::string getCmajorVersion();
    std::unique_ptr<juce::AudioPluginFormat> createCmajorPatchPluginFormat (Engine&);

    bool isCmajorPatchPluginFormat (const juce::PluginDescription&);
    bool isCmajorPatchPluginFormat (const juce::String& formatName);
    juce::String getCmajorPatchPluginFormatName();
    juce::String getCmajorPatchCompileError (Plugin&);

}} // namespace tracktion { inline namespace engine
