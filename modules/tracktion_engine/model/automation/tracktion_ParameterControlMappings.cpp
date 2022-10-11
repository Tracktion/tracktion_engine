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

static juce::String controllerIDToString (int id, int channelid)
{
    auto channel = " [" + juce::String (channelid) + "]";

    if (id >= 0x40000) return TRANS("Channel Pressure Controller") + "# " + channel;
    if (id >= 0x30000) return "RPN #"  + juce::String (id & 0x7fff) + channel;
    if (id >= 0x20000) return "NRPN #" + juce::String (id & 0x7fff) + channel;

    if (id >= 0x10000)
    {
        auto name = TRANS(juce::MidiMessage::getControllerName (id & 0x7f));

        if (name.isNotEmpty())
            name = " (" + name + ")";

        return TRANS("Controller") + " #" + juce::String (id & 0x7f) + name + channel;
    }

    return {};
}

static void removeAddAllCommandIfTooManyItems (juce::PopupMenu& menu)
{
    // if there are more than 200 parameters, strip off the initial two items ("add all" and separator)
    if (menu.getNumItems() > 201)
    {
        juce::PopupMenu m2;
        int n = 0;

        for (juce::PopupMenu::MenuItemIterator i (menu); i.next();)
            if (++n >= 2)
                m2.addItem (i.getItem());

        menu = m2;
    }
}

//==============================================================================
ParameterControlMappings::ParameterControlMappings (Edit& ed)
    : edit (ed)
{
    loadFrom (juce::ValueTree());
}

ParameterControlMappings::~ParameterControlMappings()
{
}

ParameterControlMappings* ParameterControlMappings::getCurrentlyFocusedMappings (Engine& engine)
{
    if (auto ed = engine.getUIBehaviour().getLastFocusedEdit())
        return &ed->getParameterControlMappings();

    return {};
}

//==============================================================================
int ParameterControlMappings::addMapping (int controllerID, int channel, const AutomatableParameter::Ptr& param)
{
    const juce::ScopedLock sl (lock);

    for (int i = controllerIDs.size(); --i >= 0;)
    {
        if (controllerIDs.getUnchecked (i) == controllerID
             && channelIDs[i] == channel
             && parameters[i] == param)
        {
            return i;
        }
    }

    controllerIDs.add (controllerID);
    channelIDs.add (channel);
    parameters.add (param);
    parameterFullNames.add (param->getFullName());

    return controllerIDs.size() - 1;
}

void ParameterControlMappings::removeMapping (int index)
{
    const juce::ScopedLock sl (lock);

    controllerIDs.remove (index);
    channelIDs.remove (index);
    parameters.remove (index);
    parameterFullNames.remove (index);

    tellEditAboutChange();
}

void ParameterControlMappings::showMappingsEditor (juce::DialogWindow::LaunchOptions& o)
{
    listenToRow (-1);
    checkForDeletedParams();

   #if JUCE_MODAL_LOOPS_PERMITTED
    o.runModal();
   #else
    juce::ignoreUnused (o);
   #endif

    listenToRow (-1);
    setLearntParam (false);
}

void ParameterControlMappings::handleAsyncUpdate()
{
    if (edit.engine.getMidiLearnState().isActive() && edit.getParameterChangeHandler().isParameterPending())
    {
        const MidiLearnState::ScopedChangeCaller changeCaller (edit.engine.getMidiLearnState(), MidiLearnState::added);
        auto param = edit.getParameterChangeHandler().getPendingParam (true);
        listeningOnRow = parameters.indexOf (param);

        if (listeningOnRow == -1)
            listeningOnRow = addMapping (lastControllerID, lastControllerChannel, param);

        setLearntParam (false);
        tellEditAboutChange();
    }
    else if (juce::isPositiveAndBelow (listeningOnRow, controllerIDs.size() + 1))
    {
        setLearntParam (true);
    }
}

void ParameterControlMappings::sendChange (int controllerID, float newValue, int channel)
{
    const juce::ScopedLock sl (lock);

    lastControllerID = controllerID;
    lastControllerChannel = channel;
    lastControllerValue = newValue;

    bool learnActive = edit.engine.getMidiLearnState().isActive()
                         && edit.engine.getUIBehaviour().getCurrentlyFocusedEdit() == &edit;

    if (listeningOnRow >= 0 || learnActive)
        triggerAsyncUpdate();

    if (! learnActive)
    {
        for (int index = 0; index < controllerIDs.size(); index++)
        {
            if (controllerIDs[index] == controllerID && channelIDs[index] == channel)
            {
                if (auto p = parameters.getUnchecked (index))
                {
                    jassert (Selectable::isSelectableValid (p.get()));
                    p->midiControllerMoved (newValue);
                }
            }
        }
    }
}

bool ParameterControlMappings::getParameterMapping (AutomatableParameter& param, int& channel, int& controllerID) const
{
    auto index = parameters.indexOf (&param);

    if (index < 0)
    {
        channel = controllerID = -1;
        return false;
    }

    channel = channelIDs[index];
    controllerID = controllerIDs[index];
    return true;
}

bool ParameterControlMappings::removeParameterMapping (AutomatableParameter& param)
{
    auto index = parameters.indexOf (&param);

    if (index < 0)
        return false;

    removeMapping (index);
    return true;
}

//==============================================================================
void ParameterControlMappings::tellEditAboutChange()
{
    SelectionManager::refreshAllPropertyPanels();
}

void ParameterControlMappings::checkForDeletedParams()
{
    juce::Array<AutomatableParameter*> allParams;

    for (int j = parameters.size(); --j >= 0;)
    {
        auto param = parameters[j];

        if ((param != nullptr && param->getReferenceCount() <= 1)
             || (param == nullptr && parameterFullNames[j].isNotEmpty()))
        {
            if (allParams.isEmpty())
                allParams = edit.getAllAutomatableParams (true);

            param = nullptr;

            for (int i = allParams.size(); --i >= 0;)
            {
                auto p = allParams.getUnchecked(i);

                if (p->getFullName() == parameterFullNames[j])
                {
                    jassert (p->getReferenceCount() > 0);
                    param = p;
                    break;
                }
            }

            parameters.set (j, param.get());
        }
    }
}

//==============================================================================
void ParameterControlMappings::loadFrom (const juce::ValueTree& state)
{
    const juce::ScopedLock sl (lock);

    controllerIDs.clear();
    channelIDs.clear();
    parameters.clear();
    parameterFullNames.clear();

    if (state.hasType (IDs::CONTROLLERMAPPINGS))
    {
        auto allParams = edit.getAllAutomatableParams (true);

        for (int j = 0; j < state.getNumChildren(); ++j)
        {
            auto map = state.getChild(j);

            const int id = map[IDs::id];
            const int channel = map.getProperty (IDs::channel, 1);  // Won't exist pre 2.1.0.4, default to 1
            auto paramName = map[IDs::param].toString();
            auto pluginID = EditItemID::fromProperty (map, IDs::pluginID);

            if (id != 0)
            {
                for (auto* p : allParams)
                {
                    if (p->getFullName() == paramName)
                    {
                        // we want to check the pluginID against id of parent of the parameter
                        // to make sure it gets assigned to the correct plugins parameter
                        // if then pluginID doesn't exists (pre 2.0.1.5) then all we can do
                        // is match by name
                        if (pluginID.isValid()
                              && paramName != TRANS("Master volume")
                              && paramName != TRANS("Master pan")
                              && pluginID != p->getOwnerID())
                            continue;

                        jassert (p->getReferenceCount() > 0);

                        controllerIDs.add (id);
                        channelIDs.add (channel);
                        parameters.add (p);
                        parameterFullNames.add (paramName);
                        break;
                    }
                }
            }
        }
    }
}

void ParameterControlMappings::saveTo (juce::ValueTree& state)
{
    const juce::ScopedLock sl (lock);

    checkForDeletedParams();

    state.removeAllChildren (nullptr);
    state.removeAllProperties (nullptr);

    if (controllerIDs.isEmpty())
        return;

    for (int i = 0; i < controllerIDs.size(); ++i)
    {
        if (parameters[i] != nullptr && controllerIDs[i] != 0)
        {
            auto e = createValueTree (IDs::MAP,
                                      IDs::id, controllerIDs[i],
                                      IDs::channel, channelIDs[i],
                                      IDs::param, parameters[i]->getFullName(),
                                      IDs::pluginID, parameters[i]->getOwnerID());

            state.addChild (e, -1, nullptr);
        }
    }
}

void ParameterControlMappings::addPluginToMenu (Plugin::Ptr plugin, juce::PopupMenu& menu,
                                                juce::Array<ParameterAndIndex>& allParams, int& index, int addAllItemIndex)
{
    AutomatableParameterTree::TreeNode* node = plugin->getParameterTree().rootNode.get();
    addPluginToMenu (node, menu, allParams, index, addAllItemIndex);
}

void ParameterControlMappings::addPluginToMenu (AutomatableParameterTree::TreeNode* node, juce::PopupMenu& menu,
                                                juce::Array<ParameterAndIndex>& allParams, int& index, int addAllItemIndex)
{
    for (auto subNode : node->subNodes)
    {
        if (subNode->type == AutomatableParameterTree::Parameter)
        {
            AutomatableParameter::Ptr autoParam = subNode->parameter;
            index++;

            if (autoParam->isParameterActive())
            {
                allParams.add ({ autoParam.get(), index });

                if (addAllItemIndex)
                    allParams.add ({ autoParam.get(), addAllItemIndex });
            }

            menu.addItem (index, autoParam->getParameterName().fromLastOccurrenceOf (" >> ", false, false),
                          autoParam->isParameterActive());

        }

        if (subNode->type == AutomatableParameterTree::Group)
        {
            juce::PopupMenu subMenu;

            int itemID = ++index;
            subMenu.addItem (itemID, TRANS("Add all parameters"));
            subMenu.addSeparator();

            addPluginToMenu (subNode, subMenu, allParams, index, itemID);

            removeAddAllCommandIfTooManyItems (subMenu);

            // only add the submenu if something is in it, not counting the add all parameters
            if (subMenu.getNumItems() > 1)
                menu.addSubMenu (subNode->getGroupName(), subMenu);
        }
    }
}

juce::PopupMenu ParameterControlMappings::buildMenu (juce::Array<ParameterAndIndex>& allParams)
{
    CRASH_TRACER

    juce::StringArray presets (getPresets());

    juce::PopupMenu loadPresets, deletePresets;
    addSortedListToMenu (loadPresets, presets, 60000);
    addSortedListToMenu (deletePresets, presets, 70000);

    juce::PopupMenu m;
    m.addSubMenu (TRANS("Load presets"), loadPresets, loadPresets.getNumItems() > 0);

    // save preset
    juce::Array<Plugin*> plugins;

    for (auto param : parameters)
        if (param != nullptr)
            if (auto p = param->getPlugin())
                plugins.addIfNotAlreadyThere (p);

    juce::StringArray pluginNames;

    for (auto f : plugins)
    {
        if (auto t = f->getOwnerTrack())
            pluginNames.add (t->getName() + " >> " + f->getName());
        else
            pluginNames.add (f->getName());
    }

    pluginNames.sort (true);

    juce::PopupMenu savePresets;
    addSortedListToMenu (savePresets, pluginNames, 50000);

    m.addSubMenu (TRANS("Save preset"), savePresets, savePresets.getNumItems() > 0);
    m.addSubMenu (TRANS("Delete presets"), deletePresets, deletePresets.getNumItems() > 0);

    m.addSeparator();

    // Build the menu for the main parameters
    juce::PopupMenu masterPluginsSubMenu;

    int index = 0;

    if (auto volume = edit.getMasterVolumePlugin())
        addPluginToMenu (volume, masterPluginsSubMenu, allParams, index, 0);

    for (auto plugin : edit.getMasterPluginList())
    {
        juce::PopupMenu pluginSubMenu;

        int addAllItemIndex = ++index;
        pluginSubMenu.addItem (addAllItemIndex, TRANS("Add all parameters"));
        pluginSubMenu.addSeparator();

        addPluginToMenu (plugin, pluginSubMenu, allParams, index, addAllItemIndex);
        removeAddAllCommandIfTooManyItems (pluginSubMenu);

        // only add the submenu if something is in it, not counting the add all parameters
        if (pluginSubMenu.getNumItems() > 1)
            masterPluginsSubMenu.addSubMenu (plugin->getName(), pluginSubMenu);
    }

    if (masterPluginsSubMenu.getNumItems())
        m.addSubMenu ("master plugins", masterPluginsSubMenu);

    // Build the menu for the rack parameters
    auto& rackTypes = edit.getRackList();

    if (rackTypes.size() > 0)
    {
        juce::PopupMenu racksSubMenu;

        for (int i = 0; i < rackTypes.size(); ++i)
        {
            juce::PopupMenu rackSubMenu;
            auto rackType = rackTypes.getRackType(i);

            for (auto plugin : rackType->getPlugins())
            {
                juce::PopupMenu pluginSubMenu;

                int addAllItemIndex = ++index;
                pluginSubMenu.addItem (addAllItemIndex, TRANS("Add all parameters"));
                pluginSubMenu.addSeparator();

                addPluginToMenu (plugin, pluginSubMenu, allParams, index, addAllItemIndex);
                removeAddAllCommandIfTooManyItems (pluginSubMenu);

                // only add the submenu if something is in it, not counting the add all parameters
                if (pluginSubMenu.getNumItems() > 1)
                    rackSubMenu.addSubMenu (plugin->getName(), pluginSubMenu);
            }

            if (rackSubMenu.getNumItems() > 0)
                racksSubMenu.addSubMenu (rackType->rackName, rackSubMenu);
        }

        if (racksSubMenu.getNumItems())
            m.addSubMenu ("rack filters", racksSubMenu);
    }

    // Build the menu for each track
    for (auto track : getAllTracks (edit))
    {
        juce::PopupMenu tracksSubMenu;

        // Build a menu for each plugin in the plugin panel
        for (auto plugin : track->pluginList)
        {
            juce::PopupMenu pluginSubMenu;

            int addAllItemIndex = ++index;
            pluginSubMenu.addItem (addAllItemIndex, TRANS("Add all parameters"));
            pluginSubMenu.addSeparator();

            addPluginToMenu (plugin, pluginSubMenu, allParams, index, addAllItemIndex);
            removeAddAllCommandIfTooManyItems (pluginSubMenu);

            // only add the submenu if something is in it, not counting the add all parameters
            if (pluginSubMenu.getNumItems() > 1)
                tracksSubMenu.addSubMenu (plugin->getName(), pluginSubMenu);
        }

        // Build a menu for each plugin on a clip
        for (int j = 0; j < track->getNumTrackItems(); j++)
        {
            if (auto clip = dynamic_cast<Clip*> (track->getTrackItem(j)))
            {
                if (auto pluginList = clip->getPluginList())
                {
                    for (auto plugin : *pluginList)
                    {
                        juce::PopupMenu pluginSubMenu;

                        int addAllItemIndex = ++index;
                        pluginSubMenu.addItem (addAllItemIndex, TRANS("Add all parameters"));
                        pluginSubMenu.addSeparator();

                        // add all the parameters to the submenu.
                        addPluginToMenu (plugin, pluginSubMenu, allParams, index, addAllItemIndex);
                        removeAddAllCommandIfTooManyItems (pluginSubMenu);

                        // only add the submenu if something is in it, not counting the add all parameters
                        if (pluginSubMenu.getNumItems() > 1)
                            tracksSubMenu.addSubMenu (plugin->getName(), pluginSubMenu);
                    }
                }
            }
        }

        if (tracksSubMenu.getNumItems())
            m.addSubMenu (track->getName(), tracksSubMenu);
    }

    return m;
}

void ParameterControlMappings::showMappingsListForRow (int row)
{
    CRASH_TRACER
    juce::Array<ParameterAndIndex> allParams;
    auto m = buildMenu (allParams);

   #if JUCE_MODAL_LOOPS_PERMITTED
    const int r = m.show();
   #else
    const int r = 0;
   #endif

    if (r >= 50000 && r < 51000)
    {
        savePreset (r - 50000);
    }
    else if (r >= 60000 && r < 61000)
    {
        loadPreset (r - 60000);
    }
    else if (r >= 70000 && r < 71000)
    {
        deletePreset (r - 70000);
    }
    else if (r != 0)
    {
        for (const auto& pair : allParams)
        {
            if (pair.index == r && pair.param != nullptr)
            {
                while (row >= controllerIDs.size())
                {
                    controllerIDs.add (0);
                    channelIDs.add (0);
                    parameters.add (nullptr);
                    parameterFullNames.add ({});
                }

                parameters.set (row, pair.param);
                parameterFullNames.set (row, pair.param->getFullName());

                ++row;
            }
        }

        tellEditAboutChange();
        sendChangeMessage();
    }
}

void ParameterControlMappings::listenToRow (int row)
{
    listeningOnRow = row;
    lastControllerID = 0;
    lastControllerChannel = 0;

    sendChangeMessage();
}

int ParameterControlMappings::getRowBeingListenedTo() const
{
    return listeningOnRow;
}

std::pair<juce::String, juce::String> ParameterControlMappings::getTextForRow (int rowNumber) const
{
    auto getLeftText = [&] () -> juce::String
    {
        if (rowNumber == listeningOnRow)
        {
            if (lastControllerID > 0)
                return controllerIDToString (lastControllerID, lastControllerChannel)
                         + ": " + juce::String (juce::roundToInt (lastControllerValue * 100.0f)) + "%";

            return "(" + TRANS("Move a MIDI controller") + ")";
        }

        if (controllerIDs[rowNumber] != 0)
            return controllerIDToString (controllerIDs[rowNumber], channelIDs[rowNumber]);

        return TRANS("Click here to choose a controller");
    };

    auto getRightText = [&] () -> juce::String
    {
        if (auto p = parameters[rowNumber])
            return p->getFullName();

        return TRANS("Choose Parameter") + "...";
    };

    return { getLeftText(), getRightText() };
}

ParameterControlMappings::Mapping ParameterControlMappings::getMappingForRow (int row) const
{
    return { parameters[row].get(), controllerIDs[row], channelIDs[row] };
}

void ParameterControlMappings::setLearntParam (bool keepListening)
{
    if (listeningOnRow >= 0)
    {
        CRASH_TRACER

        if (lastControllerID > 0)
        {
            if (listeningOnRow >= controllerIDs.size())
            {
                controllerIDs.add ({});
                channelIDs.add ({});
                parameters.add ({});
                parameterFullNames.add ({});
            }

            controllerIDs.set (listeningOnRow, lastControllerID);
            channelIDs.set (listeningOnRow, lastControllerChannel);

            tellEditAboutChange();
        }

        if (! keepListening)
        {
            listeningOnRow = -1;
            lastControllerID = 0;
            lastControllerChannel = 0;
        }

        sendChangeMessage();
    }
}

void ParameterControlMappings::savePreset (int index)
{
    juce::Array<Plugin*> plugins;

    for (auto param : parameters)
        if (param != nullptr)
            if (auto p = param->getPlugin())
                plugins.addIfNotAlreadyThere (p);

    juce::StringArray pluginNames;

    for (int i = 0; i < plugins.size(); ++i)
    {
        if (auto* f = plugins.getUnchecked(i))
        {
            auto name = f->getName() + "|" + juce::String (i);

            if (auto t = f->getOwnerTrack())
                name = t->getName() + " >> " + name;

            pluginNames.add (name);
        }
    }

    pluginNames.sort (true);

   #if JUCE_MODAL_LOOPS_PERMITTED
    auto plugin = plugins[pluginNames[index].getTrailingIntValue()];

    const std::unique_ptr<juce::AlertWindow> w (juce::LookAndFeel::getDefaultLookAndFeel()
                                                  .createAlertWindow (TRANS("Plugin mapping preset"),
                                                                      TRANS("Create a new plugin mapping preset"),
                                                                      {}, {}, {}, juce::AlertWindow::QuestionIcon, 0, nullptr));

    w->addTextEditor ("setName", plugin->getName(), TRANS("Name:"));
    w->addButton (TRANS("OK"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (TRANS("Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));

    int res = w->runModalLoop();
    auto name = w->getTextEditorContents ("setName");

    if (res == 0 || name.trim().isEmpty())
        return;
   #else
    juce::ignoreUnused (index);
    return;
   #endif

   #if JUCE_MODAL_LOOPS_PERMITTED
    auto xml = new juce::XmlElement ("filter");
    xml->setAttribute ("name", name);
    xml->setAttribute ("filter", plugin->getName());

    for (int i = 0; i < parameters.size(); ++i)
    {
        if (auto p = parameters[i])
        {
            if (p->getPlugin() == plugin)
            {
                auto mapping = new juce::XmlElement ("mapping");
                mapping->setAttribute ("controller", controllerIDs[i]);
                mapping->setAttribute ("channel", channelIDs[i]);
                mapping->setAttribute ("parameter", p->paramID);
                xml->addChildElement (mapping);
            }
        }
    }

    juce::XmlElement xmlNew ("FILTERMAPPINGPRESETS");
    xmlNew.addChildElement (xml);

    if (auto xmlOld = edit.engine.getPropertyStorage().getXmlProperty (SettingID::filterControlMappingPresets))
        for (auto n : xmlOld->getChildIterator())
            if (n->getStringAttribute ("name") != name)
                xmlNew.addChildElement (new juce::XmlElement (*n));

    edit.engine.getPropertyStorage().setXmlProperty (SettingID::filterControlMappingPresets, xmlNew);
   #endif
}

void ParameterControlMappings::loadPreset (int index)
{
    if (auto xml = edit.engine.getPropertyStorage().getXmlProperty (SettingID::filterControlMappingPresets))
    {
        if (auto mapping = xml->getChildElement (index))
        {
            Plugin::Array matchingPlugins;

            {
                auto plugin = mapping->getStringAttribute ("filter");

                for (auto p : getAllPlugins (edit, true))
                    if (plugin == p->getName())
                        matchingPlugins.add (p);
            }

            if (matchingPlugins.isEmpty())
            {
                edit.engine.getUIBehaviour().showWarningAlert (TRANS("Not found"),
                                                               TRANS("The plugin was not found"));
            }
            else
            {
                auto plugin = matchingPlugins.getFirst();

                if (matchingPlugins.size() > 1)
                {
                   #if JUCE_MODAL_LOOPS_PERMITTED
                    juce::ComboBox cb;
                    cb.setSize (200,20);

                    for (int i = 0; i < matchingPlugins.size(); ++i)
                        if (auto p = matchingPlugins.getUnchecked (i))
                            cb.addItem (p->getOwnerTrack() ? p->getOwnerTrack()->getName() + " >> " + p->getName()
                                                           : p->getName(),
                                        i + 1);

                    cb.setSelectedId(1);

                    const std::unique_ptr<juce::AlertWindow> w (juce::LookAndFeel::getDefaultLookAndFeel()
                                                                    .createAlertWindow (TRANS("Select plugin"),
                                                                                        TRANS("Select plugin to apply preset to:"),
                                                                                        {}, {}, {},
                                                                                        juce::AlertWindow::QuestionIcon, 0, nullptr));

                    w->addCustomComponent (&cb);
                    w->addButton (TRANS("OK"), 1, juce::KeyPress (juce::KeyPress::returnKey));
                    w->addButton (TRANS("Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));

                    int res = w->runModalLoop();

                    if (res == 1)
                        plugin = matchingPlugins[cb.getSelectedId() - 1];
                    else
                        plugin = nullptr;
                   #else
                    plugin = nullptr;
                   #endif
                }

                if (plugin != nullptr)
                {
                    for (auto item : mapping->getChildIterator())
                    {
                        controllerIDs.add (item->getStringAttribute ("controller").getIntValue());
                        channelIDs.add (item->getStringAttribute ("channel").getIntValue());
                        parameters.add (plugin->getAutomatableParameterByID (item->getStringAttribute ("parameter")));
                    }

                    tellEditAboutChange();
                    sendChangeMessage();
                }
            }
        }
    }
}

juce::StringArray ParameterControlMappings::getPresets() const
{
    juce::StringArray result;

    if (auto xml = edit.engine.getPropertyStorage().getXmlProperty (SettingID::filterControlMappingPresets))
        for (auto e : xml->getChildIterator())
            result.add (e->getStringAttribute ("name"));

    return result;
}

void ParameterControlMappings::deletePreset (int index)
{
    if (auto xml = edit.engine.getPropertyStorage().getXmlProperty (SettingID::filterControlMappingPresets))
    {
        juce::XmlElement xmlCopy (*xml);
        xmlCopy.removeChildElement (xmlCopy.getChildElement (index), true);
        edit.engine.getPropertyStorage().setXmlProperty (SettingID::filterControlMappingPresets, xmlCopy);
    }
}

}} // namespace tracktion { inline namespace engine
