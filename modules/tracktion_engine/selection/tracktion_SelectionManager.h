/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** Manages a list of items that are currently selected.
    @see Selectable
*/
class SelectionManager  : public juce::ChangeBroadcaster,
                          private SelectableListener
{
public:
    //==============================================================================
    SelectionManager (Engine&);
    ~SelectionManager();

    //==============================================================================
    int getNumObjectsSelected() const;
    Selectable* getSelectedObject (int index) const;
    bool isSelected (const Selectable*) const;
    bool isSelected (const Selectable&) const;
    const SelectableList& getSelectedObjects() const;
    SelectableClass* getFirstSelectableClass() const;

    void selectOnly (Selectable*);
    void selectOnly (Selectable&);
    void addToSelection (Selectable*);
    void addToSelection (Selectable&);
    void select (Selectable*, bool addToCurrentSelection);
    void select (Selectable&, bool addToCurrentSelection);
    void select (const SelectableList&);

    void deselectAll();
    void deselect (Selectable*);

    /** Selects related objects, e.g. the thing to the right of the current thing, etc. */
    void selectOtherObjects (SelectableClass::Relationship, bool keepOldItemsSelected);

    //==============================================================================
    template <typename SelectableClass>
    bool containsType() const
    {
        return selected.containsType<SelectableClass>();
    }

    template <typename SelectableClass>
    int getNumObjectsSelectedOfType() const
    {
        int numOfType = 0;

        for (auto s : selected)
            if (dynamic_cast<SelectableClass*> (s) != nullptr)
                ++numOfType;

        return numOfType;
    }

    template <typename SelectableClass>
    juce::Array<SelectableClass*> getItemsOfType() const
    {
        juce::Array<SelectableClass*> items;

        for (auto s : selected)
            if (auto item = dynamic_cast<SelectableClass*> (s))
                items.add (item);

        return items;
    }

    template <typename SelectableClass>
    SelectableClass* getFirstItemOfType() const
    {
        for (auto s : selected)
            if (auto item = dynamic_cast<SelectableClass*> (s))
                return item;

        return {};
    }

    //==============================================================================
    void deleteSelected();
    bool copySelected();
    bool cutSelected();

    /** Offers the selected things the chance to paste the contents of the clipboard
        onto themselves, returns false if they didn't handle it.
    */
    bool pasteSelected();

    /** Scrolls whatever is necessary to keep the selected stuff visible. */
    void keepSelectedObjectsOnScreen();

    void refreshDetailComponent();

    static void refreshAllPropertyPanels();
    static void deselectAllFromAllWindows();

    //==============================================================================
    struct ChangedSelectionDetector
    {
        bool isFirstChangeSinceSelection (SelectionManager* sm)
        {
            if (sm != nullptr)
            {
                int newCount = sm->selectionChangeCount;

                if (lastSelectionChangeCount != newCount)
                {
                    lastSelectionChangeCount = newCount;
                    return true;
                }
            }

            return false;
        }

        void reset()
        {
            lastSelectionChangeCount = 0;
        }

        int lastSelectionChangeCount = 0;
    };

    // This gets incremented each time the selection changes
    int selectionChangeCount = 0;

    //==============================================================================
    struct ScopedSelectionState
    {
        ScopedSelectionState (SelectionManager&);
        ~ScopedSelectionState();

    private:
        SelectionManager& manager;
        SelectableList selected;

        JUCE_DECLARE_NON_COPYABLE (ScopedSelectionState)
    };

    //==============================================================================
    // (used internally)
    void selectableObjectChanged (Selectable*) override;
    void selectableObjectAboutToBeDeleted (Selectable*) override;

    struct Iterator
    {
        Iterator() noexcept;
        bool next();
        SelectionManager* get() const;
        SelectionManager* operator->() const   { return get(); }

        int index;
    };

    //==============================================================================
    // This is a utility base-class that Components can inherit from, and which can then
    // be found by calling SelectionManager::findSelectionManager()
    struct ComponentWithSelectionManager
    {
        ComponentWithSelectionManager() {}
        virtual ~ComponentWithSelectionManager() {}

        virtual SelectionManager* getSelectionManager() = 0;
    };

    // walks up the parent hierarchy of a component, looking for a ComponentWithSelectionManager,
    // and if it finds one, returns its SelectionManager.
    static SelectionManager* findSelectionManager (const juce::Component*);
    static SelectionManager* findSelectionManager (const juce::Component&);

    //==============================================================================
    static SelectionManager* findSelectionManagerContaining (const Selectable*);
    static SelectionManager* findSelectionManagerContaining (const Selectable&);

    /** If this SelectionManager is being used to represent items inside a particular view of an edit,
        this id should be set so you can find it by iterating the SelectionManagers.
    */
    juce::WeakReference<Edit> edit;
    int editViewID = -1;
    EditInsertPoint* insertPoint = nullptr;

    Engine& engine;

private:
    //==============================================================================
    SelectableList selected;
    friend class Selectable;
    juce::CriticalSection lock;

    void selectionChanged();
    void clearList();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectionManager)
};

} // namespace tracktion_engine
