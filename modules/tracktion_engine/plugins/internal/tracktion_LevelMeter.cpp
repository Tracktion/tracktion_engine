/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

LevelMeterPlugin::LevelMeterPlugin (PluginCreationInfo info)  : Plugin (info)
{
    showMidiActivity.referTo (state, IDs::showMidi, getUndoManager());

    measurer.setShowMidi (showMidiActivity);
}

LevelMeterPlugin::~LevelMeterPlugin()
{
    notifyListenersOfDeletion();
}

juce::ValueTree LevelMeterPlugin::create()
{
    juce::ValueTree v (IDs::PLUGIN);
    v.setProperty (IDs::type, xmlTypeName, nullptr);
    return v;
}

const char* LevelMeterPlugin::xmlTypeName = "level";

void LevelMeterPlugin::initialise (const PluginInitialisationInfo& info)
{
    measurer.clear();
    measurer.addClient (controllerLevelClient);
    initialiseWithoutStopping (info);
}

void LevelMeterPlugin::initialiseWithoutStopping (const PluginInitialisationInfo&)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

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
    measurer.removeClient (controllerLevelClient);
    measurer.clear();
    stopTimer();
}

void LevelMeterPlugin::applyToBuffer (const PluginRenderContext& fc)
{
    SCOPED_REALTIME_CHECK

    if (fc.destBuffer != nullptr)
        measurer.processBuffer (*fc.destBuffer, fc.bufferStartSample, fc.bufferNumSamples);

    if (fc.bufferForMidiMessages != nullptr)
    {
        measurer.setShowMidi (showMidiActivity);
        measurer.processMidi (*fc.bufferForMidiMessages, nullptr);
    }
}

void LevelMeterPlugin::timerCallback()
{
    if (controllerTrack >= 0)
    {
        auto& ecm = engine.getExternalControllerManager();

        if (ecm.isAttachedToEdit (edit))
        {
            auto numChans = controllerLevelClient.getNumChannelsUsed();
            float gains[32];

            for (int i = 0; i < numChans; ++i)
                gains[i] = dbToGain (controllerLevelClient.getAndClearAudioLevel (i).dB);

            ecm.channelLevelChanged (controllerTrack, { gains, (size_t) numChans });
        }
    }
}

void LevelMeterPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    copyPropertiesToCachedValues (v, showMidiActivity);

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

} // namespace tracktion::inline engine
