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
// BEATCONNECT MODIFICATION START
const char* TextPlugin::uniqueId ("cf85e43c-3b47-420a-be17-36b7d0a306f9");
// BEATCONNECT MODIFICATION END

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
