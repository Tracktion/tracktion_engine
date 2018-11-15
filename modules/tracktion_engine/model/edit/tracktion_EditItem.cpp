/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


EditItem::EditItem (EditItemID id, Edit& ed)
    : edit (ed), itemID (id)
{
}

//==============================================================================
EditItemID EditItemID::fromVar (const juce::var& v)
{
    return fromRawID ((juce::uint64) static_cast<juce::int64> (v));
}

EditItemID EditItemID::fromString (const String& s)
{
    return fromRawID ((juce::uint64) s.getLargeIntValue());
}

EditItemID EditItemID::fromID (const juce::ValueTree& v)
{
    return EditItemID::fromVar (v.getProperty (IDs::id));
}

void EditItemID::writeID (juce::ValueTree& v, juce::UndoManager* um) const
{
    setProperty (v, IDs::id, um);
}

EditItemID EditItemID::fromRawID (juce::uint64 raw)
{
    EditItemID i;
    i.itemID = raw;
    return i;
}

static inline bool isSorted (const std::vector<EditItemID>& ids)
{
    juce::uint64 last = 0;

    for (auto id : ids)
    {
        auto i = id.getRawID();

        if (i < last)
            return false;

        last = i;
    }

    return true;
}

EditItemID EditItemID::findFirstIDNotIn (const std::vector<EditItemID>& existingIDs, juce::uint64 rawID)
{
    jassert (isSorted (existingIDs)); // the vector must be sorted

    for (auto existing : existingIDs)
    {
        auto i = existing.getRawID();

        if (i > rawID)
            break;

        ++rawID;
    }

    return fromRawID (rawID);
}

EditItemID EditItemID::fromProperty (const juce::ValueTree& v, const juce::Identifier& prop)
{
    return fromVar (v.getProperty (prop));
}

EditItemID EditItemID::fromXML (const juce::XmlElement& xml, const char* attributeName)
{
    return fromVar (xml.getStringAttribute (attributeName));
}

EditItemID EditItemID::fromXML (const juce::XmlElement& xml, const juce::Identifier& attributeName)
{
    return fromVar (xml.getStringAttribute (attributeName));
}

void EditItemID::setProperty (ValueTree& v, const juce::Identifier& prop, UndoManager* um) const
{
    v.setProperty (prop, toVar(), um);
}

void EditItemID::setXML (juce::XmlElement& xml, const juce::Identifier& attributeName) const
{
    xml.setAttribute (attributeName, toString());
}

void EditItemID::setXML (juce::XmlElement& xml, const char* attributeName) const
{
    xml.setAttribute (attributeName, toString());
}

juce::var EditItemID::toVar() const
{
    return juce::var ((juce::int64) itemID);
}

juce::String EditItemID::toString() const
{
    return toVar().toString();
}

EditItemID EditItemID::readOrCreateNewID (Edit& edit, const juce::ValueTree& v)
{
    auto i = fromID (v);

    if (i.isValid())
        return i;

    i = edit.createNewItemID();
    juce::ValueTree localTree (v);
    i.writeID (localTree, nullptr);
    return i;
}

juce::Array<EditItemID> EditItemID::parseStringList (const juce::String& list)
{
    Array<EditItemID> trackIDs;
    auto items = StringArray::fromTokens (list, ",|", {});
    trackIDs.ensureStorageAllocated (items.size());

    for (auto& s : items)
        trackIDs.add (fromVar (s));

    return trackIDs;
}

juce::String EditItemID::listToString (const juce::Array<EditItemID>& items)
{
    if (items.isEmpty())
        return {};

    if (items.size() == 1)
        return items.getFirst().toString();

    StringArray ids;
    ids.ensureStorageAllocated (items.size());

    for (auto& i : items)
        ids.add (i.toString());

    return ids.joinIntoString (",");
}

//==============================================================================
struct IDRemapping
{
    static bool isIDDeclaration (juce::StringRef att)
    {
        return att == IDs::id || att == IDs::groupID || att == IDs::linkID;
    }

    static bool isIDReference (const Identifier& parentType, juce::StringRef att)
    {
        for (auto p : { IDs::currentAutoParamPluginID, IDs::currentAutoParamTag,
                        IDs::targetTrack, IDs::sourceTrack, IDs::src, IDs::dst,
                        IDs::pluginID, IDs::rackType, IDs::paramID })
            if (p == att)
                return true;

        if (att == IDs::source && ! Clip::isClipState (parentType))
            return true;

        if (att == IDs::track && parentType == IDs::COMPSECTION)
            return true;

        return false;
    }

    static bool isIDRefOrDecl (const XmlElement& xml, juce::StringRef att)
    {
        return isIDDeclaration (att) || isIDReference (xml.getTagName(), att);
    }

    static bool isIDRefOrDecl (const ValueTree& v, juce::StringRef att)
    {
        return isIDDeclaration (att) || isIDReference (v.getType(), att);
    }

    static bool isIDList (juce::StringRef att)
    {
        return att == IDs::hiddenClips || att == IDs::lockedClips
                || att == IDs::ghostTracks || att == IDs::renderTracks;
    }

    template <typename Visitor>
    static void visitAllIDDecls (const juce::XmlElement& xml, Visitor&& visitor)
    {
        for (int i = 0; i < xml.getNumAttributes(); ++i)
            if (isIDDeclaration (xml.getAttributeName (i)))
                visitor (xml.getAttributeValue (i));

        forEachXmlChildElement (xml, e)
            visitAllIDDecls (*e, visitor);
    }

    template <typename Visitor>
    static void visitAllIDDecls (const juce::ValueTree& v, Visitor&& visitor)
    {
        for (int i = 0; i < v.getNumProperties(); ++i)
        {
            auto propName = v.getPropertyName (i);

            if (isIDDeclaration (propName))
                visitor (v.getProperty (propName));
        }

        for (const auto& child : v)
            visitAllIDDecls (child, visitor);
    }

    using StringToIDMap = std::map<juce::String, EditItemID>;

    static juce::String remapIDList (const String& list, const StringToIDMap& newIDsToApply)
    {
        Array<EditItemID> newItemIDs;

        for (auto oldID : juce::StringArray::fromTokens (list, "|,", {}))
        {
            if (oldID.isNotEmpty())
            {
                auto newID = newIDsToApply.find (oldID);

                if (newID != newIDsToApply.end())
                    newItemIDs.add (newID->second);
            }
        }

        return EditItemID::listToString (newItemIDs);
    }

    static void applyNewIDs (juce::XmlElement& xml, const StringToIDMap& newIDsToApply)
    {
        for (int i = 0; i < xml.getNumAttributes(); ++i)
        {
            auto attName = xml.getAttributeName (i);

            if (isIDRefOrDecl (xml, attName))
            {
                auto oldID = xml.getAttributeValue (i);

                if (oldID.isNotEmpty())
                {
                    auto newID = newIDsToApply.find (oldID);

                    if (newID != newIDsToApply.end())
                        xml.setAttribute (attName, newID->second.toString());
                }
            }
            else if (isIDList (attName))
            {
                xml.setAttribute (attName, remapIDList (xml.getAttributeValue (i), newIDsToApply));
            }

            if (EditItemID::applyNewIDsToExternalXML != nullptr)
                EditItemID::applyNewIDsToExternalXML (xml, attName, newIDsToApply);
        }

        forEachXmlChildElement (xml, e)
            applyNewIDs (*e, newIDsToApply);
    }

    static void applyNewIDs (juce::ValueTree& v, const StringToIDMap& newIDsToApply, UndoManager* um)
    {
        for (int i = 0; i < v.getNumProperties(); ++i)
        {
            auto propName = v.getPropertyName (i);

            if (isIDRefOrDecl (v, propName))
            {
                auto oldID = v.getProperty (propName).toString();

                if (oldID.isNotEmpty())
                {
                    auto newID = newIDsToApply.find (oldID);

                    if (newID != newIDsToApply.end())
                        newID->second.setProperty (v, propName, um);
                    else
                        DBG ("Dangling ID found: " << oldID);
                }
            }
            else if (isIDList (propName))
            {
                v.setProperty (propName, remapIDList (v.getProperty (propName), newIDsToApply), um);
            }

            if (EditItemID::applyNewIDsToExternalValueTree)
                EditItemID::applyNewIDsToExternalValueTree (v, propName, newIDsToApply, um);
        }

        for (auto child : v)
            applyNewIDs (child, newIDsToApply, um);
    }

    static void addItemsToReturnedMap (const StringToIDMap& newIDs, EditItemID::IDMap* result)
    {
        if (result != nullptr)
        {
            for (auto& i : newIDs)
            {
                auto itemID = EditItemID::fromString (i.first);

                if (itemID.isValid())
                    (*result)[itemID] = i.second;
                else
                    jassertfalse; // we're trying to get out a list of ID -> ID, but some
                                  // of the original IDs weren't valid. Could be that they were
                                  // legacy string IDs?
            }
        }
    }
};

std::vector<EditItemID> EditItemID::findAllIDs (const juce::XmlElement& xml)
{
    std::vector<EditItemID> ids;

    IDRemapping::visitAllIDDecls (xml, [&] (const String& oldID)
    {
        auto i = EditItemID::fromString (oldID);

        if (i.isValid())
            ids.push_back (i);
    });

    return ids;
}

std::vector<EditItemID> EditItemID::findAllIDs (const juce::ValueTree& v)
{
    std::vector<EditItemID> ids;

    IDRemapping::visitAllIDDecls (v, [&] (const var& oldID)
    {
        auto i = EditItemID::fromVar (oldID);

        if (i.isValid())
            ids.push_back (i);
    });

    return ids;
}

void EditItemID::remapIDs (juce::XmlElement& xml, std::function<EditItemID()> createNewID, IDMap* remappedIDs)
{
    IDRemapping::StringToIDMap newIDs;

    IDRemapping::visitAllIDDecls (xml, [&] (const String& oldID)
    {
        if (oldID.isNotEmpty())
        {
            auto& remapped = newIDs[oldID];

            if (remapped.isInvalid())
            {
                remapped = createNewID();
                //DBG ("Remapping ID: " << oldID << "  " << remapped.toString());
            }
        }
    });

    IDRemapping::applyNewIDs (xml, newIDs);
    IDRemapping::addItemsToReturnedMap (newIDs, remappedIDs);
}

void EditItemID::remapIDs (juce::ValueTree& v, juce::UndoManager* um,
                           std::function<EditItemID()> createNewID, IDMap* remappedIDs)
{
    IDRemapping::StringToIDMap newIDs;

    IDRemapping::visitAllIDDecls (v, [&] (const var& oldID)
    {
        auto oldIDString = oldID.toString();

        if (oldIDString.isNotEmpty())
        {
            auto& remapped = newIDs[oldIDString];

            if (remapped.isInvalid())
            {
                remapped = createNewID();
                //DBG ("Remapping ID: " << oldIDString << "  " << remapped.toString());
            }
        }
    });

    IDRemapping::applyNewIDs (v, newIDs, um);
    IDRemapping::addItemsToReturnedMap (newIDs, remappedIDs);
}

struct MultipleNewIDGenerator
{
    MultipleNewIDGenerator (Edit& ed, EditItemID::IDMap* remappedIDs)  : edit (ed)
    {
        if (remappedIDs != nullptr)
        {
            alreadyUsed.reserve (alreadyUsed.size() + remappedIDs->size());

            for (auto& i : *remappedIDs)
                alreadyUsed.push_back (i.second);
        }
    }

    EditItemID operator()()
    {
        auto newID = edit.createNewItemID (alreadyUsed);
        alreadyUsed.push_back (newID);
        return newID;
    }

    Edit& edit;
    std::vector<EditItemID> alreadyUsed;
};

void EditItemID::remapIDs (juce::XmlElement& xml, Edit& ed, IDMap* remappedIDs)
{
    MultipleNewIDGenerator idCreator (ed, remappedIDs);
    remapIDs (xml, idCreator, remappedIDs);
}

void EditItemID::remapIDs (juce::ValueTree& v, juce::UndoManager* um, Edit& ed, IDMap* remappedIDs)
{
    MultipleNewIDGenerator idCreator (ed, remappedIDs);
    remapIDs (v, um, idCreator, remappedIDs);
}

//==============================================================================
std::function<void (juce::ValueTree&, const juce::Identifier&, const std::map<juce::String, EditItemID>&, juce::UndoManager*)> EditItemID::applyNewIDsToExternalValueTree;
std::function<void (juce::XmlElement&, const juce::String&, const std::map<juce::String, EditItemID>&)> EditItemID::applyNewIDsToExternalXML;
