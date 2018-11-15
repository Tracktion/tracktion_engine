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
    A CachedValue that can take a std::function to constrain its value.
*/
template <typename Type>
class ConstrainedCachedValue    : private juce::ValueTree::Listener
{
public:
    //==============================================================================
    /** Default constructor.
        Creates a default CachedValue not referring to any property. To initialise the
        object, call one of the referTo() methods.
    */
    ConstrainedCachedValue();

    /** Constructor.

        Creates a CachedValue referring to a Value property inside a ValueTree.
        If you use this constructor, the fallback value will be a default-constructed
        instance of Type.

        @param tree          The ValueTree containing the property
        @param propertyID    The identifier of the property
        @param undoManager   The UndoManager to use when writing to the property
    */
    ConstrainedCachedValue (juce::ValueTree& tree, const juce::Identifier& propertyID,
                            juce::UndoManager* undoManager);

    /** Constructor.

        Creates a default Cached Value referring to a Value property inside a ValueTree,
        and specifies a fallback value to use if the property does not exist.

        @param tree          The ValueTree containing the property
        @param propertyID    The identifier of the property
        @param undoManager   The UndoManager to use when writing to the property
        @param defaultToUse  The fallback default value to use.
    */
    ConstrainedCachedValue (juce::ValueTree& tree, const juce::Identifier& propertyID,
                            juce::UndoManager* undoManager, const Type& defaultToUse);

    /** Sets a std::function to use to constain the value.
        You must call this before setting any of the values.
    */
    void setConstrainer (std::function<Type (Type)> constrainerToUse);

    //==============================================================================
    /** Returns the current value of the property. If the property does not exist,
        returns the fallback default value.

        This is the same as calling get().
    */
    operator Type() const noexcept                   { return cachedValue; }

    /** Returns the current value of the property. If the property does not exist,
        returns the fallback default value.
    */
    Type get() const noexcept                        { return cachedValue; }

    /** Dereference operator. Provides direct access to the property.  */
    Type& operator*() noexcept                       { return cachedValue; }

    /** Dereference operator. Provides direct access to members of the property
        if it is of object type.
    */
    Type* operator->() noexcept                      { return &cachedValue; }

    /** Returns true if the current value of the property (or the fallback value)
        is equal to other.
    */
    template <typename OtherType>
    bool operator== (const OtherType& other) const   { return cachedValue == other; }

    /** Returns true if the current value of the property (or the fallback value)
        is not equal to other.
    */
    template <typename OtherType>
    bool operator!= (const OtherType& other) const   { return cachedValue != other; }

    //==============================================================================
    /** Returns the current property as a Value object. */
    juce::Value getPropertyAsValue();

    /** Returns true if the current property does not exist and the CachedValue is using
        the fallback default value instead.
    */
    bool isUsingDefault() const;

    /** Returns the current fallback default value. */
    Type getDefault() const                          { return defaultValue; }

    //==============================================================================
    /** Sets the property. This will actually modify the property in the referenced ValueTree. */
    ConstrainedCachedValue& operator= (const Type& newValue);

    /** Sets the property. This will actually modify the property in the referenced ValueTree. */
    void setValue (const Type& newValue, juce::UndoManager* undoManagerToUse);

    /** Removes the property from the referenced ValueTree and makes the CachedValue
        return the fallback default value instead.
    */
    void resetToDefault();

    /** Removes the property from the referenced ValueTree and makes the CachedValue
        return the fallback default value instead.
    */
    void resetToDefault (juce::UndoManager* undoManagerToUse);

    /** Resets the fallback default value. */
    void setDefault (const Type& value)                { defaultValue = constrainer (value); }

    //==============================================================================
    /** Makes the CachedValue refer to the specified property inside the given ValueTree. */
    void referTo (juce::ValueTree& tree, const juce::Identifier& property, juce::UndoManager*);

    /** Makes the CachedValue refer to the specified property inside the given ValueTree,
        and specifies a fallback value to use if the property does not exist.
    */
    void referTo (juce::ValueTree& tree, const juce::Identifier& property, juce::UndoManager*, const Type& defaultVal);

    /** Force an update in case the referenced property has been changed from elsewhere.

        Note: The CachedValue is a ValueTree::Listener and therefore will be informed of
        changes of the referenced property anyway (and update itself). But this may happen
        asynchronously. forceUpdateOfCachedValue() forces an update immediately.
    */
    void forceUpdateOfCachedValue();

    //==============================================================================
    /** Returns a reference to the ValueTree containing the referenced property. */
    juce::ValueTree& getValueTree() noexcept                { return targetTree; }

    /** Returns the property ID of the referenced property. */
    const juce::Identifier& getPropertyID() const noexcept  { return targetProperty; }

private:
    //==============================================================================
    juce::ValueTree targetTree;
    juce::Identifier targetProperty;
    juce::UndoManager* undoManager;
    Type defaultValue;
    Type cachedValue;
    std::function<Type (Type)> constrainer;

    //==============================================================================
    void referToWithDefault (juce::ValueTree&, const juce::Identifier&, juce::UndoManager*, const Type&);
    Type getTypedValue() const;

    void valueTreePropertyChanged (juce::ValueTree& changedTree, const juce::Identifier& changedProperty) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override {}
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override {}

    JUCE_DECLARE_NON_COPYABLE (ConstrainedCachedValue)
};


//==============================================================================
template <typename Type>
inline ConstrainedCachedValue<Type>::ConstrainedCachedValue()  : undoManager (nullptr) {}

template <typename Type>
inline ConstrainedCachedValue<Type>::ConstrainedCachedValue (juce::ValueTree& v, const juce::Identifier& i, juce::UndoManager* um)
    : targetTree (v), targetProperty (i), undoManager (um),
      defaultValue(), cachedValue (getTypedValue())
{
    targetTree.addListener (this);
}

template <typename Type>
inline ConstrainedCachedValue<Type>::ConstrainedCachedValue (juce::ValueTree& v, const juce::Identifier& i, juce::UndoManager* um, const Type& defaultToUse)
    : targetTree (v), targetProperty (i), undoManager (um),
      defaultValue (defaultToUse), cachedValue (getTypedValue())
{
    targetTree.addListener (this);
}

template <typename Type>
inline void ConstrainedCachedValue<Type>::setConstrainer (std::function<Type (Type)> constrainerToUse)
{
    constrainer = std::move (constrainerToUse);
    jassert (constrainer);

    if (targetTree.isValid())
    {
        setValue (constrainer (cachedValue), undoManager);
        jassert (defaultValue == constrainer (defaultValue));
    }
}

template <typename Type>
inline juce::Value ConstrainedCachedValue<Type>::getPropertyAsValue()
{
    return targetTree.getPropertyAsValue (targetProperty, undoManager);
}

template <typename Type>
inline bool ConstrainedCachedValue<Type>::isUsingDefault() const
{
    return ! targetTree.hasProperty (targetProperty);
}

template <typename Type>
inline ConstrainedCachedValue<Type>& ConstrainedCachedValue<Type>::operator= (const Type& newValue)
{
    setValue (newValue, undoManager);
    return *this;
}

template <typename Type>
inline void ConstrainedCachedValue<Type>::setValue (const Type& newValue, juce::UndoManager* undoManagerToUse)
{
    jassert (constrainer);
    auto constrainedValue = constrainer (newValue);

    if (cachedValue != constrainedValue || isUsingDefault())
    {
        cachedValue = constrainedValue;
        targetTree.setProperty (targetProperty, juce::VariantConverter<Type>::toVar (constrainedValue), undoManagerToUse);
    }
}

template <typename Type>
inline void ConstrainedCachedValue<Type>::resetToDefault()
{
    resetToDefault (undoManager);
}

template <typename Type>
inline void ConstrainedCachedValue<Type>::resetToDefault (juce::UndoManager* undoManagerToUse)
{
    targetTree.removeProperty (targetProperty, undoManagerToUse);
    forceUpdateOfCachedValue();
}

template <typename Type>
inline void ConstrainedCachedValue<Type>::referTo (juce::ValueTree& v, const juce::Identifier& i, juce::UndoManager* um)
{
    referToWithDefault (v, i, um, Type());
}

template <typename Type>
inline void ConstrainedCachedValue<Type>::referTo (juce::ValueTree& v, const juce::Identifier& i, juce::UndoManager* um, const Type& defaultVal)
{
    referToWithDefault (v, i, um, defaultVal);
}

template <typename Type>
inline void ConstrainedCachedValue<Type>::forceUpdateOfCachedValue()
{
    cachedValue = getTypedValue();
}

template <typename Type>
inline void ConstrainedCachedValue<Type>::referToWithDefault (juce::ValueTree& v, const juce::Identifier& i, juce::UndoManager* um, const Type& defaultVal)
{
    jassert (constrainer);
    targetTree.removeListener (this);
    targetTree = v;
    targetProperty = i;
    undoManager = um;
    defaultValue = constrainer (defaultVal);
    cachedValue = getTypedValue();
    targetTree.addListener (this);
}

template <typename Type>
inline Type ConstrainedCachedValue<Type>::getTypedValue() const
{
    jassert (constrainer);

    if (const juce::var* property = targetTree.getPropertyPointer (targetProperty))
        return constrainer (juce::VariantConverter<Type>::fromVar (*property));

    return defaultValue;
}

template <typename Type>
inline void ConstrainedCachedValue<Type>::valueTreePropertyChanged (juce::ValueTree& changedTree, const juce::Identifier& changedProperty)
{
    if (changedProperty == targetProperty && targetTree == changedTree)
        forceUpdateOfCachedValue();
}

} // namespace tracktion_engine
