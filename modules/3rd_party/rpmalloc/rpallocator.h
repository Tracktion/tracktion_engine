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
#undef min
#undef max

#include "../../tracktion_graph/utilities/tracktion_RealTimeSpinLock.h"

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

                const std::lock_guard<std::mutex> l (getInitMutex());
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

                const std::lock_guard<std::mutex> l (getInitMutex());
                rpmalloc_finalize();
            }
        }
        
    private:
        static std::atomic<int>& getInitCount()
        {
            static std::atomic<int> initCount { 0 };
            return initCount;
        }
        
        static std::mutex& getInitMutex()
        {
            static std::mutex m;
            return m;
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
            
            // With high thread counts (100+) there seems to be too much contention on the
            // finalize call and it stalls so serialise them here
            const std::lock_guard l (getFinaliseMutex());
            rpmalloc_thread_finalize();
        }
        
        // This is here to ensure rpmalloc isn't deinitialised untill all threads have finished
        ScopedRPMallocInitialiser scopedRPMallocInitialiser;
    
    private:
        static tracktion::graph::RealTimeSpinLock& getFinaliseMutex()
        {
            static tracktion::graph::RealTimeSpinLock m;
            return m;
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
    rpallocator() = default;

    /** Copy constructor. */
    rpallocator (const rpallocator&) noexcept {}

    /** Move constructor. */
    rpallocator (rpallocator&&) noexcept {}

    /** Templated copy constructor. */
    template<typename _Tp1>
    rpallocator (const rpallocator<_Tp1>&) noexcept {}

    //==============================================================================
    /** Allocates a number of elements of T. */
    value_type* allocate (std::size_t numElements)
    {
        initThreadFinaliser();
        rpmalloc_thread_initialize();

        const auto numBytes = numElements * sizeof (value_type);
        
       #if LOG_RPALLOCATIONS
        std::cout << "Alloc " << numBytes << " bytes.\n";
       #endif
        
        return static_cast<value_type*> (rpmalloc (numBytes));
    }
    
    /** Dellocates a previously allocated pointer. */
    void deallocate (T* p, [[maybe_unused]] std::size_t numElements)
    {
        initThreadFinaliser();
        rpmalloc_thread_initialize();

       #if LOG_RPALLOCATIONS
        std::cout << "Dealloc " << numElements * sizeof (value_type) << " bytes.\n";
       #endif
        
        return rpfree (p);
    }

private:
    //==============================================================================
    void initThreadFinaliser()
    {
        thread_local details::ScopedRPThreadFinaliser init;
    }
};

template <typename T, typename U>
bool operator== (const rpallocator <T>&, const rpallocator <U>&) { return true; }
template <typename T, typename U>
bool operator!= (const rpallocator <T>&, const rpallocator <U>&) { return false; }
