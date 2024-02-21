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

juce::Array<Exportable*> Exportable::addAllExportables (Edit& edit)
{
    juce::Array<Exportable*> list;

    for (auto t : getAudioTracks (edit))
    {
        for (auto& c : getClipsOfTypeRecursive<Clip> (*t))
        {
            list.add (c);

            if (auto plugins = c->getPluginList())
                list.addArray (plugins->getPlugins());
        }

        for (auto s : t->getClipSlotList().getClipSlots())
            if (auto c = s->getClip())
                list.add (c);

        list.addArray (t->pluginList.getPlugins());
    }

    for (auto rf : edit.getRackList().getTypes())
        list.addArray (rf->getPlugins());

    list.addArray (edit.getMasterPluginList().getPlugins());
    list.removeAllInstancesOf (nullptr);

    return list;
}

}} // namespace tracktion { inline namespace engine
