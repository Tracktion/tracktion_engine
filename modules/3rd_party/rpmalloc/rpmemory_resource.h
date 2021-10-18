/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "rpallocator.h"

class rpmemory_resource : public tracktion_graph::memory_resource
{
public:
    rpmemory_resource() = default;
    
    void* do_allocate (std::size_t bytes, std::size_t) override
    {
        return allocator.allocate (bytes);
    }
    
    void do_deallocate (void* p, std::size_t bytes, std::size_t) override
    {
        allocator.deallocate (static_cast<char*> (p), bytes);
    }
    
    bool do_is_equal (const memory_resource& o) const noexcept override
    {
        return dynamic_cast<const rpmemory_resource*> (&o) != nullptr;
    }
    
private:
    rpallocator<char> allocator;
};
