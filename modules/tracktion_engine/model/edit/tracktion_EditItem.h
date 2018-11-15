/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

//==============================================================================
/**
    ID for objects of type EditElement - e.g. clips, tracks, plguins, etc
*/
struct EditItemID
{
    EditItemID() = default;
    EditItemID (const EditItemID&) = default;
    EditItemID& operator= (const EditItemID&) = default;

    bool isValid() const noexcept       { return itemID != 0; }
    bool isInvalid() const noexcept     { return itemID == 0; }

    static EditItemID fromID (const juce::ValueTree&);
    static EditItemID readOrCreateNewID (Edit&, const juce::ValueTree&);
    void writeID (juce::ValueTree&, juce::UndoManager*) const;

    static EditItemID fromVar (const juce::var&);
    static EditItemID fromString (const juce::String&);
    static EditItemID fromProperty (const juce::ValueTree&, const juce::Identifier&);
    static EditItemID fromXML (const juce::XmlElement&, const char* attributeName);
    static EditItemID fromXML (const juce::XmlElement&, const juce::Identifier&);

    static EditItemID findFirstIDNotIn (const std::vector<EditItemID>& existingIDsSorted, juce::uint64 startFrom = 1001);

    void setProperty (juce::ValueTree&, const juce::Identifier&, juce::UndoManager*) const;
    void setXML (juce::XmlElement&, const juce::Identifier& attributeName) const;
    void setXML (juce::XmlElement&, const char* attributeName) const;

    juce::var toVar() const;
    juce::String toString() const;

    juce::uint64 getRawID() const noexcept              { return itemID; }
    static EditItemID fromRawID (juce::uint64);

    bool operator== (EditItemID other) const noexcept   { return itemID == other.itemID; }
    bool operator!= (EditItemID other) const noexcept   { return itemID != other.itemID; }
    bool operator<  (EditItemID other) const noexcept   { return itemID <  other.itemID; }

    static juce::Array<EditItemID> parseStringList (const juce::String& list);
    static juce::String listToString (const juce::Array<EditItemID>& list);

    //==============================================================================
    static std::vector<EditItemID> findAllIDs (const juce::XmlElement&);
    static std::vector<EditItemID> findAllIDs (const juce::ValueTree&);

    using IDMap = std::map<EditItemID, EditItemID>;

    static void remapIDs (juce::XmlElement&, std::function<EditItemID()> createNewID, IDMap* remappedIDs = nullptr);
    static void remapIDs (juce::XmlElement&, Edit&, IDMap* remappedIDs = nullptr);
    static void remapIDs (juce::ValueTree&, juce::UndoManager*, std::function<EditItemID()> createNewID, IDMap* remappedIDs = nullptr);
    static void remapIDs (juce::ValueTree&, juce::UndoManager*, Edit&, IDMap* remappedIDs = nullptr);

    //==============================================================================
    /** Callback that can be set in order to update any reassigned IDs in ValueTree client code. */
    static std::function<void (juce::ValueTree&, const juce::Identifier& /*propertyName*/,
                               const std::map<juce::String, EditItemID>&, juce::UndoManager*)> applyNewIDsToExternalValueTree;

    /** Callback that can be set in order to update any reassigned IDs in XML client code. */
    static std::function<void (juce::XmlElement&, const juce::String& /*propertyName*/,
                               const std::map<juce::String, EditItemID>&)> applyNewIDsToExternalXML;

private:
    juce::uint64 itemID = 0;
};

//==============================================================================
/**
    Base class for objects that live inside an edit - e.g. clips, tracks, plguins, etc
*/
class EditItem
{
public:
    EditItem (EditItemID, Edit&);
    virtual ~EditItem() = default;

    //==============================================================================
    Edit& edit;

    /** Every EditItem has an ID which is unique within the edit. */
    const EditItemID itemID;

    virtual juce::String getName() = 0;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditItem)
};


//==============================================================================
/** */
template<typename EditItemType>
struct EditItemCache
{
    EditItemCache() = default;
    ~EditItemCache() { jassert (knownEditItems.empty()); }

    EditItemType* findItem (EditItemID id) const
    {
        auto o = knownEditItems.find (id);

        if (o != knownEditItems.cend())
            return o->second;

        return {};
    }

    template<typename Visitor>
    void visitItems (Visitor&& visitor) const
    {
        for (auto iter : knownEditItems)
            std::forward<Visitor> (visitor)(iter.second);
    }

    void addItem (EditItemType& item)
    {
        jassert (knownEditItems.find (item.itemID) == knownEditItems.end());

        if (item.itemID.isValid())
            knownEditItems[item.itemID] = &item;
    }

    void removeItem (EditItemType& item)
    {
        if (item.itemID.isValid())
            knownEditItems.erase (item.itemID);
    }

private:
    std::map<EditItemID, EditItemType*> knownEditItems;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditItemCache)
};

} // namespace tracktion_engine

namespace juce
{
    template <>
    struct VariantConverter<tracktion_engine::EditItemID>
    {
        static tracktion_engine::EditItemID fromVar (const var& v)   { return tracktion_engine::EditItemID::fromVar (v); }
        static var toVar (const tracktion_engine::EditItemID& v)     { return var ((int64) v.getRawID()); }
    };
}
