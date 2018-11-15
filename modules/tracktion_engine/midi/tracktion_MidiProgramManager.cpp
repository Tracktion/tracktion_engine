/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct BankSet
{
    BankSet() noexcept {}
    BankSet (const juce::String& nm) noexcept : name (nm) {}

    BankSet (int high, int low, const juce::String& name_) noexcept
       : name (name_), hi (high), lo (low)
    {}

    juce::String name;
    juce::StringArray patches;
    juce::Array<int> patchNumbers;
    int hi = 0, lo = 0;
};

struct ProgramSet
{
    juce::String name;
    juce::OwnedArray<BankSet> banks;
};

juce::XmlElement* exportPogramSet (ProgramSet& set)
{
    if (set.banks.isEmpty())
        return {};

    auto rootXml = new juce::XmlElement ("ProgramSet");
    rootXml->setAttribute ("name", set.name);

    for (auto bankSet : set.banks)
    {
        auto bankXml = rootXml->createNewChildElement ("bank");
        bankXml->setAttribute ("name", bankSet->name);
        bankXml->setAttribute ("high", bankSet->hi);
        bankXml->setAttribute ("low", bankSet->lo);

        for (int j = 0; j < bankSet->patches.size(); ++j)
        {
            auto programXml = bankXml->createNewChildElement ("program");
            programXml->setAttribute ("name", bankSet->patches[j]);
            programXml->setAttribute ("number", bankSet->patchNumbers[j]);
        }
    }

    return rootXml;
}

BankSet* getBankSet (ProgramSet& set, const juce::String& name)
{
    for (int i = 0; i < set.banks.size(); ++i)
        if (set.banks.getUnchecked (i)->name == name)
            return set.banks.getUnchecked (i);

    return set.banks.add (new BankSet (name));
}

juce::XmlElement* convertMidnamToXml (const juce::File& src)
{
    juce::String manufacturer;
    juce::StringArray models;

    std::unique_ptr<juce::XmlElement> rootXml (juce::XmlDocument::parse (src));

    if (rootXml == nullptr)
    {
        // might be wacky corrupt file, search for xml
        juce::MemoryBlock mb;
        src.loadFileAsData (mb);

        for (juce::uint32 i = 0; i < mb.getSize(); ++i)
            if (mb[i] == 0)
                mb[i] = 32;

        auto str = mb.toString();

        int firstidx = str.indexOf("<");
        int lastidx  = str.lastIndexOf(">");

        if (firstidx != -1 && lastidx != -1)
            str = str.substring(firstidx, lastidx);

        rootXml.reset (juce::XmlDocument::parse (str));

        if (rootXml == nullptr)
            return {};
    }

    auto devXml = rootXml->getChildByName ("MasterDeviceNames");

    if (devXml == nullptr)
        return {};

    if (auto manufacturerXml = devXml->getChildByName ("Manufacturer"))
        manufacturer += manufacturerXml->getAllSubText();

    forEachXmlChildElementWithTagName (*devXml, modelXml, "Model")
        models.add (modelXml->getAllSubText());

    ProgramSet programSet;
    programSet.name = manufacturer;

    for (int i = 0; i < models.size(); ++i)
        programSet.name += " " + models[i];

    programSet.name.trim();

    forEachXmlChildElementWithTagName (*devXml, patchNameListXml, "PatchNameList")
    {
        auto name = patchNameListXml->getStringAttribute("Name");
        auto bankSet = getBankSet (programSet, name);

        forEachXmlChildElementWithTagName (*patchNameListXml, patchXml, "Patch")
        {
            auto patchName = patchXml->getStringAttribute ("Name");

            if (patchName.isNotEmpty())
            {
                int num = patchXml->getIntAttribute("ProgramChange");

                if (! bankSet->patchNumbers.contains (num))
                {
                    bankSet->patches.add (patchName);
                    bankSet->patchNumbers.add (num);
                }
            }
        }
    }

    forEachXmlChildElementWithTagName (*devXml, channelNameSetXml, "ChannelNameSet")
    {
        forEachXmlChildElementWithTagName (*channelNameSetXml, patchBankXml, "PatchBank")
        {
            auto name = patchBankXml->getStringAttribute ("Name");
            auto bankSet = getBankSet (programSet, name);

            if (auto midiCommandsXml = patchBankXml->getChildByName ("MIDICommands"))
            {
                forEachXmlChildElementWithTagName (*midiCommandsXml, controlChangeXml, "ControlChange")
                {
                    if (controlChangeXml->hasAttribute ("Control") && controlChangeXml->hasAttribute("Value"))
                    {
                        switch (controlChangeXml->getIntAttribute ("Control"))
                        {
                            case 0 : bankSet->hi = controlChangeXml->getIntAttribute ("Value"); break;
                            case 32: bankSet->lo = controlChangeXml->getIntAttribute ("Value"); break;
                        }
                    }
                }
            }

            if (auto patchNameListXml = patchBankXml->getChildByName ("PatchNameList"))
            {
                forEachXmlChildElementWithTagName (*patchNameListXml, patchXml, "Patch")
                {
                    auto patchName = patchXml->getStringAttribute("Name");

                    if (patchName.isNotEmpty())
                    {
                        const int num = patchXml->getIntAttribute ("ProgramChange");

                        if (! bankSet->patchNumbers.contains (num))
                        {
                            bankSet->patches.add (patchName);
                            bankSet->patchNumbers.add (num);
                        }
                    }
                }
            }
        }
    }

    for (int i = programSet.banks.size(); --i >= 0;)
        if (auto b = programSet.banks.getUnchecked (i))
            if (b->patches.isEmpty())
                programSet.banks.remove (i);

    return exportPogramSet (programSet);
}

//==============================================================================
MidiProgramManager::MidiProgramSet::MidiProgramSet()
{
    for (int i = 0; i < 16; ++i)
    {
        midiBanks[i].id = i << 8;
        midiBanks[i].name = TRANS("Bank") + " " + juce::String (i + 1);
    }
}

juce::XmlElement* MidiProgramManager::MidiBank::createXml()
{
    auto n = new juce::XmlElement ("bank");
    n->setAttribute ("name", name);
    n->setAttribute ("low", id % 256);
    n->setAttribute ("high", id / 256);

    for (auto& p : programNames)
    {
        auto currentProgramXml = n->createNewChildElement ("program");
        currentProgramXml->setAttribute ("number", p.first);
        currentProgramXml->setAttribute ("name", p.second);
    }

    return n;
}

void MidiProgramManager::MidiBank::updateFromXml (const juce::XmlElement& n)
{
    name = n.getStringAttribute ("name");
    id = n.getIntAttribute ("high") * 256 + n.getIntAttribute ("low");

    forEachXmlChildElementWithTagName (n, programXml, "program")
    {
        programNames[programXml->getIntAttribute ("number")]
            = programXml->getStringAttribute ("name");
    }
}

//==============================================================================
juce::XmlElement* MidiProgramManager::MidiProgramSet::createXml()
{
    auto n = new juce::XmlElement ("ProgramSet");
    n->setAttribute ("name", name);
    n->setAttribute ("zeroBased", zeroBased);

    for (int i = 0; i < 16; ++i)
        n->addChildElement (midiBanks[i].createXml());

    return n;
}

void MidiProgramManager::MidiProgramSet::updateFromXml (const juce::XmlElement& n)
{
    name = n.getStringAttribute ("name");
    zeroBased = n.getBoolAttribute ("zeroBased", false);

    int idx = 0;

    forEachXmlChildElement (n, currentChildXml)
    {
        if (currentChildXml->hasTagName ("bank"))
            midiBanks[idx++].updateFromXml (*currentChildXml);

        if (idx >= juce::numElementsInArray (midiBanks))
            break;
    }
}

//==============================================================================
MidiProgramManager::MidiProgramManager (Engine& e)
    : engine (e)
{
    auto xml = e.getPropertyStorage().getXmlProperty (SettingID::midiProgramManager);

    if (xml != nullptr)
    {
        forEachXmlChildElement (*xml, currentSetXml)
        {
            auto* currentSet = new MidiProgramSet();
            currentSet->updateFromXml (*currentSetXml);
            programSets.add (currentSet);
        }
    }

    if (xml == nullptr || ! xml->getBoolAttribute ("createdRootGroup", false))
    {
        auto* defaultSet = new MidiProgramSet();
        defaultSet->name = TRANS("Custom");
        programSets.add (defaultSet);

        saveAll();
    }
}

void MidiProgramManager::saveAll()
{
    juce::XmlElement xml ("MidiProgramManager");
    xml.setAttribute ("createdRootGroup", true);

    for (int i = 0; i < programSets.size(); ++i)
    {
        auto* currentSet = programSets[i];
        auto* currentSetXml = currentSet->createXml();
        xml.addChildElement (currentSetXml);
    }

    engine.getPropertyStorage().setXmlProperty (SettingID::midiProgramManager, xml);
}

juce::StringArray MidiProgramManager::getMidiProgramSetNames()
{
    juce::StringArray names;
    names.add (TRANS("General MIDI"));

    for (int i = 0; i < programSets.size(); ++i)
        names.add (programSets.getUnchecked (i)->name);

    return names;
}

juce::String MidiProgramManager::getProgramName (int set, int bank, int program)
{
    if (set == 0)
        return TRANS(juce::MidiMessage::getGMInstrumentName (program));

    if (auto p = programSets[set - 1])
    {
        if (bank >= 0 && bank < juce::numElementsInArray (p->midiBanks))
        {
            auto n = p->midiBanks[bank].programNames.find (program);

            if (n != p->midiBanks[bank].programNames.end())
                return n->second;
        }
    }

    return getDefaultName (bank, program, programSets[set - 1]->zeroBased);
}

juce::String MidiProgramManager::getBankName (int set, int bank)
{
    if (auto p = programSets[set - 1])
    {
        if (bank >= 0 && bank < juce::numElementsInArray (p->midiBanks))
        {
            auto name = p->midiBanks[bank].name;

            if (name.isNotEmpty())
                return name;
        }
    }

    return TRANS("Bank") + " " + juce::String (bank + 1);
}

int MidiProgramManager::getBankID (int set, int bank)
{
    if (auto p = programSets[set - 1])
        if (bank >= 0 && bank < juce::numElementsInArray (p->midiBanks))
            return p->midiBanks[bank].id;

    return bank;
}

void MidiProgramManager::setBankID (int set, int bank, int id)
{
    if (auto p = programSets[set - 1])
        if (bank >= 0 && bank < juce::numElementsInArray (p->midiBanks))
            p->midiBanks[bank].id = id;
}

bool MidiProgramManager::isZeroBased (int set)
{
    if (auto p = programSets[set - 1])
        return p->zeroBased;

    return false;
}

void MidiProgramManager::setZeroBased (int set, bool zeroBased)
{
    if (auto p = programSets[set - 1])
        p->zeroBased = zeroBased;
}

void MidiProgramManager::setProgramName (int set, int bank, int program, const juce::String& newName)
{
    if (auto p = programSets[set - 1])
        if (bank >= 0 && bank < juce::numElementsInArray (p->midiBanks))
            p->midiBanks[bank].programNames[program] = newName;
}

void MidiProgramManager::setBankName (int set, int bank, const juce::String& newName)
{
    if (auto p = programSets[set - 1])
        if (bank >= 0 && bank < juce::numElementsInArray (p->midiBanks))
            p->midiBanks[bank].name = newName;
}

void MidiProgramManager::clearProgramName (int set, int bank, int program)
{
    if (auto p = programSets[set - 1])
        if (bank >= 0 && bank < juce::numElementsInArray (p->midiBanks))
            p->midiBanks[bank].programNames.erase (program);
}

juce::String MidiProgramManager::getDefaultName (int, int program, bool zeroBased)
{
    return TRANS("Patch") + " "
            + juce::String (program % 128 + (zeroBased ? 1 : 0));
}

juce::String MidiProgramManager::getDefaultCustomName()
{
    for (int i = 0; i < programSets.size(); ++i)
        if (programSets.getUnchecked (i)->name == TRANS("Custom"))
            return TRANS("Custom");

    return TRANS("General MIDI");
}

int MidiProgramManager::getSetIndex (const juce::String& setName)
{
    for (int i = 0; i < programSets.size(); ++i)
        if (programSets.getUnchecked (i)->name == setName)
            return i + 1;

    return 0;
}

bool MidiProgramManager::doesSetExist (const juce::String& name) const noexcept
{
    if (name == TRANS("General MIDI"))
        return true;

    for (int i = 0; i < programSets.size(); ++i)
        if (programSets.getUnchecked (i)->name == name)
            return true;

    return false;
}

bool MidiProgramManager::canEditProgramSet (int set) const noexcept
{
    return set > 0;
}

bool MidiProgramManager::canDeleteProgramSet (int set) const noexcept
{
    return set > 0;
}

void MidiProgramManager::addNewSet (const juce::String& name)
{
    auto newSet = programSets.add (new MidiProgramSet());
    newSet->name = name;
    saveAll();
}

void MidiProgramManager::addNewSet (const juce::String& name, const juce::XmlElement& element)
{
    auto newSet = programSets.add (new MidiProgramSet());
    newSet->updateFromXml (element);
    newSet->name = name;
    saveAll();
}

void MidiProgramManager::deleteSet (int set)
{
    programSets.remove (set - 1);
    saveAll();
}

bool MidiProgramManager::importSet (int set, const juce::File& file)
{
    bool ok = false;

    std::unique_ptr<juce::XmlElement> xml;

    if (file.hasFileExtension("midnam"))
        xml.reset (convertMidnamToXml (file));
    else
        xml.reset (juce::XmlDocument::parse (file));

    if (xml != nullptr)
    {
        if (auto s = programSets[set - 1])
        {
            auto oldName = s->name;
            s->updateFromXml (*xml);
            s->name = oldName;
            ok = true;
        }
    }

    saveAll();
    return ok;
}

bool MidiProgramManager::exportSet (int set, const juce::File& file)
{
    std::unique_ptr<juce::XmlElement> xml;

    if (auto programSet = programSets[set - 1])
        xml.reset (programSet->createXml());

    return xml != nullptr && xml->writeToFile (file, {});
}

static juce::File getPatchNamesZipFile (Engine& engine)
{
    auto presetZipFile = engine.getTemporaryFileManager().getTempDirectory()
                            .getChildFile ("patchnames.zip");

    if (! presetZipFile.exists())
    {
        presetZipFile.getParentDirectory().createDirectory();
        presetZipFile.replaceWithData (TracktionBinaryData::patchnames_zip, TracktionBinaryData::patchnames_zipSize);
    }

    return presetZipFile;
}

juce::StringArray MidiProgramManager::getListOfPresets (Engine& engine)
{
    juce::StringArray presets;
    juce::ZipFile zf (getPatchNamesZipFile (engine));

    for (int i = 0; i < zf.getNumEntries(); ++i)
    {
        auto entry = zf.getEntry(i);
        auto lastDot = entry->filename.lastIndexOfChar ('.');

        if (lastDot >= 0)
            presets.add (entry->filename.substring (0, lastDot));
        else
            presets.add (entry->filename);
    }

    presets.sort (true);

    return presets;
}

juce::String MidiProgramManager::getPresetXml (juce::String presetName)
{
    juce::ZipFile zf (getPatchNamesZipFile (engine));

    for (int i = 0; i < zf.getNumEntries(); ++i)
    {
        auto entry = zf.getEntry(i);
        auto lastDot = entry->filename.lastIndexOfChar ('.');
        auto curName = (lastDot >= 0) ? entry->filename.substring (0, lastDot)
                                      : entry->filename;

        if (curName == presetName)
        {
            std::unique_ptr<juce::InputStream> is (zf.createStreamForEntry(i));

            if (is != nullptr)
                return is->readEntireStreamAsString();
        }
    }

    return {};
}
