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

AuxReturnPlugin::AuxReturnPlugin (PluginCreationInfo info)  : Plugin (info)
{
    busNumber.referTo (state, IDs::busNum, getUndoManager());
}

AuxReturnPlugin::~AuxReturnPlugin()
{
    notifyListenersOfDeletion();
}

const char* AuxReturnPlugin::xmlTypeName = "auxreturn";

String AuxReturnPlugin::getName()
{
    String nm (edit.getAuxBusName (busNumber));

    if (nm.isNotEmpty())
        return "R:" + nm;

    return TRANS("Aux Return") + " #" + String (busNumber + 1);
}

String AuxReturnPlugin::getShortName (int)
{
    String nm (edit.getAuxBusName (busNumber));

    if (nm.isNotEmpty())
        return "R:" + nm;

    return "Ret:" + String (busNumber + 1);
}

void AuxReturnPlugin::initialise (const PluginInitialisationInfo&)
{
}

void AuxReturnPlugin::deinitialise()
{
}

void AuxReturnPlugin::applyToBuffer (const PluginRenderContext&)
{
}

void AuxReturnPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    CachedValue<int>* cvsInt[] = { &busNumber, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsInt);

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

}
