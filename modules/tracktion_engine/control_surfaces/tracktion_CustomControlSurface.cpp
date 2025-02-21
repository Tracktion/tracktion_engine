/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

void CustomControlSurface::CustomControlSurfaceManager::registerSurface (CustomControlSurface* item)
{
    surfaces.addIfNotAlreadyThere (item);
}

void CustomControlSurface::CustomControlSurfaceManager::unregisterSurface (CustomControlSurface* item)
{
    surfaces.removeAllInstancesOf (item);
}

void CustomControlSurface::CustomControlSurfaceManager::saveAllSettings (Engine& e)
{
    juce::XmlElement root ("MIDICUSTOMCONTROLSURFACES");

    for (auto s : surfaces)
        root.addChildElement (s->createXml());

    e.getPropertyStorage().setXmlProperty (SettingID::customMidiControllers, root);
}

//==============================================================================
CustomControlSurface::CustomControlSurface (ExternalControllerManager& ecm,
                                            const juce::String& name,
                                            ExternalControllerManager::Protocol protocol_)
    : ControlSurface (ecm),
    protocol (protocol_)
{
    init();

    deviceDescription = name;
    manager->saveAllSettings (engine);

    loadFunctions();
}

CustomControlSurface::CustomControlSurface (ExternalControllerManager& ecm, const juce::XmlElement& xml)
   : ControlSurface (ecm)
{
    protocol = xml.getStringAttribute ("protocol") == "osc" ? ExternalControllerManager::osc :
                                                              ExternalControllerManager::midi;

    init();

    deviceDescription = xml.getStringAttribute ("name");

    loadFromXml (xml);

    loadFunctions();
}

void CustomControlSurface::init()
{
    manager->registerSurface (this);

    if (protocol == ExternalControllerManager::osc)
    {
        needsMidiChannel            = false;
        needsMidiBackChannel        = false;
        needsOSCSocket              = true;
    }
    else
    {
        needsMidiChannel            = true;
        needsMidiBackChannel        = true;
        needsOSCSocket              = false;
    }
    numberOfFaderChannels           = 8;
    numberOfTrackPads               = 8;
    numCharactersForTrackNames      = needsOSCSocket ? 12 : 0;
    numParameterControls            = 18;
    numCharactersForParameterLabels = needsOSCSocket ? 12 : 0;
    wantsDummyParams                = false;
    deletable                       = true;
    listeningOnRow                  = -1;
    pluginMoveMode                  = true;
    eatsAllMidi                     = false;
    wantsClock                      = false;
    allowBankingOffEnd              = true;
}

CustomControlSurface::~CustomControlSurface()
{
    manager->unregisterSurface (this);
    clearCommandGroups();
}

void CustomControlSurface::clearCommandGroups()
{
    for (auto& s : commandGroups)
        delete s.second;

    commandGroups.clear();
}

bool CustomControlSurface::isPendingEventAssignable()
{
    if (auto ed = getEdit())
        return ed->getParameterChangeHandler().isActionFunctionPending();

    return false;
}

void CustomControlSurface::updateOrCreateMappingForID (int id, juce::String addr,
                                                       int channel, int noteNum, int controllerID)
{
    Mapping* mappingToUpdate = nullptr;

    // first try to find existing mapping
    for (auto mapping : mappings)
    {
        if (mapping->function == id)
        {
            mappingToUpdate = mapping;
            break;
        }
    }

    if (mappingToUpdate == nullptr)
        mappingToUpdate = mappings.add (new Mapping());

    mappingToUpdate->id         = controllerID;
    mappingToUpdate->addr       = addr;
    mappingToUpdate->note       = noteNum;
    mappingToUpdate->channel    = channel;
    mappingToUpdate->function   = id;

    sendChangeMessage();
}

void CustomControlSurface::addMappingSetsForID (ActionID id, juce::Array<MappingSet>& mappingSet)
{
    if (owner == nullptr)
        return;

    auto selectionColour = owner->getSelectionColour();

    for (int i = mappings.size(); --i >= 0;)
    {
        auto mapping = mappings.getUnchecked (i);

        if (mapping->function == id)
        {
            MappingSet matchedMapping;

            matchedMapping.id               = id;
            matchedMapping.controllerID     = mapping->id;
            matchedMapping.addr             = mapping->addr;
            matchedMapping.note             = mapping->note;
            matchedMapping.channel          = mapping->channel;
            matchedMapping.colour           = selectionColour;
            matchedMapping.surfaceName      = owner->getName();

            mappingSet.add (matchedMapping);
        }
    }
}

juce::String CustomControlSurface::getNameForActionID (ExternalControllerManager& ecm, ActionID id)
{
    CRASH_TRACER

    for (auto ec : ecm.getControllers())
        if (auto ccs = ec->getControlSurfaceIfType<CustomControlSurface>())
            return ccs->getFunctionName (id);

    return {};
}

juce::Array<ControlSurface*> CustomControlSurface::getCustomSurfaces (ExternalControllerManager& ecm)
{
    juce::Array<ControlSurface*> surfaces;

    if (auto n = ecm.engine.getPropertyStorage().getXmlProperty (SettingID::customMidiControllers))
    {
        for (auto controllerXml : n->getChildIterator())
        {
            if (controllerXml->hasTagName ("MIDICUSTOMCONTROLSURFACE"))
                surfaces.add (new CustomControlSurface (ecm, *controllerXml));
            else if (auto ccs = ecm.engine.getEngineBehaviour().getCustomControlSurfaceForXML (ecm, *controllerXml))
                surfaces.add (ccs);
        }
    }

    return surfaces;
}

juce::Array<CustomControlSurface::MappingSet> CustomControlSurface::getMappingSetsForID (ExternalControllerManager& ecm, ActionID id)
{
    juce::Array<MappingSet> mappingSet;

    for (auto ec : ecm.getControllers())
        if (ec->isEnabled())
            if (auto ccs = ec->getControlSurfaceIfType<CustomControlSurface>())
                ccs->addMappingSetsForID (id, mappingSet);

    return mappingSet;
}

int CustomControlSurface::getControllerNumberFromId (int id) noexcept
{
    return id == 0x40000 ? -1 : (id - 0x10000);
}

juce::XmlElement* CustomControlSurface::createXml()
{
    auto element = new juce::XmlElement ("MIDICUSTOMCONTROLSURFACE");
    element->setAttribute ("name", deviceDescription);
    element->setAttribute ("protocol", protocol == ExternalControllerManager::osc ? "osc" : "midi");
    element->setAttribute ("eatsMidi", eatsAllMidi);
    element->setAttribute ("channels", numberOfFaderChannels);
    element->setAttribute ("parameters", numParameterControls);
    element->setAttribute ("pickUpMode", pickUpMode);
    element->setAttribute ("followsSelection", followsTrackSelection);

    for (auto m : mappings)
    {
        auto mapping = element->createNewChildElement ("MAPPING");
        mapping->setAttribute ("id",       m->id);
        mapping->setAttribute ("addr",     m->addr);
        mapping->setAttribute ("channel",  m->channel);
        mapping->setAttribute ("function", m->function);
        mapping->setAttribute ("note",     m->note);
    }

    return element;
}

bool CustomControlSurface::loadFromXml (const juce::XmlElement& xml)
{
    eatsAllMidi                 = xml.getBoolAttribute ("eatsMidi", false);
    numberOfFaderChannels       = xml.getIntAttribute ("channels", 8);
    numParameterControls        = xml.getIntAttribute ("parameters", 18);
    pickUpMode                  = xml.getBoolAttribute ("pickUpMode", false);
    followsTrackSelection       = xml.getBoolAttribute ("followsSelection", false);

    mappings.clear();

    for (auto node : xml.getChildWithTagNameIterator ("MAPPING"))
    {
        auto mapping = mappings.add (new Mapping());
        mapping->id       = node->getIntAttribute ("id");
        mapping->addr     = node->getStringAttribute ("addr");
        mapping->channel  = node->getIntAttribute ("channel");
        mapping->function = node->getIntAttribute ("function");
        mapping->note     = node->getIntAttribute ("note", -1);
    }

    return true;
}

void CustomControlSurface::importSettings (const juce::File& file)
{
    importSettings (file.loadFileAsString());
}

void CustomControlSurface::importSettings (const juce::String& xmlText)
{
    bool ok = false;
    mappings.clearQuick (true);

    if (auto xml = juce::parseXML (xmlText))
    {
        loadFromXml (*xml);
        loadFunctions();
        ok = true;
    }

    manager->saveAllSettings (engine);

    if (! ok)
        engine.getUIBehaviour().showWarningAlert (TRANS("Import"), TRANS("Import failed"));
}

void CustomControlSurface::exportSettings (const juce::File& file)
{
    bool ok = false;
    std::unique_ptr<juce::XmlElement> xml (createXml());

    if (xml != nullptr)
    {
        file.deleteFile();
        file.create();
        file.appendText (xml->toString());

        if (file.getSize() > 10)
            ok = true;
    }

    if (! ok)
        engine.getUIBehaviour().showWarningAlert (TRANS("Export"), TRANS("Export failed"));
}

void CustomControlSurface::initialiseDevice (bool online_)
{
    online = online_;
    recreateOSCSockets();
}

void CustomControlSurface::shutDownDevice()
{
    online = false;
    recreateOSCSockets();
}

void CustomControlSurface::updateOSCSettings (int in, int out, juce::String addr)
{
    oscInputPort = in;
    oscOutputPort = out;
    oscOutputAddr = addr;

    recreateOSCSockets();
}

void CustomControlSurface::recreateOSCSockets()
{
    oscSender.reset();
    oscReceiver.reset();

    if (online && oscInputPort > 0)
    {
        oscReceiver = std::make_unique<juce::OSCReceiver>();
        if (! oscReceiver->connect (oscInputPort))
        {
            oscReceiver.reset();
            engine.getUIBehaviour().showInfoMessage (TRANS("Failed to open OSC input port"));
        }
        else
        {
            oscReceiver->addListener (this);
        }
    }

    if (online && oscOutputPort > 0 && oscOutputAddr.isNotEmpty())
    {
        oscSender = std::make_unique<juce::OSCSender>();
        if (! oscSender->connect (oscOutputAddr, oscOutputPort))
        {
            oscSender.reset();
            engine.getUIBehaviour().showInfoMessage (TRANS("Failed to open OSC output port"));
        }
    }
}

void CustomControlSurface::saveAllSettings (Engine& e)
{
    juce::SharedResourcePointer<CustomControlSurfaceManager> manager;
    manager->saveAllSettings (e);
}

void CustomControlSurface::updateMiscFeatures()
{
}

bool CustomControlSurface::wantsMessage (int, const juce::MidiMessage& m)
{
    // in T5 there will always be a current list box so we don't wan't to eat all the time
    if (eatsAllMidi || listeningOnRow != -1 || ((m.isNoteOn() || m.isController())
                                                && isPendingEventAssignable()))
        return true;

    // only eat messages that are mapped, audio engine gets the rest
    if (m.isController() || m.isNoteOnOrOff())
    {
        int note    = -1;
        int id      = 0;
        int channel = 0;

        if (m.isController())
        {
            int val;
            rpnParser.parseControllerMessage (m, id, channel, val);
        }
        else
        {
            note    = m.getNoteNumber();
            channel = m.getChannel();
        }

        if (getEdit() != nullptr)
        {
            for (auto mapping : mappings)
            {
                if (((id == mapping->id && id != 0) || (note == mapping->note && note != -1))
                     && channel == mapping->channel
                     && mapping->function)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

void CustomControlSurface::oscMessageReceived (const juce::OSCMessage& m)
{
    packetsIn++;

    float val = 1.0f;

    if (m.size() >= 1)
    {
        auto arg = m[0];

        if (arg.isFloat32())
            val = arg.getFloat32();
        else if (arg.isInt32())
            val = float (arg.getInt32());
    }

    auto addr = m.getAddressPattern().toString();

    if (addr.endsWith ("/z"))
    {
        // Track control touches and releases
        addr = addr.dropLastCharacters (2);
        if (val != 0.0f)
        {
            oscControlTouched[addr] = true;
            oscControlTapsWhileTouched[addr] = 0;
        }
        else
        {
            oscControlTouched[addr] = false;
            oscControlTapsWhileTouched[addr] = 0;

            auto itr = oscLastValue.find (addr);
            if (itr != oscLastValue.end())
            {
                if (oscSender)
                {
                    try
                    {
                        juce::OSCMessage mo (addr);
                        mo.addFloat32 (itr->second);

                        if (oscSender->send (mo))
                            packetsOut++;
                    }
                    catch ([[maybe_unused]] juce::OSCException& err)
                    {
                        DBG("OSC Error: " + err.description);
                    }
                }

                oscLastValue.erase (itr);
            }
        }
    }
    else
    {
        // Track control values
        lastControllerAddr = addr;
        lastControllerValue = val;

        if (listeningOnRow >= 0)
            triggerAsyncUpdate();

        oscControlTapsWhileTouched[addr]++;
        oscActiveAddr = addr;

        if (getEdit() != nullptr)
        {
            for (auto* mapping : mappings)
            {
                if (lastControllerAddr == mapping->addr)
                {
                    for (auto* actionFunction : actionFunctionList)
                    {
                        if (actionFunction->id == mapping->function)
                        {
                            auto actionFunc = actionFunction->actionFunc;
                            (this->*actionFunc) (lastControllerValue, actionFunction->param);
                        }
                    }
                }
            }
        }
    }
}

void CustomControlSurface::oscBundleReceived (const juce::OSCBundle& b)
{
    for (auto e : b)
        if (e.isMessage())
            oscMessageReceived (e.getMessage());
}

bool CustomControlSurface::isTextAction (ActionID id)
{
    switch (id)
    {
        case nameTrackId:
        case numberTrackId:
        case volTextTrackId:
        case timecodeId:
        case masterVolumeTextId:
        case emptyTextId:
        case auxTextTrackId:
        case panTextTrackId:
        case paramNameTrackId:
        case paramTextTrackId:
        case selectedPluginNameId:
            return true;
        case playId:
        case stopId:
        case recordId:
        case homeId:
        case endId:
        case rewindId:
        case fastForwardId:
        case markInId:
        case markOutId:
        case automationReadId:
        case automationRecordId:
        case addMarkerId:
        case nextMarkerId:
        case previousMarkerId:
        case nudgeLeftId:
        case nudgeRightId:
        case abortId:
        case abortRestartId:
        case jogId:
        case jumpToMarkInId:
        case jumpToMarkOutId:
        case toggleBeatsSecondsModeId:
        case toggleLoopId:
        case togglePunchId:
        case toggleClickId:
        case toggleSnapId:
        case toggleSlaveId:
        case toggleEtoEId:
        case toggleScrollId:
        case toggleAllArmId:
        case masterVolumeId:
        case masterPanId:
        case quickParamId:
        case paramTrackId:
        case clearAllSoloId:
        case volTrackId:
        case panTrackId:
        case muteTrackId:
        case soloTrackId:
        case armTrackId:
        case selectTrackId:
        case auxTrackId:
        case zoomInId:
        case zoomOutId:
        case scrollTracksUpId:
        case scrollTracksDownId:
        case scrollTracksLeftId:
        case scrollTracksRightId:
        case zoomTracksInId:
        case zoomTracksOutId:
        case toggleSelectionModeId:
        case selectLeftId:
        case selectRightId:
        case selectUpId:
        case selectDownId:
        case selectClipInTrackId:
        case selectPluginInTrackId:
        case faderBankLeftId:
        case faderBankLeft1Id:
        case faderBankLeft4Id:
        case faderBankLeft8Id:
        case faderBankLeft16Id:
        case faderBankRightId:
        case faderBankRight1Id:
        case faderBankRight4Id:
        case faderBankRight8Id:
        case faderBankRight16Id:
        case paramBankLeftId:
        case paramBankLeft1Id:
        case paramBankLeft4Id:
        case paramBankLeft8Id:
        case paramBankLeft16Id:
        case paramBankLeft24Id:
        case paramBankRightId:
        case paramBankRight1Id:
        case paramBankRight4Id:
        case paramBankRight8Id:
        case paramBankRight16Id:
        case paramBankRight24Id:
        case userAction1Id:
        case userAction2Id:
        case userAction3Id:
        case userAction4Id:
        case userAction5Id:
        case userAction6Id:
        case userAction7Id:
        case userAction8Id:
        case userAction9Id:
        case userAction10Id:
        case userAction11Id:
        case userAction12Id:
        case userAction13Id:
        case userAction14Id:
        case userAction15Id:
        case userAction16Id:
        case userAction17Id:
        case userAction18Id:
        case userAction19Id:
        case userAction20Id:
        case clip1TrackId:
        case clip2TrackId:
        case clip3TrackId:
        case clip4TrackId:
        case clip5TrackId:
        case clip6TrackId:
        case clip7TrackId:
        case clip8TrackId:
        case stopClipsTrackId:
        case sceneId:
        case clipBankUp1Id:
        case clipBankUp4Id:
        case clipBankUp8Id:
        case clipBankDown1Id:
        case clipBankDown4Id:
        case clipBankDown8Id:
        case marker1Id:
        case marker2Id:
        case marker3Id:
        case marker4Id:
        case marker5Id:
        case marker6Id:
        case marker7Id:
        case marker8Id:
        case none:
        default:
            return false;
    }
}

void CustomControlSurface::timerCallback()
{
    stopTimer();

    auto i = listeningOnRow;
    while (i < mappings.size())
    {
        i++;
        if (auto map = mappings[i]; map && ! map->isControllerAssigned())
        {
            setLearntParam (false);
            listenToRow (i);
            break;
        }
    }
}

void CustomControlSurface::acceptMidiMessage (int, const juce::MidiMessage& m)
{
    if (! m.isController() && ! m.isNoteOn())
        return;

    if (m.isController())
    {
        int val;
        lastControllerNote = -1;
        rpnParser.parseControllerMessage (m, lastControllerID, lastControllerChannel, val);
        lastControllerValue = val / 127.0f;
    }

    if (m.isNoteOn())
    {
        lastControllerID = 0;
        lastControllerNote = m.getNoteNumber();
        lastControllerChannel = m.getChannel();
        lastControllerValue = 1.0f;
    }

    if (listeningOnRow >= 0)
    {
        triggerAsyncUpdate();
        startTimer (666);
    }

    if (auto ed = getEdit())
    {
        if (engine.getMidiLearnState().isActive()
             && ed->getParameterChangeHandler().isActionFunctionPending())
        {
            const MidiLearnState::ScopedChangeCaller changeCaller (engine.getMidiLearnState(), MidiLearnState::added);
            updateOrCreateMappingForID (ed->getParameterChangeHandler().getPendingActionFunctionId (true),
                                        lastControllerAddr, lastControllerChannel, lastControllerNote, lastControllerID);
        }
        else
        {
            for (auto mapping : mappings)
            {
                if (((lastControllerID    == mapping->id   && lastControllerID   != 0) ||
                    (lastControllerNote   == mapping->note && lastControllerNote != -1)) &&
                    lastControllerChannel == mapping->channel)
                {
                    for (auto actionFunction : actionFunctionList)
                    {
                        if (actionFunction->id == mapping->function)
                        {
                            auto actionFunc = actionFunction->actionFunc;
                            (this->*actionFunc) (lastControllerValue, actionFunction->param);
                        }
                    }
                }
            }
        }
    }
}

void CustomControlSurface::moveFader (int faderIndex, float v)
{
    ControlSurface::moveFader (faderIndex, v);

    sendCommandToControllerForActionID (volTrackId + faderIndex, v);

    auto dbText = juce::Decibels::toString (volumeFaderPositionToDB (v));
    sendCommandToControllerForActionID (volTextTrackId + faderIndex, dbText);
}

void CustomControlSurface::moveMasterLevelFader (float newSliderPos)
{
    ControlSurface::moveMasterLevelFader (newSliderPos);

    sendCommandToControllerForActionID (masterVolumeId, newSliderPos);

    auto dbText = juce::Decibels::toString (volumeFaderPositionToDB (newSliderPos));
    sendCommandToControllerForActionID (masterVolumeTextId, dbText);
}

void CustomControlSurface::moveMasterPanPot (float newPos)
{
    ControlSurface::moveMasterPanPot (newPos);

    sendCommandToControllerForActionID (masterPanId, (newPos + 1.0f) / 2.0f);
}

void CustomControlSurface::movePanPot (int faderIndex, float v)
{
    ControlSurface::movePanPot (faderIndex, v);
    sendCommandToControllerForActionID (panTrackId + faderIndex, (v * 0.5f) + 0.5f);

    juce::String panText;
    auto p = juce::roundToInt (v * 100);

    if (p == 0)
        panText = "C";
    else if (p < 0)
        panText = juce::String (-p) + "L";
    else
        panText = juce::String (p) + "R";

    sendCommandToControllerForActionID (panTextTrackId + faderIndex, panText);
}

void CustomControlSurface::moveAux (int faderIndex, int num, const char* bus, float v)
{
    ControlSurface::moveAux (faderIndex, num, bus, v);

    sendCommandToControllerForActionID (auxTrackId + faderIndex, v);

    auto dbText = juce::Decibels::toString (volumeFaderPositionToDB (v));
    sendCommandToControllerForActionID (auxTextTrackId + faderIndex, dbText);
}

void CustomControlSurface::updateSoloAndMute (int faderIndex, Track::MuteAndSoloLightState state, bool isBright)
{
    const bool muteLit      = (state & Track::muteLit) != 0;
    const bool muteFlashing = (state & Track::muteFlashing) != 0;
    const bool soloLit      = (state & Track::soloLit) != 0;
    const bool soloFlashing = (state & Track::soloFlashing) != 0;
    sendCommandToControllerForActionID (muteTrackId + faderIndex, muteLit || (isBright && muteFlashing) ? 1.0f : 0.0f);
    sendCommandToControllerForActionID (soloTrackId + faderIndex, soloLit || (isBright && soloFlashing) ? 1.0f : 0.0f);
}

void CustomControlSurface::soloCountChanged (bool anySolo)
{
    sendCommandToControllerForActionID (clearAllSoloId, anySolo);
}

void CustomControlSurface::playStateChanged (bool isPlaying)
{
    sendCommandToControllerForActionID (playId, isPlaying);
}

void CustomControlSurface::recordStateChanged (bool isRecording)
{
    sendCommandToControllerForActionID (recordId, isRecording);
}

void CustomControlSurface::automationReadModeChanged (bool isReading)
{
    sendCommandToControllerForActionID (automationReadId, isReading);
}

void CustomControlSurface::automationWriteModeChanged (bool isWriting)
{
    sendCommandToControllerForActionID (automationRecordId, isWriting);
}

void CustomControlSurface::faderBankChanged (int newStartChannelNumber, const juce::StringArray& trackNames)
{
    int idx = 0;

    for (auto name : trackNames)
    {
        sendCommandToControllerForActionID (nameTrackId + idx, name);
        sendCommandToControllerForActionID (numberTrackId + idx, juce::String (newStartChannelNumber + idx + 1));
        idx++;
    }
}

void CustomControlSurface::channelLevelChanged (int, float, float) {}

void CustomControlSurface::trackSelectionChanged (int faderIndex, bool isSelected)
{
    sendCommandToControllerForActionID (selectTrackId + faderIndex, isSelected);
}

void CustomControlSurface::trackRecordEnabled (int faderIndex, bool recordEnabled)
{
    sendCommandToControllerForActionID (armTrackId + faderIndex, recordEnabled);
}

void CustomControlSurface::masterLevelsChanged (float, float) {}

void CustomControlSurface::timecodeChanged (int barsOrHours, int beatsOrMinutes,
                                            int ticksOrSeconds, int millisecs, bool isBarsBeats, bool isFrames)
{
    char d[32];

    if (isBarsBeats)
        sprintf (d, sizeof (d), "%3d %02d %03d", barsOrHours, beatsOrMinutes, ticksOrSeconds);
    else if (isFrames)
        sprintf (d, sizeof (d), "%02d:%02d:%02d.%02d", barsOrHours, beatsOrMinutes, ticksOrSeconds, millisecs);
    else
        sprintf (d, sizeof (d), "%02d:%02d:%02d.%03d", barsOrHours, beatsOrMinutes, ticksOrSeconds, millisecs);

    sendCommandToControllerForActionID (timecodeId, juce::String (d));
    sendCommandToControllerForActionID (emptyTextId, juce::String());
}

void CustomControlSurface::clickOnOffChanged (bool enabled)
{
    sendCommandToControllerForActionID (toggleClickId, enabled);
}

void CustomControlSurface::snapOnOffChanged (bool enabled)
{
    sendCommandToControllerForActionID (toggleSnapId, enabled);
}

void CustomControlSurface::loopOnOffChanged (bool enabled)
{
    sendCommandToControllerForActionID (toggleLoopId, enabled);
}

void CustomControlSurface::slaveOnOffChanged (bool enabled)
{
    sendCommandToControllerForActionID (toggleSlaveId, enabled);
}

void CustomControlSurface::punchOnOffChanged (bool enabled)
{
    sendCommandToControllerForActionID (togglePunchId, enabled);
}

void CustomControlSurface::scrollOnOffChanged (bool enabled)
{
    sendCommandToControllerForActionID (toggleScrollId, enabled);
}

void CustomControlSurface::parameterChanged (int paramIndex, const ParameterSetting& setting)
{
    ControlSurface::parameterChanged (paramIndex, setting);

    if (paramIndex >= 0)
    {
        sendCommandToControllerForActionID (paramTrackId + paramIndex, setting.value);
        sendCommandToControllerForActionID (paramNameTrackId + paramIndex, juce::String (setting.label));
        sendCommandToControllerForActionID (paramTextTrackId + paramIndex, juce::String (setting.valueDescription));
    }
}

void CustomControlSurface::clearParameter (int paramIndex)
{
    if (paramIndex >= 0)
    {
        sendCommandToControllerForActionID (paramTrackId + paramIndex, 0.0f);
        sendCommandToControllerForActionID (paramNameTrackId + paramIndex, juce::String());
        sendCommandToControllerForActionID (paramTextTrackId + paramIndex, juce::String());
    }
}

void CustomControlSurface::currentSelectionChanged (juce::String pluginName)
{
    sendCommandToControllerForActionID (selectedPluginNameId, pluginName);
}

bool CustomControlSurface::canChangeSelectedPlugin()
{
    return true;
}

void CustomControlSurface::deleteController()
{
    manager->unregisterSurface (this);
    manager->saveAllSettings (engine);
}

void CustomControlSurface::sendCommandToControllerForActionID (int actionID, bool value)
{
    sendCommandToControllerForActionID (actionID, value ? 1.0f : 0.0f);
}

void CustomControlSurface::sendCommandToControllerForActionID (int actionID, float value)
{
    for (auto mapping : mappings)
    {
        if (mapping->function == actionID)
        {
            const auto oscAddr          = mapping->addr;
            const int midiController    = getControllerNumberFromId (mapping->id); // MIDI CC
            const int midiNote          = mapping->note;                           // MIDI Note
            const int midiChannel       = mapping->channel;                        // MIDI Channel

            if (needsOSCSocket && oscAddr.isNotEmpty())
            {
                // Only send a value if the control is currently not being touched
                auto itr = oscControlTouched.find (oscAddr);
                if (itr == oscControlTouched.end() || ! itr->second)
                {
                    if (oscSender)
                    {
                        try
                        {
                            juce::OSCMessage m (oscAddr);
                            m.addFloat32 (value);

                            if (oscSender->send (m))
                                packetsOut++;
                        }
                        catch ([[maybe_unused]] juce::OSCException& err)
                        {
                            DBG("OSC Error: " + err.description);
                        }
                    }
                }
                else
                {
                    oscLastValue[oscAddr] = value;
                }
            }
            else if (needsMidiBackChannel && midiChannel != -1)
            {
                if (midiNote != -1)
                {
                    if (value <= 0.0f)  sendMidiCommandToController (0, juce::MidiMessage::noteOff (midiChannel, midiNote, value));
                    else                sendMidiCommandToController (0, juce::MidiMessage::noteOn (midiChannel, midiNote, value));
                }

                if (midiController != -1)
                    sendMidiCommandToController (0, juce::MidiMessage::controllerEvent (midiChannel, midiController,
                                                                                     juce::MidiMessage::floatValueToMidiByte (value)));
            }
        }
    }
}

void CustomControlSurface::sendCommandToControllerForActionID (int actionID, juce::String value)
{
    for (auto mapping : mappings)
    {
        if (mapping->function == actionID)
        {
            const auto oscAddr = mapping->addr;

            if (needsOSCSocket && oscAddr.isNotEmpty())
            {
                if (oscSender)
                {
                    try
                    {
                        juce::OSCMessage m (oscAddr);
                        m.addString (value);

                        if (oscSender->send (m))
                            packetsOut++;
                    }
                    catch ([[maybe_unused]] juce::OSCException& err)
                    {
                        DBG("OSC Error: " + err.description);
                    }
                }
            }
        }
    }
}

//==============================================================================
bool CustomControlSurface::removeMapping (ActionID id, int controllerID, int note, int channel)
{
    for (int i = mappings.size(); --i >= 0;)
    {
        if (auto m = mappings.getUnchecked (i))
        {
            if (m->function == id && m->id == controllerID
                 && m->note == note && m->channel == channel)
            {
                removeMapping (i);
                return true;
            }
        }
    }

    return false;
}

//==============================================================================
void CustomControlSurface::showMappingsEditor (juce::DialogWindow::LaunchOptions& o)
{
   #if JUCE_MODAL_LOOPS_PERMITTED
    if (needsMidiChannel && owner->getMidiInputDevice (0).isEmpty())
    {
        engine.getUIBehaviour().showWarningAlert (TRANS("Error"),
                                                  TRANS("You must set a MIDI input device!"));
        return;
    }

    listenToRow (-1);
    o.runModal();
    setLearntParam (false);
    listenToRow (-1);

    manager->saveAllSettings (engine);
   #else
    juce::ignoreUnused (o);
   #endif
}

int CustomControlSurface::getNumMappings() const
{
    return mappings.size();
}

void CustomControlSurface::listenToRow (int row)
{
    listeningOnRow = row;
    lastControllerValue = 0;
    lastControllerID = 0;
    lastControllerNote = -1;
    lastControllerChannel = 0;

    sendChangeMessage();
}

int CustomControlSurface::getRowBeingListenedTo() const
{
    return listeningOnRow;
}

void CustomControlSurface::showMappingsListForRow (int row)
{
   #if JUCE_MODAL_LOOPS_PERMITTED
    int r = 0;
    auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    if (auto underMouse = mouse.getComponentUnderMouse())
    {
        auto pt = mouse.getScreenPosition();
        auto opts = juce::PopupMenu::Options().withTargetComponent (underMouse)
                                              .withTargetScreenArea ({ int (pt.x), int (pt.y), 1, 1 });
        r = contextMenu.showMenu (opts);
    }
    else
    {
        r = contextMenu.show();
    }
   #else
    int r = 0;
   #endif

    auto cg = commandGroups.find (r);

    if (cg != commandGroups.end())
    {
        auto set = cg->second;

        for (int i = 0; i < set->size(); ++i)
        {
            if (row >= mappings.size())
                mappings.add (new Mapping());

            mappings[row]->function = (*set)[i];
            row++;
        }

        sendChangeMessage();
    }
    else if (r != 0)
    {
        if (row >= mappings.size())
            mappings.add (new Mapping());

        mappings[row]->function = r;

        sendChangeMessage();
    }
}

CustomControlSurface::Mapping* CustomControlSurface::getMappingForRow (int row) const
{
    return mappings[row];
}

std::pair<juce::String, juce::String> CustomControlSurface::getTextForRow (int rowNumber) const
{
    const int numMappings = mappings.size();
    jassert (juce::isPositiveAndNotGreaterThan (rowNumber, numMappings));

    auto mappingForRow = mappings[rowNumber];

    auto getLeftText = [&] () -> juce::String
    {
        if (rowNumber == listeningOnRow)
        {
            if (lastControllerAddr.isNotEmpty())
                return lastControllerAddr;

            if (lastControllerID > 0 && lastControllerNote == -1)
                return controllerIDToString (lastControllerID, lastControllerChannel)
                         + ": " + juce::String (juce::roundToInt (lastControllerValue * 100.0f)) + "%";

            if (lastControllerNote != -1 && lastControllerID == 0)
                return noteIDToString (lastControllerNote, lastControllerChannel);

            if (needsOSCSocket)
                return "(" + TRANS("Move a controller, double click to type OSC path") + ")";

            return "(" + TRANS("Move a controller") + ")";
        }

        if (rowNumber < numMappings && mappingForRow->addr.isNotEmpty())
            return mappingForRow->addr;

        if (rowNumber < numMappings && mappingForRow->id != 0)
            return controllerIDToString (mappingForRow->id, mappingForRow->channel);

        if (rowNumber < numMappings && mappingForRow->note != -1)
            return noteIDToString (mappingForRow->note, mappingForRow->channel);

        if (rowNumber < numMappings && needsOSCSocket)
            return TRANS("Click here to choose a controller, double click to type OSC path");

        return TRANS("Click here to choose a controller");
    };

    return { getLeftText(), mappingForRow != nullptr ? getFunctionName (mappingForRow->function) : juce::String() };
}

juce::String CustomControlSurface::noteIDToString (int note, int channelid) const
{
    auto text = TRANS("Note On") + " "
                   + juce::MidiMessage::getMidiNoteName (note, true, true,
                                                         engine.getEngineBehaviour().getMiddleCOctave());

    auto channel = juce::String::formatted (" [%d]", channelid);

    return text + channel;
}

juce::String CustomControlSurface::controllerIDToString (int id, int channelid) const
{
    auto channel = juce::String::formatted (" [%d]", channelid);

    if (id >= 0x40000)
        return TRANS("Channel Pressure Controller") + channel;

    if (id >= 0x30000)
        return "RPN #" + juce::String (id & 0x7fff) + channel;

    if (id >= 0x20000)
        return "NRPN #" + juce::String (id & 0x7fff) + channel;

    if (id >= 0x10000)
    {
        auto name = TRANS(juce::MidiMessage::getControllerName (id & 0x7f));

        if (name.isNotEmpty())
            name = " (" + name + ")";

        return TRANS("Controller") + " #" + juce::String (id & 0x7f) + name + channel;
    }

    return {};
}

juce::String CustomControlSurface::getFunctionName (int id) const
{
    for (int i = 0; i < actionFunctionList.size(); ++i)
        if (actionFunctionList.getUnchecked (i)->id == id)
            return actionFunctionList.getUnchecked (i)->name;

    return {};
}

void CustomControlSurface::setLearntParam (bool keepListening)
{
    if (listeningOnRow >= 0)
    {
        if (lastControllerID > 0 || lastControllerNote != -1 || lastControllerAddr.isNotEmpty())
        {
            if (listeningOnRow >= mappings.size())
                mappings.add (new Mapping());

            mappings[listeningOnRow]->id      = lastControllerID;
            mappings[listeningOnRow]->addr    = lastControllerAddr;
            mappings[listeningOnRow]->note    = lastControllerNote;
            mappings[listeningOnRow]->channel = lastControllerChannel;
        }

        if (! keepListening)
        {
            listeningOnRow        = -1;
            lastControllerID      = 0;
            lastControllerAddr    = {};
            lastControllerNote    = -1;
            lastControllerChannel = 0;
        }

        sendChangeMessage();
    }
}

void CustomControlSurface::removeMapping (int index)
{
    mappings.remove (index);
}

void CustomControlSurface::handleAsyncUpdate()
{
    CRASH_TRACER
    sendChangeMessage();

    if (listeningOnRow >= 0 && listeningOnRow == mappings.size())
        setLearntParam (true);
}

void CustomControlSurface::loadFunctions()
{
    CRASH_TRACER

    clearCommandGroups();
    contextMenu.clear();
    actionFunctionList.clear();


    juce::PopupMenu userFunctionSubMenu;
    auto userFunctionSubMenuSet = new juce::SortedSet<int>();
    addAllCommandItem (userFunctionSubMenu);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 1"), userAction1Id, &CustomControlSurface::userFunction1);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 2"), userAction2Id, &CustomControlSurface::userFunction2);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 3"), userAction3Id, &CustomControlSurface::userFunction3);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 4"), userAction4Id, &CustomControlSurface::userFunction4);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 5"), userAction5Id, &CustomControlSurface::userFunction5);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 6"), userAction6Id, &CustomControlSurface::userFunction6);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 7"), userAction7Id, &CustomControlSurface::userFunction7);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 8"), userAction8Id, &CustomControlSurface::userFunction8);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 9"), userAction9Id, &CustomControlSurface::userFunction9);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 10"), userAction10Id, &CustomControlSurface::userFunction10);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 11"), userAction11Id, &CustomControlSurface::userFunction11);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 12"), userAction12Id, &CustomControlSurface::userFunction12);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 13"), userAction13Id, &CustomControlSurface::userFunction13);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 14"), userAction14Id, &CustomControlSurface::userFunction14);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 15"), userAction15Id, &CustomControlSurface::userFunction15);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 16"), userAction16Id, &CustomControlSurface::userFunction16);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 17"), userAction17Id, &CustomControlSurface::userFunction17);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 18"), userAction18Id, &CustomControlSurface::userFunction18);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 19"), userAction19Id, &CustomControlSurface::userFunction19);
    addFunction (userFunctionSubMenu, *userFunctionSubMenuSet, TRANS("User Function"), TRANS("User Function 20"), userAction20Id, &CustomControlSurface::userFunction20);

    commandGroups [nextCmdGroupIndex++] = userFunctionSubMenuSet;

    juce::PopupMenu transportSubMenu;
    auto transportSubMenuSet = new juce::SortedSet<int>();
    addAllCommandItem (transportSubMenu);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Play"), playId, &CustomControlSurface::play);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Stop"), stopId, &CustomControlSurface::stop);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Record"), recordId, &CustomControlSurface::record);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Home"), homeId, &CustomControlSurface::home);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("End"), endId, &CustomControlSurface::end);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Rewind"), rewindId, &CustomControlSurface::rewind);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Fast Forward"), fastForwardId, &CustomControlSurface::fastForward);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Mark-In"), markInId, &CustomControlSurface::markIn);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Mark-Out"), markOutId, &CustomControlSurface::markOut);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Automation Read"), automationReadId, &CustomControlSurface::automationReading);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Automation Record"), automationRecordId, &CustomControlSurface::automationWriting);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Add Marker"), addMarkerId, &CustomControlSurface::addMarker);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Next Marker"), nextMarkerId, &CustomControlSurface::nextMarker);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Previous Marker"), previousMarkerId, &CustomControlSurface::prevMarker);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Marker 1"), marker1Id, &CustomControlSurface::marker1);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Marker 2"), marker2Id, &CustomControlSurface::marker2);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Marker 3"), marker3Id, &CustomControlSurface::marker3);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Marker 4"), marker4Id, &CustomControlSurface::marker4);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Marker 5"), marker5Id, &CustomControlSurface::marker5);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Marker 6"), marker6Id, &CustomControlSurface::marker6);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Marker 7"), marker7Id, &CustomControlSurface::marker7);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Marker 8"), marker8Id, &CustomControlSurface::marker8);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Nudge Left"), nudgeLeftId, &CustomControlSurface::nudgeLeft);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Nudge Right"), nudgeRightId, &CustomControlSurface::nudgeRight);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Abort"), abortId, &CustomControlSurface::abort);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Abort & Restart"), abortRestartId, &CustomControlSurface::abortRestart);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Jog"), jogId, &CustomControlSurface::jog);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Jump to the Mark-In Point"), jumpToMarkInId, &CustomControlSurface::jumpToMarkIn);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Jump to the Mark-Out Point"), jumpToMarkOutId, &CustomControlSurface::jumpToMarkOut);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Timecode"), timecodeId, &CustomControlSurface::null);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Clear all solos"), clearAllSoloId, &CustomControlSurface::clearAllSolo);
    commandGroups [nextCmdGroupIndex++] = transportSubMenuSet;

    juce::PopupMenu optionsSubMenu;
    auto optionsSubMenuSet = new juce::SortedSet<int>();
    addAllCommandItem (optionsSubMenu);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle beats/seconds mode"), toggleBeatsSecondsModeId, &CustomControlSurface::toggleBeatsSecondsMode);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle loop"), toggleLoopId, &CustomControlSurface::toggleLoop);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle punch"), togglePunchId, &CustomControlSurface::togglePunch);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle click"), toggleClickId, &CustomControlSurface::toggleClick);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle snap"), toggleSnapId, &CustomControlSurface::toggleSnap);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle slave"), toggleSlaveId, &CustomControlSurface::toggleSlave);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle E-to-E"), toggleEtoEId, &CustomControlSurface::toggleEtoE);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle scroll"), toggleScrollId, &CustomControlSurface::toggleScroll);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle all arm"), toggleAllArmId, &CustomControlSurface::toggleAllArm);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Empty Text"), emptyTextId, &CustomControlSurface::null);
    commandGroups [nextCmdGroupIndex++] = optionsSubMenuSet;

    juce::PopupMenu pluginSubMenu;
    auto pluginSubMenuSet = new juce::SortedSet<int>();
    addAllCommandItem (pluginSubMenu);
    addFunction (pluginSubMenu, *pluginSubMenuSet, TRANS("Plugin"), TRANS("Master volume"), masterVolumeId, &CustomControlSurface::masterVolume);
    addFunction (pluginSubMenu, *pluginSubMenuSet, TRANS("Plugin"), TRANS("Master volume text"), masterVolumeTextId, &CustomControlSurface::null);
    addFunction (pluginSubMenu, *pluginSubMenuSet, TRANS("Plugin"), TRANS("Master pan"), masterPanId, &CustomControlSurface::masterPan);
    addFunction (pluginSubMenu, *pluginSubMenuSet, TRANS("Plugin"), TRANS("Quick control parameter"), quickParamId, &CustomControlSurface::quickParam);
    commandGroups [nextCmdGroupIndex++] = pluginSubMenuSet;
    addPluginFunction (pluginSubMenu, TRANS("Plugin"), TRANS("Automatable parameters"), paramTrackId, &CustomControlSurface::paramTrack);
    addPluginFunction (pluginSubMenu, TRANS("Plugin"), TRANS("Automatable parameter name"), paramNameTrackId, &CustomControlSurface::paramTrack);
    addPluginFunction (pluginSubMenu, TRANS("Plugin"), TRANS("Automatable parameter text"), paramTextTrackId, &CustomControlSurface::paramTrack);

    juce::PopupMenu trackSubMenu;
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Name"), nameTrackId, &CustomControlSurface::null);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Number text"), numberTrackId, &CustomControlSurface::null);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Volume"), volTrackId, &CustomControlSurface::volTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Volume text"), volTextTrackId, &CustomControlSurface::null);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Pan"), panTrackId, &CustomControlSurface::panTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Pan Text"), panTextTrackId, &CustomControlSurface::null);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Mute"), muteTrackId, &CustomControlSurface::muteTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Solo"), soloTrackId, &CustomControlSurface::soloTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Arm"), armTrackId, &CustomControlSurface::armTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Select"), selectTrackId, &CustomControlSurface::selectTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Aux"), auxTrackId, &CustomControlSurface::auxTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Aux Text"), auxTextTrackId, &CustomControlSurface::null);

    juce::PopupMenu clipSubMenu;
    juce::PopupMenu clipBankSubMenu;

    if (engine.getEngineBehaviour().areClipSlotsEnabled())
    {
        addTrackFunction (clipSubMenu, TRANS("Clip Launcher"), TRANS("Launch Clip #1"), clip1TrackId, &CustomControlSurface::launchClip1);
        addTrackFunction (clipSubMenu, TRANS("Clip Launcher"), TRANS("Launch Clip #2"), clip2TrackId, &CustomControlSurface::launchClip2);
        addTrackFunction (clipSubMenu, TRANS("Clip Launcher"), TRANS("Launch Clip #3"), clip3TrackId, &CustomControlSurface::launchClip3);
        addTrackFunction (clipSubMenu, TRANS("Clip Launcher"), TRANS("Launch Clip #4"), clip4TrackId, &CustomControlSurface::launchClip4);
        addTrackFunction (clipSubMenu, TRANS("Clip Launcher"), TRANS("Launch Clip #5"), clip5TrackId, &CustomControlSurface::launchClip5);
        addTrackFunction (clipSubMenu, TRANS("Clip Launcher"), TRANS("Launch Clip #6"), clip6TrackId, &CustomControlSurface::launchClip6);
        addTrackFunction (clipSubMenu, TRANS("Clip Launcher"), TRANS("Launch Clip #7"), clip7TrackId, &CustomControlSurface::launchClip7);
        addTrackFunction (clipSubMenu, TRANS("Clip Launcher"), TRANS("Launch Clip #8"), clip8TrackId, &CustomControlSurface::launchClip8);
        addTrackFunction (clipSubMenu, TRANS("Clip Launcher"), TRANS("Stop Clips"), stopClipsTrackId, &CustomControlSurface::stopClips);
        addSceneFunction (clipSubMenu, TRANS("Clip Launcher"), TRANS("Launch Scene"), sceneId, &CustomControlSurface::launchScene);

        auto clipBankSubMenuSet = new juce::SortedSet<int>();
        addAllCommandItem (clipBankSubMenu);
        addFunction (clipBankSubMenu, *clipBankSubMenuSet, TRANS("Switch Clip Launch bank"), TRANS("Up") + " 1", clipBankUp1Id, &CustomControlSurface::clipBankUp1);
        addFunction (clipBankSubMenu, *clipBankSubMenuSet, TRANS("Switch Clip Launch bank"), TRANS("Up") + " 4", clipBankUp4Id, &CustomControlSurface::clipBankUp4);
        addFunction (clipBankSubMenu, *clipBankSubMenuSet, TRANS("Switch Clip Launch bank"), TRANS("Up") + " 8", clipBankUp8Id, &CustomControlSurface::clipBankUp8);
        addFunction (clipBankSubMenu, *clipBankSubMenuSet, TRANS("Switch Clip Launch bank"), TRANS("Down") + " 1", clipBankDown1Id, &CustomControlSurface::clipBankDown1);
        addFunction (clipBankSubMenu, *clipBankSubMenuSet, TRANS("Switch Clip Launch bank"), TRANS("Down") + " 4", clipBankDown4Id, &CustomControlSurface::clipBankDown4);
        addFunction (clipBankSubMenu, *clipBankSubMenuSet, TRANS("Switch Clip Launch bank"), TRANS("Down") + " 8", clipBankDown8Id, &CustomControlSurface::clipBankDown8);
        commandGroups [nextCmdGroupIndex++] = clipBankSubMenuSet;
    }

    juce::PopupMenu navigationSubMenu;
    auto navigationSubMenuSet = new juce::SortedSet<int>();
    addAllCommandItem (navigationSubMenu);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Zoom in"), zoomInId, &CustomControlSurface::zoomIn);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Zoom out"), zoomOutId, &CustomControlSurface::zoomOut);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Scroll tracks up"), scrollTracksUpId, &CustomControlSurface::scrollTracksUp);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Scroll tracks down"), scrollTracksDownId, &CustomControlSurface::scrollTracksDown);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Scroll tracks left"), scrollTracksLeftId, &CustomControlSurface::scrollTracksLeft);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Scroll tracks right"), scrollTracksRightId, &CustomControlSurface::scrollTracksRight);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Zoom tracks in"), zoomTracksInId, &CustomControlSurface::zoomTracksIn);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Zoom tracks out"), zoomTracksOutId, &CustomControlSurface::zoomTracksOut);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Toggle selection mode"), toggleSelectionModeId, &CustomControlSurface::toggleSelectionMode);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Select left"), selectLeftId, &CustomControlSurface::selectLeft);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Select right"), selectRightId, &CustomControlSurface::selectRight);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Select up"), selectUpId, &CustomControlSurface::selectUp);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Select down"), selectDownId, &CustomControlSurface::selectDown);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Selected plugin name"), selectedPluginNameId, &CustomControlSurface::null);

    commandGroups [nextCmdGroupIndex++] = navigationSubMenuSet;
    addTrackFunction (navigationSubMenu, TRANS("Navigation"), TRANS("Select clip in track"), selectClipInTrackId, &CustomControlSurface::selectClipInTrack);
    addTrackFunction (navigationSubMenu, TRANS("Navigation"), TRANS("Select plugin in track"), selectPluginInTrackId, &CustomControlSurface::selectFilterInTrack);

    juce::PopupMenu bankSubMenu;
    auto bankSubMenuSet = new juce::SortedSet<int>();
    addAllCommandItem (bankSubMenu);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Left"), faderBankLeftId, &CustomControlSurface::faderBankLeft);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Left") + " 1", faderBankLeft1Id, &CustomControlSurface::faderBankLeft1);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Left") + " 4", faderBankLeft4Id, &CustomControlSurface::faderBankLeft4);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Left") + " 8", faderBankLeft8Id, &CustomControlSurface::faderBankLeft8);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Left") + " 16", faderBankLeft16Id, &CustomControlSurface::faderBankLeft16);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Right"), faderBankRightId, &CustomControlSurface::faderBankRight);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Right") + " 1", faderBankRight1Id, &CustomControlSurface::faderBankRight1);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Right") + " 4", faderBankRight4Id, &CustomControlSurface::faderBankRight4);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Right") + " 8", faderBankRight8Id, &CustomControlSurface::faderBankRight8);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Right") + " 16", faderBankRight16Id, &CustomControlSurface::faderBankRight16);
    commandGroups [nextCmdGroupIndex++] = bankSubMenuSet;

    juce::PopupMenu paramBankSubMenu;
    auto paramBankSubMenuSet = new juce::SortedSet<int>();
    addAllCommandItem (paramBankSubMenu);
    addFunction (paramBankSubMenu, *paramBankSubMenuSet, TRANS("Switch param bank"), TRANS("Left"), paramBankLeftId, &CustomControlSurface::paramBankLeft);
    addFunction (paramBankSubMenu, *paramBankSubMenuSet, TRANS("Switch param bank"), TRANS("Left") + " 1", paramBankLeft1Id, &CustomControlSurface::paramBankLeft1);
    addFunction (paramBankSubMenu, *paramBankSubMenuSet, TRANS("Switch param bank"), TRANS("Left") + " 4", paramBankLeft4Id, &CustomControlSurface::paramBankLeft4);
    addFunction (paramBankSubMenu, *paramBankSubMenuSet, TRANS("Switch param bank"), TRANS("Left") + " 8", paramBankLeft8Id, &CustomControlSurface::paramBankLeft8);
    addFunction (paramBankSubMenu, *paramBankSubMenuSet, TRANS("Switch param bank"), TRANS("Left") + " 16", paramBankLeft16Id, &CustomControlSurface::paramBankLeft16);
    addFunction (paramBankSubMenu, *paramBankSubMenuSet, TRANS("Switch param bank"), TRANS("Left") + " 24", paramBankLeft24Id, &CustomControlSurface::paramBankLeft24);
    addFunction (paramBankSubMenu, *paramBankSubMenuSet, TRANS("Switch param bank"), TRANS("Right"), paramBankRightId, &CustomControlSurface::paramBankRight);
    addFunction (paramBankSubMenu, *paramBankSubMenuSet, TRANS("Switch param bank"), TRANS("Right") + " 1", paramBankRight1Id, &CustomControlSurface::paramBankRight1);
    addFunction (paramBankSubMenu, *paramBankSubMenuSet, TRANS("Switch param bank"), TRANS("Right") + " 4", paramBankRight4Id, &CustomControlSurface::paramBankRight4);
    addFunction (paramBankSubMenu, *paramBankSubMenuSet, TRANS("Switch param bank"), TRANS("Right") + " 8", paramBankRight8Id, &CustomControlSurface::paramBankRight8);
    addFunction (paramBankSubMenu, *paramBankSubMenuSet, TRANS("Switch param bank"), TRANS("Right") + " 16", paramBankRight16Id, &CustomControlSurface::paramBankRight16);
    addFunction (paramBankSubMenu, *paramBankSubMenuSet, TRANS("Switch param bank"), TRANS("Right") + " 24", paramBankRight24Id, &CustomControlSurface::paramBankRight24);
    commandGroups [nextCmdGroupIndex++] = paramBankSubMenuSet;

    contextMenu.addSubMenu (TRANS("User Functions"),    userFunctionSubMenu);
    contextMenu.addSubMenu (TRANS("Transport"),         transportSubMenu);
    contextMenu.addSubMenu (TRANS("Options"),           optionsSubMenu);
    contextMenu.addSubMenu (TRANS("Plugin"),            pluginSubMenu);
    contextMenu.addSubMenu (TRANS("Track"),             trackSubMenu);

    if (engine.getEngineBehaviour().areClipSlotsEnabled())
    {
        contextMenu.addSubMenu (TRANS("Clip Launcher"),     clipSubMenu);
        contextMenu.addSubMenu (TRANS("Clip Launcher Bank"),clipBankSubMenu);
    }

    contextMenu.addSubMenu (TRANS("Navigation"),        navigationSubMenu);
    contextMenu.addSubMenu (TRANS("Switch fader bank"), bankSubMenu);
    contextMenu.addSubMenu (TRANS("Switch param bank"), paramBankSubMenu);

   #if JUCE_DEBUG
    // check for duplicate ids that some sloppy programmer put in
    for (int i = 0; i < actionFunctionList.size(); ++i)
    {
        juce::SortedSet<int> set;

        if (set.contains (actionFunctionList[i]->id))
            jassertfalse;

        set.add (actionFunctionList[i]->id);
    }
   #endif
}

void CustomControlSurface::addAllCommandItem (juce::PopupMenu& menu)
{
    menu.addItem (nextCmdGroupIndex, TRANS("Add all commands"));
    menu.addSeparator();
}

void CustomControlSurface::addFunction (juce::PopupMenu& menu, juce::SortedSet<int>& commandSet,
                                        const juce::String& group, const juce::String& name,
                                        ActionID aid, ActionFunction actionFunc)
{
    if (isTextAction (aid) && ! needsOSCSocket)
        return;

    int id = (int) aid;

    ActionFunctionInfo* afi = new ActionFunctionInfo();

    afi->name       = name;
    afi->group      = group;
    afi->id         = id;
    afi->actionFunc = actionFunc;
    afi->param      = 0;

    actionFunctionList.add(afi);
    menu.addItem (afi->id, afi->name);
    commandSet.add (afi->id);
}

void CustomControlSurface::addPluginFunction (juce::PopupMenu& menu,
                                              const juce::String& group, const juce::String& name,
                                              ActionID aid, ActionFunction actionFunc)
{
    if (isTextAction (aid) && ! needsOSCSocket)
        return;

    int id = (int) aid;

    juce::PopupMenu subMenu;
    addAllCommandItem (subMenu);

    auto subMenuSet = new juce::SortedSet<int>();

    for (int i = 0; i < numParameterControls; ++i)
    {
        ActionFunctionInfo* afi = new ActionFunctionInfo();

        afi->name       = name + " " + TRANS("Parameter") + " #" + juce::String (i + 1);
        afi->group      = group;
        afi->id         = id + i;
        afi->actionFunc = actionFunc;
        afi->param      = i;

        actionFunctionList.add(afi);
        subMenu.addItem (afi->id, TRANS("Parameter") + " #" + juce::String (i + 1));
        subMenuSet->add(afi->id);
    }

    menu.addSubMenu(name, subMenu);
    commandGroups[nextCmdGroupIndex++] = subMenuSet;
}

void CustomControlSurface::addTrackFunction (juce::PopupMenu& menu,
                                             const juce::String& group, const juce::String& name,
                                             ActionID aid, ActionFunction actionFunc)
{
    if (isTextAction (aid) && ! needsOSCSocket)
        return;

    int id = (int) aid;

    juce::PopupMenu subMenu;
    addAllCommandItem (subMenu);

    auto subMenuSet = new juce::SortedSet<int>();

    for (int i = 0; i < numberOfFaderChannels; ++i)
    {
        ActionFunctionInfo* afi = new ActionFunctionInfo();

        afi->name       = name + " " + TRANS("Track") + " #" + juce::String (i + 1);
        afi->group      = group;
        afi->id         = id + i;
        afi->actionFunc = actionFunc;
        afi->param      = i;

        actionFunctionList.add (afi);
        subMenu.addItem (afi->id, TRANS("Track") + " #" + juce::String (i + 1));
        subMenuSet->add (afi->id);
    }

    menu.addSubMenu (name, subMenu);
    commandGroups[nextCmdGroupIndex++] = subMenuSet;
}

void CustomControlSurface::addSceneFunction (juce::PopupMenu& menu,
                                             const juce::String& group, const juce::String& name,
                                             ActionID aid, ActionFunction actionFunc)
{
    if (isTextAction (aid) && ! needsOSCSocket)
        return;

    int id = (int) aid;

    juce::PopupMenu subMenu;
    addAllCommandItem (subMenu);

    auto subMenuSet = new juce::SortedSet<int>();

    for (int i = 0; i < 8; ++i)
    {
        ActionFunctionInfo* afi = new ActionFunctionInfo();

        afi->name       = name + " " + TRANS("Scene") + " #" + juce::String (i + 1);
        afi->group      = group;
        afi->id         = id + i;
        afi->actionFunc = actionFunc;
        afi->param      = i;

        actionFunctionList.add (afi);
        subMenu.addItem (afi->id, TRANS("Scene") + " #" + juce::String (i + 1));
        subMenuSet->add (afi->id);
    }

    menu.addSubMenu (name, subMenu);
    commandGroups[nextCmdGroupIndex++] = subMenuSet;
}

bool CustomControlSurface::shouldActOnValue (float val)
{
    if (needsOSCSocket)
    {
        // If the control doesn't track touches, try and debounce via time
        if (oscControlTouched.find (oscActiveAddr) == oscControlTouched.end())
        {
            double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
            double lastUsed = oscLastUsedTime[oscActiveAddr];

            if (now - lastUsed > 0.75)
            {
                oscLastUsedTime[oscActiveAddr] = now;
                return true;
            }
            return false;
        }

        // Otherwise, only act on press
        if (oscControlTapsWhileTouched[oscActiveAddr] == 1)
            return true;

        return false;
    }

    return val > 0.001f;
}

static bool isValueNonZero (float val) noexcept
{
    return val > 0.001f;
}

void CustomControlSurface::play (float val, int)    { if (shouldActOnValue (val)) userPressedPlay(); }
void CustomControlSurface::stop (float val, int)    { if (shouldActOnValue (val)) userPressedStop(); }
void CustomControlSurface::record (float val, int)  { if (shouldActOnValue (val)) userPressedRecord(); }
void CustomControlSurface::home (float val, int)    { if (shouldActOnValue (val)) userPressedHome(); }
void CustomControlSurface::end (float val, int)     { if (shouldActOnValue (val)) userPressedEnd(); }

void CustomControlSurface::rewind (float val, int)          { userChangedRewindButton (isValueNonZero (val)); }
void CustomControlSurface::fastForward (float val, int)     { userChangedFastForwardButton (isValueNonZero (val)); }

void CustomControlSurface::masterVolume (float val, int)    { userMovedMasterLevelFader (val); }
void CustomControlSurface::masterPan (float val, int)       { userMovedMasterPanPot (val * 2.0f - 1.0f); }

void CustomControlSurface::quickParam (float val, int)      { userMovedQuickParam (val); }
void CustomControlSurface::volTrack (float val, int param)  { userMovedFader (param, val); }

void CustomControlSurface::panTrack (float val, int param)  { userMovedPanPot (param, val * 2.0f - 1.0f); }
void CustomControlSurface::muteTrack (float val, int param) { if (shouldActOnValue (val)) userPressedMute (param, false); }
void CustomControlSurface::soloTrack (float val, int param) { if (shouldActOnValue (val)) userPressedSolo (param); }

void CustomControlSurface::armTrack (float val, int param)      { if (shouldActOnValue (val)) userPressedRecEnable (param, false); }
void CustomControlSurface::selectTrack (float val, int param)   { if (shouldActOnValue (val)) userSelectedTrack (param); }

void CustomControlSurface::auxTrack (float val, int param)              { userMovedAux (param, 0, val); }
void CustomControlSurface::selectClipInTrack (float val, int param)     { if (shouldActOnValue (val)) userSelectedClipInTrack (param); }
void CustomControlSurface::selectFilterInTrack (float val, int param)   { if (shouldActOnValue (val)) userSelectedPluginInTrack (param); }

void CustomControlSurface::launchClip1 (float val, int param)   { if (shouldActOnValue (val)) userLaunchedClip (param, 0, isValueNonZero (val)); }
void CustomControlSurface::launchClip2 (float val, int param)   { if (shouldActOnValue (val)) userLaunchedClip (param, 1, isValueNonZero (val)); }
void CustomControlSurface::launchClip3 (float val, int param)   { if (shouldActOnValue (val)) userLaunchedClip (param, 2, isValueNonZero (val)); }
void CustomControlSurface::launchClip4 (float val, int param)   { if (shouldActOnValue (val)) userLaunchedClip (param, 3, isValueNonZero (val)); }
void CustomControlSurface::launchClip5 (float val, int param)   { if (shouldActOnValue (val)) userLaunchedClip (param, 4, isValueNonZero (val)); }
void CustomControlSurface::launchClip6 (float val, int param)   { if (shouldActOnValue (val)) userLaunchedClip (param, 5, isValueNonZero (val)); }
void CustomControlSurface::launchClip7 (float val, int param)   { if (shouldActOnValue (val)) userLaunchedClip (param, 6, isValueNonZero (val)); }
void CustomControlSurface::launchClip8 (float val, int param)   { if (shouldActOnValue (val)) userLaunchedClip (param, 7, isValueNonZero (val)); }
void CustomControlSurface::stopClips (float val, int param)     { if (shouldActOnValue (val)) userStoppedClip (param, isValueNonZero (val)); }
void CustomControlSurface::launchScene (float val, int param)   { if (shouldActOnValue (val)) userLaunchedScene (param, isValueNonZero (val)); }
void CustomControlSurface::clipBankUp1 (float val, int)         { if (shouldActOnValue (val)) userChangedPadBanks (-1); }
void CustomControlSurface::clipBankUp4 (float val, int)         { if (shouldActOnValue (val)) userChangedPadBanks (-4); }
void CustomControlSurface::clipBankUp8 (float val, int)         { if (shouldActOnValue (val)) userChangedPadBanks (-8); }
void CustomControlSurface::clipBankDown1 (float val, int)       { if (shouldActOnValue (val)) userChangedPadBanks (1); }
void CustomControlSurface::clipBankDown4 (float val, int)       { if (shouldActOnValue (val)) userChangedPadBanks (4); }
void CustomControlSurface::clipBankDown8 (float val, int)       { if (shouldActOnValue (val)) userChangedPadBanks (8); }

void CustomControlSurface::markIn (float val, int)                  { if (shouldActOnValue (val)) userPressedMarkIn(); }
void CustomControlSurface::markOut (float val, int)                 { if (shouldActOnValue (val)) userPressedMarkOut(); }
void CustomControlSurface::automationReading (float val, int)       { if (shouldActOnValue (val)) userPressedAutomationReading(); }
void CustomControlSurface::automationWriting (float val, int)       { if (shouldActOnValue (val)) userPressedAutomationWriting(); }
void CustomControlSurface::toggleBeatsSecondsMode (float val, int)  { if (shouldActOnValue (val)) userToggledBeatsSecondsMode(); }
void CustomControlSurface::toggleLoop (float val, int)              { if (shouldActOnValue (val)) userToggledLoopOnOff(); }
void CustomControlSurface::togglePunch (float val, int)             { if (shouldActOnValue (val)) userToggledPunchOnOff(); }
void CustomControlSurface::toggleClick (float val, int)             { if (shouldActOnValue (val)) userToggledClickOnOff(); }
void CustomControlSurface::toggleSnap (float val, int)              { if (shouldActOnValue (val)) userToggledSnapOnOff(); }
void CustomControlSurface::toggleSlave (float val, int)             { if (shouldActOnValue (val)) userToggledSlaveOnOff(); }
void CustomControlSurface::toggleEtoE (float val, int)              { if (shouldActOnValue (val)) userToggledEtoE(); }
void CustomControlSurface::toggleScroll (float val, int)            { if (shouldActOnValue (val)) userToggledScroll(); }
void CustomControlSurface::toggleAllArm (float val, int)            { if (shouldActOnValue (val)) userPressedArmAll(); }
void CustomControlSurface::zoomIn (float val, int)                  { if (shouldActOnValue (val)) userZoomedIn(); }
void CustomControlSurface::zoomOut (float val, int)                 { if (shouldActOnValue (val)) userZoomedOut(); }
void CustomControlSurface::scrollTracksUp (float val, int)          { if (shouldActOnValue (val)) userScrolledTracksUp(); }
void CustomControlSurface::scrollTracksDown (float val, int)        { if (shouldActOnValue (val)) userScrolledTracksDown(); }
void CustomControlSurface::scrollTracksLeft (float val, int)        { if (shouldActOnValue (val)) userScrolledTracksLeft(); }
void CustomControlSurface::scrollTracksRight (float val, int)       { if (shouldActOnValue (val)) userScrolledTracksRight(); }
void CustomControlSurface::zoomTracksIn (float val, int)            { if (shouldActOnValue (val)) userZoomedTracksIn(); }
void CustomControlSurface::zoomTracksOut (float val, int)           { if (shouldActOnValue (val)) userZoomedTracksOut(); }

void CustomControlSurface::toggleSelectionMode (float val, int)     { if (shouldActOnValue (val)) pluginMoveMode = ! pluginMoveMode; }

void CustomControlSurface::selectLeft (float val, int)  { if (shouldActOnValue (val)) selectOtherObject (SelectableClass::Relationship::moveLeft, pluginMoveMode); }
void CustomControlSurface::selectRight (float val, int) { if (shouldActOnValue (val)) selectOtherObject (SelectableClass::Relationship::moveRight, pluginMoveMode); }
void CustomControlSurface::selectUp (float val, int)    { if (shouldActOnValue (val)) selectOtherObject (SelectableClass::Relationship::moveUp, pluginMoveMode); }
void CustomControlSurface::selectDown (float val, int)  { if (shouldActOnValue (val)) selectOtherObject (SelectableClass::Relationship::moveDown, pluginMoveMode); }

void CustomControlSurface::faderBankLeft   (float val, int)   { if (shouldActOnValue (val)) userChangedFaderBanks (-numberOfFaderChannels); }
void CustomControlSurface::faderBankLeft1  (float val, int)   { if (shouldActOnValue (val)) userChangedFaderBanks (-1); }
void CustomControlSurface::faderBankLeft4  (float val, int)   { if (shouldActOnValue (val)) userChangedFaderBanks (-4); }
void CustomControlSurface::faderBankLeft8  (float val, int)   { if (shouldActOnValue (val)) userChangedFaderBanks (-8); }
void CustomControlSurface::faderBankLeft16 (float val, int)   { if (shouldActOnValue (val)) userChangedFaderBanks (-16); }

void CustomControlSurface::faderBankRight   (float val, int)  { if (shouldActOnValue (val)) userChangedFaderBanks (numberOfFaderChannels); }
void CustomControlSurface::faderBankRight1  (float val, int)  { if (shouldActOnValue (val)) userChangedFaderBanks (1); }
void CustomControlSurface::faderBankRight4  (float val, int)  { if (shouldActOnValue (val)) userChangedFaderBanks (4); }
void CustomControlSurface::faderBankRight8  (float val, int)  { if (shouldActOnValue (val)) userChangedFaderBanks (8); }
void CustomControlSurface::faderBankRight16 (float val, int)  { if (shouldActOnValue (val)) userChangedFaderBanks (16); }

void CustomControlSurface::paramBankLeft   (float val, int)   { if (shouldActOnValue (val)) userChangedParameterBank (-numParameterControls); }
void CustomControlSurface::paramBankLeft1  (float val, int)   { if (shouldActOnValue (val)) userChangedParameterBank (-1); }
void CustomControlSurface::paramBankLeft4  (float val, int)   { if (shouldActOnValue (val)) userChangedParameterBank (-4); }
void CustomControlSurface::paramBankLeft8  (float val, int)   { if (shouldActOnValue (val)) userChangedParameterBank (-8); }
void CustomControlSurface::paramBankLeft16 (float val, int)   { if (shouldActOnValue (val)) userChangedParameterBank (-16); }
void CustomControlSurface::paramBankLeft24 (float val, int)   { if (shouldActOnValue (val)) userChangedParameterBank (-24); }

void CustomControlSurface::paramBankRight   (float val, int)  { if (shouldActOnValue (val)) userChangedParameterBank (numParameterControls); }
void CustomControlSurface::paramBankRight1  (float val, int)  { if (shouldActOnValue (val)) userChangedParameterBank (1); }
void CustomControlSurface::paramBankRight4  (float val, int)  { if (shouldActOnValue (val)) userChangedParameterBank (4); }
void CustomControlSurface::paramBankRight8  (float val, int)  { if (shouldActOnValue (val)) userChangedParameterBank (8); }
void CustomControlSurface::paramBankRight16 (float val, int)  { if (shouldActOnValue (val)) userChangedParameterBank (16); }
void CustomControlSurface::paramBankRight24 (float val, int)  { if (shouldActOnValue (val)) userChangedParameterBank (24); }

void CustomControlSurface::userFunction1    (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (0);  }
void CustomControlSurface::userFunction2    (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (1);  }
void CustomControlSurface::userFunction3    (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (2);  }
void CustomControlSurface::userFunction4    (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (3);  }
void CustomControlSurface::userFunction5    (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (4);  }
void CustomControlSurface::userFunction6    (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (5);  }
void CustomControlSurface::userFunction7    (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (6);  }
void CustomControlSurface::userFunction8    (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (7);  }
void CustomControlSurface::userFunction9    (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (8);  }
void CustomControlSurface::userFunction10   (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (9);  }
void CustomControlSurface::userFunction11   (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (10); }
void CustomControlSurface::userFunction12   (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (11); }
void CustomControlSurface::userFunction13   (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (12); }
void CustomControlSurface::userFunction14   (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (13); }
void CustomControlSurface::userFunction15   (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (14); }
void CustomControlSurface::userFunction16   (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (15); }
void CustomControlSurface::userFunction17   (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (16); }
void CustomControlSurface::userFunction18   (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (17); }
void CustomControlSurface::userFunction19   (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (18); }
void CustomControlSurface::userFunction20   (float val, int)  { if (shouldActOnValue (val)) userPressedUserAction (19); }

void CustomControlSurface::addMarker (float val, int)
{
    if (shouldActOnValue (val))
        if (auto e = getEdit())
            e->getMarkerManager().createMarker (-1, e->getTransport().getPosition(), {}, externalControllerManager.getSelectionManager());
}

void CustomControlSurface::prevMarker (float val, int)  { if (shouldActOnValue (val)) userPressedPreviousMarker(); }
void CustomControlSurface::nextMarker (float val, int)  { if (shouldActOnValue (val)) userPressedNextMarker(); }
void CustomControlSurface::nudgeLeft  (float val, int)  { if (shouldActOnValue (val)) userNudgedLeft(); }
void CustomControlSurface::nudgeRight (float val, int)  { if (shouldActOnValue (val)) userNudgedRight(); }

void CustomControlSurface::marker1 (float val, int) { if (shouldActOnValue (val)) userPressedGoToMarker (0); }
void CustomControlSurface::marker2 (float val, int) { if (shouldActOnValue (val)) userPressedGoToMarker (1); }
void CustomControlSurface::marker3 (float val, int) { if (shouldActOnValue (val)) userPressedGoToMarker (2); }
void CustomControlSurface::marker4 (float val, int) { if (shouldActOnValue (val)) userPressedGoToMarker (3); }
void CustomControlSurface::marker5 (float val, int) { if (shouldActOnValue (val)) userPressedGoToMarker (4); }
void CustomControlSurface::marker6 (float val, int) { if (shouldActOnValue (val)) userPressedGoToMarker (5); }
void CustomControlSurface::marker7 (float val, int) { if (shouldActOnValue (val)) userPressedGoToMarker (6); }
void CustomControlSurface::marker8 (float val, int) { if (shouldActOnValue (val)) userPressedGoToMarker (7); }

void CustomControlSurface::paramTrack (float val, int param)
{
    userMovedParameterControl (param, val);
}

void CustomControlSurface::abort (float val, int)           { if (shouldActOnValue (val)) userPressedAbort(); }
void CustomControlSurface::abortRestart (float val, int)    { if (shouldActOnValue (val)) userPressedAbortRestart(); }

void CustomControlSurface::jumpToMarkIn  (float val, int)   { if (shouldActOnValue (val)) userPressedJumpToMarkIn(); }
void CustomControlSurface::jumpToMarkOut (float val, int)   { if (shouldActOnValue (val)) userPressedJumpToMarkOut(); }

void CustomControlSurface::clearAllSolo (float val, int)    { if (shouldActOnValue (val)) userPressedClearAllSolo(); }

void CustomControlSurface::jog (float val, int)
{
    int x = juce::roundToInt (val * 127.0f);

    if (x <= 64)
        userMovedJogWheel (float(x) / 2);
    else
        userMovedJogWheel (float(x - 128) / 2);
}

bool CustomControlSurface::eatsAllMessages()
{
    return eatsAllMidi;
}

bool CustomControlSurface::canSetEatsAllMessages()
{
    return true;
}

void CustomControlSurface::setEatsAllMessages (bool eatAll)
{
    eatsAllMidi = eatAll;
    manager->saveAllSettings (engine);
}

void CustomControlSurface::markerChanged (int, const MarkerSetting&)
{
}

void CustomControlSurface::clearMarker (int)
{
}

}} // namespace tracktion { inline namespace engine
