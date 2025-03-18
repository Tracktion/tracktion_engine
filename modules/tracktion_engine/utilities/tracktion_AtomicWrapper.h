/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
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
template<typename Type,
         typename Constrainer = DummyConstrainer<Type>,
         auto MemoryOrderLoad = std::memory_order_seq_cst,
         auto MemoryOrderStore = std::memory_order_seq_cst>
struct AtomicWrapper
{
    //==============================================================================
    /** Constructor */
    AtomicWrapper() = default;

    /** Constructs a copy of another wrapper type. */
    template<typename OtherType>
    AtomicWrapper (const OtherType& other)
    {
        value.store (Constrainer::constrain (other), MemoryOrderStore);
    }

    /** Constructs a copy of another AtomicWrapper. */
    AtomicWrapper (const AtomicWrapper& other)
    {
        value.store (other.value, MemoryOrderStore);
    }

    /** Assigns another AtomicWrapper's underlying value to this one atomically. */
    AtomicWrapper& operator= (const AtomicWrapper& other) noexcept
    {
        value.store (other.value, MemoryOrderStore);
        return *this;
    }

    /** Assigns a value to this one atomically. */
    AtomicWrapper& operator= (const Type& other) noexcept
    {
        value.store (Constrainer::constrain (other), MemoryOrderStore);
        return *this;
    }

    /** Compares the underlaying value of another wrapper with this one. */
    bool operator== (const AtomicWrapper& other) const noexcept
    {
        return value.load (MemoryOrderLoad) == other.value.load (MemoryOrderLoad);
    }

    /** Compares the underlaying value of another wrapper with this one. */
    bool operator!= (const AtomicWrapper& other) const noexcept
    {
        return value.load (MemoryOrderLoad) != other.value.load (MemoryOrderLoad);
    }

    /** Compares another value with this one. */
    bool operator== (const Type& other) const noexcept
    {
        return value.load (MemoryOrderLoad) == other;
    }

    /** Compares another value with this one. */
    bool operator!= (const Type& other) const noexcept
    {
        return value.load (MemoryOrderLoad) != other;
    }

    //==============================================================================
    /** Returns the underlying value as a var. */
    operator juce::var() const noexcept
    {
        return Constrainer::constrain (value.load (MemoryOrderLoad));
    }

    /** Returns the underlying value. */
    operator Type() const noexcept
    {
        return Constrainer::constrain (value.load (MemoryOrderLoad));
    }

    //==============================================================================
private:
    std::atomic<Type> value { Type() };
};

template<typename Type, typename Constrainer = DummyConstrainer<Type>>
using AtomicWrapperRelaxed = AtomicWrapper<Type, Constrainer, std::memory_order_relaxed, std::memory_order_relaxed>;

template<typename Type, typename Constrainer = DummyConstrainer<Type>>
using AtomicWrapperAcqRel = AtomicWrapper<Type, Constrainer, std::memory_order_acquire, std::memory_order_release>;

}} // namespace tracktion { inline namespace engine


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

namespace juce
{
    template<typename T, typename C, auto L, auto S>
    struct VariantConverter<tracktion::AtomicWrapper<T, C, L, S>>
    {
        static T fromVar (const var& v) { return VariantConverter<T>::fromVar (v); }
        static var toVar (T v)          { return VariantConverter<T>::toVar (v); }
    };
}