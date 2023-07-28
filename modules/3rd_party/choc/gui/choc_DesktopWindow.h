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

#ifndef CHOC_DESKTOPWINDOW_HEADER_INCLUDED
#define CHOC_DESKTOPWINDOW_HEADER_INCLUDED


//==============================================================================
namespace choc::ui
{

/// Represents the position and size of a DesktopWindow or other UI elements.
struct Bounds
{
    int x = 0, y = 0, width = 0, height = 0;
};

/**
    A very basic desktop window class.

    The main use-case for this is as a simple way to host other UI elements
    such as the choc::ui::WebView.

    Because this is a GUI, it needs a message loop to be running. If you're using
    it inside an app which already runs a message loop, it should just work,
    or you can use choc::messageloop::run() and choc::messageloop::stop() for an easy
    but basic loop.

    Note that on Linux this uses GTK, so to build it you'll need to:
       1. Install the libgtk-3-dev package.
       2. Link the gtk+3.0 library in your build.
          You might want to have a look inside choc/tests/CMakeLists.txt for
          an example of how to add this packages to your build without too
          much fuss.

    For an example of how to use this class, see `choc/tests/main.cpp` where
    there's a simple demo.
*/
struct DesktopWindow
{
    DesktopWindow (Bounds);
    ~DesktopWindow();

    /// Sets the title of the window that the browser is inside
    void setWindowTitle (const std::string& newTitle);

    /// Gives the window a child/content view to display.
    /// The pointer being passed in will be a platform-specific native handle,
    /// so a HWND on Windows, an NSView* on OSX, etc.
    void setContent (void* nativeView);

    /// Shows or hides the window. It's visible by default when created.
    void setVisible (bool visible);

    /// Enables/disables user resizing of the window
    void setResizable (bool);

    /// Changes the window's position
    void setBounds (Bounds);

    /// Gives the window a given size and positions it in the middle of the
    /// default monitor
    void centreWithSize (int width, int height);

    /// Sets a minimum size below which the user can't shrink the window
    void setMinimumSize (int minWidth, int minHeight);
    /// Sets a maximum size above which the user can't grow the window
    void setMaximumSize (int maxWidth, int maxHeight);

    /// Tries to bring this window to the front of the Z-order.
    void toFront();

    /// Returns the native OS handle, which may be a HWND on Windows, an
    /// NSWindow* on OSX or a GtkWidget* on linux.
    void* getWindowHandle() const;

    /// An optional callback that will be called when the parent window is resized
    std::function<void()> windowResized;
    /// An optional callback that will be called when the parent window is closed
    std::function<void()> windowClosed;

private:
    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl;
};

//==============================================================================
/// This Windows-only function turns on high-DPI awareness for the current
/// process. On other OSes where no equivalent call is needed, this function is
/// just a stub.
void setWindowsDPIAwareness();


} // namespace choc::ui


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

struct choc::ui::DesktopWindow::Pimpl
{
    Pimpl (DesktopWindow& w, Bounds bounds)  : owner (w)
    {
        if (! gtk_init_check (nullptr, nullptr))
            return;

        window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        g_object_ref_sink (G_OBJECT (window));

        destroyHandlerID = g_signal_connect (G_OBJECT (window), "destroy",
                                             G_CALLBACK (+[](GtkWidget*, gpointer arg)
                                             {
                                                 static_cast<Pimpl*> (arg)->windowDestroyEvent();
                                             }),
                                             this);
        setBounds (bounds);
        setVisible (true);
    }

    ~Pimpl()
    {
        if (destroyHandlerID != 0 && window != nullptr)
            g_signal_handler_disconnect (G_OBJECT (window), destroyHandlerID);

        g_clear_object (&window);
    }

    void windowDestroyEvent()
    {
        g_clear_object (&window);

        if (owner.windowClosed != nullptr)
            owner.windowClosed();
    }

    void* getWindowHandle() const     { return (void*) window; }

    void setWindowTitle (const std::string& newTitle)
    {
        gtk_window_set_title (GTK_WINDOW (window), newTitle.c_str());
    }

    void setContent (void* view)
    {
        if (content != nullptr)
            gtk_container_remove (GTK_CONTAINER (window), content);

        content = GTK_WIDGET (view);
        gtk_container_add (GTK_CONTAINER (window), content);
        gtk_widget_grab_focus (content);
    }

    void setVisible (bool visible)
    {
        if (visible)
            gtk_widget_show_all (window);
        else
            gtk_widget_hide (window);
    }

    void setResizable (bool b)
    {
        gtk_window_set_resizable (GTK_WINDOW (window), b);
    }

    void setMinimumSize (int w, int h)
    {
        GdkGeometry g;
        g.min_width = w;
        g.min_height = h;
        gtk_window_set_geometry_hints (GTK_WINDOW (window), nullptr, &g, GDK_HINT_MIN_SIZE);
    }

    void setMaximumSize (int w, int h)
    {
        GdkGeometry g;
        g.max_width = w;
        g.max_height = h;
        gtk_window_set_geometry_hints (GTK_WINDOW (window), nullptr, &g, GDK_HINT_MAX_SIZE);
    }

    void setBounds (Bounds b)
    {
        setSize (b.width, b.height);
        gtk_window_move (GTK_WINDOW (window), b.x, b.y);
    }

    void setSize (int w, int h)
    {
        if (gtk_window_get_resizable (GTK_WINDOW (window)))
            gtk_window_resize (GTK_WINDOW (window), w, h);
        else
            gtk_widget_set_size_request (window, w, h);
    }

    void centreWithSize (int w, int h)
    {
        setSize (w, h);
        gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
    }

    void toFront()
    {
        gtk_window_activate_default (GTK_WINDOW (window));
    }

    DesktopWindow& owner;
    GtkWidget* window = {};
    GtkWidget* content = {};
    unsigned long destroyHandlerID = 0;
};

inline void choc::ui::setWindowsDPIAwareness() {}

//==============================================================================
#elif CHOC_APPLE

#include "choc_MessageLoop.h"

namespace choc::ui
{

namespace
{
    // Including CodeGraphics.h can create all kinds of messy C/C++ symbol clashes
    // with other headers, but all we actually need are these coordinate structs:
   #if defined (__LP64__) && __LP64__
    using CGFloat = double;
   #else
    using CGFloat = float;
   #endif

    struct CGPoint { CGFloat x = 0, y = 0; };
    struct CGSize  { CGFloat width = 0, height = 0; };
    struct CGRect  { CGPoint origin; CGSize size; };
}

static inline CGSize createCGSize (double w, double h)  { return { (CGFloat) w, (CGFloat) h }; }
static inline CGRect createCGRect (choc::ui::Bounds b)  { return { { (CGFloat) b.x, (CGFloat) b.y }, { (CGFloat) b.width, (CGFloat) b.height } }; }

inline void setWindowsDPIAwareness() {}

struct DesktopWindow::Pimpl
{
    Pimpl (DesktopWindow& w, Bounds bounds)  : owner (w)
    {
        using namespace choc::objc;
        AutoReleasePool autoreleasePool;

        call<void> (getSharedNSApplication(), "setActivationPolicy:", NSApplicationActivationPolicyRegular);

        window = call<id> (call<id> (getClass ("NSWindow"), "alloc"),
                           "initWithContentRect:styleMask:backing:defer:",
                           createCGRect (bounds),
                           NSWindowStyleMaskTitled, NSBackingStoreBuffered, (int) 0);

        delegate = createDelegate();
        objc_setAssociatedObject (delegate, "choc_window", (id) this, OBJC_ASSOCIATION_ASSIGN);
        call<void> (window, "setDelegate:", delegate);
    }

    ~Pimpl()
    {
        objc::AutoReleasePool autoreleasePool;
        objc::call<void> (window, "setDelegate:", nullptr);
        objc::call<void> (window, "close");
        objc::call<void> (delegate, "release");
    }

    void* getWindowHandle() const     { return (void*) window; }

    void setWindowTitle (const std::string& newTitle)
    {
        objc::AutoReleasePool autoreleasePool;
        objc::call<void> (window, "setTitle:", objc::getNSString (newTitle));
    }

    void setContent (void* view)
    {
        objc::AutoReleasePool autoreleasePool;
        objc::call<void> (window, "setContentView:", (id) view);
    }

    void setVisible (bool visible)
    {
        objc::AutoReleasePool autoreleasePool;
        objc::call<void> (window, "setIsVisible:", (BOOL) visible);
    }

    void setResizable (bool b)
    {
        objc::AutoReleasePool autoreleasePool;
        auto style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable
                        | (b ? NSWindowStyleMaskResizable : 0);

        objc::call<void> (window, "setStyleMask:", (unsigned long) style);
    }

    void setMinimumSize (int w, int h) { objc::AutoReleasePool p; objc::call<void> (window, "setContentMinSize:", createCGSize (w, h)); }
    void setMaximumSize (int w, int h) { objc::AutoReleasePool p; objc::call<void> (window, "setContentMaxSize:", createCGSize (w, h)); }

    CGRect getFrameRectForContent (Bounds b)
    {
        return objc::call<CGRect> (window, "frameRectForContentRect:", createCGRect (b));
    }

    void centreWithSize (int w, int h)
    {
        objc::AutoReleasePool autoreleasePool;
        objc::call<void> (window, "setFrame:display:animate:", getFrameRectForContent ({ 0, 0, w, h }), (BOOL) 1, (BOOL) 0);
        objc::call<void> (window, "center");
    }

    void setBounds (Bounds b)
    {
        objc::AutoReleasePool autoreleasePool;
        objc::call<void> (window, "setFrame:display:animate:", getFrameRectForContent (b), (BOOL) 1, (BOOL) 0);
    }

    void toFront()
    {
        objc::AutoReleasePool autoreleasePool;
        objc::call<void> (objc::getSharedNSApplication(), "activateIgnoringOtherApps:", (BOOL) 1);
        objc::call<void> (window, "makeKeyAndOrderFront:", (id) nullptr);
    }

    static Pimpl& getPimplFromContext (id self)
    {
        auto view = (Pimpl*) objc_getAssociatedObject (self, "choc_window");
        CHOC_ASSERT (view != nullptr);
        return *view;
    }

    id createDelegate()
    {
        static DelegateClass dc;
        return objc::call<id> ((id) dc.delegateClass, "new");
    }

    DesktopWindow& owner;
    id window = {}, delegate = {};

    struct DelegateClass
    {
        DelegateClass()
        {
            delegateClass = choc::objc::createDelegateClass ("NSResponder", "CHOCDesktopWindowDelegate_");

            if (auto* p = objc_getProtocol ("NSWindowDelegate"))
                class_addProtocol (delegateClass, p);

            class_addMethod (delegateClass, sel_registerName ("windowShouldClose:"),
                            (IMP) (+[](id self, SEL, id) -> BOOL
                            {
                                objc::AutoReleasePool autoreleasePool;
                                auto& p = getPimplFromContext (self);
                                p.window = {};

                                if (auto callback = p.owner.windowClosed)
                                    choc::messageloop::postMessage ([callback] { callback(); });

                                return TRUE;
                            }),
                            "c@:@");

            class_addMethod (delegateClass, sel_registerName ("windowDidResize:"),
                            (IMP) (+[](id self, SEL, id)
                            {
                                objc::AutoReleasePool autoreleasePool;

                                if (auto callback = getPimplFromContext (self).owner.windowResized)
                                    callback();
                            }),
                            "v@:@");

            class_addMethod (delegateClass, sel_registerName ("applicationShouldTerminateAfterLastWindowClosed:"),
                            (IMP) (+[](id, SEL, id) -> BOOL { return 0; }),
                            "c@:@");

            objc_registerClassPair (delegateClass);
        }

        ~DelegateClass()
        {
            objc_disposeClassPair (delegateClass);
        }

        Class delegateClass = {};
    };

    static constexpr long NSWindowStyleMaskTitled = 1;
    static constexpr long NSWindowStyleMaskMiniaturizable = 4;
    static constexpr long NSWindowStyleMaskResizable = 8;
    static constexpr long NSWindowStyleMaskClosable = 2;
    static constexpr long NSBackingStoreBuffered = 2;
    static constexpr long NSApplicationActivationPolicyRegular = 0;
};

} // namespace choc::ui

//==============================================================================
#elif CHOC_WINDOWS

#undef  WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#undef  NOMINMAX
#define NOMINMAX
#define Rectangle Rectangle_renamed_to_avoid_name_collisions
#include <windows.h>
#undef Rectangle

namespace choc::ui
{

static RECT boundsToRect (Bounds b)
{
    RECT r;
    r.left = b.x;
    r.top = b.y;
    r.right = b.x + b.width;
    r.bottom = b.y + b.height;
    return r;
}

template <typename FunctionType>
FunctionType getUser32Function (const char* name)
{
    if (auto user32 = choc::file::DynamicLibrary ("user32.dll"))
        return reinterpret_cast<FunctionType> (user32.findFunction (name));

    return {};
}

struct HWNDHolder
{
    HWNDHolder() = default;
    HWNDHolder (HWND h) : hwnd (h) {}
    HWNDHolder (const HWNDHolder&) = delete;
    HWNDHolder& operator= (const HWNDHolder&) = delete;
    HWNDHolder (HWNDHolder&& other) : hwnd (other.hwnd) { other.hwnd = {}; }
    HWNDHolder& operator= (HWNDHolder&& other)  { hwnd = other.hwnd; other.hwnd = {}; return *this; }
    ~HWNDHolder() { if (IsWindow (hwnd)) DestroyWindow (hwnd); }

    operator HWND() const  { return hwnd; }
    operator void*() const  { return (void*) hwnd; }

    HWND hwnd = {};
};

struct WindowClass
{
    WindowClass (std::wstring name, WNDPROC wndProc)
    {
        name += std::to_wstring (rand());

        moduleHandle = GetModuleHandle (nullptr);
        auto icon = (HICON) LoadImage (moduleHandle, IDI_APPLICATION, IMAGE_ICON,
                                       GetSystemMetrics (SM_CXSMICON),
                                       GetSystemMetrics (SM_CYSMICON),
                                       LR_DEFAULTCOLOR);

        WNDCLASSEXW wc;
        ZeroMemory (&wc, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.style = CS_OWNDC;
        wc.hInstance = moduleHandle;
        wc.lpszClassName = name.c_str();
        wc.hIcon = icon;
        wc.hIconSm = icon;
        wc.lpfnWndProc = wndProc;

        classAtom = (LPCWSTR) (uintptr_t) RegisterClassExW (&wc);
        CHOC_ASSERT (classAtom != 0);
    }

    ~WindowClass()
    {
        UnregisterClassW (classAtom, moduleHandle);
    }

    HWNDHolder createWindow (DWORD style, int w, int h, void* userData)
    {
        if (auto hwnd = CreateWindowW (classAtom, L"", style, CW_USEDEFAULT, CW_USEDEFAULT,
                                       w, h, nullptr, nullptr, moduleHandle, nullptr))
        {
            SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR) userData);
            return hwnd;
        }

        return {};
    }

    auto getClassName() const    { return classAtom; }

    HINSTANCE moduleHandle = {};
    LPCWSTR classAtom = {};
};

static std::string createUTF8FromUTF16 (const std::wstring& utf16)
{
    if (! utf16.empty())
    {
        auto numWideChars = static_cast<int> (utf16.size());
        auto resultSize = WideCharToMultiByte (CP_UTF8, WC_ERR_INVALID_CHARS, utf16.data(), numWideChars, nullptr, 0, nullptr, nullptr);

        if (resultSize > 0)
        {
            std::string result;
            result.resize (static_cast<size_t> (resultSize));

            if (WideCharToMultiByte (CP_UTF8, WC_ERR_INVALID_CHARS, utf16.data(), numWideChars, result.data(), resultSize, nullptr, nullptr) > 0)
                return result;
        }
    }

    return {};
}

static std::wstring createUTF16StringFromUTF8 (std::string_view utf8)
{
    if (! utf8.empty())
    {
        auto numUTF8Bytes = static_cast<int> (utf8.size());
        auto resultSize = MultiByteToWideChar (CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), numUTF8Bytes, nullptr, 0);

        if (resultSize > 0)
        {
            std::wstring result;
            result.resize (static_cast<size_t> (resultSize));

            if (MultiByteToWideChar (CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), numUTF8Bytes, result.data(), resultSize) > 0)
                return result;
        }
    }

    return {};
}

inline void setWindowsDPIAwareness()
{
    if (auto setProcessDPIAwarenessContext = getUser32Function<int(__stdcall *)(void*)> ("SetProcessDpiAwarenessContext"))
        setProcessDPIAwarenessContext (/*DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2*/ (void*) -4);
}

//==============================================================================
struct DesktopWindow::Pimpl
{
    Pimpl (DesktopWindow& w, Bounds b)  : owner (w)
    {
        hwnd = windowClass.createWindow (WS_OVERLAPPEDWINDOW, 640, 480, this);

        if (hwnd.hwnd == nullptr)
            return;

        setBounds (b);
        ShowWindow (hwnd, SW_SHOW);
        UpdateWindow (hwnd);
        SetFocus (hwnd);
    }

    void* getWindowHandle() const     { return hwnd; }

    void setWindowTitle (const std::string& newTitle)
    {
        SetWindowTextW (hwnd, createUTF16StringFromUTF8 (newTitle).c_str());
    }

    void setContent (void* childHandle)
    {
        if (auto child = getFirstChildWindow())
        {
            ShowWindow (child, SW_HIDE);
            SetParent (child, nullptr);
        }

        auto child = (HWND) childHandle;
        auto flags = GetWindowLongPtr (child, -16);
        flags = (flags & ~(decltype (flags)) WS_POPUP) | (decltype (flags)) WS_CHILD;
        SetWindowLongPtr (child, -16, flags);

        SetParent (child, hwnd);
        resizeContentToFit();
        ShowWindow (child, IsWindowVisible (hwnd) ? SW_SHOW : SW_HIDE);
    }

    void setVisible (bool visible)
    {
        ShowWindow (hwnd, visible ? SW_SHOW : SW_HIDE);

        if (visible)
            InvalidateRect (hwnd, nullptr, 0);
    }

    void setResizable (bool b)
    {
        auto style = GetWindowLong (hwnd, GWL_STYLE);

        if (b)
            style |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
        else
            style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

        SetWindowLong (hwnd, GWL_STYLE, style);
    }

    void setMinimumSize (int w, int h)
    {
        minimumSize.x = w;
        minimumSize.y = h;
    }

    void setMaximumSize (int w, int h)
    {
        maximumSize.x = w;
        maximumSize.y = h;
    }

    void getMinMaxInfo (MINMAXINFO& m) const
    {
        if (maximumSize.x > 0 && maximumSize.y > 0)
        {
            m.ptMaxSize = maximumSize;
            m.ptMaxTrackSize = maximumSize;
        }

        if (minimumSize.x > 0 && minimumSize.y > 0)
            m.ptMinTrackSize = minimumSize;
    }

    void centreWithSize (int w, int h)
    {
        setBounds ({ 0, 0, w, h }, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_FRAMECHANGED);
    }

    void setBounds (Bounds b)
    {
        setBounds (b, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }

    void setBounds (Bounds b, DWORD flags)
    {
        auto r = boundsToRect (scaleBounds (b, getWindowDPI() / 96.0));
        AdjustWindowRect (&r, WS_OVERLAPPEDWINDOW, 0);
        SetWindowPos (hwnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top, flags);
        resizeContentToFit();
    }

    void toFront()
    {
        BringWindowToTop (hwnd);
    }

private:
    DesktopWindow& owner;
    HWNDHolder hwnd;
    POINT minimumSize = {}, maximumSize = {};
    WindowClass windowClass { L"CHOCWindow", (WNDPROC) wndProc };

    Bounds scaleBounds (Bounds b, double scale)
    {
        b.x      = static_cast<decltype(b.x)> (b.x * scale);
        b.y      = static_cast<decltype(b.y)> (b.y * scale);
        b.width  = static_cast<decltype(b.width)> (b.width * scale);
        b.height = static_cast<decltype(b.height)> (b.height * scale);

        return b;
    }

    HWND getFirstChildWindow()
    {
        HWND result = {};

        if (IsWindow (hwnd))
        {
            EnumChildWindows (hwnd, +[](HWND w, LPARAM context)
            {
                *reinterpret_cast<HWND*> (context) = w;
                return FALSE;
            }, (LPARAM) &result);
        }

        return result;
    }

    void resizeContentToFit()
    {
        if (auto child = getFirstChildWindow())
        {
            RECT r;
            GetClientRect (hwnd, &r);
            SetWindowPos (child, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top,
                          SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_FRAMECHANGED);
        }
    }

    static void enableNonClientDPIScaling (HWND h)
    {
        if (auto fn = getUser32Function<BOOL(__stdcall*)(HWND)> ("EnableNonClientDpiScaling"))
            fn (h);
    }

    uint32_t getWindowDPI() const
    {
        if (auto getDpiForWindow = getUser32Function<UINT(__stdcall*)(HWND)> ("GetDpiForWindow"))
            return getDpiForWindow (hwnd);

        return 96;
    }

    static Pimpl* getPimpl (HWND h)     { return (Pimpl*) GetWindowLongPtr (h, GWLP_USERDATA); }

    static LRESULT wndProc (HWND h, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg)
        {
            case WM_NCCREATE:        enableNonClientDPIScaling (h); break;
            case WM_SIZE:            if (auto w = getPimpl (h)) w->resizeContentToFit(); break;
            case WM_CLOSE:           if (auto w = getPimpl (h)) if (w->owner.windowClosed != nullptr) w->owner.windowClosed(); return 0;
            case WM_GETMINMAXINFO:   if (auto w = getPimpl (h)) w->getMinMaxInfo (*(LPMINMAXINFO) lp); return 0;
            default:                 break;
        }

        return DefWindowProcW (h, msg, wp, lp);
    }
};

} // namespace choc::ui

#else
 #error "choc WebView only supports OSX, Windows or Linux!"
#endif

namespace choc::ui
{

//==============================================================================
inline DesktopWindow::DesktopWindow (Bounds b) { pimpl = std::make_unique<Pimpl> (*this, b); }
inline DesktopWindow::~DesktopWindow()  {}

inline void* DesktopWindow::getWindowHandle() const                        { return pimpl->getWindowHandle(); }
inline void DesktopWindow::setContent (void* view)                         { pimpl->setContent (view); }
inline void DesktopWindow::setVisible (bool visible)                       { pimpl->setVisible (visible); }
inline void DesktopWindow::setWindowTitle (const std::string& title)       { pimpl->setWindowTitle (title); }
inline void DesktopWindow::setMinimumSize (int w, int h)                   { pimpl->setMinimumSize (w, h); }
inline void DesktopWindow::setMaximumSize (int w, int h)                   { pimpl->setMaximumSize (w, h); }
inline void DesktopWindow::setResizable (bool b)                           { pimpl->setResizable (b); }
inline void DesktopWindow::setBounds (Bounds b)                            { pimpl->setBounds (b); }
inline void DesktopWindow::centreWithSize (int w, int h)                   { pimpl->centreWithSize (w, h); }
inline void DesktopWindow::toFront()                                       { pimpl->toFront(); }

} // namespace choc::ui

#endif // CHOC_DESKTOPWINDOW_HEADER_INCLUDED
