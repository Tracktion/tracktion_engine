/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
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
