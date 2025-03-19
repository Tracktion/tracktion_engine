//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Classy Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2022 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_OBJC_HELPERS_HEADER_INCLUDED
#define CHOC_OBJC_HELPERS_HEADER_INCLUDED

#include "choc_Platform.h"

#if CHOC_APPLE

#include <memory>
#include <objc/runtime.h>
#include <objc/message.h>
#include <type_traits>

#include "choc_Assert.h"


//==============================================================================
/**
    This namespace contains a random assortment of helpers for reducing the
    boilerplate needed when writing C++ code that uses the obj-C API.

    This is an annoying and long-winded way to interop with obj-C, but in a
    header-only class that will be included in a C++ compile unit, there's not
    really any other way to do it.
*/
namespace choc::objc
{
    //==============================================================================
    /// Invokes an obj-C selector for a given target and some arbitrary
    /// parameters, also allowing you to specify the return type.
    template <typename ReturnType, typename... Args>
    ReturnType call (id target, const char* selector, Args... args)
    {
        constexpr const auto msgSend = ([]
        {
          #if defined (__x86_64__)
            if constexpr (std::is_void_v<ReturnType>)
                return objc_msgSend;
            else if constexpr (sizeof (ReturnType) > 16)
                return objc_msgSend_stret;
            else
                return objc_msgSend;
          #elif defined (__arm64__)
            return objc_msgSend;
          #else
            #error "Unknown or unsupported architecture!"
          #endif
        })();

        return reinterpret_cast<ReturnType(*)(id, SEL, Args...)> (msgSend) (target, sel_registerName (selector), args...);
    }

    /// Calls a selector on a named class rather than an object.
    template <typename ReturnType, typename... Args>
    ReturnType callClass (const char* targetClassName, const char* selector, Args... args)
    {
        return call<ReturnType> ((id) objc_getClass (targetClassName), selector, std::forward<Args> (args)...);
    }

    /// Invokes an obj-C selector for the superclass of a given target.
    template <typename ReturnType, typename... Args>
    ReturnType callSuper (id target, const char* selector, Args... args)
    {
        constexpr const auto msgSendSuper = ([]
        {
          #if defined (__x86_64__)
            if constexpr (std::is_void_v<ReturnType>)
                return objc_msgSendSuper;
            else if constexpr (sizeof (ReturnType) > 16)
                return objc_msgSendSuper_stret;
            else
                return objc_msgSendSuper;
          #elif defined (__arm64__)
            return objc_msgSendSuper;
          #else
            #error "Unknown or unsupported architecture!"
          #endif
        })();

        objc_super superInfo =
        {
            target,
            (Class) call<id> (target, "superclass")
        };

        return reinterpret_cast<ReturnType(*)(objc_super*, SEL, Args...)> (msgSendSuper) (&superInfo, sel_registerName (selector), args...);
    }

    //==============================================================================
    // This stuff lets you write autorelease scopes that work either with or without
    // ARC support being enabled in the compiler.
   #if __has_feature(objc_arc)
    #define CHOC_AUTORELEASE_BEGIN @autoreleasepool {
    #define CHOC_AUTORELEASE_END }
    #define CHOC_OBJC_CAST_BRIDGED __bridge
   #else
    struct AutoReleasePool
    {
        AutoReleasePool()  { pool = callClass<id> ("NSAutoreleasePool", "new"); }
        ~AutoReleasePool() { call<void> (pool, "release"); }

        id pool;
    };

    #define CHOC_MAKE_AR_NAME2(line)  autoreleasePool_ ## line
    #define CHOC_MAKE_AR_NAME1(line)  CHOC_MAKE_AR_NAME2(line)
    #define CHOC_AUTORELEASE_BEGIN { choc::objc::AutoReleasePool CHOC_MAKE_AR_NAME1(__LINE__);
    #define CHOC_AUTORELEASE_END }
    #define CHOC_OBJC_CAST_BRIDGED
   #endif

    //==============================================================================
    /// Creates a delegate class from a base. A random suffix will be added to the
    /// new class name provided, to avoid clashes in the global namespace.
    inline Class createDelegateClass (const char* baseClass, const char* newClassName)
    {
        auto time = std::chrono::high_resolution_clock::now().time_since_epoch();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds> (time).count();
        auto uniqueDelegateName = newClassName + std::to_string (static_cast<uint32_t> (micros));
        auto c = objc_allocateClassPair (objc_getClass (baseClass), uniqueDelegateName.c_str(), 0);
        CHOC_ASSERT (c);
        return c;
    }

    //==============================================================================
    /// Converts an NSString to a std::string
    inline std::string getString (id nsString)      { if (nsString) return std::string (call<const char*> (nsString, "UTF8String")); return {}; }
    /// Converts a raw UTF8 string to an NSString
    inline id getNSString (const char* s)           { return callClass<id> ("NSString", "stringWithUTF8String:", s != nullptr ? s : ""); }
    /// Converts a UTF8 std::string to an NSString
    inline id getNSString (const std::string& s)    { return getNSString (s.c_str()); }

    /// Converts a bool to a NSNumber
    inline id getNSNumberBool (bool b)              { return callClass<id> ("NSNumber", "numberWithBool:", (BOOL) b); }

    /// Invokes `[NSApplication sharedApplication]`
    inline id getSharedNSApplication()              { return callClass<id> ("NSApplication", "sharedApplication"); }

    //==============================================================================
    // Including CodeGraphics.h can create all kinds of messy C/C++ symbol clashes
    // with other headers, and in many cases, all we need are these structs..
   #if defined (__LP64__) && __LP64__
    using CGFloat = double;
   #else
    using CGFloat = float;
   #endif

    struct CGPoint { CGFloat x = 0, y = 0; };
    struct CGSize  { CGFloat width = 0, height = 0; };
    struct CGRect  { CGPoint origin; CGSize size; };

} // namespace choc::objc

#endif // CHOC_APPLE
#endif // CHOC_OBJC_HELPERS_HEADER_INCLUDED
