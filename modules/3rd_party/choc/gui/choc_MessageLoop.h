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

#ifndef CHOC_MESSAGELOOP_HEADER_INCLUDED
#define CHOC_MESSAGELOOP_HEADER_INCLUDED

#include <memory>
#include <string>
#include <functional>
#include <mutex>
#include "../platform/choc_Platform.h"
#include "../platform/choc_Assert.h"


//==============================================================================
/**
    This namespace provides some bare-minimum event loop and message dispatch
    functions.

    Note that on Linux this uses GTK, so to build it you'll need to:
       1. Install the libgtk-3-dev package.
       2. Link the gtk+3.0 library in your build.
          You might want to have a look inside choc/tests/CMakeLists.txt for
          an example of how to add this packages to your build without too
          much fuss.
*/
namespace choc::messageloop
{
    /// On some platforms (yep, I mean Windows), it's only possible to send
    /// messages after some initialisation has been run on the message thread
    /// itself.
    /// If you call choc::messageloop::run(), then that will automatically
    /// set things up correctly and you don't need to call this function.
    /// But if you're running in a program where there's a 3rd-party event
    /// loop, you'll need to manually call choc::messageloop::initialise() on
    /// your application's message thread at the start of your program to make
    /// sure that any threaded calls to postMessage() work correctly.
    void initialise();

    /// Synchronously runs the system message loop.
    void run();

    /// Posts a message to make the message loop exit and terminate the app.
    void stop();

    /// Posts a function be invoked asynchronously by the message thread.
    ///
    /// Tip: if you might need to cancel the callback after posting it,
    /// you could use a `choc::threading::ThreadSafeFunctor` to wrap your
    /// target function, which lets you safely nullify it.
    void postMessage (std::function<void()>&&);

    //==============================================================================
    /// Manages a periodic timer whose callbacks happen on the message loop.
    ///
    /// You can create a Timer with a callback function and interval, and the
    /// function will be repeatedly called (on the message thread) until the Timer
    /// is deleted, or clear() is called, or the callback function returns false.
    ///
    struct Timer
    {
        Timer() = default;
        Timer (const Timer&) = delete;
        Timer (Timer&&) = default;
        Timer& operator= (Timer&&) = default;
        ~Timer() = default;

        /// The callback function should return true to keep the
        /// timer running, or false to stop it.
        using Callback = std::function<bool()>;

        /// Creates and starts a Timer for a particular interval (in
        /// milliseconds) and a callback.
        /// The callback will continue to be called at this interval
        /// until either the Timer object is deleted, or clear() is
        /// called, or the callback returns `false`.
        Timer (uint32_t intervalMillisecs,
               Callback&& callbackFunction);

        /// Stops and clears the timer. (You can also clear a Timer
        /// by assigning an empty Timer to it).
        void clear()                    { pimpl.reset(); }

        /// Returns true if the Timer has been initialised with
        /// a callback, or false if it's just an empty object.
        operator bool() const           { return pimpl != nullptr; }

    private:
        struct Pimpl;
        std::unique_ptr<Pimpl> pimpl;
    };
}


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

#if CHOC_LINUX

#include "../platform/choc_DisableAllWarnings.h"
#include <gtk/gtk.h>
#include "../platform/choc_ReenableAllWarnings.h"

namespace choc::messageloop
{

inline void initialise() {}
inline void run()   { gtk_main(); }
inline void stop()  { gtk_main_quit(); }

inline void postMessage (std::function<void()>&& fn)
{
    g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                     (GSourceFunc) ([](void* f) -> int
                     {
                         (*static_cast<std::function<void()>*>(f))();
                         return G_SOURCE_REMOVE;
                     }),
                     new std::function<void()> (std::move (fn)),
                     [] (void* f) { delete static_cast<std::function<void()>*>(f); });
}

struct Timer::Pimpl
{
    Pimpl (Callback&& c, uint32_t interval) : callback (std::move (c))
    {
        handle = g_timeout_add (interval, staticCallback, this);
    }

    ~Pimpl()
    {
        g_source_remove (handle);
    }

    static gboolean staticCallback (gpointer context)
    {
        return static_cast<Pimpl*> (context)->callback();
    }

    Callback callback;
    guint handle;
};

//==============================================================================
#elif CHOC_APPLE

#include <unordered_set>
#include <objc/runtime.h>
#include <objc/message.h>
#include <dispatch/dispatch.h>

#include <type_traits>

namespace choc::objc
{
    static inline id getClass (const char* s)              { return (id) objc_getClass (s); }

    template <typename ReturnType, typename... Args>
    static ReturnType call (id target, const char* selector, Args... args)
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

    static inline id getNSString (const char* s)           { return call<id> (getClass ("NSString"), "stringWithUTF8String:", s); }
    static inline id getNSString (const std::string& s)    { return getNSString (s.c_str()); }
    static inline id getNSNumberBool (bool b)              { return call<id> (getClass ("NSNumber"), "numberWithBool:", (BOOL) b); }
    static inline id getSharedNSApplication()              { return call<id> (getClass ("NSApplication"), "sharedApplication"); }

    static inline Class createDelegateClass (const char* baseClass, const char* root)
    {
        auto time = std::chrono::high_resolution_clock::now().time_since_epoch();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds> (time).count();
        auto uniqueDelegateName = root + std::to_string (static_cast<uint32_t> (micros));

        auto c = objc_allocateClassPair (objc_getClass (baseClass), uniqueDelegateName.c_str(), 0);
        CHOC_ASSERT (c);
        return c;
    }

    struct AutoReleasePool
    {
        AutoReleasePool()  { pool = call<id> (getClass ("NSAutoreleasePool"), "new"); }
        ~AutoReleasePool() { call<void> (pool, "release"); }

        id pool;
    };
}

namespace choc::messageloop
{

inline void initialise() {}

inline void run()
{
    objc::AutoReleasePool autoreleasePool;
    objc::call<void> (objc::getSharedNSApplication(), "run");
}

inline void stop()
{
    using namespace choc::objc;
    static constexpr long NSEventTypeApplicationDefined = 15;

    AutoReleasePool autoreleasePool;

    call<void> (getSharedNSApplication(), "stop:", (id) nullptr);

    // After sending the stop message, we need to post a dummy event to
    // kick the message loop, otherwise it can just sit there and hang
    struct NSPoint { double x = 0, y = 0; };
    id dummyEvent = call<id> (getClass ("NSEvent"), "otherEventWithType:location:modifierFlags:timestamp:windowNumber:context:subtype:data1:data2:",
                              NSEventTypeApplicationDefined, NSPoint(), 0, 0, 0, nullptr, (short) 0, 0, 0);
    call<void> (getSharedNSApplication(), "postEvent:atStart:", dummyEvent, YES);
}

inline void postMessage (std::function<void()>&& fn)
{
    dispatch_async_f (dispatch_get_main_queue(),
                      new std::function<void()> (std::move (fn)),
                      (dispatch_function_t) (+[](void* arg)
                      {
                          objc::AutoReleasePool autoReleasePool;
                          std::unique_ptr<std::function<void()>> f (static_cast<std::function<void()>*> (arg));
                          (*f)();
                      }));
}

struct Timer::Pimpl
{
    Pimpl (Callback&& c, uint32_t i) : callback (std::move (c)), interval (i)
    {
        getList().add (this);
        dispatch();
    }

    ~Pimpl()
    {
        getList().remove (this);
    }

    static void staticCallback (void* context)
    {
        if (getList().invokeIfStillAlive (static_cast<Pimpl*> (context)))
        {
            objc::AutoReleasePool autoReleasePool;
            static_cast<Pimpl*> (context)->dispatch();
        }
    }

    void dispatch()
    {
        dispatch_after_f (dispatch_time (DISPATCH_TIME_NOW, interval * 1000000),
                          dispatch_get_main_queue(), this, staticCallback);
    }

    Callback callback;
    const int64_t interval;

    struct TimerList
    {
        std::recursive_mutex lock;
        std::unordered_set<Pimpl*> timers;

        bool invokeIfStillAlive (Pimpl* p)
        {
            std::lock_guard<decltype(lock)> l (lock);

            // must check before AND after the call because the Pimpl
            // may be deleted during the callback
            return timers.find (p) != timers.end()
                    && p->callback()
                    && timers.find (p) != timers.end();
        }

        void add (Pimpl* p)
        {
            std::lock_guard<decltype(lock)> l (lock);
            timers.insert (p);
        }

        void remove (Pimpl* p)
        {
            std::lock_guard<decltype(lock)> l (lock);
            timers.erase (p);
        }
    };

    static TimerList& getList()
    {
        static TimerList list;
        return list;
    }
};

//==============================================================================
#elif CHOC_WINDOWS

#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#undef  NOMINMAX
#define NOMINMAX
#define Rectangle Rectangle_renamed_to_avoid_name_collisions
#include <windows.h>
#undef Rectangle

namespace choc::messageloop
{

struct MessageWindow
{
    MessageWindow()
    {
        className = "choc_" + std::to_string (rand());

        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof (wc);
        wc.hInstance = GetModuleHandleA (nullptr);
        wc.lpszClassName = className.c_str();
        wc.lpfnWndProc = windowProc;

        RegisterClassExA (std::addressof (wc));

        hwnd = CreateWindowA (className.c_str(), "choc", 0, 0, 0, 0, 0,
                              nullptr, nullptr, wc.hInstance, nullptr);
    }

    ~MessageWindow()
    {
        DestroyWindow (hwnd);
        UnregisterClassA (className.c_str(), nullptr);
    }

    static LRESULT CALLBACK windowProc (HWND h, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_APP && wParam == magicWParam)
        {
            std::unique_ptr<std::function<void()>> f (reinterpret_cast<std::function<void()>*> (lParam));
            (*f)();
        }

        return DefWindowProc (h, message, wParam, lParam);
    }

    static inline constexpr WPARAM magicWParam = 0xc40cc40c;

    HWND hwnd;
    std::string className;
    DWORD threadID = GetCurrentThreadId();
};

struct LockedMessageWindow
{
    HWND hwnd;
    std::unique_lock<std::mutex> lock;
};

static LockedMessageWindow getSharedMessageWindow (bool reacreateIfWrongThread = false)
{
    static std::unique_ptr<MessageWindow> window;
    static std::mutex lock;

    std::unique_lock<std::mutex> l (lock);

    if (window == nullptr || (reacreateIfWrongThread && window->threadID != GetCurrentThreadId()))
        window = std::make_unique<MessageWindow>();

    return LockedMessageWindow { window->hwnd, std::move (l) };
}

inline void initialise()
{
    getSharedMessageWindow (true);
}

inline void run()
{
    initialise();

    for (;;)
    {
        MSG msg;

        if (GetMessage (std::addressof (msg), nullptr, 0, 0) == -1)
            break;

        if (msg.message == WM_QUIT)
            break;

        if (msg.hwnd)
        {
            TranslateMessage (std::addressof (msg));
            DispatchMessage (std::addressof (msg));
        }
    }
}

inline void stop()
{
    PostQuitMessage (0);
}

inline void postMessage (std::function<void()>&& fn)
{
    PostMessageA (getSharedMessageWindow().hwnd, WM_APP, MessageWindow::magicWParam,
                  (LPARAM) new std::function<void()> (std::move (fn)));
}

struct Timer::Pimpl
{
    Pimpl (Callback&& c, uint32_t interval) : callback (std::move (c))
    {
        timerID = SetTimer (getSharedMessageWindow().hwnd, reinterpret_cast<UINT_PTR> (this),
                            interval, (TIMERPROC) staticCallback);
    }

    ~Pimpl()
    {
        KillTimer (getSharedMessageWindow().hwnd, timerID);
    }

    void invoke()
    {
        // local copy in case this Pimpl has been deleted after the callback
        auto localIDCopy = timerID;

        if (! callback())
            KillTimer (getSharedMessageWindow().hwnd, localIDCopy);
    }

    static void staticCallback (HWND, UINT, UINT_PTR p, DWORD) noexcept
    {
        reinterpret_cast<Pimpl*> (p)->invoke();
    }

    Callback callback;
    UINT_PTR timerID;
};

#else
 #error "choc::messageloop only supports OSX, Windows or Linux!"
#endif

inline Timer::Timer (uint32_t interval, Callback&& cb)
{
    CHOC_ASSERT (cb != nullptr); // The callback must be a valid function!
    pimpl = std::make_unique<Pimpl> (std::move (cb), interval);
}

} // namespace choc::messageloop


#endif // CHOC_MESSAGELOOP_HEADER_INCLUDED
