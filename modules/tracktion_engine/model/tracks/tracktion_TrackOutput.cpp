/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


TrackOutput::TrackOutput (AudioTrack& t)
    : owner (t)
{
    auto um = &t.edit.getUndoManager();
    state = t.state.getOrCreateChildWithName (IDs::OUTPUTDEVICES, um);
    auto devicesTree = state.getOrCreateChildWithName (IDs::DEVICE, um);
    outputDevice.referTo (devicesTree, IDs::name, um);

    state.addListener (this);
}

TrackOutput::~TrackOutput()
{
    state.removeListener (this);
}

//==============================================================================
void TrackOutput::initialise()
{
    updateOutput();

    if (outputDevice.get().isEmpty() && getDestinationTrack() == nullptr)
        setOutputToDefaultDevice (false);
}

void TrackOutput::flushStateToValueTree()
{
    if (! destTrackID.isValid())
        return;

    if (auto at = dynamic_cast<AudioTrack*> (findTrackForID (owner.edit, destTrackID)))
        setOutputToTrack (at);
}

void TrackOutput::updateOutput()
{
    // Also called after edit has created all the tracks - check our output isn't dangling
    for (auto t = getDestinationTrack(); t != nullptr; t = t->getOutput().getDestinationTrack())
        t->setFrozen (false, Track::groupFreeze);

    auto oldTrackID = destTrackID;
    destTrackID = {};

    if (outputDevice.get().startsWith ("track "))
    {
        auto trackNum = outputDevice.get().upToFirstOccurrenceOf ("(", false, false).trim().getTrailingIntValue();

        for (auto at : getAudioTracks (owner.edit))
        {
            if (at->getAudioTrackNumber() == trackNum)
            {
                if (! at->getOutput().feedsInto (&owner))  // check for recursion
                    destTrackID = at->itemID;

                break;
            }
        }

        if (oldTrackID == destTrackID)
            return;
    }

    owner.edit.restartPlayback();
    owner.changed();
    owner.setFrozen (false, Track::groupFreeze);
}

//==============================================================================
bool TrackOutput::outputsToDevice (const String& deviceName, bool compareDefaultDevices) const
{
    if (auto destTrack = getDestinationTrack())
        return deviceName.startsWithIgnoreCase (TRANS("Track") + " ")
                && (deviceName.upToFirstOccurrenceOf ("(", false, false).trim().getTrailingIntValue()
                      == destTrack->getAudioTrackNumber());

    if (outputDevice.get().equalsIgnoreCase (deviceName))
        return true;

    if (compareDefaultDevices)
    {
        auto& dm = owner.edit.engine.getDeviceManager();

        if (auto defaultWave = dm.getDefaultWaveOutDevice())
        {
            if (defaultWave->isEnabled())
            {
                auto defWaveName = defaultWave->getName();

                bool b1 = deviceName.equalsIgnoreCase (DeviceManager::getDefaultAudioDeviceName (false))
                           && outputDevice.get().equalsIgnoreCase (defWaveName);

                bool b2 = deviceName.equalsIgnoreCase (defWaveName)
                           && outputDevice.get().equalsIgnoreCase (DeviceManager::getDefaultAudioDeviceName (false));

                if (b1 || b2)
                    return true;
            }
        }

        if (auto defaultMIDI = dm.getDefaultMidiOutDevice())
        {
            if (defaultMIDI->isEnabled())
            {
                auto defMidiName = defaultMIDI->getName();

                if ((deviceName.equalsIgnoreCase (DeviceManager::getDefaultMidiDeviceName (false))
                       && outputDevice.get().equalsIgnoreCase (defMidiName))
                     || (deviceName.equalsIgnoreCase (defMidiName)
                           && outputDevice.get().equalsIgnoreCase (DeviceManager::getDefaultMidiDeviceName (false))))
                    return true;
            }
        }
    }

    return false;
}

AudioTrack* TrackOutput::getDestinationTrack() const
{
    if (destTrackID.isValid())
    {
        auto t = findTrackForID (owner.edit, destTrackID);

        if (t != &owner) // recursion check
            return dynamic_cast<AudioTrack*> (t);
    }

    return {};
}

bool TrackOutput::outputsToDestTrack (AudioTrack& t) const
{
    return t.itemID == destTrackID;
}

OutputDevice* TrackOutput::getOutputDevice (bool traceThroughDestTracks) const
{
    auto dev = owner.edit.engine.getDeviceManager().findOutputDeviceWithName (outputDevice);

    if ((dev == nullptr || ! dev->isEnabled()) && traceThroughDestTracks)
        if (auto t = getDestinationTrack())
            dev = t->getOutput().getOutputDevice (true);

    return dev;
}

juce::String TrackOutput::getOutputName() const
{
    if (auto t = getDestinationTrack())
        return TRANS("Track") + " " + String (t->getAudioTrackNumber());

    return outputDevice;
}

juce::String TrackOutput::getDescriptiveOutputName() const
{
    if (auto t = getDestinationTrack())
    {
        if (! t->getName().startsWithIgnoreCase (TRANS("Track") + " "))
            return TRANS("Feeds into track 123 (XZZX)")
                     .replace ("123", String (t->getAudioTrackNumber()))
                     .replace ("XZZX", "(" + t->getName() + ")");

        return TRANS("Feeds into track 123")
                 .replace ("123", String (t->getAudioTrackNumber()));
    }

    if (auto* dev = owner.edit.engine.getDeviceManager().findOutputDeviceWithName (outputDevice))
        return dev->getAlias();

    return outputDevice;
}

void TrackOutput::setOutputByName (const String& name)
{
    if (name.startsWith (TRANS("Track") + " "))
        outputDevice = String ("track ") + String (name.upToFirstOccurrenceOf ("(", false, false).trim().getTrailingIntValue());
    else
        outputDevice = name;
}

bool TrackOutput::canPlayAudio() const
{
    if (auto out = getOutputDevice (false))
        if (! out->isMidi())
            return true;

    if (auto t = getDestinationTrack())
        return t->canPlayAudio();

    return false;
}

bool TrackOutput::canPlayMidi() const
{
    if (auto out = getOutputDevice (false))
        if (out->isMidi())
            return true;

    if (auto t = getDestinationTrack())
        return t->canPlayMidi();

    return false;
}

void TrackOutput::setOutputToTrack (AudioTrack* track)
{
    outputDevice = track != nullptr ? String ("track") + " " + String (track->getAudioTrackNumber())
                                    : String();
}

void TrackOutput::setOutputToDefaultDevice (bool isMidi)
{
    outputDevice = isMidi ? DeviceManager::getDefaultMidiDeviceName (false)
                          : DeviceManager::getDefaultAudioDeviceName (false);
}

static bool feedsIntoAnyOf (AudioTrack* t, const Array<AudioTrack*>& tracks)
{
    if (tracks.contains (t))
        return true;

    auto& output = t->getOutput();

    for (auto track : tracks)
        if (output.feedsInto (track))
            return true;

    return false;
}

void TrackOutput::getPossibleOutputDeviceNames (const Array<AudioTrack*>& tracks,
                                                StringArray& s, StringArray& a,
                                                BigInteger& hasAudio,
                                                BigInteger& hasMidi)
{
    if (tracks.isEmpty())
        return;

    s.add (DeviceManager::getDefaultAudioDeviceName (false));
    a.add (DeviceManager::getDefaultAudioDeviceName (true));
    hasAudio.setBit (0);

    s.add (DeviceManager::getDefaultMidiDeviceName (false));
    a.add (DeviceManager::getDefaultMidiDeviceName (true));
    hasMidi.setBit (1);

    auto& dm = tracks.getFirst()->edit.engine.getDeviceManager();

    for (int i = 0; i < dm.getNumOutputDevices(); ++i)
    {
        if (auto out = dm.getOutputDeviceAt (i))
        {
            if (out->isEnabled())
            {
                if (auto m = dynamic_cast<MidiOutputDevice*> (out))
                {
                    if (m->isConnectedToExternalController())
                        continue;

                    hasMidi.setBit (s.size(), true);
                }
                else
                {
                    hasAudio.setBit (s.size(), true);
                }

                s.add (out->getName());
                a.add (out->getAlias());
            }
        }
    }
}

void TrackOutput::getPossibleOutputNames (const Array<AudioTrack*>& tracks,
                                          StringArray& s, StringArray& a,
                                          BigInteger& hasAudio,
                                          BigInteger& hasMidi)
{
    if (tracks.isEmpty())
        return;

    getPossibleOutputDeviceNames (tracks, s, a, hasAudio, hasMidi);

    auto& edit = tracks[0]->edit;

    for (auto t : getAudioTracks (edit))
    {
        if (t->createsOutput() && ! feedsIntoAnyOf (t, tracks))
        {
            String trackName;
            const String trackNum (t->getAudioTrackNumber());

            if (! t->getName().startsWithIgnoreCase (TRANS("Track") + " "))
                trackName = (TRANS("Track") + " " + trackNum + " (" + t->getName() + ")");
            else
                trackName = (TRANS("Track") + " " + trackNum);

            s.add (trackName);
            a.add (trackName);
        }
    }
}

bool TrackOutput::feedsInto (const AudioTrack* dest) const
{
    if (auto t = getDestinationTrack())
        return t == dest || t->getOutput().feedsInto (dest);

    return false;
}

bool TrackOutput::injectLiveMidiMessage (const MidiMessage& message)
{
    auto dev = owner.edit.engine.getDeviceManager().findOutputDeviceWithName (outputDevice);

    if (dev != nullptr && dev->isMidi() && dev->isEnabled())
    {
        if (auto midiDev = dynamic_cast<MidiOutputDevice*> (dev))
        {
            midiDev->fireMessage (message);
            return true;
        }
    }
    else
    {
        if (auto t = getDestinationTrack())
        {
            t->getOutput().injectLiveMidiMessage (message);
            return true;
        }
    }

    return false;
}

void TrackOutput::valueTreePropertyChanged (ValueTree& v, const Identifier& ident)
{
    if (v.hasType (IDs::DEVICE) && ident == IDs::name)
    {
        outputDevice.forceUpdateOfCachedValue();
        updateOutput();
    }
}
