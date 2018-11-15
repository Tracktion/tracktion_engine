/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct SelectableUpdateTimer  : public AsyncUpdater,
                                private DeletedAtShutdown
{
    SelectableUpdateTimer() {}

    void add (Selectable* s)
    {
        const ScopedLock sl (lock);
        selectables.add (s);
    }

    void remove (Selectable* s)
    {
        const ScopedLock sl (lock);
        selectables.removeValue (s);
    }

    bool isValid (const Selectable* s) const
    {
        const ScopedLock sl (lock);
        return selectables.contains (const_cast<Selectable*> (s));
    }

    void handleAsyncUpdate() override
    {
        CRASH_TRACER
        Array<Selectable*> needingUpdate;

        {
            const ScopedLock sl (lock);

            for (auto s : selectables)
                if (s->needsAnUpdate)
                    needingUpdate.add (s);
        }

        for (auto s : needingUpdate)
            if (isValid (s))
                s->sendChangeCallbackToListenersIfNeeded();
    }

    CriticalSection listenerLock;

private:
    SortedSet<Selectable*> selectables;
    CriticalSection lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectableUpdateTimer)
};

static SelectableUpdateTimer* updateTimerInstance = nullptr;

void Selectable::initialise()
{
    if (updateTimerInstance == nullptr)
        updateTimerInstance = new SelectableUpdateTimer();
}

//==============================================================================
Selectable::Selectable()
{
    if (updateTimerInstance)
        updateTimerInstance->add (this);
}

Selectable::~Selectable()
{
    masterReference.clear();

    if (! hasNotifiedListenersOfDeletion)
    {
        // must call notifyListenersOfDeletion() in the innermost subclass's destructor!
        jassertfalse;

        notifyListenersOfDeletion();
    }
}

void Selectable::selectionStatusChanged (bool)
{
}

void Selectable::changed()
{
    if (selectableListeners.size() > 0)
    {
        needsAnUpdate = true;
        updateTimerInstance->triggerAsyncUpdate();
    }
}

void Selectable::cancelAnyPendingUpdates()
{
    needsAnUpdate = false;
}

bool Selectable::isSelectableValid (const Selectable* s) noexcept
{
    return s != nullptr && updateTimerInstance->isValid (s);
}

void Selectable::addSelectableListener (SelectableListener* l)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    jassert (l != nullptr);
    jassert (! isCallingListeners);
    selectableListeners.add (l);
}

void Selectable::removeSelectableListener (SelectableListener* l)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    jassert (! isCallingListeners);
    selectableListeners.remove (l);
}

void Selectable::sendChangeCallbackToListenersIfNeeded()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    needsAnUpdate = false;

    const ScopedValueSetter<bool> svs (isCallingListeners, true);

    WeakRef self (this);

    selectableListeners.call ([self] (SelectableListener& l)
                              {
                                  if (auto s = self.get())
                                      l.selectableObjectChanged (s);
                                  else
                                      jassertfalse;
                              });
}

void Selectable::propertiesChanged()
{
    for (SelectionManager::Iterator sm; sm.next();)
    {
        if (sm->isSelected (this))
        {
            auto list = sm->getSelectedObjects();
            sm->deselectAll();

            for (auto s : list)
                sm->select (s, true);
        }
    }
}

void Selectable::notifyListenersOfDeletion()
{
    if (! hasNotifiedListenersOfDeletion)
    {
        hasNotifiedListenersOfDeletion = true;

        updateTimerInstance->remove (this);

        if (! selectableListeners.isEmpty())
        {
            CRASH_TRACER

            WeakRef self (this);

            selectableListeners.call ([self] (SelectableListener& l)
                                      {
                                          if (auto s = self.get())
                                              l.selectableObjectAboutToBeDeleted (s);
                                          else
                                              jassertfalse;
                                      });
        }

        selectableAboutToBeDeleted();
        deselect();
    }
}

void Selectable::deselect()
{
    for (SelectionManager::Iterator sm; sm.next();)
        sm->deselect (this);
}

//==============================================================================
SelectableClass::SelectableClass() {}
SelectableClass::~SelectableClass() {}

static Array<SelectableClass::ClassInstanceBase*>& getAllSelectableClasses()
{
    static Array<SelectableClass::ClassInstanceBase*> classes;
    return classes;
}

SelectableClass::ClassInstanceBase::ClassInstanceBase()   { getAllSelectableClasses().add (this); }
SelectableClass::ClassInstanceBase::~ClassInstanceBase()  { getAllSelectableClasses().removeAllInstancesOf (this); }

SelectableClass* SelectableClass::findClassFor (const Selectable& s)
{
   #if ! JUCE_DEBUG
    for (auto cls : getAllSelectableClasses())
        if (auto c = cls->getClassForObject (&s))
            return c;
   #else
    SelectableClass* result = nullptr;

    for (auto cls : getAllSelectableClasses())
    {
        if (auto c = cls->getClassForObject (&s))
        {
            if (result == nullptr)
                result = c;
            else
                jassertfalse; // more than one SelectableClass thinks it applies to this object
        }
    }

    if (result != nullptr)
        return result;
   #endif

    jassertfalse;
    return {};
}

SelectableClass* SelectableClass::findClassFor (const Selectable* s)
{
    return s != nullptr ? findClassFor (*s) : nullptr;
}

//==============================================================================
String SelectableClass::getDescriptionOfSelectedGroup (const SelectableList& selectedObjects)
{
    if (selectedObjects.size() == 1)
    {
        if (auto s = selectedObjects.getFirst())
        {
            jassert (Selectable::isSelectableValid (s));
            return s->getSelectableDescription();
        }
    }

    StringArray names;

    for (auto o : selectedObjects)
        if (o != nullptr)
            names.addIfNotAlreadyThere (o->getSelectableDescription());

    if (names.size() == 1)
        return String (TRANS("123 Objects of Type: XYYZ"))
               .replace ("123", String (selectedObjects.size()))
               .replace ("XYYZ", names[0]);

    return String (TRANS("123 Objects"))
            .replace ("123", String (selectedObjects.size()));
}

void SelectableClass::deleteSelected (const SelectableList&, bool) {}
void SelectableClass::addClipboardEntriesFor (AddClipboardEntryParams&) {}

bool SelectableClass::pasteClipboard (const SelectableList&, int)
{
    return false;
}

bool SelectableClass::canClassesBeSelectedAtTheSameTime (SelectableClass* otherClass)
{
    return otherClass == this;
}

bool SelectableClass::canObjectsBeSelectedAtTheSameTime (Selectable&, Selectable&)
{
    return true;
}

void SelectableClass::selectOtherObjects (const SelectableClass::SelectOtherObjectsParams&)
{
}

void SelectableClass::keepSelectedObjectOnScreen (const SelectableList&)
{
}

bool SelectableClass::canCutSelected (const SelectableList&)
{
    return true;
}

bool SelectableClass::areAllObjectsOfUniformType (const SelectableList& list)
{
    if (list.size() <= 1)
        return true;

    auto s1 = list.getUnchecked (0);

    for (int i = list.size(); --i > 0;)
    {
        auto s2 = list.getUnchecked (i);

        if (typeid (*s1) != typeid (*s2))
            return false;
    }

    return true;
}

//==============================================================================
static Array<SelectionManager*> allManagers;

SelectionManager::SelectionManager (Engine& e) : ChangeBroadcaster(), engine (e)
{
    allManagers.add (this);
}

SelectionManager::~SelectionManager()
{
    clearList();
    allManagers.removeAllInstancesOf (this);
}

void SelectionManager::selectionChanged()
{
    ++selectionChangeCount;
    sendChangeMessage();
}

SelectableClass* SelectionManager::getFirstSelectableClass() const
{
    return SelectableClass::findClassFor (selected.getFirst());
}

void SelectionManager::clearList()
{
    for (auto s : selected)
        if (Selectable::isSelectableValid (s))
            s->removeSelectableListener (this);

    selected.clear();
}

SelectionManager::Iterator::Iterator() noexcept  : index (-1) {}

bool SelectionManager::Iterator::next()
{
    return isPositiveAndBelow (++index, allManagers.size());
}

SelectionManager* SelectionManager::Iterator::get() const
{
    return allManagers[index];
}

int SelectionManager::getNumObjectsSelected() const
{
    return selected.size();
}

Selectable* SelectionManager::getSelectedObject (int index) const
{
    const ScopedLock sl (lock);
    return selected [index];
}

const SelectableList& SelectionManager::getSelectedObjects() const
{
    return selected;
}

bool SelectionManager::isSelected (const Selectable* object) const
{
    return object != nullptr && isSelected (*object);
}

bool SelectionManager::isSelected (const Selectable& object) const
{
    return selected.contains (const_cast<Selectable*> (&object));
}

void SelectionManager::deselectAll()
{
    const ScopedLock sl (lock);

    if (selected.size() > 0)
    {
        for (auto s : SelectableList (selected))
            if (Selectable::isSelectableValid (s))
                s->selectionStatusChanged (false);

        clearList();
        selectionChanged();
    }
}

static bool canSelectAtTheSameTime (const SelectableList& selected, Selectable& newItem)
{
    auto newItemClass = SelectableClass::findClassFor (newItem);

    for (int i = 0; i < selected.size(); ++i)
    {
        auto s = selected.getUnchecked (i);

        if (auto currentClass = selected.getSelectableClass (i))
            if (! (currentClass->canClassesBeSelectedAtTheSameTime (newItemClass)
                    && currentClass->canObjectsBeSelectedAtTheSameTime (*s, newItem)))
                return false;
    }

    return true;
}

void SelectionManager::selectOnly (Selectable* s)       { select (s, false); }
void SelectionManager::selectOnly (Selectable& s)       { select (s, false); }
void SelectionManager::addToSelection (Selectable* s)   { select (s, true); }
void SelectionManager::addToSelection (Selectable& s)   { select (s, true); }

void SelectionManager::select (Selectable* s, bool addToCurrentSelection)
{
    if (s != nullptr)
        select (*s, addToCurrentSelection);
}

void SelectionManager::select (Selectable& s, bool addToCurrentSelection)
{
    const ScopedLock sl (lock);

    if (! Selectable::isSelectableValid (&s))
    {
        selected.removeAllInstancesOf (&s);
        return;
    }

    if (! selected.contains (&s))
    {
        addToCurrentSelection = addToCurrentSelection
                                 && canSelectAtTheSameTime (selected, s);

        if (! addToCurrentSelection)
            deselectAll();

        selected.add (&s);
        s.selectionStatusChanged (true);
        selectionChanged();
        s.addSelectableListener (this);
    }
    else if (! addToCurrentSelection)
    {
        for (int i = selected.size(); --i >= 0;)
            if (selected.getUnchecked (i) != &s)
                deselect (selected.getUnchecked (i));
    }
}

void SelectionManager::select (const SelectableList& list)
{
    if (list != selected)
    {
        deselectAll();

        for (auto s : list)
            select (s, true);
    }
}

void SelectionManager::deselect (Selectable* s)
{
    const ScopedLock sl (lock);
    auto index = selected.indexOf (s);

    if (index >= 0)
    {
        s->removeSelectableListener (this);
        selected.remove (index);
        s->selectionStatusChanged (false);
        selectionChanged();
    }
}

void SelectionManager::selectableObjectChanged (Selectable*)
{
}

void SelectionManager::selectableObjectAboutToBeDeleted (Selectable* s)
{
    const ScopedLock sl (lock);
    auto index = selected.indexOf (s);

    if (index >= 0)
    {
        selected.remove (index);
        selectionChanged();
    }
}

void SelectionManager::refreshDetailComponent()
{
    selectionChanged();
}

void SelectionManager::refreshAllPropertyPanels()
{
    for (SelectionManager::Iterator sm; sm.next();)
        sm->refreshDetailComponent();
}

void SelectionManager::deselectAllFromAllWindows()
{
    for (SelectionManager::Iterator sm; sm.next();)
        sm->deselectAll();
}

SelectionManager* SelectionManager::findSelectionManagerContaining (const Selectable* s)
{
    for (SelectionManager::Iterator sm; sm.next();)
        if (sm->isSelected (s))
            return sm.get();

    return {};
}

SelectionManager* SelectionManager::findSelectionManagerContaining (const Selectable& s)
{
    return findSelectionManagerContaining (&s);
}

//==============================================================================
bool SelectionManager::copySelected()
{
    const ScopedLock sl (lock);

    // only allow groups of the same type to be selected at once.
    if (auto cls = getFirstSelectableClass())
    {
        auto& clipboard = *Clipboard::getInstance();

        SelectableClass::AddClipboardEntryParams clipboardParams (clipboard);
        clipboardParams.items = selected;

        if (editViewID != -1)
        {
            clipboardParams.edit = edit;
            clipboardParams.editViewID = editViewID;
        }

        clipboard.clear();
        cls->addClipboardEntriesFor (clipboardParams);

        return ! clipboard.isEmpty();
    }

    return false;
}

void SelectionManager::deleteSelected()
{
    const ScopedLock sl (lock);

    if (auto cls = getFirstSelectableClass())
        // use a local copy of the list, as it will change as things get deleted + deselected
        cls->deleteSelected (SelectableList (selected), false);
}

bool SelectionManager::cutSelected()
{
    const ScopedLock sl (lock);

    if (auto cls = getFirstSelectableClass())
    {
        if (cls->canCutSelected (selected) && copySelected())
        {
            // use a local copy of the list, as it will change as things get deleted + deselected
            cls->deleteSelected (SelectableList (selected), true);
            return true;
        }
    }

    return false;
}

bool SelectionManager::pasteSelected()
{
    const ScopedLock sl (lock);

    if (auto cls = getFirstSelectableClass())
        // use a local copy of the list, as it will change as things get added + selected
        return cls->pasteClipboard (SelectableList (selected), editViewID);

    return false;
}

void SelectionManager::selectOtherObjects (SelectableClass::Relationship relationship, bool keepOldItemsSelected)
{
    const ScopedLock sl (lock);

    if (auto cls = getFirstSelectableClass())
    {
        SelectableClass::SelectOtherObjectsParams params =
        {
            *this,
            selected,
            relationship,
            keepOldItemsSelected,
            editViewID
        };

        cls->selectOtherObjects (params);
    }
}

void SelectionManager::keepSelectedObjectsOnScreen()
{
    const ScopedLock sl (lock);

    if (auto cls = getFirstSelectableClass())
        cls->keepSelectedObjectOnScreen (selected);
}

SelectionManager* SelectionManager::findSelectionManager (const Component* c)
{
    if (auto smc = c->findParentComponentOfClass<ComponentWithSelectionManager>())
        return smc->getSelectionManager();

    return {};
}

SelectionManager* SelectionManager::findSelectionManager (const juce::Component& c)
{
    return findSelectionManager (&c);
}

SelectionManager::ScopedSelectionState::ScopedSelectionState (SelectionManager& m)
    : manager (m), selected (manager.selected)
{
}

SelectionManager::ScopedSelectionState::~ScopedSelectionState()
{
    manager.select (selected);
}

//==============================================================================
SelectableClass* SelectableList::getSelectableClass (int index) const
{
    if (index >= items.size())
        return {};

    if (index < classes.size())
        if (auto sc = classes.getUnchecked (index))
            if (sc != nullptr)
                return sc;

    classes.resize (items.size());
    auto selectableClass = SelectableClass::findClassFor (items[index]);
    classes.setUnchecked (index, selectableClass);

    return selectableClass;
}

std::pair<Selectable*, SelectableClass*> SelectableList::getSelectableAndClass (int index) const
{
    return std::pair<Selectable*, SelectableClass*> (items[index], getSelectableClass (index));
}
