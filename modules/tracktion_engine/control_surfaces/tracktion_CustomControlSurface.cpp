/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


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
    XmlElement root ("MIDICUSTOMCONTROLSURFACES");

    for (auto* s : surfaces)
        root.addChildElement (s->createXml());

    Engine::getInstance().getPropertyStorage().setXmlProperty (SettingID::customMidiControllers, root);
}

//==============================================================================
CustomControlSurface::CustomControlSurface (ExternalControllerManager& ecm, const String& name)
   : ControlSurface (ecm)
{
    init();

    deviceDescription = name;
    manager->saveAllSettings();

    loadFunctions();
}

CustomControlSurface::CustomControlSurface (ExternalControllerManager& ecm, const XmlElement& xml)
   : ControlSurface (ecm)
{
    init();

    deviceDescription = xml.getStringAttribute ("name");
    loadFromXml (xml);

    loadFunctions();
}

void CustomControlSurface::init()
{
    manager->registerSurface (this);

    needsMidiChannel                = true;
    needsMidiBackChannel            = false;
    numberOfFaderChannels           = 8;
    numCharactersForTrackNames      = 0;
    numParameterControls            = 18;
    numCharactersForParameterLabels = 0;
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

void CustomControlSurface::updateOrCreateMappingForID (int id, int channel, int noteNum, int controllerID)
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
    auto element = new XmlElement ("MIDICUSTOMCONTROLSURFACE");
    element->setAttribute ("name", deviceDescription);
    element->setAttribute ("eatsMidi", eatsAllMidi);
    element->setAttribute ("channels", numberOfFaderChannels);
    element->setAttribute ("parameters", numParameterControls);

    for (auto m : mappings)
    {
        auto mapping = element->createNewChildElement ("MAPPING");
        mapping->setAttribute ("id",       m->id);
        mapping->setAttribute ("channel",  m->channel);
        mapping->setAttribute ("function", m->function);
        mapping->setAttribute ("note",     m->note);
    }

    return element;
}

bool CustomControlSurface::loadFromXml (const XmlElement& xml)
{
    eatsAllMidi                 = xml.getBoolAttribute("eatsMidi", false);
    numberOfFaderChannels       = xml.getIntAttribute("channels", 8);
    numParameterControls        = xml.getIntAttribute("parameters", 18);

    forEachXmlChildElementWithTagName (xml, node, "MAPPING")
    {
        auto mapping = mappings.add (new Mapping());
        mapping->id       = node->getIntAttribute("id");
        mapping->channel  = node->getIntAttribute("channel");
        mapping->function = node->getIntAttribute("function");
        mapping->note     = node->getIntAttribute("note", -1);
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

void CustomControlSurface::initialiseDevice (bool) {}
void CustomControlSurface::shutDownDevice() {}

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
                                        lastControllerChannel, lastControllerNote, lastControllerID);
        }
        else
        {
            for (auto* mapping : mappings)
            {
                if (((lastControllerID    == mapping->id   && lastControllerID   != 0) ||
                    (lastControllerNote   == mapping->note && lastControllerNote != -1)) &&
                    lastControllerChannel == mapping->channel)
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

void CustomControlSurface::moveFader (int, float) {}
void CustomControlSurface::moveMasterLevelFader (float, float) {}
void CustomControlSurface::movePanPot (int, float) {}
void CustomControlSurface::moveAux (int, const char*, float) {}
void CustomControlSurface::updateSoloAndMute (int, Track::MuteAndSoloLightState, bool) {}
void CustomControlSurface::soloCountChanged (bool) {}
void CustomControlSurface::playStateChanged (bool) {}
void CustomControlSurface::recordStateChanged (bool) {}
void CustomControlSurface::automationReadModeChanged (bool) {}
void CustomControlSurface::automationWriteModeChanged (bool) {}
void CustomControlSurface::faderBankChanged (int, const StringArray&) {}
void CustomControlSurface::channelLevelChanged (int, float) {}
void CustomControlSurface::trackSelectionChanged (int, bool) {}
void CustomControlSurface::trackRecordEnabled (int, bool) {}
void CustomControlSurface::masterLevelsChanged (float, float) {}
void CustomControlSurface::timecodeChanged (int, int, int, int, bool, bool) {}
void CustomControlSurface::clickOnOffChanged (bool) {}
void CustomControlSurface::snapOnOffChanged (bool) {}
void CustomControlSurface::loopOnOffChanged (bool) {}
void CustomControlSurface::slaveOnOffChanged (bool) {}
void CustomControlSurface::punchOnOffChanged (bool) {}
void CustomControlSurface::parameterChanged (int, const ParameterSetting&) {}
void CustomControlSurface::clearParameter (int) {}
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
    if (owner->getMidiInputDevice().isEmpty())
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

    auto* mappingForRow = mappings[rowNumber];

    auto getLeftText = [&] () -> String
    {
        if (rowNumber == listeningOnRow)
        {
            if (lastControllerID > 0 && lastControllerNote == -1)
                return controllerIDToString (lastControllerID, lastControllerChannel) + ": " + String (roundToInt (lastControllerValue * 100.0f)) + "%";

            if (lastControllerNote != -1 && lastControllerID == 0)
                return noteIDToString (lastControllerNote, lastControllerChannel);

            return "(" + TRANS("Move a MIDI controller") + ")";
        }

        if (rowNumber < numMappings && mappingForRow->id != 0)
            return controllerIDToString (mappingForRow->id, mappingForRow->channel);

        if (rowNumber < numMappings && mappingForRow->note != -1)
            return noteIDToString (mappingForRow->note, mappingForRow->channel);

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
        if (lastControllerID > 0 || lastControllerNote != -1)
        {
            if (listeningOnRow >= mappings.size())
                mappings.add (new Mapping());

            mappings[listeningOnRow]->id      = lastControllerID;
            mappings[listeningOnRow]->note    = lastControllerNote;
            mappings[listeningOnRow]->channel = lastControllerChannel;
        }

        if (! keepListening)
        {
            listeningOnRow        = -1;
            lastControllerID      = 0;
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
    commandGroups [nextCmdGroupIndex++] = optionsSubMenuSet;

    PopupMenu pluginSubMenu;
    auto pluginSubMenuSet = new SortedSet<int>();
    addAllCommandItem (pluginSubMenu);
    addFunction (pluginSubMenu, *pluginSubMenuSet, TRANS("Plugin"), TRANS("Master volume"), 8, &CustomControlSurface::masterVolume);
    addFunction (pluginSubMenu, *pluginSubMenuSet, TRANS("Plugin"), TRANS("Master pan"), 9, &CustomControlSurface::masterPan);
    addFunction (pluginSubMenu, *pluginSubMenuSet, TRANS("Plugin"), TRANS("Quick control parameter"), 24, &CustomControlSurface::quickParam);
    commandGroups [nextCmdGroupIndex++] = pluginSubMenuSet;
    addPluginFunction (pluginSubMenu, TRANS("Plugin"), TRANS("Automatable parameters"), 1600, &CustomControlSurface::paramTrack);

    PopupMenu trackSubMenu;
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Volume"), 1800, &CustomControlSurface::volTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Pan"), 1700, &CustomControlSurface::panTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Mute"), 1100, &CustomControlSurface::muteTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Solo"), 1200, &CustomControlSurface::soloTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Arm"), 1300, &CustomControlSurface::armTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Select"), 1400, &CustomControlSurface::selectTrack);
    addTrackFunction (trackSubMenu, TRANS("Track"), TRANS("Aux"), 1500, &CustomControlSurface::auxTrack);

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
    PopupMenu subMenu;
    addAllCommandItem (subMenu);

    auto subMenuSet = new SortedSet<int>();

    for (int i = 0; i < 16; ++i)
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

static bool isValueNonZero (float val) noexcept
{
    return val > 0.001f;
}

void CustomControlSurface::play (float val, int)    { if (isValueNonZero (val)) userPressedPlay(); }
void CustomControlSurface::stop (float val, int)    { if (isValueNonZero (val)) userPressedStop(); }
void CustomControlSurface::record (float val, int)  { if (isValueNonZero (val)) userPressedRecord(); }
void CustomControlSurface::home (float val, int)    { if (isValueNonZero (val)) userPressedHome(); }
void CustomControlSurface::end (float val, int)     { if (isValueNonZero (val)) userPressedEnd(); }

void CustomControlSurface::rewind (float val, int)          { userChangedRewindButton (isValueNonZero (val)); }
void CustomControlSurface::fastForward (float val, int)     { userChangedFastForwardButton (isValueNonZero (val)); }

void CustomControlSurface::masterVolume (float val, int)    { userMovedMasterLevelFader (val); }
void CustomControlSurface::masterPan (float val, int)       { userMovedMasterPanPot (val); }

void CustomControlSurface::quickParam (float val, int)      { userMovedQuickParam (val); }
void CustomControlSurface::volTrack (float val, int param)  { userMovedFader (param, val); }

void CustomControlSurface::panTrack (float val, int param)  { userMovedPanPot(param, val * 2.0f - 1.0f); }
void CustomControlSurface::muteTrack (float val, int param) { if (isValueNonZero (val)) userPressedMute (param, false); }
void CustomControlSurface::soloTrack (float val, int param) { if (isValueNonZero (val)) userPressedSolo (param); }

void CustomControlSurface::armTrack (float val, int param)      { if (isValueNonZero (val)) userPressedRecEnable (param, false); }
void CustomControlSurface::selectTrack (float val, int param)   { if (isValueNonZero (val)) userSelectedTrack (param); }

void CustomControlSurface::auxTrack (float val, int param)              { if (isValueNonZero (val)) userMovedAux (param, val); }
void CustomControlSurface::selectClipInTrack (float val, int param)     { if (isValueNonZero (val)) userSelectedClipInTrack (param); }
void CustomControlSurface::selectFilterInTrack (float val, int param)   { if (isValueNonZero (val)) userSelectedPluginInTrack (param); }

void CustomControlSurface::markIn (float val, int)                  { if (isValueNonZero (val)) userPressedMarkIn(); }
void CustomControlSurface::markOut (float val, int)                 { if (isValueNonZero (val)) userPressedMarkOut(); }
void CustomControlSurface::automationReading (float val, int)       { if (isValueNonZero (val)) userPressedAutomationReading(); }
void CustomControlSurface::automationWriting (float val, int)       { if (isValueNonZero (val)) userPressedAutomationWriting(); }
void CustomControlSurface::toggleBeatsSecondsMode (float val, int)  { if (isValueNonZero (val)) userToggledBeatsSecondsMode(); }
void CustomControlSurface::toggleLoop (float val, int)              { if (isValueNonZero (val)) userToggledLoopOnOff(); }
void CustomControlSurface::togglePunch (float val, int)             { if (isValueNonZero (val)) userToggledPunchOnOff(); }
void CustomControlSurface::toggleClick (float val, int)             { if (isValueNonZero (val)) userToggledClickOnOff(); }
void CustomControlSurface::toggleSnap (float val, int)              { if (isValueNonZero (val)) userToggledSnapOnOff(); }
void CustomControlSurface::toggleSlave (float val, int)             { if (isValueNonZero (val)) userToggledSlaveOnOff(); }
void CustomControlSurface::toggleEtoE (float val, int)              { if (isValueNonZero (val)) userToggledEtoE(); }
void CustomControlSurface::toggleScroll (float val, int)            { if (isValueNonZero (val)) userToggledScroll(); }
void CustomControlSurface::zoomIn (float val, int)                  { if (isValueNonZero (val)) userZoomedIn(); }
void CustomControlSurface::zoomOut (float val, int)                 { if (isValueNonZero (val)) userZoomedOut(); }
void CustomControlSurface::scrollTracksUp (float val, int)          { if (isValueNonZero (val)) userScrolledTracksUp(); }
void CustomControlSurface::scrollTracksDown (float val, int)        { if (isValueNonZero (val)) userScrolledTracksDown(); }
void CustomControlSurface::scrollTracksLeft (float val, int)        { if (isValueNonZero (val)) userScrolledTracksLeft(); }
void CustomControlSurface::scrollTracksRight (float val, int)       { if (isValueNonZero (val)) userScrolledTracksRight(); }
void CustomControlSurface::zoomTracksIn (float val, int)            { if (isValueNonZero (val)) userZoomedTracksIn(); }
void CustomControlSurface::zoomTracksOut (float val, int)           { if (isValueNonZero (val)) userZoomedTracksOut(); }

void CustomControlSurface::toggleSelectionMode (float val, int)     { if (isValueNonZero (val)) pluginMoveMode = ! pluginMoveMode; }

void CustomControlSurface::selectLeft (float val, int)  { if (isValueNonZero (val)) selectOtherObject (SelectableClass::Relationship::moveLeft, pluginMoveMode); }
void CustomControlSurface::selectRight (float val, int) { if (isValueNonZero (val)) selectOtherObject (SelectableClass::Relationship::moveRight, pluginMoveMode); }
void CustomControlSurface::selectUp (float val, int)    { if (isValueNonZero (val)) selectOtherObject (SelectableClass::Relationship::moveUp, pluginMoveMode); }
void CustomControlSurface::selectDown (float val, int)  { if (isValueNonZero (val)) selectOtherObject (SelectableClass::Relationship::moveDown, pluginMoveMode); }

void CustomControlSurface::faderBankLeft   (float val, int)   { if (isValueNonZero (val)) userChangedFaderBanks (-numberOfFaderChannels); }
void CustomControlSurface::faderBankLeft1  (float val, int)   { if (isValueNonZero (val)) userChangedFaderBanks (-1); }
void CustomControlSurface::faderBankLeft4  (float val, int)   { if (isValueNonZero (val)) userChangedFaderBanks (-4); }
void CustomControlSurface::faderBankLeft8  (float val, int)   { if (isValueNonZero (val)) userChangedFaderBanks (-8); }
void CustomControlSurface::faderBankLeft16 (float val, int)   { if (isValueNonZero (val)) userChangedFaderBanks (-16); }

void CustomControlSurface::faderBankRight   (float val, int)  { if (isValueNonZero (val)) userChangedFaderBanks (numberOfFaderChannels); }
void CustomControlSurface::faderBankRight1  (float val, int)  { if (isValueNonZero (val)) userChangedFaderBanks (1); }
void CustomControlSurface::faderBankRight4  (float val, int)  { if (isValueNonZero (val)) userChangedFaderBanks (4); }
void CustomControlSurface::faderBankRight8  (float val, int)  { if (isValueNonZero (val)) userChangedFaderBanks (8); }
void CustomControlSurface::faderBankRight16 (float val, int)  { if (isValueNonZero (val)) userChangedFaderBanks (16); }

void CustomControlSurface::addMarker (float val, int)
{
    if (isValueNonZero (val))
        if (auto e = getEdit())
            e->getMarkerManager().createMarker (-1, e->getTransport().position, 0.0, externalControllerManager.getSelectionManager());
}

void CustomControlSurface::prevMarker (float val, int)  { if (isValueNonZero (val)) userPressedPreviousMarker(); }
void CustomControlSurface::nextMarker (float val, int)  { if (isValueNonZero (val)) userPressedNextMarker(); }
void CustomControlSurface::nudgeLeft  (float val, int)  { if (isValueNonZero (val)) userNudgedLeft(); }
void CustomControlSurface::nudgeRight (float val, int)  { if (isValueNonZero (val)) userNudgedRight(); }

void CustomControlSurface::paramTrack (float val, int param)
{
    userMovedParameterControl (param + 2, val);
}

void CustomControlSurface::abort (float val, int)           { if (isValueNonZero (val)) userPressedAbort(); }
void CustomControlSurface::abortRestart (float val, int)    { if (isValueNonZero (val)) userPressedAbortRestart(); }

void CustomControlSurface::jumpToMarkIn  (float val, int)   { if (isValueNonZero (val)) userPressedJumpToMarkIn(); }
void CustomControlSurface::jumpToMarkOut (float val, int)   { if (isValueNonZero (val)) userPressedJumpToMarkOut(); }

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
