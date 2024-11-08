
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

class SelectableClass;
class Selectable;
class SelectionManager;

//==============================================================================
class SelectableListener
{
public:
    virtual ~SelectableListener() {}

    virtual void selectableObjectChanged (Selectable*) = 0;
    virtual void selectableObjectAboutToBeDeleted (Selectable*) = 0;
};

//==============================================================================
/**
    Base class for things that can be selected, and whose properties can appear
    in the properties panel.

*/
class Selectable
{
public:
    //==============================================================================
    Selectable();
    virtual ~Selectable();

    static void initialise();

    //==============================================================================
    /** Subclasses must return a description of what they are. */
    virtual juce::String getSelectableDescription() = 0;

    /** Can be overridden to tell this object that it has just been selected
        or deselected
    */
    virtual void selectionStatusChanged (bool isNowSelected);

    //==============================================================================
    /** This should be called to send a change notification to any
        SelectableListeners that are registered with this object.
    */
    virtual void changed();

    /** Called just before the selectable is about to be deleted so any subclasses
        should still be valid at this point.
    */
    virtual void selectableAboutToBeDeleted() {}

    //==============================================================================
    class Listener : public SelectableListener
    {
    public:
        Listener (Selectable&);
        ~Listener() override;

        void selectableObjectChanged (Selectable*) override {}
        void selectableObjectAboutToBeDeleted (Selectable*) override {}

    private:
        juce::WeakReference<Selectable> selectable;
    };

    //==============================================================================
    void addListener (SelectableListener*);
    void removeListener (SelectableListener*);

    void addSelectableListener (SelectableListener*);
    void removeSelectableListener (SelectableListener*);

    /** checks whether this object has been deleted.

        This is pretty reliable, and can be called with a null pointer, but isn't 100%
        perfect, as it can provide a false-positive when an object is deleted, and another
        one later created that happens to have the same memory address.
    */
    static bool isSelectableValid (const Selectable*) noexcept;

    /** If changed() has been called, this will cancel any pending async change notificaions. */
    void cancelAnyPendingUpdates();

    // deselects this from all selectionmanagers
    void deselect();

    // Tells any SelectableManagers to rebuild their property panels if this object is selected
    void propertiesChanged();

    // MUST be called by all subclasses of Selectable in their destructor!
    void notifyListenersOfDeletion();

    //==============================================================================
    // This is deprecated: use SafeSelectable<Selectable> instead
    using WeakRef = juce::WeakReference<Selectable>;
    WeakRef::Master masterReference;
    [[ deprecated ("Use makeSafeRef() and SafeSelectable instead") ]] WeakRef getWeakRef() { return { this }; }

private:
    //==============================================================================
    juce::ListenerList<SelectableListener> selectableListeners;

    friend struct SelectableUpdateTimer;
    bool needsAnUpdate = false, hasNotifiedListenersOfDeletion = false, isCallingListeners = false;

    void sendChangeCallbackToListenersIfNeeded();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Selectable)
};


//==============================================================================
/**
    A list of Selectables, similar to a juce::Array but contains a cached list
    of the SelectableClasses for each entry.
*/
struct SelectableList
{
    SelectableList() = default;

    SelectableList (const std::initializer_list<Selectable*>& initialItems)
    {
        items.addArray (initialItems);
    }

    template<typename SelectableType>
    SelectableList (const juce::Array<SelectableType*>& initialItems)
    {
        items.addArray (initialItems);
    }

    template<typename SelectableType>
    SelectableList (const juce::ReferenceCountedArray<SelectableType>& initialItems)
    {
        items.addArray (initialItems);
    }

    template<typename SelectableType>
    SelectableList (const std::vector<SelectableType*>& initialItems)
    {
        items.addArray (initialItems.data(), static_cast<int> (initialItems.size()));
    }

    //==============================================================================
    /** Returns the selectable class for a given Selectable in the list. */
    SelectableClass* getSelectableClass (int index) const;

    /** Returns the selectable and it's associated class.
        N.B. This does no bounds checking so make sure index is in range!
    */
    std::pair<Selectable*, SelectableClass*> getSelectableAndClass (int index) const;

    //==============================================================================
    template <typename SubclassType>
    juce::Array<SubclassType*> getItemsOfType() const
    {
        juce::Array<SubclassType*> results;

        for (auto s : items)
            if (auto i = dynamic_cast<SubclassType*> (s))
                results.add (i);

        return results;
    }

    template <typename SubclassType>
    SubclassType* getFirstOfType() const
    {
        return dynamic_cast<SubclassType*> (items.getFirst());
    }

    template <typename SubclassType>
    bool containsType() const
    {
        for (auto s : items)
            if (dynamic_cast<SubclassType*> (s) != nullptr)
                return true;

        return false;
    }

    //==============================================================================
    int size() const                                            { return items.size(); }
    bool isEmpty() const                                        { return items.isEmpty(); }
    bool isNotEmpty() const                                     { return ! isEmpty(); }

    inline Selectable** begin()                                 { return items.begin(); }
    inline Selectable* const* begin() const                     { return items.begin(); }
    inline Selectable** end()                                   { return items.end(); }
    inline Selectable* const* end() const                       { return items.end(); }
    inline Selectable** data()                                  { return begin(); }
    inline Selectable* const* data() const                      { return begin(); }

    Selectable* operator[] (int index) const                    { return items[index]; }
    inline Selectable* getUnchecked (int index) const           { return items.getUnchecked (index); }
    inline Selectable* getFirst() const                         { return items.getFirst(); }
    inline Selectable* getLast() const                          { return items.getLast(); }

    template <class OtherArrayType>
    inline void addArray (const OtherArrayType& arrayToAddFrom, int startIndex = 0, int numElementsToAdd = -1)
    {
        classes.clearQuick();
        items.addArray (arrayToAddFrom, startIndex, numElementsToAdd);
    }

    template <class OtherArrayType>
    inline void mergeArray (const OtherArrayType& arrayToMergeFrom)
    {
        classes.clearQuick();

        for (auto& s : arrayToMergeFrom)
            addIfNotAlreadyThere (s);
    }

    inline void add (Selectable* newElement)                    { items.add (newElement); }
    inline bool addIfNotAlreadyThere (Selectable* newElement)   { return items.addIfNotAlreadyThere (newElement); }

    inline void clear()                                         { classes.clear(); items.clear(); }
    inline void remove (int indexToRemove)                      { classes.clearQuick(); return items.remove (indexToRemove); }
    inline int removeAllInstancesOf (Selectable* s)             { classes.clearQuick(); return items.removeAllInstancesOf (s); }
    inline Selectable* removeAndReturn (int indexToRemove)      { classes.clearQuick(); return items.removeAndReturn (indexToRemove); }
    inline bool contains (Selectable* elementToLookFor) const   { return items.contains (elementToLookFor); }
    inline int indexOf (Selectable* elementToLookFor) const     { return items.indexOf (elementToLookFor); }

    template <class OtherArrayType>
    inline bool operator== (const OtherArrayType& other) const  { return items == other; }

    template <class OtherArrayType>
    inline bool operator!= (const OtherArrayType& other) const  { return items != other; }

    inline bool operator== (const SelectableList& other) const  { return items == other.items; }
    inline bool operator!= (const SelectableList& other) const  { return items != other.items; }

    using ScopedLockType = juce::Array<Selectable*>::ScopedLockType;
    inline const juce::DummyCriticalSection& getLock() const noexcept { return items.getLock(); }

private:
    juce::Array<Selectable*> items;
    mutable juce::Array<SelectableClass*> classes;
};


//==============================================================================
/** Holds a pointer to some type of Selectable, which automatically becomes null if
    the selectable is deleted.

    The SelectableType template parameter must be Selectable, or some subclass of Selectable.
*/
template<typename SelectableType>
class SafeSelectable
{
public:
    /** Creates a null SafeSelectable. */
    SafeSelectable() = default;

    /** Creates a SafeSelectable that points at the given selectable. */
    SafeSelectable (SelectableType& selectable)                     : weakRef (&selectable) {}

    /** Creates a SafeSelectable that points at the given selectable. */
    SafeSelectable (SelectableType* selectable)                     : weakRef (selectable) {}

    /** Creates a copy of another SafeSelectable. */
    SafeSelectable (const SafeSelectable& other) noexcept           : weakRef (other.weakRef) {}

    /** Copies another pointer to this one. */
    SafeSelectable& operator= (const SafeSelectable& other)         { weakRef = other.weakRef; return *this; }

    /** Copies another pointer to this one. */
    SafeSelectable& operator= (SelectableType* newSelectable)       { weakRef = newSelectable; return *this; }

    /** Returns the selectable that this pointer refers to, or null if the selectable no longer exists. */
    SelectableType* get() const noexcept                            { return dynamic_cast<SelectableType*> (weakRef.get()); }

    /** Returns the selectable that this pointer refers to, or null if the selectable no longer exists. */
    operator SelectableType*() const noexcept                       { return get(); }

    /** Returns the selectable that this pointer refers to, or null if the selectable no longer exists. */
    SelectableType* operator->() const noexcept                     { return get(); }

    bool operator== (SelectableType* other) const noexcept          { return weakRef.get() == other; }
    bool operator!= (SelectableType* other) const noexcept          { return weakRef.get() != other; }

    bool operator== (const SafeSelectable& other) const noexcept    { return weakRef.get() == other.weakRef.get(); }
    bool operator!= (const SafeSelectable& other) const noexcept    { return weakRef.get() != other.weakRef.get(); }

    bool operator== (const SelectableType& other) const noexcept    { return weakRef.get() == &other; }
    bool operator!= (const SelectableType& other) const noexcept    { return weakRef.get() != &other; }

    bool operator== (decltype(nullptr)) const noexcept              { return weakRef == nullptr; }
    bool operator!= (decltype(nullptr)) const noexcept              { return weakRef != nullptr; }

private:
    juce::WeakReference<Selectable> weakRef;
};

/** Creates a SafeSelectable for a given selectable object. */
template<typename SelectableType>
SafeSelectable<SelectableType> makeSafeRef (SelectableType& selectable)
{
    return SafeSelectable<SelectableType> (selectable);
}

/** Creates a std::vector<SafeSelectable<Something>> for a given juce::Array of selectable objects. */
template<typename Iterable>
auto makeSafeVector (const Iterable& selectables) -> std::vector<SafeSelectable<typename std::remove_reference<decltype(*selectables[0])>::type>>
{
    using SelectableType = typename std::remove_reference<decltype(*selectables[0])>::type;
    static_assert (std::is_base_of_v<Selectable, SelectableType>);
    std::vector<SafeSelectable<SelectableType>> v;
    v.reserve (static_cast<size_t> (selectables.size()));

    for (auto* s : selectables)
        v.emplace_back (*s);

    return v;
}

//==============================================================================
//==============================================================================
/**
    A SelectableListener that safely handles listener to a Selectable and uses
    a lambda for its callback.
*/
class LambdaSelectableListener  : public SelectableListener
{
public:
    /** Constructs an empty listener. */
    LambdaSelectableListener() = default;

    /** Destructor. */
    ~LambdaSelectableListener() override
    {
        reset();
    }

    /** Constructs a listener for a Selectable. */
    LambdaSelectableListener (Selectable& s)
    {
        reset (&s);
    }

    /** Resets the Selectable. */
    void reset (Selectable* s = nullptr)
    {
        if (ref)
            ref->removeSelectableListener (this);

        ref = s;

        if (ref)
            ref->addSelectableListener (this);
    }

    std::function<void()> onSelectableChanged;          /*<< Assignable callback for change events. */
    std::function<void()> onSelectableAboutToBeDeleted; /*<< Assignable callback for deletion events. */

private:
    SafeSelectable<Selectable> ref;

    void selectableObjectChanged (Selectable*) override
    {
        if (onSelectableChanged)
            onSelectableChanged();
    }

    void selectableObjectAboutToBeDeleted (Selectable*) override
    {
        if (onSelectableAboutToBeDeleted)
            onSelectableAboutToBeDeleted();
    }
};


}} // namespace tracktion { inline namespace engine
