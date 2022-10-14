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

const char* TextPlugin::xmlTypeName ("text");

TextPlugin::TextPlugin (PluginCreationInfo info)  : Plugin (info)
{
    auto um = getUndoManager();

    textTitle.referTo (state, IDs::title, um);
    textBody.referTo (state, IDs::body, um);
}

TextPlugin::~TextPlugin()
{
    notifyListenersOfDeletion();
}

juce::ValueTree TextPlugin::create()
{
    return createValueTree (IDs::PLUGIN,
                            IDs::type, xmlTypeName);
}

}} // namespace tracktion { inline namespace engine
