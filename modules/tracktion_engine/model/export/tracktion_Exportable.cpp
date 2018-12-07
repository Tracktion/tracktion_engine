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

Array<Exportable*> Exportable::addAllExportables (Edit& edit)
{
    Array<Exportable*> list;

    for (auto t : getAudioTracks (edit))
    {
        for (auto& c : t->getClips())
        {
            list.add (c);

            if (auto plugins = c->getPluginList())
                list.addArray (plugins->getPlugins());
        }

        list.addArray (t->pluginList.getPlugins());
    }

    for (auto rf : edit.getRackList().getTypes())
        list.addArray (rf->getPlugins());

    list.addArray (edit.getMasterPluginList().getPlugins());
    list.removeAllInstancesOf (nullptr);

    return list;
}

}
