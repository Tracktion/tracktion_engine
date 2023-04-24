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
    initialiseWithoutStopping (info);
}

void LevelMeterPlugin::initialiseWithoutStopping (const PluginInitialisationInfo&)
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
            auto l = measurer.getLevelCache();
            ecm.channelLevelChanged (controllerTrack, dbToGain (l.first), dbToGain (l.second));
        }
    }
}

void LevelMeterPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    juce::CachedValue<bool>* cvsBool[] = { &showMidiActivity, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsBool);

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

}} // namespace tracktion { inline namespace engine
