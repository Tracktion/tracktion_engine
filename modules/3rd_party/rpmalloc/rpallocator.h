/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "rpmalloc.h"

#ifndef LOG_RPALLOCATIONS
 #define LOG_RPALLOCATIONS 0
#endif

//==============================================================================
//==============================================================================
namespace details
{
    struct ScopedRPMallocInitialiser
    {
        ScopedRPMallocInitialiser()
        {
           #if LOG_RPALLOCATIONS
            std::cout << std::this_thread::get_id() << " ScopedRPMallocInitialiser(): " << getInitCount().load() << "\n";
           #endif
            
            if (getInitCount().fetch_add (1) == 0)
            {
               #if LOG_RPALLOCATIONS
                std::cout << "\trpmalloc_initialize\n";
               #endif
                rpmalloc_initialize();
            }
        }
        
        ~ScopedRPMallocInitialiser()
        {
           #if LOG_RPALLOCATIONS
            std::cout << std::this_thread::get_id() << " ~ScopedRPMallocInitialiser(): " << getInitCount().load() << "\n";
           #endif
            
            if (getInitCount().fetch_sub (1) == 1)
            {
               #if LOG_RPALLOCATIONS
                std::cout << "\trpmalloc_finalize\n";
               #endif
                rpmalloc_finalize();
            }
        }
        
    private:
        std::atomic<int>& getInitCount()
        {
            static std::atomic<int> initCount { 0 };
            return initCount;
        }
    };

    struct ScopedRPThreadFinaliser
    {
        ScopedRPThreadFinaliser() = default;
        
        ~ScopedRPThreadFinaliser()
        {
           #if LOG_RPALLOCATIONS
            std::cout << std::this_thread::get_id() << " ~ScopedRPThreadFinaliser()\n";
           #endif
            rpmalloc_thread_finalize();
        }
    };
}


//==============================================================================
//==============================================================================
/**
    Allocator that uses a global instance of rpmalloc.
    You can allocate and deallocate from any thread.
    
    Simply supply this as a template argument to a container or use directly to
    allocate memory.
    @code std::vector<int, rpallocator<int>> vec; @endcode
*/
template<typename T>
class rpallocator
{
public:
    //==============================================================================
    using value_type = T;

    //==============================================================================
    /** Constructor. */
    rpallocator() noexcept = default;

    /** Copy constructor. */
    rpallocator (const rpallocator&) noexcept {}

    /** Move constructor. */
    rpallocator (rpallocator&&) noexcept {}

    //==============================================================================
    /** Allocates a number of elements of T. */
    value_type* allocate (std::size_t numElements)
    {
        rpmalloc_thread_initialize();
        thread_local details::ScopedRPThreadFinaliser init;
        
        const auto numBytes = numElements * sizeof (value_type);
        
       #if LOG_RPALLOCATIONS
        std::cout << "Alloc " << numBytes << " bytes.\n";
       #endif
        
        return static_cast<value_type*> (rpmalloc (numBytes));
    }
    
    /** Dellocates a previously allocated pointer. */
    void deallocate (T* p, [[maybe_unused]] std::size_t numElements)
    {
        rpmalloc_thread_initialize();
        thread_local details::ScopedRPThreadFinaliser init;

       #if LOG_RPALLOCATIONS
        std::cout << "Dealloc " << numElements * sizeof (value_type) << " bytes.\n";
       #endif
        
        return rpfree (p);
    }

private:
    //==============================================================================
    details::ScopedRPMallocInitialiser scopedRPMallocInitialiser;
};
