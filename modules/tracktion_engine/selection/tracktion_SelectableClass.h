/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** Represents a type of object that can be selected.

    Each Selectable must be able to provide a SelectableClass for itself, so that
    when there are a bunch of similar objects selected, the class object knows how
    to deal with them as a group.
*/
class SelectableClass
{
public:
    //==============================================================================
    SelectableClass();
    virtual ~SelectableClass();

    static SelectableClass* findClassFor (const Selectable&);
    static SelectableClass* findClassFor (const Selectable*);

    //==============================================================================
    /** Must return a description of this particular group of objects.
        This will be shown on the properties panel.
    */
    virtual juce::String getDescriptionOfSelectedGroup (const SelectableList&);

    /** if it's possible for an object of this class to be selected at the same
        time as an object of the class passed in, this should return true. Default
        is only to be true if the other object is actually this one.
    */
    virtual bool canClassesBeSelectedAtTheSameTime (SelectableClass* otherClass);

    /** This is only called if canClassesBeSelectedAtTheSameTime() has already returned
        true for the other object. Returns true by default.
    */
    virtual bool canObjectsBeSelectedAtTheSameTime (Selectable& object1, Selectable& object2);

    //==============================================================================
    struct AddClipboardEntryParams
    {
        AddClipboardEntryParams (Clipboard& c) : clipboard (c) {}

        SelectableList items;
        Edit* edit = nullptr;
        int editViewID = -1;
        Clipboard& clipboard;
    };

    /** A class should use this to create XML clipboard entries for the given set of items. */
    virtual void addClipboardEntriesFor (AddClipboardEntryParams&);

    /** Deletes this set of objects.
        The partOfCutOperation flag is set if it's being called from SelectableManager::cutSelected()
    */
    virtual void deleteSelected (const SelectableList&, bool partOfCutOperation);

    /** This gives the selected items a first chance to paste the clipboard contents when the
        user presses ctrl-v. If it doesn't want to handle this, it should return false, and the
        edit object will be offered the clipboard
    */
    virtual bool pasteClipboard (const SelectableList& currentlySelectedItems, int editViewID);

    virtual bool canCutSelected (const SelectableList& selectedObjects);

    //==============================================================================
    enum class Relationship
    {
        moveUp, moveDown, moveLeft, moveRight,
        moveToHome, moveToEnd,
        selectAll
    };

    struct SelectOtherObjectsParams
    {
        SelectionManager& selectionManager;
        SelectableList currentlySelectedItems;
        Relationship relationship;
        bool keepExistingItemsSelected = false;
        int editViewID = -1;
    };

    /** Must try to find and select objects that are related to these ones in the specified way. */
    virtual void selectOtherObjects (const SelectOtherObjectsParams&);

    /** if implemented, this should do whatever is appropriate to make these objects
        visible - e.g. scrolling to them, etc.
    */
    virtual void keepSelectedObjectOnScreen (const SelectableList& currentlySelectedObjects);

    static bool areAllObjectsOfUniformType (const SelectableList& selectedObjects);

    //==============================================================================
    struct ClassInstanceBase
    {
        ClassInstanceBase();
        virtual ~ClassInstanceBase();
        virtual SelectableClass* getClassForObject (const Selectable*) = 0;
    };

    template <typename ClassType, typename ObjectType>
    struct ClassInstance  : public ClassInstanceBase
    {
        SelectableClass* getClassForObject (const Selectable* s) override  { return dynamic_cast<const ObjectType*> (s) != nullptr ? &cls : nullptr; }
        ClassType cls;
    };

    #define DECLARE_SELECTABLE_OBJECT_AND_CLASS(ObjectType, ClassType) \
        static SelectableClass::ClassInstance<ClassType, ObjectType> JUCE_JOIN_MACRO (selectableClass ## ClassType, __LINE__);

    #define DECLARE_SELECTABLE_CLASS(ObjectType) \
        DECLARE_SELECTABLE_OBJECT_AND_CLASS (ObjectType, ObjectType ## SelectableClass)

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectableClass)
};


//==============================================================================
class SelectableClassWithVolume
{
public:
    virtual ~SelectableClassWithVolume() {}

    virtual void setVolumeDB (const SelectableList&, float dB, SelectionManager*) = 0;
    virtual float getVolumeDB (const SelectableList&) = 0;
    virtual void resetVolume (const SelectableList&) = 0;
};

//==============================================================================
class SelectableClassWithPan
{
public:
    virtual ~SelectableClassWithPan() {}

    virtual void setPan (const SelectableList&, float pan, SelectionManager*) = 0;
    virtual float getPan (const SelectableList&) = 0;
    virtual void resetPan (const SelectableList&) = 0;
};

} // namespace tracktion_engine
