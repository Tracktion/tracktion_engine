/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


LevelMeterPlugin::LevelMeterPlugin (PluginCreationInfo info)  : Plugin (info)
{
    showMidiActivity.referTo (state, IDs::showMidi, getUndoManager());

    measurer.setShowMidi (showMidiActivity);
}

LevelMeterPlugin::~LevelMeterPlugin()
{
    notifyListenersOfDeletion();
}

ValueTree LevelMeterPlugin::create()
{
    ValueTree v (IDs::PLUGIN);
    v.setProperty (IDs::type, xmlTypeName, nullptr);
    return v;
}

const char* LevelMeterPlugin::xmlTypeName = "level";

void LevelMeterPlugin::initialise (const PlaybackInitialisationInfo& info)
{
    measurer.clear();
    initialiseWithoutStopping (info);
}

void LevelMeterPlugin::initialiseWithoutStopping (const PlaybackInitialisationInfo&)
{
    if (auto t = getOwnerTrack())
    {
        controllerTrack = t->getIndexInEditTrackList();
        startTimer (1000 / 50);
        return;
    }

    controllerTrack = -1;
    stopTimer();
}

void LevelMeterPlugin::deinitialise()
{
    measurer.clear();
    stopTimer();
}

void LevelMeterPlugin::applyToBuffer (const AudioRenderContext& fc)
{
    SCOPED_REALTIME_CHECK

    if (fc.destBuffer != nullptr)
        measurer.processBuffer (*fc.destBuffer, fc.bufferStartSample, fc.bufferNumSamples);

    if (fc.bufferForMidiMessages != nullptr)
    {
        measurer.setShowMidi (showMidiActivity);
        measurer.processMidi (*fc.bufferForMidiMessages, 0);
    }
}

void LevelMeterPlugin::timerCallback()
{
    if (controllerTrack >= 0)
    {
        auto& ecm = engine.getExternalControllerManager();

        if (ecm.isAttachedToEdit (edit))
            ecm.channelLevelChanged (controllerTrack, dbToGain (measurer.getLevelCache()));
    }
}

void LevelMeterPlugin::restorePluginStateFromValueTree (const ValueTree& v)
{
    CachedValue<bool>* cvsBool[] = { &showMidiActivity, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsBool);
}
