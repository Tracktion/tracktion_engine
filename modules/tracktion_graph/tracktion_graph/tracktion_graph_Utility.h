/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once


namespace tracktion_graph
{

template<typename T>
void hash_combine (size_t& seed, const T& v)
{
    seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template<typename It>
std::size_t hash_range (It first, It last)
{
    size_t seed = 0;

    for (; first != last; ++first)
        hash_combine (seed, *first);

    return seed;
}

template<typename It>
void hash_range (std::size_t& seed, It first, It last)
{
    for (; first != last; ++first)
        hash_combine (seed, *first);
}

}
