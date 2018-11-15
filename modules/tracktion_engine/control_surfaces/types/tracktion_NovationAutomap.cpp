/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


#ifdef JUCE_MSVC
 #pragma warning (push)
 #pragma warning (disable: 4100)
#endif

#if TRACKTION_ENABLE_AUTOMAP

#ifdef __clang__
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wunused-parameter"
#endif

#include "AutomapSDK/include/Automap.h"

#ifdef __clang__
 #pragma clang diagnostic pop
#endif

#if JUCE_WINDOWS
 #if JUCE_64BIT
  #pragma comment (lib, "AutomapClient64.lib")
 #else
  #pragma comment (lib, "AutomapClient.lib")
 #endif
#endif

#ifdef JUCE_MSVC
 #pragma warning (pop)
#endif

namespace tracktion_engine
{

//=============================================================================
class AutoMap   : private AsyncUpdater
{
public:
    AutoMap (NovationAutomap& n) : novationAutomap (n) {}

    virtual void paramChanged (AutomatableParameter*) = 0;
    virtual void playChanged (bool) = 0;
    virtual void recordChanged (bool) = 0;
    virtual void timeChanged() = 0;
    virtual void faderBankChanged (const StringArray& trackNames) = 0;
    virtual void armChanged (int ch) = 0;
    virtual void updateSoloAndMute (int channelNum, Track::MuteAndSoloLightState, bool isBright) = 0;
    virtual void select() = 0;
    virtual Selectable* getSelectableObject() = 0;

    void paramChangedInt (AutomatableParameter* param)
    {
        if (! inSetParamValue)
        {
            {
                const ScopedLock sl (dirtyParamLock);
                dirtyParams.add (param);
            }

            triggerAsyncUpdate();
        }
    }

    void handleAsyncUpdate()
    {
        CRASH_TRACER
        Array<AutomatableParameter*> params;

        {
            const ScopedLock sl (dirtyParamLock);
            params.swapWith (dirtyParams);
        }

        for (int i = 0; i < params.size(); ++i)
            paramChanged (params.getUnchecked (i));
    }

    Automap::Connection* getConnection()
    {
        return connection;
    }

    NovationAutomap& novationAutomap;
    Automap::Connection* connection = nullptr;

    CriticalSection dirtyParamLock;
    Array<AutomatableParameter*> dirtyParams;

    bool inSetParamValue = false;
};

//=============================================================================
static bool fillInParamInfo (Automap::ParamInfo& data, int min, int max, int step, const char* name, int trackNum = -1, bool folderPrefix = false)
{
    data.minInteger = min;
    data.maxInteger = max;
    data.stepInteger = step;

    if (trackNum != -1)
        String (String (folderPrefix ? "fldr" : String()) + String (trackNum) + ": " + name).copyToUTF8 (data.name, AUTOMAP_NAME_LENGTH);

    return true;
}

static bool fillInTrackParamInfo (Track* t, StringRef name, Automap::ParamInfo& data, int min, int max, int step, bool folderPrefix = false)
{
    if (auto at = dynamic_cast<AudioTrack*> (t))
        return fillInParamInfo (data, min, max, step, name, at->getAudioTrackNumber());

    if (auto ft = dynamic_cast<FolderTrack*> (t))
        return fillInParamInfo (data, min, max, step, name, ft->getFolderTrackNumber(), folderPrefix);

    return fillInParamInfo (data, min, max, step, name, -1);
}

//=============================================================================
class HostAutoMap : public AutoMap,
                    public Automap::Client,
                    public MultiTimer
{
public:
    HostAutoMap (NovationAutomap& n) : AutoMap (n)
    {
        AutomapConnectionStatus stat = ERROR_UNKNOWN;

        {
            CRASH_TRACER
            connection = Automap::Connect (this, &stat);
        }

        CRASH_TRACER

        if (stat == CONNECTION_OK && connection != nullptr)
        {
            paramCountChanged();

            connection->Initialize();
            connection->SetAutoFocus();

            startTimer (1, 50);
        }
        else
        {
            TRACKTION_LOG_ERROR("Automap connection failed");
        }
    }

    ~HostAutoMap()
    {
        CRASH_TRACER
        Automap::Disconnect (connection);
        stopTimer (1);
    }

    void GetMfr (char* mfr) const override                  { String ("Tracktion Software Corporation Inc.").copyToUTF8 (mfr, AUTOMAP_NAME_LENGTH); }
    void GetName (char* name) const override                { String ("Tracktion").copyToUTF8 (name, AUTOMAP_NAME_LENGTH); }
    void GetType (char* type) const override                { String ("Mixer").copyToUTF8 (type, AUTOMAP_NAME_LENGTH); }
    Automap::GUID GetGUID() const override                  { return Automap::GUID ("{850A73BE-CF55-4C4E-95CF-698F0D789280}"); }
    void GetSaveName (char* name) const override            { String ("Tracktion").copyToUTF8 (name, AUTOMAP_NAME_LENGTH); }
    void GetInstanceName (char* name) const override        { String ("Tracktion").copyToUTF8 (name, AUTOMAP_NAME_LENGTH); }
    void SaveInstanceName (const char*) override            {}
    int GetNumParams() const override                       { return params.size(); }

    bool GetParamInfo (int idx, Automap::ParamInfo& data) override
    {
        CRASH_TRACER

        if (auto p = params[idx])
        {
            auto at = dynamic_cast<AudioTrack*> (p->t);
            auto ft = dynamic_cast<FolderTrack*> (p->t);

            if (at == nullptr && ft == nullptr)
                return false;

            switch (p->type)
            {
                case Param::Vol:        return fillInTrackParamInfo (p->t, "Vol", data, 0, 255, 1, ft != nullptr);
                case Param::Pan:        return fillInTrackParamInfo (p->t, "Pan", data, 0, 255, 1, ft != nullptr);
                case Param::Mute:       return fillInTrackParamInfo (p->t, "Mute", data, 0, 1, 1, true);
                case Param::Solo:       return fillInTrackParamInfo (p->t, "Solo", data, 0, 1, 1, true);
                case Param::Arm:        return fillInTrackParamInfo (p->t, "Arm", data, 0, 1, 1);
                case Param::Vca:        return fillInTrackParamInfo (p->t, "VCA", data, 0, 255, 1, true);
                case Param::Expand:     return fillInTrackParamInfo (p->t, "Expand", data, 0, 1, 1, true);
                default:                jassertfalse; break;
            }
        }

        return false;
    }

    static float copyParamTextAndReturnNormalised (AutomatableParameter& param, char* valueTextOut)
    {
        param.getCurrentValueAsString().copyToUTF8 (valueTextOut, AUTOMAP_NAME_LENGTH);
        return param.getCurrentNormalisedValue();
    }

    float GetParamValue (int idx, char valueTextOut [AUTOMAP_NAME_LENGTH+1]) override
    {
        CRASH_TRACER

        if (auto p = params[idx])
        {
            auto at = dynamic_cast<AudioTrack*> (p->t);
            auto ft = dynamic_cast<FolderTrack*> (p->t);

            switch (p->type)
            {
            case Param::Vol:
                if (auto vp = getVolumeAndPanPlugin (at, ft))
                    if (auto ap = vp->getAutomatableParameter(0))
                        return copyParamTextAndReturnNormalised (*ap, valueTextOut);
                break;

            case Param::Pan:
                if (auto vp = getVolumeAndPanPlugin (at, ft))
                    if (auto ap = vp->getAutomatableParameter(1))
                        return copyParamTextAndReturnNormalised (*ap, valueTextOut);
                break;

            case Param::Mute:
                strcpy (valueTextOut, p->t->isMuted (false) ? "On" : "Off");
                return p->t->isMuted (false) ? 1.0f : 0.0f;

            case Param::Solo:
                strcpy (valueTextOut, p->t->isSolo (false) ? "On" : "Off");
                return p->t->isSolo (false) ? 1.0f : 0.0f;

            case Param::Arm:
                if (at != nullptr)
                {
                    if (auto in = at->edit.getEditInputDevices().getInputInstance (*at, 0))
                    {
                        const bool armed = in->isRecordingEnabled();
                        strcpy (valueTextOut, armed ? "On" : "Off");
                        return armed ? 1.0f : 0.0f;
                    }
                }
                break;

            case Param::Vca:
                if (ft != nullptr)
                    if (auto vca = ft->getVCAPlugin())
                        if (auto ap = vca->getAutomatableParameter(0))
                            return copyParamTextAndReturnNormalised (*ap, valueTextOut);
                break;

            case Param::Expand:
                if (ft != nullptr)
                {
                    if (novationAutomap.externalControllerManager.isFolderTrackOpen)
                    {
                        bool open = novationAutomap.externalControllerManager.isFolderTrackOpen (*ft);
                        strcpy (valueTextOut, open ? "Open" : "Closed");
                        return open ? 1.0f : 0.0f;
                    }
                }

                break;

            default:
                jassertfalse;
                break;
            }
        }

        return 0.0f;
    }

    void SetParamValue (int idx, float value) override
    {
        CRASH_TRACER
        ScopedValueSetter<bool> sv (inSetParamValue, true, false);

        if (auto p = params[idx])
        {
            auto at = dynamic_cast<AudioTrack*> (p->t);
            auto ft = dynamic_cast<FolderTrack*> (p->t);

            switch (p->type)
            {
                case Param::Vol:
                    if (auto vp = getVolumeAndPanPlugin (at, ft))
                        if (auto ap = vp->getAutomatableParameter(0))
                            ap->midiControllerMoved(value);

                    break;

                case Param::Pan:
                    if (auto vp = getVolumeAndPanPlugin (at, ft))
                        if (auto ap = vp->getAutomatableParameter(1))
                            ap->midiControllerMoved (value);

                    break;

                case Param::Mute:
                    p->t->setMute (value == 1.0f);
                    break;

                case Param::Solo:
                    p->t->setSolo (value == 1.0f);
                    break;

                case Param::Arm:
                    if (at != nullptr)
                        if (auto in = at->edit.getEditInputDevices().getInputInstance (*at, 0))
                            in->setRecordingEnabled (value == 1.0f);

                    break;

                case Param::Vca:
                    if (ft != nullptr)
                        if (auto vca = ft->getVCAPlugin())
                            if (auto ap = vca->getAutomatableParameter(0))
                                ap->midiControllerMoved (value);

                    break;

                case Param::Expand:
                    if (ft != nullptr)
                        if (novationAutomap.externalControllerManager.setFolderTrackOpen)
                            novationAutomap.externalControllerManager.setFolderTrackOpen (*ft, value == 1.0f);

                    break;

                default:
                    jassertfalse;
                    break;
            }
        }
    }

    int GetNumPresets() const override
    {
        return 0;
    }

    int GetCurrentPreset (char name[AUTOMAP_NAME_LENGTH + 1]) const override
    {
        (void) name;
        return -1;
    }

    void SetCurrentPreset (int) override
    {
    }

    bool SupportsTransport (AutomapTransport) const override
    {
        return true;
    }

    void OnTransport (AutomapTransport type, float param) override
    {
        CRASH_TRACER

        switch (type)
        {
            case TRANSPORT_PLAY:    if (param == 1.0f) novationAutomap.userPressedPlay();   break;
            case TRANSPORT_STOP:    if (param == 1.0f) novationAutomap.userPressedStop();   break;
            case TRANSPORT_RECORD:  if (param == 1.0f) novationAutomap.userPressedRecord(); break;

            case TRANSPORT_LOOP:
                if (auto ed = getEdit())
                    ed->getTransport().looping = param == 1.0f;

                break;

            case TRANSPORT_FORWARD: novationAutomap.userChangedFastForwardButton (param == 1.0f); break;
            case TRANSPORT_REWIND:  novationAutomap.userChangedRewindButton (param == 1.0f);      break;

            case TRANSPORT_TEMPO:
                if (auto ed = getEdit())
                    ed->tempoSequence.getTempoAt (ed->getTransport().position).setBpm ((double) param);

                break;

            default:
                jassertfalse;
                break;
        }
    }

    Automap::Connection* GetConnection() override
    {
        return connection;
    }

    Automap::Editor* GetEditor() override
    {
        return {};
    }

    void paramChanged (AutomatableParameter* param) override
    {
        for (int i = 0; i < params.size(); ++i)
        {
            if (params[i]->param == param)
            {
                connection->SendEvent (i, inSetParamValue);
                break;
            }
        }
    }

    void playChanged (bool p) override
    {
        connection->SendTransport (TRANSPORT_PLAY, p ? 1.0f : 0.0f);
    }

    void recordChanged (bool r) override
    {
        connection->SendTransport (TRANSPORT_RECORD, r ? 1.0f : 0.0f);
    }

    void timeChanged() override
    {
        CRASH_TRACER

        if (auto ed = getEdit())
        {
            double bpm = ed->tempoSequence.getTempoAt (ed->getTransport().position).getBpm();

            if (std::abs (bpm - lastTempo) > 0.25)
            {
                lastTempo = bpm;
                connection->SendTransport (TRANSPORT_TEMPO, (float) bpm);
            }
        }
    }

    void faderBankChanged (const StringArray&) override
    {
        CRASH_TRACER

        if (auto ed = getEdit())
        {
            juce::Array<EditItemID> newChannels;

            for (auto t : getAllTracks (*ed))
                if (t->isAudioTrack() || t->isFolderTrack())
                    newChannels.add (t->itemID);

            if (newChannels != currentChannels)
            {
                currentChannels = std::move (newChannels);
                startTimer (2, 120);
            }
        }
    }

    void paramCountChanged()
    {
        CRASH_TRACER

        params.clear();

        if (auto ed = getEdit())
        {
            for (auto t : getAllTracks (*ed))
            {
                if (auto at = dynamic_cast<AudioTrack*> (t))
                {
                    auto vp = at->getVolumePlugin();
                    params.add (new Param (at, Param::Vol,  vp != nullptr ? vp->getAutomatableParameter (0).get() : nullptr));
                    params.add (new Param (at, Param::Pan,  vp != nullptr ? vp->getAutomatableParameter (1).get() : nullptr));
                    params.add (new Param (at, Param::Mute, nullptr));
                    params.add (new Param (at, Param::Solo, nullptr));
                    params.add (new Param (at, Param::Arm,  nullptr));
                }

                if (auto ft = dynamic_cast<FolderTrack*> (t))
                {
                    if (auto vca = ft->getVCAPlugin())
                        params.add (new Param (ft, Param::Vca, vca->getAutomatableParameter (0).get()));

                    if (auto vp = ft->getVolumePlugin())
                    {
                        params.add (new Param (ft, Param::Vol, vp->getAutomatableParameter (0).get()));
                        params.add (new Param (ft, Param::Pan, vp->getAutomatableParameter (1).get()));
                    }

                    params.add (new Param (ft, Param::Expand, nullptr));
                    params.add (new Param (ft, Param::Mute,   nullptr));
                    params.add (new Param (ft, Param::Solo,   nullptr));
                }
            }
        }
    }

    void timerCallback (int timerId) override
    {
        CRASH_TRACER

        if (timerId == 1)
        {
            connection->Update();
        }
        else if (timerId == 2)
        {
            paramCountChanged();
            connection->ParamCountChanged();
            stopTimer (2);
        }
    }

    void updateSoloAndMute (int channelNum, Track::MuteAndSoloLightState, bool) override
    {
        CRASH_TRACER

        if (auto t = novationAutomap.externalControllerManager.getChannelTrack (channelNum))
        {
            for (int i = 0; i < params.size(); ++i)
            {
                auto* param = params.getUnchecked(i);

                if (param->type == Param::Mute && t == param->t)
                {
                    connection->SendEvent (i, inSetParamValue);
                    break;
                }

                if (param->type == Param::Solo && t == param->t)
                {
                    connection->SendEvent (i, inSetParamValue);
                    break;
                }
            }
        }
    }

    void armChanged (int ch) override
    {
        CRASH_TRACER

        if (auto t = novationAutomap.externalControllerManager.getChannelTrack (ch))
        {
            for (int i = 0; i < params.size(); ++i)
            {
                auto* param = params.getUnchecked(i);

                if (param->type == Param::Arm && t == param->t)
                {
                    connection->SendEvent (i, inSetParamValue);
                    break;
                }
            }
        }
    }

    void select() override
    {
        CRASH_TRACER
        connection->SetAutoFocus();
    }

    Selectable* getSelectableObject() override
    {
        return getEdit();
    }

    Edit* getEdit() const               { return novationAutomap.getEdit(); }

private:
    struct Param
    {
        enum Type { Vol, Pan, Mute, Solo, Vca, Expand, Arm };

        Param (Track* t_, Type type_, AutomatableParameter* param_)
            : t (t_), type (type_), param (param_)
        {
        }

        Track* t;
        Type type;
        AutomatableParameter* param;
    };

    static VolumeAndPanPlugin* getVolumeAndPanPlugin (AudioTrack* at, FolderTrack* ft)
    {
        return at != nullptr ? at->getVolumePlugin()
                             : ft != nullptr ? ft->getVolumePlugin() : nullptr;
    }

    juce::OwnedArray<Param> params;
    double lastTempo = 0;
    juce::Array<EditItemID> currentChannels;
};

//=============================================================================
class PluginAutoMap : public AutoMap,
                      public Automap::Client,
                      private Timer
{
public:
    PluginAutoMap (NovationAutomap& na, Plugin& p)
       : AutoMap (na), plugin (&p)
    {
        CRASH_TRACER

        AutomapConnectionStatus stat;
        connection = Automap::Connect (this, &stat);

        if (stat == CONNECTION_OK)
        {
            connection->Initialize();
            connection->SetAutoFocus();

            startTimer (50);
        }
        else
        {
            TRACKTION_LOG_ERROR("Automap connection failed");
        }
    }

    ~PluginAutoMap()
    {
        CRASH_TRACER

        Automap::Disconnect (connection);
        stopTimer();
    }

    void timerCallback() override
    {
        CRASH_TRACER
        connection->Update();
    }

    void GetName (char* name) const override        { plugin->getName().copyToUTF8 (name, AUTOMAP_NAME_LENGTH); }
    void GetMfr (char* mfr) const override          { plugin->getVendor().copyToUTF8 (mfr, AUTOMAP_NAME_LENGTH); }

    void GetType (char* type) const override
    {
        if (auto ep = dynamic_cast<ExternalPlugin*> (plugin.get()))
            String (ep->isSynth() ? "VSTi" : "VST").copyToUTF8 (type, AUTOMAP_NAME_LENGTH);
        else
            String ("Native").copyToUTF8 (type, AUTOMAP_NAME_LENGTH);
    }

    Automap::GUID GetGUID() const override
    {
        String guid = novationAutomap.guids[getPluginString()];

        if (guid.isNotEmpty())
            return Automap::GUID (guid.toUTF8());

        char buffer[128];
        Automap::GUID g = Automap::GUID::Create();
        g.ToString (buffer);
        guid = buffer;

        novationAutomap.guids.set (getPluginString(), guid);

        plugin->engine.getPropertyStorage().setProperty (SettingID::automapGuids1, novationAutomap.guids.getAllKeys().joinIntoString (";"));
        plugin->engine.getPropertyStorage().setProperty (SettingID::automapGuids2, novationAutomap.guids.getAllValues().joinIntoString (";"));

        return g;
    }

    String getPluginString() const
    {
        if (auto ep = dynamic_cast<ExternalPlugin*> (plugin.get()))
            return ep->getFile().getFileNameWithoutExtension();

        auto* f = plugin.get();
        return String (typeid (*f).name());
    }

    void GetSaveName (char* name) const override
    {
        if (auto t = plugin->getOwnerTrack())
            String (plugin->getName() + " [" + t->getName() + "]").copyToUTF8 (name, AUTOMAP_NAME_LENGTH);
        else
            plugin->getName().copyToUTF8 (name, AUTOMAP_NAME_LENGTH);
    }

    void GetInstanceName (char* name) const override
    {
        if (auto t = plugin->getOwnerTrack())
            String (plugin->getName() + " [" + t->getName() + "]").copyToUTF8 (name, AUTOMAP_NAME_LENGTH);
        else
            plugin->getName().copyToUTF8 (name, AUTOMAP_NAME_LENGTH);
    }

    void SaveInstanceName (const char*) override  {}
    int GetNumParams() const override             { return plugin->getNumAutomatableParameters(); }

    bool GetParamInfo (int idx, Automap::ParamInfo& data) override
    {
        CRASH_TRACER

        if (auto p = plugin->getAutomatableParameter (idx))
        {
            data.minInteger = 0;
            data.maxInteger = p->isDiscrete() ? p->getNumberOfStates() : 256;
            data.stepInteger = 1;
            p->getParameterName().copyToUTF8 (data.name, AUTOMAP_NAME_LENGTH);

            return true;
        }

        return false;
    }

    float GetParamValue (int idx, char valueTextOut[AUTOMAP_NAME_LENGTH + 1]) override
    {
        if (auto p = plugin->getAutomatableParameter (idx))
        {
            p->getCurrentValueAsString().copyToUTF8 (valueTextOut, AUTOMAP_NAME_LENGTH);
            return p->getCurrentNormalisedValue();
        }

        return 0.0f;
    }

    void SetParamValue (int idx, float value) override
    {
        CRASH_TRACER

        ScopedValueSetter<bool> sv (inSetParamValue, true, false);

        if (auto p = plugin->getAutomatableParameter (idx))
            p->midiControllerMoved (value);
    }

    int GetNumPresets() const override
    {
        if (auto ep = dynamic_cast<ExternalPlugin*> (plugin.get()))
            return ep->getNumPrograms();

        return 0;
    }

    int GetCurrentPreset (char name[AUTOMAP_NAME_LENGTH + 1]) const override
    {
        CRASH_TRACER

        if (auto ep = dynamic_cast<ExternalPlugin*> (plugin.get()))
        {
            if (name != nullptr)
                ep->getProgramName (ep->getCurrentProgram()).copyToUTF8 (name, AUTOMAP_NAME_LENGTH);

            return ep->getCurrentProgram();
        }

        return -1;
    }

    void SetCurrentPreset (int n) override
    {
        CRASH_TRACER

        if (auto ep = dynamic_cast<ExternalPlugin*> (plugin.get()))
            ep->setCurrentProgram (n, true);
    }

    bool SupportsTransport (AutomapTransport) const override    { return true; }
    void OnTransport (AutomapTransport, float) override         {}
    Automap::Connection* GetConnection() override               { return connection; }
    Automap::Editor* GetEditor() override                       { return {}; }

    void paramChanged (AutomatableParameter* param) override
    {
        if (plugin != nullptr)
        {
            for (int i = 0; i < plugin->getNumAutomatableParameters(); ++i)
            {
                if (plugin->getAutomatableParameter (i).get() == param)
                {
                    connection->SendEvent (i, inSetParamValue);
                    break;
                }
            }
        }
        else
        {
            jassertfalse;
        }
    }

    void playChanged (bool p) override
    {
        connection->SendTransport (TRANSPORT_PLAY, p ? 1.0f : 0.0f);
    }

    void recordChanged (bool r) override
    {
        connection->SendTransport (TRANSPORT_RECORD, r ? 1.0f : 0.0f);
    }

    void timeChanged() override
    {
        CRASH_TRACER

        if (plugin != nullptr)
        {
            if (auto edit = novationAutomap.getEdit())
            {
                auto bpm = edit->tempoSequence.getTempoAt (edit->getTransport().position).getBpm();

                if (std::abs (bpm - lastTempo) > 0.25)
                {
                    lastTempo = bpm;
                    connection->SendTransport (TRANSPORT_TEMPO, (float) bpm);
                }

                return;
            }
        }

        jassertfalse;
    }

    void updateSoloAndMute (int, Track::MuteAndSoloLightState, bool) override {}
    void faderBankChanged (const StringArray&) override     {}
    void armChanged (int) override                          {}
    void select() override                                  { connection->SetAutoFocus(); }
    Selectable* getSelectableObject() override              { return plugin.get(); }

private:
    const Plugin::Ptr plugin;
    double lastTempo = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginAutoMap)
};

//=============================================================================
NovationAutomap::NovationAutomap (ExternalControllerManager& ecm)  : ControlSurface (ecm)
{
    CRASH_TRACER

    deviceDescription               = "Novation Automap Universal";
    needsMidiChannel                = false;
    needsMidiBackChannel            = false;
    wantsClock                      = false;
    deletable                       = false;
    numberOfFaderChannels           = 128;
    numCharactersForTrackNames      = 32;
    numParameterControls            = 1024;
    numCharactersForParameterLabels = 128;
    numMarkers                      = 0;
    numCharactersForMarkerLabels    = 0;
    numAuxes                        = 0;
    numCharactersForAuxLabels       = 0;
    wantsAuxBanks                   = false;
    followsTrackSelection           = false;

    mapPlugin = engine.getPropertyStorage().getProperty (SettingID::automapVst, false);
    mapNative = engine.getPropertyStorage().getProperty (SettingID::automapNative, false);

    StringArray keys, values;

    keys.addTokens (engine.getPropertyStorage().getProperty (SettingID::automapGuids1).toString(), ";", {});
    values.addTokens (engine.getPropertyStorage().getProperty (SettingID::automapGuids2).toString(), ";", {});

    for (int i = 0; i < jmin (keys.size(), values.size()); ++i)
        guids.set (keys[i], values[i]);
}

NovationAutomap::~NovationAutomap()
{
    notifyListenersOfDeletion();
    currentEditChanged (nullptr);

    hostAutomap = nullptr;
    pluginAutomap.clear();

    engine.getPropertyStorage().setProperty (SettingID::automapVst, mapPlugin);
    engine.getPropertyStorage().setProperty (SettingID::automapNative, mapNative);
    engine.getPropertyStorage().setProperty (SettingID::automapGuids1, guids.getAllKeys().joinIntoString (";"));
    engine.getPropertyStorage().setProperty (SettingID::automapGuids2, guids.getAllValues().joinIntoString (";"));
}

void NovationAutomap::currentEditChanged (Edit* e)
{
    CRASH_TRACER
    auto newSelectionManager = externalControllerManager.getSelectionManager();

    if (getEdit() != e || selectionManager != newSelectionManager)
    {
        if (selectionManager != nullptr)
            selectionManager->removeChangeListener (this);

        ControlSurface::currentEditChanged (e);

        if ((selectionManager = newSelectionManager) != nullptr)
            if (e != nullptr)
                selectionManager->addChangeListener (this);
    }

    if (hostAutomap != nullptr)
    {
        hostAutomap = nullptr;
        pluginAutomap.clear();
    }

    if (auto edit = getEdit())
    {
        if (enabled)
        {
            createAllPluginAutomaps();
            hostAutomap = std::make_unique<HostAutoMap> (*this);
            load (*edit);
        }
    }
}

void NovationAutomap::initialiseDevice (bool connect)
{
    CRASH_TRACER
    enabled = connect;

    if (enabled && getEdit() != nullptr && hostAutomap == nullptr)
    {
        createAllPluginAutomaps();
        hostAutomap = std::make_unique<HostAutoMap> (*this);
        load (*getEdit());
    }
    else if (! enabled && hostAutomap != nullptr)
    {
        hostAutomap = nullptr;
        pluginAutomap.clear();
    }
}

void NovationAutomap::paramChanged (AutomatableParameter* param)
{
    if (hostAutomap != nullptr)
        hostAutomap->paramChangedInt (param);

    for (auto map : pluginAutomap)
        if (map->getSelectableObject() == param->getPlugin())
            map->paramChangedInt (param);
}

void NovationAutomap::createAllPluginAutomaps()
{
    if (auto ed = getEdit())
    {
        for (auto p : getAllPlugins (*ed, true))
        {
            bool isExternal = dynamic_cast<ExternalPlugin*> (p) != nullptr;

            bool isNative = ! isExternal
                                && dynamic_cast<LevelMeterPlugin*> (p) == nullptr
                                && dynamic_cast<VolumeAndPanPlugin*> (p) == nullptr
                                && dynamic_cast<VCAPlugin*> (p) == nullptr;

            if (((mapPlugin && isExternal) || (mapNative && isNative)) && p->getNumAutomatableParameters() > 0)
                pluginAutomap.add (new PluginAutoMap (*this, *p));
        }
    }
}

void NovationAutomap::changeListenerCallback (ChangeBroadcaster*)
{
    if (enabled)
    {
        CRASH_TRACER

        if (auto sm = externalControllerManager.getSelectionManager())
        {
            auto s = sm->getSelectedObject (0);

            for (auto p : pluginAutomap)
            {
                if (p->getSelectableObject() == s)
                {
                    pluginSelected = true;
                    p->select();
                    return;
                }
            }
        }

        if (hostAutomap && pluginSelected)
        {
            pluginSelected = false;
            hostAutomap->select();
        }
    }
}

void NovationAutomap::removePlugin (Plugin* p)
{
    for (int i = pluginAutomap.size(); --i >= 0;)
        if (pluginAutomap.getUnchecked(i)->getSelectableObject() == p)
            pluginAutomap.remove(i);
}

void NovationAutomap::pluginChanged (Plugin* p)
{
    if (! enabled)
        return;

    CRASH_TRACER

    if (&p->edit != getEdit())
    {
        removePlugin (p);
        return;
    }

    bool isExternal = dynamic_cast<ExternalPlugin*> (p) != nullptr;

    bool isNative = ! isExternal
                        && dynamic_cast<LevelMeterPlugin*> (p) == nullptr
                        && dynamic_cast<VolumeAndPanPlugin*> (p) == nullptr
                        && dynamic_cast<VCAPlugin*> (p) == nullptr;

    if (((mapPlugin && isExternal) || (mapNative && isNative)) && p->getNumAutomatableParameters() > 0)
    {
        removePlugin (p);
        pluginAutomap.add (new PluginAutoMap (*this, *p));
    }
}

static Automap::GUID getGUID (Edit& ed)
{
    auto guidString = ed.getAutomapState() [IDs::guid].toString();

    if (guidString.length() >= 32 + 6)
        return Automap::GUID (guidString.toUTF8());

    return Automap::GUID::Create();
}

void NovationAutomap::save (Edit& edit)
{
    CRASH_TRACER

    auto state = edit.getAutomapState();

    if (enabled && hostAutomap != nullptr && hostAutomap->getSelectableObject() == &edit)
    {
        auto instanceGuid = getGUID (edit);

        char buf[128] = { 0 };
        instanceGuid.ToString(buf);

        state.setProperty (IDs::guid, String (buf), nullptr);

        hostAutomap->getConnection()->SaveInstanceGUID (instanceGuid);
    }
    else
    {
        state.removeAllProperties (nullptr);
        state.removeAllChildren (nullptr);
    }
}

void NovationAutomap::load (Edit& edit)
{
    CRASH_TRACER

    if (enabled && hostAutomap != nullptr && hostAutomap->getSelectableObject() == &edit)
        hostAutomap->getConnection()->LoadInstanceGUID (getGUID (edit));
}

void NovationAutomap::shutDownDevice()                          {}
void NovationAutomap::updateMiscFeatures()                      {}
void NovationAutomap::acceptMidiMessage (const MidiMessage&)    {}
void NovationAutomap::moveFader (int, float)                    {}
void NovationAutomap::moveMasterLevelFader (float, float)       {}
void NovationAutomap::movePanPot (int, float)                   {}
void NovationAutomap::moveAux (int, const char*, float)         {}
void NovationAutomap::clearAux (int)                            {}
void NovationAutomap::soloCountChanged (bool)                   {}

void NovationAutomap::updateSoloAndMute (int channelNum, Track::MuteAndSoloLightState state, bool isBright)
{
    if (hostAutomap != nullptr)
        hostAutomap->updateSoloAndMute (channelNum, state, isBright);

}
void NovationAutomap::playStateChanged (bool isPlaying)
{
    if (hostAutomap != nullptr)
        hostAutomap->playChanged(isPlaying);
}

void NovationAutomap::recordStateChanged (bool isRecording)
{
    if (hostAutomap != nullptr)
        hostAutomap->recordChanged(isRecording);
}

void NovationAutomap::automationReadModeChanged (bool)          {}
void NovationAutomap::automationWriteModeChanged (bool)         {}

void NovationAutomap::faderBankChanged (int, const StringArray& trackNames)
{
    if (hostAutomap != nullptr)
        hostAutomap->faderBankChanged (trackNames);
}

void NovationAutomap::channelLevelChanged (int, float)          {}
void NovationAutomap::trackSelectionChanged (int, bool)         {}

void NovationAutomap::trackRecordEnabled (int channel, bool)
{
    if (hostAutomap != nullptr)
        hostAutomap->armChanged (channel);
}

void NovationAutomap::masterLevelsChanged (float, float)        {}

void NovationAutomap::timecodeChanged (int, int, int, int, bool, bool)
{
    if (hostAutomap != nullptr)
        hostAutomap->timeChanged();
}

void NovationAutomap::clickOnOffChanged (bool)                  {}
void NovationAutomap::snapOnOffChanged (bool)                   {}
void NovationAutomap::loopOnOffChanged (bool)                   {}
void NovationAutomap::slaveOnOffChanged (bool)                  {}
void NovationAutomap::punchOnOffChanged (bool)                  {}
void NovationAutomap::undoStatusChanged (bool, bool)            {}
void NovationAutomap::parameterChanged (int, const ParameterSetting&)   {}
void NovationAutomap::clearParameter (int)                      {}
void NovationAutomap::markerChanged (int, const MarkerSetting&) {}
void NovationAutomap::clearMarker (int)                         {}
void NovationAutomap::auxBankChanged (int)                      {}
bool NovationAutomap::wantsMessage (const MidiMessage&)         { return false; }
bool NovationAutomap::eatsAllMessages()                         { return false; }
bool NovationAutomap::canSetEatsAllMessages()                   { return false; }
void NovationAutomap::setEatsAllMessages (bool)                 {}
bool NovationAutomap::canChangeSelectedPlugin()                 { return true; }
void NovationAutomap::currentSelectionChanged()                 {}
bool NovationAutomap::showingPluginParams()                     { return true; }
bool NovationAutomap::showingMarkers()                          { return false; }
bool NovationAutomap::showingTracks()                           { return true; }
void NovationAutomap::pluginBypass (bool)                       {}
bool NovationAutomap::isPluginSelected (Plugin*)                { return false; }

}

#endif
