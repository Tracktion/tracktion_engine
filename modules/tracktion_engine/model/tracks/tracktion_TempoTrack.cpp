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

MasterTrack::MasterTrack (Edit& e, const ValueTree& v)
    : Track (e, v, 40, 13, 200)
{    
}

void MasterTrack::initialise()
{
    Track::initialise();
    
    pluginList.initialise (edit.getMasterPluginList().state);
}

MasterTrack::~MasterTrack()
{
    notifyListenersOfDeletion();
}

bool MasterTrack::isMasterTrack() const              { return true; }
juce::String MasterTrack::getName()                  { return TRANS("Master"); }
String MasterTrack::getSelectableDescription()       { return TRANS("Master Track"); }
bool MasterTrack::canContainPlugin (Plugin* p) const { return p->canBeAddedToMaster(); }

}
