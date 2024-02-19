/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace core
{

template<class Container, class T, class BinaryOperation>
inline T accumulate (const Container& container, T init)
{
    return std::accumulate (std::begin (container), std::end (container), init);
}

template<class Container, class T, class BinaryOperation>
inline T accumulate (const Container& container, T init, BinaryOperation op)
{
    return std::accumulate (std::begin (container), std::end (container), init, op);
}

template<class Container, class UnaryFunction>
UnaryFunction for_each (const Container& container, UnaryFunction f)
{
    return std::for_each (std::begin (container), std::end (container), f);
}

template<class Container, class UnaryFunction>
bool contains (const Container& container, UnaryFunction f)
{
    return std::find_if (std::begin (container), std::end (container), f)
           != std::end (container);
}

/** Returns true if a specific value is found in a container. */
template<class Container>
bool contains_v (const Container& container, typename Container::value_type v)
{
    return std::find (std::begin (container), std::end (container), v)
           != std::end (container);
}

template<class Container>
inline void sort (Container& container)
{
    return std::sort (std::begin (container), std::end (container));
}

template<class Container, class Compare>
inline void sort (Container& container, Compare comp)
{
    return std::sort (std::begin (container), std::end (container), comp);
}

template<class Container>
inline void stable_sort (Container& container)
{
    return std::stable_sort (std::begin (container), std::end (container));
}

template<class Container, class Compare>
inline void stable_sort (Container& container, Compare comp)
{
    return std::stable_sort (std::begin (container), std::end (container), comp);
}

template<class Container>
inline std::optional<size_t> index_of (const Container& container, typename Container::value_type v)
{
    if (auto iter = std::find (container.begin(), container.end(), v);
        iter != container.end())
       return std::distance (container.begin(), iter);

    return {};
}

template<class Container, class IndexType>
inline std::optional<typename Container::value_type> get_checked (const Container& container, IndexType index)
{
    const auto i = static_cast<typename Container::size_type> (index);

    if (i >= 0 && i < container.size())
        return container[i];

    return {};
}

template<class Type>
bool assign_if_valid (Type& dest, const std::optional<Type>& src)
{
    if (! src)
        return false;

    dest = *src;
    return true;
}

template<class SmartPointerContainer>
SmartPointerContainer& erase_if_null (SmartPointerContainer& container)
{
    container.erase (std::remove_if (container.begin(), container.end(),
                                     [] (auto& c) { return ! c; }),
                     container.end());
    return container;
}

}}
