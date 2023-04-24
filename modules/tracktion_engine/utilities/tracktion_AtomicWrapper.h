/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
/**
    Dummy constrainer which should optimise away to nothing.
*/
template<typename Type>
struct DummyConstrainer
{
    static Type constrain (const Type& v)
    {
        return v;
    }
};


//==============================================================================
//==============================================================================
/**
    Wraps an atomic with an interface compatible with var so it can be used
    within CachedValues in a thread safe way.
    Optionally supply a Constrainer to limit the value in some way.
*/
template<typename Type, typename Constrainer = DummyConstrainer<Type>>
struct AtomicWrapper
{
    //==============================================================================
    /** Constructor */
    AtomicWrapper() = default;

    /** Constructs a copy of another wrapper type. */
    template<typename OtherType>
    AtomicWrapper (const OtherType& other)
    {
        value.store (Constrainer::constrain (other));
    }

    /** Constructs a copy of another AtomicWrapper. */
    AtomicWrapper (const AtomicWrapper& other)
    {
        value.store (other.value);
    }

    /** Assigns another AtomicWrapper's underlying value to this one atomically. */
    AtomicWrapper& operator= (const AtomicWrapper& other) noexcept
    {
        value.store (other.value);
        return *this;
    }

    /** Compares the unerlaying value of another wrapper with this one. */
    bool operator== (const AtomicWrapper& other) const noexcept
    {
        return value.load() == other.value.load();
    }

    /** Compares the unerlaying value of another wrapper with this one. */
    bool operator!= (const AtomicWrapper& other) const noexcept
    {
        return value.load() != other.value.load();
    }

    //==============================================================================
    /** Returns the underlying value as a var. */
    operator juce::var() const noexcept
    {
        return Constrainer::constrain (value.load());
    }

    /** Returns the underlying value. */
    operator Type() const noexcept
    {
        return Constrainer::constrain (value.load());
    }

    //==============================================================================
private:
    std::atomic<Type> value { Type() };
};

}} // namespace tracktion { inline namespace engine
