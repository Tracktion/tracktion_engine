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

struct SelectableUpdateTimer  : public juce::AsyncUpdater,
                                private juce::DeletedAtShutdown
{
    SelectableUpdateTimer (std::function<void()> onDelete_)
        : onDelete (onDelete_) {}

    ~SelectableUpdateTimer() override
    {
        if (onDelete)
            onDelete();
    }
    void add (Selectable* s)
    {
        const juce::ScopedLock sl (lock);
        selectables.add (s);
    }

    void remove (Selectable* s)
    {
        const juce::ScopedLock sl (lock);
        selectables.removeValue (s);
    }

    bool isValid (const Selectable* s) const
    {
        const juce::ScopedLock sl (lock);
        return selectables.contains (const_cast<Selectable*> (s));
    }

    void handleAsyncUpdate() override
    {
        CRASH_TRACER
        std::vector<Selectable*> needingUpdate;

        {
            const juce::ScopedLock sl (lock);

            for (auto s : selectables)
                if (s->needsAnUpdate)
                    needingUpdate.push_back (s);
        }

        for (auto s : needingUpdate)
            if (isValid (s))
                s->sendChangeCallbackToListenersIfNeeded();
    }

    juce::CriticalSection listenerLock;

private:
    juce::SortedSet<Selectable*> selectables;
    juce::CriticalSection lock;
    std::function<void()> onDelete;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectableUpdateTimer)
};

static SelectableUpdateTimer* updateTimerInstance = nullptr;

void Selectable::initialise()
{
    if (updateTimerInstance == nullptr)
        updateTimerInstance = new SelectableUpdateTimer ([]() { updateTimerInstance = nullptr; });
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
       #if JUCE_DEBUG
        if (! selectableListeners.isEmpty())
        {
            // must call notifyListenersOfDeletion() in the innermost subclass's destructor!
            jassertfalse;
        }
       #endif

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

void Selectable::addListener (SelectableListener* l)
{
    addSelectableListener (l);
}

void Selectable::removeListener (SelectableListener* l)
{
    removeSelectableListener (l);
}

void Selectable::addSelectableListener (SelectableListener* l)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    jassert (l != nullptr);
    jassert (! isCallingListeners);
    jassert (! hasNotifiedListenersOfDeletion);
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

    const juce::ScopedValueSetter<bool> svs (isCallingListeners, true);

    selectableListeners.call ([self = makeSafeRef (*this)] (SelectableListener& l)
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

            // Use a local copy in case any listeners get deleted during the callbacks
            juce::ListenerList<SelectableListener> copy;

            for (auto l : selectableListeners.getListeners())
                copy.add (l);

            copy.call ([self = makeSafeRef (*this)] (SelectableListener& l)
                       {
                           if (auto s = self.get())
                           {
                               if (! s->selectableListeners.contains (&l))
                                   return;

                               l.selectableObjectAboutToBeDeleted (s);
                           }
                           else
                           {
                               jassertfalse;
                           }
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
Selectable::Listener::Listener (Selectable& s)
{
    selectable = &s;
    if (selectable)
        selectable->addSelectableListener (this);
}

Selectable::Listener::~Listener()
{
    if (selectable)
        selectable->removeSelectableListener (this);
}

//==============================================================================
SelectableClass::SelectableClass() {}
SelectableClass::~SelectableClass() {}

static juce::Array<SelectableClass::ClassInstanceBase*>& getAllSelectableClasses()
{
    static juce::Array<SelectableClass::ClassInstanceBase*> classes;
    return classes;
}

static inline std::unordered_map<std::type_index, SelectableClass*>& getSelectableClassCache()
{
    static std::unordered_map<std::type_index, SelectableClass*> cache;
    return cache;
}

SelectableClass::ClassInstanceBase::ClassInstanceBase()   { getAllSelectableClasses().add (this); }
SelectableClass::ClassInstanceBase::~ClassInstanceBase()  { getAllSelectableClasses().removeAllInstancesOf (this); }

SelectableClass* SelectableClass::findClassFor (const Selectable& s)
{
   #if ! JUCE_DEBUG
    auto& cache = getSelectableClassCache();
    const std::type_index typeIndex (typeid (s));

    if (auto found = cache.find (typeIndex); found != cache.end())
        return found->second;

    for (auto cls : getAllSelectableClasses())
    {
        if (auto c = cls->getClassForObject (&s))
        {
            cache[typeIndex] = c;
            return c;
        }
    }
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

    return {};
}

SelectableClass* SelectableClass::findClassFor (const Selectable* s)
{
    return s != nullptr ? findClassFor (*s) : nullptr;
}

//==============================================================================
juce::String SelectableClass::getDescriptionOfSelectedGroup (const SelectableList& selectedObjects)
{
    if (selectedObjects.size() == 1)
        if (auto s = selectedObjects.getFirst())
            return s->getSelectableDescription();

    juce::StringArray names;

    for (auto o : selectedObjects)
        if (o != nullptr)
            names.addIfNotAlreadyThere (o->getSelectableDescription());

    if (names.size() == 1)
        return juce::String (TRANS("123 Objects of Type: XYYZ"))
               .replace ("123", juce::String (selectedObjects.size()))
               .replace ("XYYZ", names[0]);

    return juce::String (TRANS("123 Objects"))
            .replace ("123", juce::String (selectedObjects.size()));
}

bool SelectableClass::canBeSelected (const Selectable&) { return true; }
void SelectableClass::deleteSelected (const DeleteSelectedParams&) {}
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
static juce::Array<SelectionManager*> allManagers;

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
    for (auto s : makeSafeVector (selected))
        if (s != nullptr)
            s->removeSelectableListener (this);

    selected.clear();
}

SelectionManager::Iterator::Iterator() {}

bool SelectionManager::Iterator::next()
{
    return juce::isPositiveAndBelow (++index, allManagers.size());
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
    const juce::ScopedLock sl (lock);
    return selected[index];
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
    const juce::ScopedLock sl (lock);

    if (selected.size() > 0)
    {
        for (auto s : makeSafeVector (selected))
            if (s != nullptr)
                s->selectionStatusChanged (false);

        clearList();
        selectionChanged();
    }
}

static bool canBeSelected (Selectable& newItem)
{
    if (auto newItemClass = SelectableClass::findClassFor (newItem))
        return newItemClass->canBeSelected (newItem);
    return true;
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
    const juce::ScopedLock sl (lock);

    if (! Selectable::isSelectableValid (&s))
    {
        selected.removeAllInstancesOf (&s);
        return;
    }

    if (! canBeSelected (s))
        return;

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

void SelectionManager::select (const SelectableList& listSrc)
{
    SelectableList list;
    for (auto s : listSrc)
        if (Selectable::isSelectableValid (s) && canBeSelected (*s))
            list.add (s);

    if (list != selected)
    {
        deselectAll();

        for (auto s : list)
            select (s, true);
    }
}

void SelectionManager::deselect (Selectable* s)
{
    const juce::ScopedLock sl (lock);
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
    const juce::ScopedLock sl (lock);
    auto index = selected.indexOf (s);

    if (index >= 0)
    {
        selected.remove (index);
        selectionChanged();
    }
}

void SelectionManager::refreshPropertyPanel()
{
    selectionChanged();
}

void SelectionManager::refreshAllPropertyPanels()
{
    for (SelectionManager::Iterator sm; sm.next();)
        sm->refreshPropertyPanel();
}

void SelectionManager::refreshAllPropertyPanelsShowing (Selectable& s)
{
    for (SelectionManager::Iterator sm; sm.next();)
        if (sm->isSelected (s))
            sm->refreshPropertyPanel();
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
    const juce::ScopedLock sl (lock);

    // only allow groups of the same type to be selected at once.
    if (auto cls = getFirstSelectableClass())
    {
        auto& clipboard = *Clipboard::getInstance();

        SelectableClass::AddClipboardEntryParams clipboardParams (clipboard);
        clipboardParams.items = selected;

        if (editViewID != -1)
        {
            clipboardParams.edit = getEdit();
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
    const juce::ScopedLock sl (lock);

    if (auto cls = getFirstSelectableClass())
        // use a local copy of the list, as it will change as things get deleted + deselected
        cls->deleteSelected ({this, selected, false});
}

bool SelectionManager::cutSelected()
{
    const juce::ScopedLock sl (lock);

    if (auto cls = getFirstSelectableClass())
    {
        if (cls->canCutSelected (selected) && copySelected())
        {
            // use a local copy of the list, as it will change as things get deleted + deselected
            cls->deleteSelected ({this, selected, true});
            return true;
        }
    }

    return false;
}

bool SelectionManager::pasteSelected()
{
    const juce::ScopedLock sl (lock);

    if (auto cls = getFirstSelectableClass())
        // use a local copy of the list, as it will change as things get added + selected
        return cls->pasteClipboard (SelectableList (selected), editViewID);

    return false;
}

void SelectionManager::selectOtherObjects (SelectableClass::Relationship relationship, bool keepOldItemsSelected)
{
    const juce::ScopedLock sl (lock);

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
    const juce::ScopedLock sl (lock);

    if (auto cls = getFirstSelectableClass())
        cls->keepSelectedObjectOnScreen (selected);
}

Edit* SelectionManager::getEdit() const
{
    return edit.get();
}

SelectionManager* SelectionManager::findSelectionManager (const juce::Component* c)
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
bool SelectionManager::ChangedSelectionDetector::isFirstChangeSinceSelection (SelectionManager* sm)
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

void SelectionManager::ChangedSelectionDetector::reset()
{
    lastSelectionChangeCount = 0;
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

}} // namespace tracktion { inline namespace engine
