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

void CustomControlSurface::CustomControlSurfaceManager::registerSurface (CustomControlSurface* item)
{
    surfaces.addIfNotAlreadyThere (item);
}

void CustomControlSurface::CustomControlSurfaceManager::unregisterSurface (CustomControlSurface* item)
{
    surfaces.removeAllInstancesOf (item);
}

void CustomControlSurface::CustomControlSurfaceManager::saveAllSettings()
{
    juce::XmlElement root ("MIDICUSTOMCONTROLSURFACES");

    for (auto s : surfaces)
        root.addChildElement (s->createXml());

    Engine::getInstance().getPropertyStorage().setXmlProperty (SettingID::customMidiControllers, root);
}

//==============================================================================
CustomControlSurface::CustomControlSurface (ExternalControllerManager& ecm, const String& name,
                                            ExternalControllerManager::Protocol protocol_)
    : ControlSurface (ecm),
    protocol (protocol_)
{
    init();

    deviceDescription = name;
    manager->saveAllSettings();

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
    numCharactersForTrackNames      = needsOSCSocket ? 12 : 0;
    numParameterControls            = 18;
    numCharactersForParameterLabels = needsOSCSocket ? 12 : 0;
    deletable                       = true;
    listeningOnRow                  = -1;
    pluginMoveMode                  = true;
    eatsAllMidi                     = false;
    wantsClock                      = false;
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

void CustomControlSurface::updateOrCreateMappingForID (int id, String addr, int channel, int noteNum, int controllerID)
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

void CustomControlSurface::addMappingSetsForID (ActionID id, Array<MappingSet>& mappingSet)
{
    if (owner == nullptr)
        return;

    const Colour selectionColour (owner->getSelectionColour());

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

Array<ControlSurface*> CustomControlSurface::getCustomSurfaces (ExternalControllerManager& ecm)
{
    Array<ControlSurface*> surfaces;

    if (auto n = ecm.engine.getPropertyStorage().getXmlProperty (SettingID::customMidiControllers))
        forEachXmlChildElement (*n, controllerXml)
            surfaces.add (new CustomControlSurface (ecm, *controllerXml));

    return surfaces;
}

Array<CustomControlSurface::MappingSet> CustomControlSurface::getMappingSetsForID (ExternalControllerManager& ecm, ActionID id)
{
    Array<MappingSet> mappingSet;

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

XmlElement* CustomControlSurface::createXml()
{
    auto element = new juce::XmlElement ("MIDICUSTOMCONTROLSURFACE");
    element->setAttribute ("name", deviceDescription);
    element->setAttribute ("protocol", protocol == ExternalControllerManager::osc ? "osc" : "midi");
    element->setAttribute ("eatsMidi", eatsAllMidi);
    element->setAttribute ("channels", numberOfFaderChannels);
    element->setAttribute ("parameters", numParameterControls);

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

    forEachXmlChildElementWithTagName (xml, node, "MAPPING")
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

void CustomControlSurface::importSettings (const File& file)
{
    bool ok = false;
    mappings.clearQuick (true);

    std::unique_ptr<XmlElement> xml (XmlDocument::parse (file));

    if (xml != nullptr)
    {
        loadFromXml (*xml);
        ok = true;
    }

    manager->saveAllSettings();

    if (! ok)
        engine.getUIBehaviour().showWarningAlert (TRANS("Import"), TRANS("Import failed"));
}

void CustomControlSurface::exportSettings (const File& file)
{
    bool ok = false;
    std::unique_ptr<XmlElement> xml (createXml());

    if (xml != nullptr)
    {
        file.deleteFile();
        file.create();
        file.appendText (xml->createDocument ({}));

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
        oscReceiver = std::make_unique<OSCReceiver>();
        if (! oscReceiver->connect (oscInputPort))
            oscReceiver.reset();
        else
            oscReceiver->addListener (this);
    }
    if (online && oscOutputPort > 0 && oscOutputAddr.isNotEmpty())
    {
        oscSender = std::make_unique<OSCSender>();
        if (! oscSender->connect (oscOutputAddr, oscOutputPort))
            oscSender.reset();
    }
}

void CustomControlSurface::saveAllSettings()
{
    SharedResourcePointer<CustomControlSurfaceManager> manager;
    manager->saveAllSettings();
}

void CustomControlSurface::updateMiscFeatures()
{
}

bool CustomControlSurface::wantsMessage (const MidiMessage& m)
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
    if (m.size() >= 1)
    {
        auto arg = m[0];
        float val = 0;
        
        if (arg.isFloat32())
            val = arg.getFloat32();
        else if (arg.isInt32())
            val = float (arg.getInt32());
        else
            return;
        
        auto addr = m.getAddressPattern().toString();
        
        DBG("In: " + addr + " " + String (val));
        
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
                        OSCMessage mo (addr);
                        mo.addFloat32 (itr->second);
                        oscSender->send (mo);
                        
                        DBG("Out: " + addr + " " + String (itr->second));
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
            
            if (auto ed = getEdit())
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
        case volTextTrackId:
        case timecodeId:
        case masterVolumeTextId:
        case emptyTextId:
        case auxTextTrackId:
        case panTextTrackId:
        case paramNameTrackId:
        case paramTextTrackId:
            return true;
        default:
            return false;
    }
}

void CustomControlSurface::acceptMidiMessage (const MidiMessage& m)
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
        triggerAsyncUpdate();

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
    sendCommandToControllerForActionID (volTrackId + faderIndex, v);
    
    auto dbText = Decibels::toString (volumeFaderPositionToDB (v));
    sendCommandToControllerForActionID (volTextTrackId + faderIndex, dbText);
}

void CustomControlSurface::moveMasterLevelFader (float newLeftSliderPos, float newRightSliderPos)
{
    const float panApproximation = ((newLeftSliderPos - newRightSliderPos) * 0.5f) + 0.5f;
    sendCommandToControllerForActionID (masterPanId, panApproximation);
    sendCommandToControllerForActionID (masterVolumeId, jmax (newLeftSliderPos, newRightSliderPos));
    
    auto dbText = Decibels::toString (volumeFaderPositionToDB (jmax (newLeftSliderPos, newRightSliderPos)));
    sendCommandToControllerForActionID (masterVolumeTextId, dbText);
}

void CustomControlSurface::movePanPot (int faderIndex, float v)
{
    sendCommandToControllerForActionID (panTrackId + faderIndex, (v * 0.5f) + 0.5f);
    
    String panText;
    int p = roundToInt (v * 100);
    if (p == 0)
        panText = "C";
    else if (p < 0)
        panText = String (-p) + "L";
    else
        panText = String (p) + "R";
    
    sendCommandToControllerForActionID (panTextTrackId + faderIndex, panText);
}

void CustomControlSurface::moveAux (int faderIndex, const char*, float v)
{
    sendCommandToControllerForActionID (auxTrackId + faderIndex, v);
    
    auto dbText = Decibels::toString (volumeFaderPositionToDB (v));
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
    
void CustomControlSurface::faderBankChanged (int, const StringArray& trackNames)
{
    int idx = 0;
    for (auto name : trackNames)
    {
        sendCommandToControllerForActionID (nameTrackId + idx, name);
        idx++;
    }
}
    
void CustomControlSurface::channelLevelChanged (int, float) {}

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

    sendCommandToControllerForActionID (timecodeId, String (d));
    sendCommandToControllerForActionID (emptyTextId, String());
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

void CustomControlSurface::parameterChanged (int paramIndex, const ParameterSetting& setting)
{
    if (paramIndex - 2 >= 0)
    {
        sendCommandToControllerForActionID (paramTrackId + paramIndex - 2, setting.value);
        sendCommandToControllerForActionID (paramNameTrackId + paramIndex - 2, String (setting.label));
        sendCommandToControllerForActionID (paramTextTrackId + paramIndex - 2, String (setting.valueDescription));
    }
}
    
void CustomControlSurface::clearParameter (int paramIndex)
{
    if (paramIndex - 2 >= 0)
    {
        sendCommandToControllerForActionID (paramTrackId + paramIndex - 2, 0.0f);
        sendCommandToControllerForActionID (paramNameTrackId + paramIndex - 2, String());
        sendCommandToControllerForActionID (paramTextTrackId + paramIndex - 2, String());
    }
}
    
void CustomControlSurface::currentSelectionChanged() {}

bool CustomControlSurface::canChangeSelectedPlugin()
{
    return true;
}

void CustomControlSurface::deleteController()
{
    manager->unregisterSurface (this);
    manager->saveAllSettings();
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
                        OSCMessage m (oscAddr);
                        m.addFloat32 (value);
                        oscSender->send (m);
                        
                        DBG("Out: " + oscAddr + " " + String (value));
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
                    if (value <= 0.0f)  sendMidiCommandToController (MidiMessage::noteOff (midiChannel, midiNote, value));
                    else                sendMidiCommandToController (MidiMessage::noteOn (midiChannel, midiNote, value));
                }

                if (midiController != -1)
                    sendMidiCommandToController (MidiMessage::controllerEvent (midiChannel, midiController, MidiMessage::floatValueToMidiByte (value)));
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
                    OSCMessage m (oscAddr);
                    m.addString (value);
                    oscSender->send (m);
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
void CustomControlSurface::showMappingsEditor (DialogWindow::LaunchOptions& o)
{
   #if JUCE_MODAL_LOOPS_PERMITTED
    if (needsMidiChannel && owner->getMidiInputDevice().isEmpty())
    {
        engine.getUIBehaviour().showWarningAlert (TRANS("Error"),
                                                  TRANS("You must set a MIDI input device!"));
        return;
    }

    listenToRow (-1);
    o.runModal();
    listenToRow (-1);

    setLearntParam (false);
    manager->saveAllSettings();
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
    int r = contextMenu.show();
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

std::pair<String, String> CustomControlSurface::getTextForRow (int rowNumber) const
{
    const int numMappings = mappings.size();
    jassert (isPositiveAndNotGreaterThan (rowNumber, numMappings));

    auto mappingForRow = mappings[rowNumber];

    auto getLeftText = [&] () -> String
    {
        if (rowNumber == listeningOnRow)
        {
            if (lastControllerAddr.isNotEmpty())
                return lastControllerAddr;
            
            if (lastControllerID > 0 && lastControllerNote == -1)
                return controllerIDToString (lastControllerID, lastControllerChannel) + ": " + String (roundToInt (lastControllerValue * 100.0f)) + "%";

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

    return { getLeftText(), mappingForRow != nullptr ? getFunctionName (mappingForRow->function) : String() };
}

juce::String CustomControlSurface::noteIDToString (int note, int channelid) const
{
    auto text = TRANS("Note On") + " "
                   + MidiMessage::getMidiNoteName (note, true, true,
                                                   engine.getEngineBehaviour().getMiddleCOctave());

    auto channel = juce::String::formatted (" [%d]", channelid);

    return text + channel;
}

juce::String CustomControlSurface::controllerIDToString (int id, int channelid) const
{
    auto channel = String::formatted (" [%d]", channelid);

    if (id >= 0x40000)
        return TRANS("Channel Pressure Controller") + channel;

    if (id >= 0x30000)
        return "RPN #" + String (id & 0x7fff) + channel;

    if (id >= 0x20000)
        return "NRPN #" + String (id & 0x7fff) + channel;

    if (id >= 0x10000)
    {
        String name (TRANS(MidiMessage::getControllerName (id & 0x7f)));

        if (name.isNotEmpty())
            name = " (" + name + ")";

        return TRANS("Controller") + " #" + String (id & 0x7f) + name + channel;
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

    PopupMenu transportSubMenu;

    auto transportSubMenuSet = new SortedSet<int>();
    addAllCommandItem (transportSubMenu);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Play"), 1, &CustomControlSurface::play);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Stop"), 2, &CustomControlSurface::stop);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Record"), 3, &CustomControlSurface::record);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Home"), 4, &CustomControlSurface::home);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("End"), 5, &CustomControlSurface::end);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Rewind"), 6, &CustomControlSurface::rewind);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Fast Forward"), 7, &CustomControlSurface::fastForward);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Mark-In"), 10, &CustomControlSurface::markIn);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Mark-Out"), 11, &CustomControlSurface::markOut);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Automation Read"), 12, &CustomControlSurface::automationReading);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Automation Record"), 23, &CustomControlSurface::automationWriting);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Add Marker"), 17, &CustomControlSurface::addMarker);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Next Marker"), 13, &CustomControlSurface::nextMarker);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Previous Marker"), 14, &CustomControlSurface::prevMarker);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Nudge Left"), 15, &CustomControlSurface::nudgeLeft);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Nudge Right"), 16, &CustomControlSurface::nudgeRight);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Abort"), 18, &CustomControlSurface::abort);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Abort & Restart"), 19, &CustomControlSurface::abortRestart);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Jog"), 20, &CustomControlSurface::jog);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Jump to the Mark-In Point"), 21, &CustomControlSurface::jumpToMarkIn);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Jump to the Mark-Out Point"), 22, &CustomControlSurface::jumpToMarkOut);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Timecode"), 25, &CustomControlSurface::null);
    addFunction (transportSubMenu, *transportSubMenuSet, TRANS("Transport"), TRANS("Clear all solos"), 27, &CustomControlSurface::clearAllSolo);
    commandGroups [nextCmdGroupIndex++] = transportSubMenuSet;

    PopupMenu optionsSubMenu;
    auto optionsSubMenuSet = new SortedSet<int>();
    addAllCommandItem (optionsSubMenu);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle beats/seconds mode"), 50, &CustomControlSurface::toggleBeatsSecondsMode);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle loop"), 51, &CustomControlSurface::toggleLoop);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle punch"), 52, &CustomControlSurface::togglePunch);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle click"), 53, &CustomControlSurface::toggleClick);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle snap"), 54, &CustomControlSurface::toggleSnap);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle slave"), 55, &CustomControlSurface::toggleSlave);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle E-to-E"), 56, &CustomControlSurface::toggleEtoE);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle scroll"), 57, &CustomControlSurface::toggleScroll);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Toggle all arm"), 58, &CustomControlSurface::toggleAllArm);
    addFunction (optionsSubMenu, *optionsSubMenuSet, TRANS("Options"), TRANS("Empty Text"), 9998, &CustomControlSurface::null);
    commandGroups [nextCmdGroupIndex++] = optionsSubMenuSet;

    PopupMenu pluginSubMenu;
    auto pluginSubMenuSet = new SortedSet<int>();
    addAllCommandItem (pluginSubMenu);
    addFunction (pluginSubMenu, *pluginSubMenuSet, TRANS("Plugin"), TRANS("Master volume"), 8, &CustomControlSurface::masterVolume);
    addFunction (pluginSubMenu, *pluginSubMenuSet, TRANS("Plugin"), TRANS("Master volume text"), 26, &CustomControlSurface::null);
    addFunction (pluginSubMenu, *pluginSubMenuSet, TRANS("Plugin"), TRANS("Master pan"), 9, &CustomControlSurface::masterPan);
    addFunction (pluginSubMenu, *pluginSubMenuSet, TRANS("Plugin"), TRANS("Quick control parameter"), 24, &CustomControlSurface::quickParam);
    commandGroups [nextCmdGroupIndex++] = pluginSubMenuSet;
    addPluginFunction (pluginSubMenu, TRANS("Plugin"), TRANS("Automatable parameters"), 1600, &CustomControlSurface::paramTrack);
    addPluginFunction (pluginSubMenu, TRANS("Plugin"), TRANS("Automatable parameter name"), 2500, &CustomControlSurface::paramTrack);
    addPluginFunction (pluginSubMenu, TRANS("Plugin"), TRANS("Automatable parameter text"), 2600, &CustomControlSurface::paramTrack);

    PopupMenu trackSubMenu;
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Name"), 2100, &CustomControlSurface::null);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Volume"), 1800, &CustomControlSurface::volTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Volume text"), 2200, &CustomControlSurface::null);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Pan"), 1700, &CustomControlSurface::panTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Pan Text"), 2400, &CustomControlSurface::null);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Mute"), 1100, &CustomControlSurface::muteTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Solo"), 1200, &CustomControlSurface::soloTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Arm"), 1300, &CustomControlSurface::armTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Select"), 1400, &CustomControlSurface::selectTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Aux"), 1500, &CustomControlSurface::auxTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Aux Text"), 2300, &CustomControlSurface::null);

    PopupMenu navigationSubMenu;
    auto navigationSubMenuSet = new SortedSet<int>();
    addAllCommandItem (navigationSubMenu);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Zoom in"), 100, &CustomControlSurface::zoomIn);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Zoom out"), 101, &CustomControlSurface::zoomOut);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Scroll tracks up"), 102, &CustomControlSurface::scrollTracksUp);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Scroll tracks down"), 103, &CustomControlSurface::scrollTracksDown);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Scroll tracks left"), 104, &CustomControlSurface::scrollTracksLeft);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Scroll tracks right"), 105, &CustomControlSurface::scrollTracksRight);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Zoom tracks in"), 106, &CustomControlSurface::zoomTracksIn);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Zoom tracks out"), 107, &CustomControlSurface::zoomTracksOut);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Toggle selection mode"), 108, &CustomControlSurface::toggleSelectionMode);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Select left"), 109, &CustomControlSurface::selectLeft);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Select right"), 110, &CustomControlSurface::selectRight);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Select up"), 111, &CustomControlSurface::selectUp);
    addFunction (navigationSubMenu, *navigationSubMenuSet, TRANS("Navigation"), TRANS("Select down"), 112, &CustomControlSurface::selectDown);
    commandGroups [nextCmdGroupIndex++] = navigationSubMenuSet;
    addTrackFunction (navigationSubMenu, TRANS("Navigation"), TRANS("Select clip in track"), 1900, &CustomControlSurface::selectClipInTrack);
    addTrackFunction (navigationSubMenu, TRANS("Navigation"), TRANS("Select plugin in track"), 2000, &CustomControlSurface::selectFilterInTrack);

    PopupMenu bankSubMenu;
    auto bankSubMenuSet = new SortedSet<int>();
    addAllCommandItem (bankSubMenu);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Left"), 208, &CustomControlSurface::faderBankLeft);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Left") + " 1", 200, &CustomControlSurface::faderBankLeft1);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Left") + " 4", 201, &CustomControlSurface::faderBankLeft4);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Left") + " 8", 202, &CustomControlSurface::faderBankLeft8);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Left") + " 16", 206, &CustomControlSurface::faderBankLeft16);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Right"), 209, &CustomControlSurface::faderBankRight);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Right") + " 1", 203, &CustomControlSurface::faderBankRight1);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Right") + " 4", 204, &CustomControlSurface::faderBankRight4);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Right") + " 8", 205, &CustomControlSurface::faderBankRight8);
    addFunction (bankSubMenu, *bankSubMenuSet, TRANS("Switch fader bank"), TRANS("Right") + " 16", 207, &CustomControlSurface::faderBankRight16);
    commandGroups [nextCmdGroupIndex++] = bankSubMenuSet;

    contextMenu.addSubMenu (TRANS("Transport"),  transportSubMenu);
    contextMenu.addSubMenu (TRANS("Options"),    optionsSubMenu);
    contextMenu.addSubMenu (TRANS("Plugin"),     pluginSubMenu);
    contextMenu.addSubMenu (TRANS("Track"),      trackSubMenu);
    contextMenu.addSubMenu (TRANS("Navigation"), navigationSubMenu);
    contextMenu.addSubMenu (TRANS("Switch fader bank"), bankSubMenu);

   #if JUCE_DEBUG
    // check for duplicate ids that some sloppy programmer put in
    for (int i = 0; i < actionFunctionList.size(); ++i)
    {
        SortedSet<int> set;

        if (set.contains (actionFunctionList[i]->id))
            jassertfalse;

        set.add (actionFunctionList[i]->id);
    }
   #endif
}

void CustomControlSurface::addAllCommandItem (PopupMenu& menu)
{
    menu.addItem (nextCmdGroupIndex, TRANS("Add all commands"));
    menu.addSeparator();
}

void CustomControlSurface::addFunction (PopupMenu& menu, SortedSet<int>& commandSet,
                                        const String& group, const String& name,
                                        int id, ActionFunction actionFunc)
{
    if (isTextAction ((ActionID)id) && ! needsOSCSocket)
        return;

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

void CustomControlSurface::addPluginFunction (PopupMenu& menu,
                                              const String& group, const String& name,
                                              int id, ActionFunction actionFunc)
{
    if (isTextAction ((ActionID)id) && ! needsOSCSocket)
        return;

    PopupMenu subMenu;
    addAllCommandItem (subMenu);

    auto subMenuSet = new SortedSet<int>();

    for (int i = 0; i < numParameterControls; ++i)
    {
        ActionFunctionInfo* afi = new ActionFunctionInfo();

        afi->name       = name + " " + TRANS("Parameter") + " #" + String (i + 1);
        afi->group      = group;
        afi->id         = id + i;
        afi->actionFunc = actionFunc;
        afi->param      = i;

        actionFunctionList.add(afi);
        subMenu.addItem (afi->id, TRANS("Parameter") + " #" + String (i + 1));
        subMenuSet->add(afi->id);
    }

    menu.addSubMenu(name, subMenu);
    commandGroups[nextCmdGroupIndex++] = subMenuSet;
}

void CustomControlSurface::addTrackFunction (PopupMenu& menu,
                                             const String& group, const String& name,
                                             int id, ActionFunction actionFunc)
{
    if (isTextAction ((ActionID)id) && ! needsOSCSocket)
        return;
    
    PopupMenu subMenu;
    addAllCommandItem (subMenu);

    auto subMenuSet = new SortedSet<int>();

    for (int i = 0; i < numberOfFaderChannels; ++i)
    {
        ActionFunctionInfo* afi = new ActionFunctionInfo();

        afi->name       = name + " " + TRANS("Track") + " #" + String (i + 1);
        afi->group      = group;
        afi->id         = id + i;
        afi->actionFunc = actionFunc;
        afi->param      = i;

        actionFunctionList.add (afi);
        subMenu.addItem (afi->id, TRANS("Track") + " #" + String (i + 1));
        subMenuSet->add (afi->id);
    }

    menu.addSubMenu(name, subMenu);
    commandGroups[nextCmdGroupIndex++] = subMenuSet;
}

bool CustomControlSurface::shouldActOnValue (float val)
{
    if (needsOSCSocket)
    {
        // If the control doesn't track touches, always act on it
        if (oscControlTouched.find (oscActiveAddr) == oscControlTouched.end())
            return true;
        
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
void CustomControlSurface::masterPan (float val, int)       { userMovedMasterPanPot (val); }

void CustomControlSurface::quickParam (float val, int)      { userMovedQuickParam (val); }
void CustomControlSurface::volTrack (float val, int param)  { userMovedFader (param, val); }

void CustomControlSurface::panTrack (float val, int param)  { userMovedPanPot (param, val * 2.0f - 1.0f); }
void CustomControlSurface::muteTrack (float val, int param) { if (shouldActOnValue (val)) userPressedMute (param, false); }
void CustomControlSurface::soloTrack (float val, int param) { if (shouldActOnValue (val)) userPressedSolo (param); }

void CustomControlSurface::armTrack (float val, int param)      { if (shouldActOnValue (val)) userPressedRecEnable (param, false); }
void CustomControlSurface::selectTrack (float val, int param)   { if (shouldActOnValue (val)) userSelectedTrack (param); }

void CustomControlSurface::auxTrack (float val, int param)              { userMovedAux (param, val); }
void CustomControlSurface::selectClipInTrack (float val, int param)     { if (shouldActOnValue (val)) userSelectedClipInTrack (param); }
void CustomControlSurface::selectFilterInTrack (float val, int param)   { if (shouldActOnValue (val)) userSelectedPluginInTrack (param); }

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

void CustomControlSurface::addMarker (float val, int)
{
    if (shouldActOnValue (val))
        if (auto e = getEdit())
            e->getMarkerManager().createMarker (-1, e->getTransport().position, 0.0, externalControllerManager.getSelectionManager());
}

void CustomControlSurface::prevMarker (float val, int)  { if (shouldActOnValue (val)) userPressedPreviousMarker(); }
void CustomControlSurface::nextMarker (float val, int)  { if (shouldActOnValue (val)) userPressedNextMarker(); }
void CustomControlSurface::nudgeLeft  (float val, int)  { if (shouldActOnValue (val)) userNudgedLeft(); }
void CustomControlSurface::nudgeRight (float val, int)  { if (shouldActOnValue (val)) userNudgedRight(); }

void CustomControlSurface::paramTrack (float val, int param)
{
    userMovedParameterControl (param + 2, val);
}

void CustomControlSurface::abort (float val, int)           { if (shouldActOnValue (val)) userPressedAbort(); }
void CustomControlSurface::abortRestart (float val, int)    { if (shouldActOnValue (val)) userPressedAbortRestart(); }

void CustomControlSurface::jumpToMarkIn  (float val, int)   { if (shouldActOnValue (val)) userPressedJumpToMarkIn(); }
void CustomControlSurface::jumpToMarkOut (float val, int)   { if (shouldActOnValue (val)) userPressedJumpToMarkOut(); }
    
void CustomControlSurface::clearAllSolo (float val, int)    { if (shouldActOnValue (val)) userPressedClearAllSolo(); }

void CustomControlSurface::jog (float val, int)
{
    int x = roundToInt (val * 127.0f);

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
    manager->saveAllSettings();
}

void CustomControlSurface::markerChanged (int, const MarkerSetting&)
{
}

void CustomControlSurface::clearMarker (int)
{
}

}
