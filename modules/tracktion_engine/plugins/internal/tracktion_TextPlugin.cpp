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

ValueTree TextPlugin::create()
{
    ValueTree v (IDs::PLUGIN);
    v.setProperty (IDs::type, xmlTypeName, nullptr);
    return v;
}

}
