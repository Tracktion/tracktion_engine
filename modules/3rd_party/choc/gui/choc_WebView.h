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

#ifndef CHOC_WEBVIEW_HEADER_INCLUDED
#define CHOC_WEBVIEW_HEADER_INCLUDED

#include <optional>
#include <unordered_map>
#include <vector>
#include <functional>
#include "../platform/choc_Platform.h"
#include "../text/choc_JSON.h"

//==============================================================================
namespace choc::ui
{

/**
    Creates an embedded browser which can be placed inside some kind of parent window.

    After creating a WebView object, its getViewHandle() returns a platform-specific
    handle that can be added to whatever kind of window is appropriate. The
    choc::ui::DesktopWindow class is an example of a window that can have the
    webview added to it via its choc::ui::DesktopWindow::setContent() method.

    There are unfortunately a few extra build steps needed for using WebView
    in your projects:

    - On OSX, you'll need to add the `WebKit` framework to your project

    - On Linux, you'll need to:
       1. Install the libgtk-3-dev and libwebkit2gtk-4.0-dev packages.
       2. Link the gtk+3.0 and webkit2gtk-4.0 libraries in your build.
          You might want to have a look inside choc/tests/CMakeLists.txt for
          an example of how to add those packages to your build without too
          much fuss.

    - On Windows, no extra build steps needed!! This is a bigger deal than it
      sounds, because normally to use an embedded Edge browser on Windows is a
      total PITA, involving downloading a special Microsoft SDK with redistributable
      DLLs, etc, but I've jumped through many ugly hoops to make this class
      fully self-contained.

    Because this is a GUI, it needs a message loop to be running. If you're using
    it inside an app which already runs a message loop, it should just work,
    or you can use choc::messageloop::run() and choc::messageloop::stop() for an easy
    but basic loop.

    For an example of how to use this class, see `choc/tests/main.cpp` where
    there's a simple demo.
*/
class WebView
{
public:
    /// Contains optional settings to pass to a WebView constructor.
    struct Options
    {
        /// If supported, this enables developer features in the browser
        bool enableDebugMode = false;

        /// On OSX, setting this to true will allow the first click on a non-focused
        /// webview to be used as input, rather than the default behaviour, which is
        /// for the first click to give the webview focus but not trigger any action.
        bool acceptsFirstMouseClick = false;

        /// Serve resources to the browser from a C++ callback function.
        /// This can effectively be used to implement a basic web server,
        /// serving resources to the browser in any way the client code chooses.
        /// Given the path URL component (i.e. starting from "/"), the client should
        /// return some bytes, and the associated MIME type, for that resource.
        /// When provided, this function will initially be called with the root path
        /// ("/") in order to serve the initial content for the page.
        /// (i.e. the client should return some HTML with a "text/html" MIME type).
        /// Subsequent relative requests made from the page (i.e. via `img` tags,
        /// `fetch` calls from javascript etc) will result in subsequent calls here.
        struct Resource
        {
            std::vector<uint8_t> data;
            std::string mimeType;
        };

        using Path = std::string;
        using FetchResource = std::function<std::optional<Resource>(const Path&)>;
        FetchResource fetchResource;
    };

    /// Creates a WebView with default options
    WebView();
    /// Creates a WebView with some options
    WebView (const Options&);

    WebView (const WebView&) = delete;
    WebView (WebView&&) = default;
    WebView& operator= (WebView&&) = default;
    ~WebView();

    /// Directly sets the HTML content of the browser
    void setHTML (const std::string& html);

    /// Directly evaluates some javascript
    void evaluateJavascript (const std::string& script);

    /// Sends the browser to this URL
    void navigate (const std::string& url);

    /// A callback function which can be passed to bind().
    using CallbackFn = std::function<choc::value::Value(const choc::value::ValueView& args)>;
    /// Binds a C++ function to a named javascript function that can be called
    /// by code running in the browser.
    void bind (const std::string& functionName, CallbackFn&& function);
    /// Removes a previously-bound function.
    void unbind (const std::string& functionName);

    /// Adds a script to run when the browser loads a page
    void addInitScript (const std::string& script);

    /// Returns a platform-specific handle for this view
    void* getViewHandle() const;

private:
    //==============================================================================
    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl;

    std::unordered_map<std::string, CallbackFn> bindings;
    void invokeBinding (const std::string&);
};

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

#include "../platform/choc_DisableAllWarnings.h"
#include <JavaScriptCore/JavaScript.h>
#include <webkit2/webkit2.h>
#include "../platform/choc_ReenableAllWarnings.h"

struct choc::ui::WebView::Pimpl
{
    Pimpl (WebView& v, const Options& options)
        : owner (v), fetchResource (options.fetchResource)
    {
        if (! gtk_init_check (nullptr, nullptr))
            return;

        webviewContext = webkit_web_context_new();
        g_object_ref_sink (G_OBJECT (webviewContext));
        webview = webkit_web_view_new_with_context (webviewContext);
        g_object_ref_sink (G_OBJECT (webview));
        manager = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (webview));

        signalHandlerID = g_signal_connect (manager, "script-message-received::external",
                                            G_CALLBACK (+[] (WebKitUserContentManager*, WebKitJavascriptResult* r, gpointer arg)
                                            {
                                                static_cast<Pimpl*> (arg)->invokeCallback (r);
                                            }),
                                            this);

        webkit_user_content_manager_register_script_message_handler (manager, "external");
        addInitScript ("window.external = { invoke: function(s) { window.webkit.messageHandlers.external.postMessage(s); } }");

        WebKitSettings* settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (webview));
        webkit_settings_set_javascript_can_access_clipboard (settings, true);

        if (options.enableDebugMode)
        {
            webkit_settings_set_enable_write_console_messages_to_stdout (settings, true);
            webkit_settings_set_enable_developer_extras (settings, true);
        }

        if (options.fetchResource)
        {
            const auto onResourceRequested = [](auto* request, auto* context)
            {
                try
                {
                    const auto* path = webkit_uri_scheme_request_get_path (request);

                    if (const auto resource = static_cast<Pimpl*> (context)->fetchResource (path))
                    {
                        const auto& [bytes, mimeType] = *resource;

                        auto* streamBytes = g_bytes_new (bytes.data(), static_cast<gsize> (bytes.size()));
                        auto* stream = g_memory_input_stream_new_from_bytes (streamBytes);
                        g_bytes_unref (streamBytes);

                        auto* response = webkit_uri_scheme_response_new (stream, static_cast<gint64> (bytes.size()));
                        webkit_uri_scheme_response_set_status (response, 200, nullptr);
                        webkit_uri_scheme_response_set_content_type (response, mimeType.c_str());

                        auto* headers = soup_message_headers_new (SOUP_MESSAGE_HEADERS_RESPONSE);
                        soup_message_headers_append (headers, "Cache-Control", "no-store");
                        webkit_uri_scheme_response_set_http_headers (response, headers); // response takes ownership of the headers

                        webkit_uri_scheme_request_finish_with_response (request, response);

                        g_object_unref (stream);
                        g_object_unref (response);
                    }
                    else
                    {
                        auto* stream = g_memory_input_stream_new();
                        auto* response = webkit_uri_scheme_response_new (stream, -1);
                        webkit_uri_scheme_response_set_status (response, 404, nullptr);

                        webkit_uri_scheme_request_finish_with_response (request, response);

                        g_object_unref (stream);
                        g_object_unref (response);
                    }
                }
                catch (...)
                {
                    auto* error = g_error_new (WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_FAILED, "Something went wrong");
                    webkit_uri_scheme_request_finish_error (request, error);
                    g_error_free (error);
                }
            };

            webkit_web_context_register_uri_scheme (webviewContext, "choc", onResourceRequested, this, nullptr);

            navigate ("choc://choc.choc/");
        }

        gtk_widget_show_all (webview);
    }

    ~Pimpl()
    {
        if (signalHandlerID != 0 && webview != nullptr)
            g_signal_handler_disconnect (manager, signalHandlerID);

        g_clear_object (&webview);
        g_clear_object (&webviewContext);
    }

    void* getViewHandle() const     { return (void*) webview; }

    void evaluateJavascript (const std::string& js)
    {
        webkit_web_view_run_javascript (WEBKIT_WEB_VIEW (webview), js.c_str(), nullptr, nullptr, nullptr);
    }

    void navigate (const std::string& url)
    {
        webkit_web_view_load_uri (WEBKIT_WEB_VIEW (webview), url.c_str());
    }

    void setHTML (const std::string& html)
    {
        webkit_web_view_load_html (WEBKIT_WEB_VIEW (webview), html.c_str(), nullptr);
    }

    void addInitScript (const std::string& js)
    {
        if (manager != nullptr)
            webkit_user_content_manager_add_script (manager, webkit_user_script_new (js.c_str(),
                                                                                     WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
                                                                                     WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
                                                                                     nullptr, nullptr));
    }

    void invokeCallback (WebKitJavascriptResult* r)
    {
        auto s = jsc_value_to_string (webkit_javascript_result_get_js_value (r));
        owner.invokeBinding (s);
        g_free (s);
    }

    WebView& owner;
    Options::FetchResource fetchResource;
    WebKitWebContext* webviewContext = {};
    GtkWidget* webview = {};
    WebKitUserContentManager* manager = {};
    unsigned long signalHandlerID = 0;
};

//==============================================================================
#elif CHOC_APPLE

#include "choc_MessageLoop.h"

struct choc::ui::WebView::Pimpl
{
    Pimpl (WebView& v, const Options& optionsToUse)
        : owner (v), options (optionsToUse)
    {
        using namespace choc::objc;
        AutoReleasePool autoreleasePool;

        auto config = call<id> (getClass ("WKWebViewConfiguration"), "new");

        auto prefs = call<id> (config, "preferences");
        call<id> (prefs, "setValue:forKey:", getNSNumberBool (true), getNSString ("fullScreenEnabled"));
        call<id> (prefs, "setValue:forKey:", getNSNumberBool (true), getNSString ("DOMPasteAllowed"));
        call<id> (prefs, "setValue:forKey:", getNSNumberBool (true), getNSString ("javaScriptCanAccessClipboard"));

        if (options.enableDebugMode)
            call<id> (prefs, "setValue:forKey:", getNSNumberBool (true), getNSString ("developerExtrasEnabled"));

        delegate = createDelegate();
        objc_setAssociatedObject (delegate, "choc_webview", (id) this, OBJC_ASSOCIATION_ASSIGN);

        manager = call<id> (config, "userContentController");
        call<void> (manager, "retain");
        call<void> (manager, "addScriptMessageHandler:name:", delegate, getNSString ("external"));

        addInitScript ("window.external = { invoke: function(s) { window.webkit.messageHandlers.external.postMessage(s); } };");

        if (options.fetchResource)
            call<void> (config, "setURLSchemeHandler:forURLScheme:", delegate, getNSString ("choc"));

        webview = call<id> (allocateWebview(), "initWithFrame:configuration:", CGRect(), config);
        objc_setAssociatedObject (webview, "choc_webview", (id) this, OBJC_ASSOCIATION_ASSIGN);

        call<void> (config, "release");

        if (options.fetchResource)
            navigate ("choc://choc.choc/");
    }

    ~Pimpl()
    {
        objc::AutoReleasePool autoreleasePool;
        objc_setAssociatedObject (delegate, "choc_webview", nil, OBJC_ASSOCIATION_ASSIGN);
        objc_setAssociatedObject (webview, "choc_webview", nil, OBJC_ASSOCIATION_ASSIGN);
        objc::call<void> (webview, "release");
        objc::call<void> (manager, "removeScriptMessageHandlerForName:", objc::getNSString ("external"));
        objc::call<void> (manager, "release");
        objc::call<void> (delegate, "release");
    }

    void* getViewHandle() const     { return (void*) webview; }

    void addInitScript (const std::string& script)
    {
        using namespace choc::objc;
        AutoReleasePool autoreleasePool;
        auto s = call<id> (call<id> (getClass ("WKUserScript"), "alloc"), "initWithSource:injectionTime:forMainFrameOnly:",
                                     getNSString (script), WKUserScriptInjectionTimeAtDocumentStart, (BOOL) 1);
        call<void> (manager, "addUserScript:", s);
        call<void> (s, "release");
    }

    void navigate (const std::string& url)
    {
        using namespace choc::objc;
        AutoReleasePool autoreleasePool;
        auto nsURL = call<id> (getClass ("NSURL"), "URLWithString:", getNSString (url));
        call<void> (webview, "loadRequest:", call<id> (getClass ("NSURLRequest"), "requestWithURL:", nsURL));
    }

    void setHTML (const std::string& html)
    {
        objc::AutoReleasePool autoreleasePool;
        objc::call<void> (webview, "loadHTMLString:baseURL:", objc::getNSString (html), (id) nullptr);
    }

    void evaluateJavascript (const std::string& script)
    {
        objc::AutoReleasePool autoreleasePool;
        objc::call<void> (webview, "evaluateJavaScript:completionHandler:", objc::getNSString (script), (id) nullptr);
    }

private:
    id createDelegate()
    {
        static DelegateClass dc;
        return objc::call<id> ((id) dc.delegateClass, "new");
    }

    id allocateWebview()
    {
        static WebviewClass c;
        return objc::call<id> ((id) c.webviewClass, "alloc");
    }

    void onResourceRequested (id task)
    {
        using namespace choc::objc;
        AutoReleasePool autoreleasePool;

        try
        {
            const auto requestUrl = call<id> (call<id> (task, "request"), "URL");

            const auto makeResponse = [&](auto responseCode, id headerFields = nullptr)
            {
                return call<id> (call<id> (call<id> (getClass ("NSHTTPURLResponse"), "alloc"),
                                           "initWithURL:statusCode:HTTPVersion:headerFields:",
                                           requestUrl,
                                           responseCode,
                                           getNSString ("HTTP/1.1"),
                                           headerFields), "autorelease");
            };

            const auto* path = objc::call<const char*> (call<id> (requestUrl, "path"),  "UTF8String");

            if (const auto resource = options.fetchResource (path))
            {
                const auto& [bytes, mimeType] = *resource;

                const auto contentLength = std::to_string (bytes.size());
                const id headerKeys[] = { getNSString ("Content-Length"), getNSString ("Content-Type"), getNSString ("Cache-Control") };
                const id headerObjects[] = { getNSString (contentLength), getNSString (mimeType), getNSString ("no-store") };
                const auto headerFields = call<id> (getClass ("NSDictionary"),
                                                    "dictionaryWithObjects:forKeys:count:",
                                                    headerObjects,
                                                    headerKeys,
                                                    sizeof (headerObjects) / sizeof (id));

                call<void> (task, "didReceiveResponse:", makeResponse (200, headerFields));

                const auto data = call<id> (getClass ("NSData"), "dataWithBytes:length:", bytes.data(), bytes.size());
                call<void> (task, "didReceiveData:", data);
            }
            else
            {
                call<void> (task, "didReceiveResponse:", makeResponse (404));
            }

            call<void> (task, "didFinish");
        }
        catch (...)
        {
            const auto error = call<id> (getClass ("NSError"), "errorWithDomain:code:userInfo:",
                                         getNSString ("NSURLErrorDomain"), -1, nullptr);
            call<void> (task, "didFailWithError:", error);
        }
    }

    WebView& owner;
    Options options;
    id webview = {}, manager = {}, delegate = {};

    struct WebviewClass
    {
        WebviewClass()
        {
            webviewClass = choc::objc::createDelegateClass ("WKWebView", "CHOCWebView_");

            class_addMethod (webviewClass, sel_registerName ("acceptsFirstMouse:"),
                            (IMP) (+[](id self, SEL, id) -> BOOL
                            {
                                if (auto p = reinterpret_cast<Pimpl*> (objc_getAssociatedObject (self, "choc_webview")))
                                    return p->options.acceptsFirstMouseClick;

                                return false;
                            }), "B@:@");

            objc_registerClassPair (webviewClass);
        }

        ~WebviewClass()
        {
            objc_disposeClassPair (webviewClass);
        }

        Class webviewClass = {};
    };

    struct DelegateClass
    {
        DelegateClass()
        {
            delegateClass = choc::objc::createDelegateClass ("NSObject", "CHOCWebViewDelegate_");

            class_addMethod (delegateClass, sel_registerName ("userContentController:didReceiveScriptMessage:"),
                            (IMP) (+[](id self, SEL, id, id msg)
                            {
                                if (auto p = reinterpret_cast<Pimpl*> (objc_getAssociatedObject (self, "choc_webview")))
                                {
                                    auto body = objc::call<id> (msg, "body");
                                    p->owner.invokeBinding (objc::call<const char*> (body, "UTF8String"));
                                }
                            }),
                            "v@:@@");

            class_addMethod (delegateClass, sel_registerName ("webView:startURLSchemeTask:"),
                            (IMP) (+[](id self, SEL, id, id task)
                            {
                                if (auto p = reinterpret_cast<Pimpl*> (objc_getAssociatedObject (self, "choc_webview")))
                                    p->onResourceRequested (task);
                            }),
                            "v@:@@");

            class_addMethod (delegateClass, sel_registerName ("webView:stopURLSchemeTask:"), (IMP) (+[](id, SEL, id, id) {}), "v@:@@");

            objc_registerClassPair (delegateClass);
        }

        ~DelegateClass()
        {
            objc_disposeClassPair (delegateClass);
        }

        Class delegateClass = {};
    };

    static constexpr long WKUserScriptInjectionTimeAtDocumentStart = 0;

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
};

//==============================================================================
#elif CHOC_WINDOWS

#include "../platform/choc_DynamicLibrary.h"

// If you want to supply your own mechanism for finding the Microsoft
// Webview2Loader.dll file, then define the CHOC_FIND_WEBVIEW2LOADER_DLL
// macro, which must evaluate to a choc::file::DynamicLibrary object
// which points to the DLL.
#ifndef CHOC_FIND_WEBVIEW2LOADER_DLL
 #define CHOC_USE_INTERNAL_WEBVIEW_DLL 1
 #define CHOC_FIND_WEBVIEW2LOADER_DLL choc::ui::getWebview2LoaderDLL()
 #include "../platform/choc_MemoryDLL.h"
 namespace choc::ui
 {
    using WebViewDLL = choc::memory::MemoryDLL;
    static WebViewDLL getWebview2LoaderDLL();
 }
#else
 namespace choc::ui
 {
    using WebViewDLL = choc::file::DynamicLibrary;
 }
#endif

#include "choc_DesktopWindow.h"

#ifndef __webview2_h__
#define __webview2_h__

#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include <atomic>
#include <shlobj.h>
#include <rpc.h>
#include <rpcndr.h>
#include <objidl.h>
#include <oaidl.h>

#ifndef __RPCNDR_H_VERSION__
#error "This code requires an updated version of <rpcndr.h>"
#endif

extern "C"
{

struct EventRegistrationToken { __int64 value; };

typedef interface ICoreWebView2 ICoreWebView2;
typedef interface ICoreWebView2Controller ICoreWebView2Controller;
typedef interface ICoreWebView2Environment ICoreWebView2Environment;
typedef interface ICoreWebView2HttpHeadersCollectionIterator ICoreWebView2HttpHeadersCollectionIterator;
typedef interface ICoreWebView2HttpRequestHeaders ICoreWebView2HttpRequestHeaders;
typedef interface ICoreWebView2HttpResponseHeaders ICoreWebView2HttpResponseHeaders;
typedef interface ICoreWebView2WebResourceRequest ICoreWebView2WebResourceRequest;
typedef interface ICoreWebView2WebResourceRequestedEventArgs ICoreWebView2WebResourceRequestedEventArgs;
typedef interface ICoreWebView2WebResourceRequestedEventHandler ICoreWebView2WebResourceRequestedEventHandler;
typedef interface ICoreWebView2WebResourceResponse ICoreWebView2WebResourceResponse;

MIDL_INTERFACE("8B4F98CE-DB0D-4E71-85FD-C4C4EF1F2630")
ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT, ICoreWebView2Environment*) = 0;
};

MIDL_INTERFACE("86EF6808-3C3F-4C6F-975E-8CE0B98F70BA")
ICoreWebView2CreateCoreWebView2ControllerCompletedHandler : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT, ICoreWebView2Controller*) = 0;
};

MIDL_INTERFACE("DA66D884-6DA8-410E-9630-8C48F8B3A40E")
ICoreWebView2Environment : public IUnknown
{
public:
     virtual HRESULT STDMETHODCALLTYPE CreateCoreWebView2Controller(HWND, ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*) = 0;
     virtual HRESULT STDMETHODCALLTYPE CreateWebResourceResponse(IStream*, int, LPCWSTR, LPCWSTR, ICoreWebView2WebResourceResponse**) = 0;
     virtual HRESULT STDMETHODCALLTYPE get_BrowserVersionString(LPWSTR*) = 0;
     virtual HRESULT STDMETHODCALLTYPE add_NewBrowserVersionAvailable(void*, EventRegistrationToken*) = 0;
     virtual HRESULT STDMETHODCALLTYPE remove_NewBrowserVersionAvailable(EventRegistrationToken) = 0;
};

MIDL_INTERFACE("B263B5AE-9C54-4B75-B632-40AE1A0B6912")
ICoreWebView2WebMessageReceivedEventArgs : public IUnknown
{
public:
     virtual HRESULT STDMETHODCALLTYPE get_Source(LPWSTR *) = 0;
     virtual HRESULT STDMETHODCALLTYPE get_WebMessageAsJson(LPWSTR *) = 0;
     virtual HRESULT STDMETHODCALLTYPE TryGetWebMessageAsString(LPWSTR *) = 0;
};

MIDL_INTERFACE("199328C8-9964-4F5F-84E6-E875B1B763D6")
ICoreWebView2WebMessageReceivedEventHandler : public IUnknown
{
public:
     virtual HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *) = 0;
};

enum COREWEBVIEW2_PERMISSION_KIND
{
    COREWEBVIEW2_PERMISSION_KIND_UNKNOWN_PERMISSION = 0,
    COREWEBVIEW2_PERMISSION_KIND_MICROPHONE         = (COREWEBVIEW2_PERMISSION_KIND_UNKNOWN_PERMISSION + 1),
    COREWEBVIEW2_PERMISSION_KIND_CAMERA             = (COREWEBVIEW2_PERMISSION_KIND_MICROPHONE + 1),
    COREWEBVIEW2_PERMISSION_KIND_GEOLOCATION        = (COREWEBVIEW2_PERMISSION_KIND_CAMERA + 1),
    COREWEBVIEW2_PERMISSION_KIND_NOTIFICATIONS      = (COREWEBVIEW2_PERMISSION_KIND_GEOLOCATION + 1),
    COREWEBVIEW2_PERMISSION_KIND_OTHER_SENSORS      = (COREWEBVIEW2_PERMISSION_KIND_NOTIFICATIONS + 1),
    COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ     = (COREWEBVIEW2_PERMISSION_KIND_OTHER_SENSORS + 1)
};

enum COREWEBVIEW2_PERMISSION_STATE
{
    COREWEBVIEW2_PERMISSION_STATE_DEFAULT  = 0,
    COREWEBVIEW2_PERMISSION_STATE_ALLOW    = (COREWEBVIEW2_PERMISSION_STATE_DEFAULT + 1),
    COREWEBVIEW2_PERMISSION_STATE_DENY     = (COREWEBVIEW2_PERMISSION_STATE_ALLOW + 1)
};

enum COREWEBVIEW2_MOVE_FOCUS_REASON
{
    COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC = 0,
    COREWEBVIEW2_MOVE_FOCUS_REASON_NEXT         = (COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC + 1),
    COREWEBVIEW2_MOVE_FOCUS_REASON_PREVIOUS     = (COREWEBVIEW2_MOVE_FOCUS_REASON_NEXT + 1)
};

enum COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT {};
enum COREWEBVIEW2_WEB_RESOURCE_CONTEXT
{
    COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL = 0
};

MIDL_INTERFACE("A7ED8BF0-3EC9-4E39-8427-3D6F157BD285")
ICoreWebView2Deferral : public IUnknown
{
public:
     virtual HRESULT STDMETHODCALLTYPE Complete() = 0;
};

MIDL_INTERFACE("97055cd4-512c-4264-8b5f-e3f446cea6a5")
ICoreWebView2WebResourceRequest : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_Uri(LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Uri(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Method(LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Method(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Content(IStream**) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Content(IStream*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Headers(ICoreWebView2HttpRequestHeaders**) = 0;
};

MIDL_INTERFACE("774B5EA1-3FAD-435C-B1FC-A77D1ACD5EAF")
ICoreWebView2PermissionRequestedEventArgs : public IUnknown
{
public:
     virtual HRESULT STDMETHODCALLTYPE get_Uri(LPWSTR *) = 0;
     virtual HRESULT STDMETHODCALLTYPE get_PermissionKind(COREWEBVIEW2_PERMISSION_KIND *) = 0;
     virtual HRESULT STDMETHODCALLTYPE get_IsUserInitiated(BOOL *) = 0;
     virtual HRESULT STDMETHODCALLTYPE get_State(COREWEBVIEW2_PERMISSION_STATE *) = 0;
     virtual HRESULT STDMETHODCALLTYPE put_State(COREWEBVIEW2_PERMISSION_STATE) = 0;
     virtual HRESULT STDMETHODCALLTYPE GetDeferral(ICoreWebView2Deferral **) = 0;
};

MIDL_INTERFACE("543B4ADE-9B0B-4748-9AB7-D76481B223AA")
ICoreWebView2PermissionRequestedEventHandler : public IUnknown
{
public:
     virtual HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *, ICoreWebView2PermissionRequestedEventArgs *) = 0;
};

MIDL_INTERFACE("189B8AAF-0426-4748-B9AD-243F537EB46B")
ICoreWebView2 : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_Settings(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Source(LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Navigate(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE NavigateToString(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_NavigationStarting(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_NavigationStarting(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ContentLoading(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ContentLoading(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_SourceChanged(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_SourceChanged(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_HistoryChanged(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_HistoryChanged(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_NavigationCompleted(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_NavigationCompleted(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_FrameNavigationStarting(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_FrameNavigationStarting(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_FrameNavigationCompleted(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_FrameNavigationCompleted(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ScriptDialogOpening(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ScriptDialogOpening(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_PermissionRequested(ICoreWebView2PermissionRequestedEventHandler*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_PermissionRequested(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ProcessFailed(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ProcessFailed(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddScriptToExecuteOnDocumentCreated(LPCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveScriptToExecuteOnDocumentCreated(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE ExecuteScript(LPCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT, void*, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Reload() = 0;
    virtual HRESULT STDMETHODCALLTYPE PostWebMessageAsJson(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE PostWebMessageAsString(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_WebMessageReceived(ICoreWebView2WebMessageReceivedEventHandler*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_WebMessageReceived(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE CallDevToolsProtocolMethod(LPCWSTR, LPCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_BrowserProcessId(UINT32*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CanGoBack(BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CanGoForward(BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GoBack() = 0;
    virtual HRESULT STDMETHODCALLTYPE GoForward() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDevToolsProtocolEventReceiver(LPCWSTR, void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stop() = 0;
    virtual HRESULT STDMETHODCALLTYPE add_NewWindowRequested(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_NewWindowRequested(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_DocumentTitleChanged(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_DocumentTitleChanged(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_DocumentTitle(LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddHostObjectToScript(LPCWSTR, VARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveHostObjectFromScript(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE OpenDevToolsWindow() = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ContainsFullScreenElementChanged(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ContainsFullScreenElementChanged(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ContainsFullScreenElement(BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_WebResourceRequested(ICoreWebView2WebResourceRequestedEventHandler*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_WebResourceRequested(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddWebResourceRequestedFilter(const LPCWSTR, const COREWEBVIEW2_WEB_RESOURCE_CONTEXT) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveWebResourceRequestedFilter(const LPCWSTR, const COREWEBVIEW2_WEB_RESOURCE_CONTEXT) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_WindowCloseRequested(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_WindowCloseRequested(EventRegistrationToken) = 0;
};

MIDL_INTERFACE("7CCC5C7F-8351-4572-9077-9C1C80913835")
ICoreWebView2Controller : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_IsVisible(BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsVisible(BOOL) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Bounds(RECT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Bounds(RECT) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ZoomFactor(double*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_ZoomFactor(double) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ZoomFactorChanged(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ZoomFactorChanged(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBoundsAndZoomFactor(RECT, double) = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_MoveFocusRequested(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_MoveFocusRequested(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_GotFocus(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_GotFocus(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_LostFocus(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_LostFocus(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_AcceleratorKeyPressed(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_AcceleratorKeyPressed(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ParentWindow(HWND*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_ParentWindow(HWND) = 0;
    virtual HRESULT STDMETHODCALLTYPE NotifyParentWindowPositionChanged() = 0;
    virtual HRESULT STDMETHODCALLTYPE Close() = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CoreWebView2(ICoreWebView2**) = 0;
};

STDAPI CreateCoreWebView2EnvironmentWithOptions(PCWSTR, PCWSTR, void*, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);

MIDL_INTERFACE("4212F3A7-0FBC-4C9C-8118-17ED6370C1B3")
ICoreWebView2HttpHeadersCollectionIterator : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetCurrentHeader(LPWSTR*, LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_HasCurrentHeader(BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveNext(BOOL*) = 0;
};

MIDL_INTERFACE("2C1F04DF-C90E-49E4-BD25-4A659300337B")
ICoreWebView2HttpRequestHeaders : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetHeader(LPCWSTR, LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetHeaders(LPCWSTR, ICoreWebView2HttpHeadersCollectionIterator**) = 0;
    virtual HRESULT STDMETHODCALLTYPE Contains(LPCWSTR, BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetHeader(LPCWSTR, LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveHeader(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIterator(ICoreWebView2HttpHeadersCollectionIterator**) = 0;
};

MIDL_INTERFACE("B5F6D4D5-1BFF-4869-85B8-158153017B04")
ICoreWebView2HttpResponseHeaders : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE AppendHeader(LPCWSTR, LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE Contains(LPCWSTR, BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetHeader(LPCWSTR, LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetHeaders(LPCWSTR, ICoreWebView2HttpHeadersCollectionIterator**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIterator(ICoreWebView2HttpHeadersCollectionIterator**) = 0;
};

MIDL_INTERFACE("2D7B3282-83B1-41CA-8BBF-FF18F6BFE320")
ICoreWebView2WebResourceRequestedEventArgs : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_Request(ICoreWebView2WebResourceRequest**) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Response(ICoreWebView2WebResourceResponse**) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Response(ICoreWebView2WebResourceResponse*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeferral(ICoreWebView2Deferral**) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ResourceContext(COREWEBVIEW2_WEB_RESOURCE_CONTEXT*) = 0;
};

MIDL_INTERFACE("F6DC79F2-E1FA-4534-8968-4AFF10BBAA32")
ICoreWebView2WebResourceRequestedEventHandler : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs*) = 0;
};

MIDL_INTERFACE("5953D1FC-B08F-46DD-AFD3-66B172419CD0")
ICoreWebView2WebResourceResponse : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_Content(IStream**) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Content(IStream*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Headers(ICoreWebView2HttpResponseHeaders**) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_StatusCode(int*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_StatusCode(int) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ReasonPhrase(LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_ReasonPhrase(LPCWSTR) = 0;
};

}

#endif // __webview2_h__

namespace choc::ui
{

//==============================================================================
struct WebView::Pimpl
{
    Pimpl (WebView& v, const Options& options)
        : owner (v), fetchResource (options.fetchResource)
    {
        // You cam define this macro to provide a custom way of getting a
        // choc::file::DynamicLibrary that contains the redistributable
        // Microsoft WebView2Loader.dll
        webviewDLL = CHOC_FIND_WEBVIEW2LOADER_DLL;

        if (! webviewDLL)
            return;

        hwnd = windowClass.createWindow (WS_POPUP, 400, 400, this);

        if (hwnd.hwnd == nullptr)
            return;

        SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR) this);

        if (createEmbeddedWebView())
        {
            resizeContentToFit();
            // coreWebViewController->MoveFocus (COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        }
    }

    ~Pimpl()
    {
        if (coreWebView != nullptr)
        {
            coreWebView->Release();
            coreWebView = nullptr;
        }

        if (coreWebViewController != nullptr)
        {
            coreWebViewController->Release();
            coreWebViewController = nullptr;
        }

        if (coreWebViewEnvironment != nullptr)
        {
            coreWebViewEnvironment->Release();
            coreWebViewEnvironment = nullptr;
        }
    }

    void* getViewHandle() const     { return (void*) hwnd.hwnd; }

    void navigate (const std::string& url)
    {
        coreWebView->Navigate (createUTF16StringFromUTF8 (url).c_str());
    }

    void addInitScript (const std::string& script)
    {
        coreWebView->AddScriptToExecuteOnDocumentCreated (createUTF16StringFromUTF8 (script).c_str(), nullptr);
    }

    void evaluateJavascript (const std::string& script)
    {
        coreWebView->ExecuteScript (createUTF16StringFromUTF8 (script).c_str(), nullptr);
    }

    void setHTML (const std::string& html)
    {
        coreWebView->NavigateToString (createUTF16StringFromUTF8 (html).c_str());
    }

private:
    WindowClass windowClass { L"CHOCWebView", (WNDPROC) wndProc };
    HWNDHolder hwnd;
    const std::string resourceRequestFilterUriPrefix = "https://choc.localhost/";

    static Pimpl* getPimpl (HWND h)     { return (Pimpl*) GetWindowLongPtr (h, GWLP_USERDATA); }

    static LRESULT wndProc (HWND h, UINT msg, WPARAM wp, LPARAM lp)
    {
        if (msg == WM_SIZE)
            if (auto w = getPimpl (h))
                w->resizeContentToFit();

        if (msg == WM_SHOWWINDOW)
            if (auto w = getPimpl (h); w->coreWebViewController != nullptr)
                w->coreWebViewController->put_IsVisible (wp != 0);

        return DefWindowProcW (h, msg, wp, lp);
    }

    void resizeContentToFit()
    {
        if (coreWebViewController != nullptr)
        {
            RECT r;
            GetWindowRect (hwnd, &r);
            r.right -= r.left; r.bottom -= r.top;
            r.left = r.top = 0;
            coreWebViewController->put_Bounds (r);
        }
    }

    bool createEmbeddedWebView()
    {
        auto userDataFolder = getUserDataFolder();

        if (! userDataFolder.empty())
        {
            auto handler = new EventHandler (*this);
            webviewInitialising.test_and_set();

            if (auto createWebView = (decltype(&CreateCoreWebView2EnvironmentWithOptions))
                                        webviewDLL.findFunction ("CreateCoreWebView2EnvironmentWithOptions"))
            {
                if (createWebView (nullptr, userDataFolder.c_str(), nullptr, handler) == S_OK)
                {
                    MSG msg;

                    while (webviewInitialising.test_and_set() && GetMessage (std::addressof (msg), nullptr, 0, 0))
                    {
                        TranslateMessage (std::addressof (msg));
                        DispatchMessage (std::addressof (msg));
                    }

                    addInitScript ("window.external = { invoke: function(s) { window.chrome.webview.postMessage(s); } }");

                    if (fetchResource)
                    {
                        const auto wildcardFilter = createUTF16StringFromUTF8 (resourceRequestFilterUriPrefix.c_str()) + L"*";
                        coreWebView->AddWebResourceRequestedFilter (wildcardFilter.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);

                        EventRegistrationToken token;
                        coreWebView->add_WebResourceRequested (handler, std::addressof (token));

                        navigate (resourceRequestFilterUriPrefix);
                    }

                    return true;
                }
            }
        }

        return false;
    }

    bool environmentCreated (ICoreWebView2Environment* env)
    {
        if (coreWebViewEnvironment != nullptr)
            return false;

        env->AddRef();
        coreWebViewEnvironment = env;

        return true;
    }

    void webviewCreated (ICoreWebView2Controller* controller, ICoreWebView2* view)
    {
        controller->AddRef();
        view->AddRef();
        coreWebViewController = controller;
        coreWebView = view;
        webviewInitialising.clear();
    }

    HRESULT onResourceRequested (ICoreWebView2WebResourceRequestedEventArgs* args)
    {
        struct ScopedExit
        {
            using Fn = std::function<void()>;

            explicit ScopedExit (Fn&& fn) : onExit (std::move (fn)) {}

            ScopedExit (const ScopedExit&) = delete;
            ScopedExit (ScopedExit&&) = delete;
            ScopedExit& operator= (const ScopedExit&) = delete;
            ScopedExit& operator= (ScopedExit&&) = delete;

            ~ScopedExit()
            {
                if (onExit)
                    onExit();
            }

            Fn onExit;
        };

        const auto makeCleanup = [](auto*& ptr, auto cleanup)
        {
            return [&ptr, cleanup]
            {
                if (ptr)
                    cleanup (ptr);
            };
        };

        const auto makeCleanupIUnknown = [=](auto*& ptr)
        {
            return makeCleanup (ptr, [](auto* p) { p->Release(); });
        };

        try
        {
            ICoreWebView2WebResourceRequest* request = {};
            const auto cleanupRequest = ScopedExit (makeCleanupIUnknown (request));

            if (args->get_Request (std::addressof (request)) != S_OK)
                return E_FAIL;

            LPWSTR uri = {};
            const auto cleanupUri = ScopedExit (makeCleanup (uri, CoTaskMemFree));

            if (request->get_Uri (std::addressof (uri)) != S_OK)
                return E_FAIL;

            const auto path = createUTF8FromUTF16 (uri).substr (resourceRequestFilterUriPrefix.size() - 1);

            ICoreWebView2WebResourceResponse* response = {};
            const auto cleanupResponse = ScopedExit (makeCleanupIUnknown (response));

            if (const auto resource = fetchResource (path))
            {
                const auto makeMemoryStream = [](const auto* data, auto length) -> IStream*
                {
                    choc::file::DynamicLibrary lib ("shlwapi.dll");
                    using SHCreateMemStreamFn = IStream*(__stdcall *)(const BYTE*, UINT);
                    auto fn = reinterpret_cast<SHCreateMemStreamFn> (lib.findFunction ("SHCreateMemStream"));
                    return fn ? fn (data, length) : nullptr;
                };

                const auto& [bytes, mimeType] = *resource;

                auto* stream = makeMemoryStream (reinterpret_cast<const BYTE*> (bytes.data()), static_cast<UINT> (bytes.size()));
                const auto cleanupStream = ScopedExit (makeCleanupIUnknown (stream));

                if (! stream)
                    return E_FAIL;

                const auto mimeTypeHeader = std::string ("Content-Type: ") + mimeType;
                const auto cacheControlHeader = "Cache-Control: no-store";
                const std::vector<std::string> headersToConcatenate { mimeTypeHeader, cacheControlHeader };
                const auto headers = createUTF16StringFromUTF8 (choc::text::joinStrings (headersToConcatenate, "\n"));

                if (coreWebViewEnvironment->CreateWebResourceResponse (stream, 200, L"OK", headers.c_str(), std::addressof (response)) != S_OK)
                    return E_FAIL;
            }
            else
            {
                if (coreWebViewEnvironment->CreateWebResourceResponse (nullptr, 404, L"Not Found", nullptr, std::addressof (response)) != S_OK)
                    return E_FAIL;
            }

            if (args->put_Response (response) != S_OK)
                return E_FAIL;
        }
        catch (...)
        {
            return E_FAIL;
        }

        return S_OK;
    }

    //==============================================================================
    struct EventHandler  : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
                           public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
                           public ICoreWebView2WebMessageReceivedEventHandler,
                           public ICoreWebView2PermissionRequestedEventHandler,
                           public ICoreWebView2WebResourceRequestedEventHandler
    {
        EventHandler (Pimpl& p) : ownerPimpl (p) {}
        EventHandler (const EventHandler&) = delete;
        EventHandler (EventHandler&&) = delete;
        EventHandler& operator= (const EventHandler&) = delete;
        EventHandler& operator= (EventHandler&&) = delete;
        virtual ~EventHandler() {}

        HRESULT STDMETHODCALLTYPE QueryInterface (REFIID, LPVOID*) override   { return E_NOINTERFACE; }
        ULONG STDMETHODCALLTYPE AddRef() override     { return ++refCount; }
        ULONG STDMETHODCALLTYPE Release() override    { auto newCount = --refCount; if (newCount == 0) delete this; return newCount; }

        HRESULT STDMETHODCALLTYPE Invoke (HRESULT, ICoreWebView2Environment* env) override
        {
            if (env == nullptr)
                return E_FAIL;

            if (! ownerPimpl.environmentCreated (env))
                return E_FAIL;

            env->CreateCoreWebView2Controller (ownerPimpl.hwnd, this);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Invoke (HRESULT, ICoreWebView2Controller* controller) override
        {
            ICoreWebView2* view = {};
            controller->get_CoreWebView2 (std::addressof (view));
            EventRegistrationToken token;
            view->add_WebMessageReceived (this, std::addressof (token));
            view->add_PermissionRequested (this, std::addressof (token));
            ownerPimpl.webviewCreated (controller, view);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Invoke (ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) override
        {
            LPWSTR message = {};
            args->TryGetWebMessageAsString (std::addressof (message));
            ownerPimpl.owner.invokeBinding (createUTF8FromUTF16 (message));
            sender->PostWebMessageAsString (message);
            CoTaskMemFree (message);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Invoke (ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* args) override
        {
            COREWEBVIEW2_PERMISSION_KIND permissionKind;
            args->get_PermissionKind (std::addressof (permissionKind));

            if (permissionKind == COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ)
                args->put_State (COREWEBVIEW2_PERMISSION_STATE_ALLOW);

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Invoke (ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) override
        {
            return ownerPimpl.onResourceRequested (args);
        }

        Pimpl& ownerPimpl;
        std::atomic<unsigned long> refCount { 0 };
    };

    //==============================================================================
    WebView& owner;
    WebViewDLL webviewDLL;
    Options::FetchResource fetchResource;
    ICoreWebView2Environment* coreWebViewEnvironment = nullptr;
    ICoreWebView2* coreWebView = nullptr;
    ICoreWebView2Controller* coreWebViewController = nullptr;
    std::atomic_flag webviewInitialising = ATOMIC_FLAG_INIT;

    //==============================================================================
    static std::wstring getUserDataFolder()
    {
        wchar_t currentExePath[MAX_PATH] = {};
        wchar_t dataPath[MAX_PATH] = {};

        GetModuleFileNameW (nullptr, currentExePath, MAX_PATH);
        auto currentExeName = std::wstring (currentExePath);
        auto lastSlash = currentExeName.find_last_of (L'\\');

        if (lastSlash != std::wstring::npos)
            currentExeName = currentExeName.substr (lastSlash + 1);

        if (SUCCEEDED (SHGetFolderPathW (nullptr, CSIDL_APPDATA, nullptr, 0, dataPath)))
        {
            auto path = std::wstring (dataPath);

            if (! path.empty() && path.back() != L'\\')
                path += L"\\";

            return path + currentExeName;
        }

        return {};
    }
};

} // namespace choc::ui

#else
 #error "choc WebView only supports OSX, Windows or Linux!"
#endif

namespace choc::ui
{

//==============================================================================
inline WebView::WebView() : WebView (Options()) {}
inline WebView::WebView (const Options& options)  { pimpl = std::make_unique<Pimpl> (*this, options); }

inline WebView::~WebView()
{
    pimpl.reset();
}

inline void WebView::navigate (const std::string& url)               { pimpl->navigate (url.empty() ? "about:blank" : url); }
inline void WebView::setHTML (const std::string& html)               { pimpl->setHTML (html); }
inline void WebView::addInitScript (const std::string& script)       { pimpl->addInitScript (script); }
inline void WebView::evaluateJavascript (const std::string& script)  { pimpl->evaluateJavascript (script); }
inline void* WebView::getViewHandle() const                          { return pimpl->getViewHandle(); }

inline void WebView::bind (const std::string& functionName, CallbackFn&& fn)
{
    std::string script = R"((function() {
      var fnBinding = window._fnBindings = (window._fnBindings || { messageID: 1 });
      window["FUNCTION_NAME"] = function() {
        var messageID = ++fnBinding.messageID;
        var promise = new Promise(function(resolve, reject) {
          fnBinding[messageID] = { resolve: resolve, reject: reject, };
        });
        window.external.invoke(JSON.stringify({
          id: messageID,
          fn: "FUNCTION_NAME",
          params: Array.prototype.slice.call(arguments),
        }));
        return promise;
      }
    })())";

    script = choc::text::replace (script, "FUNCTION_NAME", functionName);
    addInitScript (script);
    evaluateJavascript (script);
    bindings[functionName] = std::move (fn);
}

inline void WebView::unbind (const std::string& functionName)
{
    if (auto found = bindings.find (functionName); found != bindings.end())
    {
        evaluateJavascript ("delete window[\"" + functionName + "\"];");
        bindings.erase (found);
    }
}

inline void WebView::invokeBinding (const std::string& msg)
{
    try
    {
        auto json = choc::json::parse (msg);
        auto b = bindings.find (std::string (json["fn"].getString()));
        auto callbackID = json["id"].getWithDefault<int64_t>(0);

        if (callbackID == 0 || b == bindings.end())
            return;

        auto callbackItem = "window._fnBindings[" + std::to_string (callbackID) + "]";

        try
        {
            auto result = b->second (json["params"]);

            auto call = callbackItem + ".resolve(" + choc::json::toString (result) + "); delete " + callbackItem + ";";
            evaluateJavascript (call);
            return;
        }
        catch (const std::exception&)
        {}

        auto call = callbackItem + ".reject(); delete " + callbackItem + ";";
        evaluateJavascript (call);
    }
    catch (const std::exception&)
    {}
}

} // namespace choc::ui


//==============================================================================
//==============================================================================
/*
    The data monstrosity that follows is a binary dump of the redistributable
    WebView2Loader.dll files from Microsoft, embedded here to avoid you
    needing to install the DLLs alongside your app.

    More details on how the microsoft webview2 packages work can be found at:
    https://docs.microsoft.com/en-us/microsoft-edge/webview2/concepts/distribution

    The copyright notice that goes with the redistributable files is
    printed below, and you should note that if you embed this code in your
    own app, you'll need to comply with this license!

    |
    |  Copyright (C) Microsoft Corporation. All rights reserved.
    |
    |  Redistribution and use in source and binary forms, with or without
    |  modification, are permitted provided that the following conditions are
    |  met:
    |
    |  * Redistributions of source code must retain the above copyright
    |  notice, this list of conditions and the following disclaimer.
    |  * Redistributions in binary form must reproduce the above
    |  copyright notice, this list of conditions and the following disclaimer
    |  in the documentation and/or other materials provided with the
    |  distribution.
    |  * The name of Microsoft Corporation, or the names of its contributors
    |  may not be used to endorse or promote products derived from this
    |  software without specific prior written permission.
    |
    |  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    |  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    |  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    |  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    |  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    |  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    |  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    |  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    |  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    |  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    |  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    |

*/

#if CHOC_WINDOWS

#ifdef CHOC_REGISTER_OPEN_SOURCE_LICENCE
 CHOC_REGISTER_OPEN_SOURCE_LICENCE (WebView2Loader, R"(
==============================================================================
Microsoft WebView2Loader.dll redistributable file license:

Copyright (C) Microsoft Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.
* The name of Microsoft Corporation, or the names of its contributors
may not be used to endorse or promote products derived from this
software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
)")
#endif

inline choc::ui::WebViewDLL choc::ui::getWebview2LoaderDLL()
{
   #ifdef _M_ARM64
    static constexpr unsigned char dllData[130488] = {
      77,90,120,0,1,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,120,0,0,0,14,31,186,14,0,180,9,205,33,184,1,76,205,33,84,104,105,115,32,112,114,111,103,114,97,109,32,99,97,110,110,
      111,116,32,98,101,32,114,117,110,32,105,110,32,68,79,83,32,109,111,100,101,46,36,0,0,80,69,0,0,100,170,8,0,211,27,187,98,0,0,0,0,0,0,0,0,240,0,34,32,11,2,14,0,0,28,1,0,0,182,0,0,0,0,0,0,112,76,0,0,0,16,0,0,0,0,0,128,1,0,0,0,0,16,0,0,0,2,0,0,
      10,0,0,0,0,0,0,0,10,0,0,0,0,0,0,0,0,48,2,0,0,4,0,0,131,7,2,0,3,0,96,65,0,0,16,0,0,0,0,0,0,16,0,0,0,0,0,0,0,0,16,0,0,0,0,0,0,16,0,0,0,0,0,0,0,0,0,0,16,0,0,0,49,160,1,0,242,0,0,0,35,161,1,0,40,0,0,0,0,16,2,0,136,5,0,0,0,224,1,0,80,11,0,0,0,214,
      1,0,184,39,0,0,0,32,2,0,152,6,0,0,28,157,1,0,28,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,184,153,1,0,40,0,0,0,32,49,1,0,56,1,0,0,0,0,0,0,0,0,0,0,184,163,1,0,104,2,0,0,168,158,1,0,96,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,116,101,120,116,0,0,0,
      168,27,1,0,0,16,0,0,0,28,1,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,0,96,46,114,100,97,116,97,0,0,212,139,0,0,0,48,1,0,0,140,0,0,0,32,1,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,0,64,46,100,97,116,97,0,0,0,188,27,0,0,0,192,1,0,0,12,0,0,0,172,1,0,0,0,0,0,
      0,0,0,0,0,0,0,0,64,0,0,192,46,112,100,97,116,97,0,0,80,11,0,0,0,224,1,0,0,12,0,0,0,184,1,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,0,64,46,48,48,99,102,103,0,0,8,0,0,0,0,240,1,0,0,2,0,0,0,196,1,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,0,64,46,116,108,115,0,0,0,0,
      9,0,0,0,0,0,2,0,0,2,0,0,0,198,1,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,0,192,46,114,115,114,99,0,0,0,136,5,0,0,0,16,2,0,0,6,0,0,0,200,1,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,0,64,46,114,101,108,111,99,0,0,152,6,0,0,0,32,2,0,0,8,0,0,0,206,1,0,0,0,0,0,0,0,0,
      0,0,0,0,0,64,0,0,66,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,4,64,120,66,5,0,52,229,3,0,170,64,4,0,55,161,12,64,242,96,1,0,84,
      162,44,64,146,95,192,63,241,204,2,0,84,160,116,64,76,1,168,113,110,34,0,38,30,34,1,0,52,165,64,0,145,165,236,124,146,31,32,3,213,160,116,223,76,1,168,113,110,34,0,38,30,162,255,255,53,165,64,0,209,1,3,0,156,0,152,96,78,0,28,33,78,0,168,112,110,
      2,0,38,30,66,8,0,82,160,0,0,203,64,4,128,139,192,3,95,214,33,64,0,209,225,7,129,203,162,36,64,120,194,0,0,52,33,4,0,241,172,255,255,84,236,255,255,23,162,36,64,120,226,255,255,53,165,8,0,209,160,0,0,203,0,252,65,147,192,3,95,214,0,0,128,210,
      192,3,95,214,7,0,6,0,5,0,4,0,3,0,2,0,1,0,0,0,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,
      204,204,204,204,204,204,204,204,204,204,204,229,3,0,170,96,4,0,55,63,32,0,241,35,4,0,84,163,12,64,242,192,1,0,84,162,44,64,146,95,192,63,241,140,4,0,84,99,64,0,209,227,3,3,203,160,116,64,76,1,168,113,110,34,0,38,30,130,5,0,52,33,4,131,203,165,
      0,3,139,63,32,0,241,67,2,0,84,35,252,67,147,160,116,223,76,1,168,113,110,34,0,38,30,66,4,0,52,99,4,0,241,97,255,255,84,33,8,64,242,192,1,0,84,33,32,0,209,225,3,1,203,165,4,1,203,160,116,223,76,1,168,113,110,34,0,38,30,226,2,0,52,6,0,0,20,161,
      0,0,180,162,36,64,120,226,1,0,52,33,4,0,241,168,255,255,84,160,0,0,203,0,252,65,147,192,3,95,214,99,64,0,209,227,7,131,203,162,36,64,120,194,0,0,52,33,4,0,241,160,0,0,84,99,4,0,241,108,255,255,84,221,255,255,23,165,8,0,209,160,0,0,203,0,252,
      65,147,192,3,95,214,165,64,0,209,33,1,0,156,0,152,96,78,0,28,33,78,0,168,112,110,2,0,38,30,66,8,0,82,160,0,0,203,64,4,128,139,192,3,95,214,7,0,6,0,5,0,4,0,3,0,2,0,1,0,0,0,204,204,204,204,255,131,0,209,3,164,128,210,99,50,163,242,227,3,0,169,
      225,11,1,169,255,131,0,145,192,3,95,214,192,3,95,214,243,83,186,169,245,91,1,169,247,99,2,169,249,107,3,169,251,115,4,169,253,123,5,169,253,67,1,145,115,80,64,169,117,88,65,169,119,96,66,169,121,104,67,169,123,112,68,169,97,40,64,249,64,0,63,
      214,253,123,69,169,251,115,68,169,249,107,67,169,247,99,66,169,245,91,65,169,243,83,198,168,192,3,95,214,243,83,186,169,245,91,1,169,247,99,2,169,249,107,3,169,251,115,4,169,253,123,5,169,253,67,1,145,115,80,64,169,117,88,65,169,119,96,66,169,
      121,104,67,169,123,112,68,169,97,40,64,249,64,0,63,214,253,123,69,169,251,115,68,169,249,107,67,169,247,99,66,169,245,91,65,169,243,83,198,168,192,3,95,214,204,204,204,204,204,204,204,204,255,67,0,209,209,0,0,240,49,6,64,249,241,99,49,203,241,
      7,0,249,192,3,95,214,204,204,204,204,204,204,204,204,209,0,0,240,240,7,64,249,49,6,64,249,240,99,48,203,31,2,17,235,129,0,0,84,255,67,0,145,192,3,95,214,31,32,3,213,224,3,16,170,2,0,0,20,204,204,204,204,253,123,1,169,253,67,0,145,230,0,0,148,
      253,123,65,169,192,3,95,214,204,204,204,204,255,195,1,209,253,123,1,169,243,83,2,169,245,91,3,169,247,99,4,169,249,107,5,169,251,115,6,169,33,0,64,249,225,3,0,249,243,3,0,170,244,3,1,170,245,3,2,170,246,3,3,170,226,3,3,170,164,255,255,151,224,
      3,19,170,225,3,20,170,226,3,21,170,227,3,22,170,83,80,64,169,85,88,65,169,87,96,66,169,89,104,67,169,91,112,68,169,93,40,64,249,0,0,63,214,243,3,0,170,158,255,255,151,224,3,19,170,225,15,64,249,66,0,128,210,147,255,255,151,224,3,19,170,251,115,
      70,169,249,107,69,169,247,99,68,169,245,91,67,169,243,83,66,169,253,123,65,169,255,195,1,145,192,3,95,214,204,204,204,204,3,0,1,203,127,0,2,235,195,10,0,84,195,1,0,180,33,0,128,249,95,32,0,241,227,3,0,170,34,2,0,84,98,0,16,54,32,32,255,13,96,
      32,191,13,98,0,8,54,32,0,255,13,96,0,191,13,98,0,0,54,34,0,64,57,98,0,0,57,192,3,95,214,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,66,128,0,241,3,5,0,84,66,128,0,241,99,4,0,84,33,32,128,249,66,0,1,241,195,3,0,84,63,12,64,242,97,0,0,84,127,
      12,64,242,96,1,0,84,32,32,223,76,36,32,223,76,33,64,128,249,33,96,128,249,96,32,159,76,100,32,159,76,66,0,2,241,34,255,255,84,98,2,48,54,16,0,0,20,32,4,64,173,34,12,65,173,36,20,66,173,38,28,67,173,33,0,2,145,33,64,128,249,33,96,128,249,96,4,
      0,173,98,12,1,173,100,20,2,173,102,28,3,173,99,0,2,145,66,0,2,241,98,254,255,84,98,0,48,54,32,32,223,76,96,32,159,76,98,0,40,54,32,160,223,76,96,160,159,76,98,0,32,54,32,112,223,76,96,112,159,76,98,0,24,54,32,112,223,12,96,112,159,12,98,0,16,
      54,32,32,255,13,96,32,191,13,98,0,8,54,32,0,255,13,96,0,191,13,98,0,0,54,34,0,64,57,98,0,0,57,192,3,95,214,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,33,0,2,139,33,240,159,248,95,32,0,241,3,0,2,139,130,2,0,84,162,0,16,54,33,16,0,
      209,99,16,0,209,32,32,96,13,96,32,32,13,162,0,8,54,33,8,0,209,99,8,0,209,32,0,96,13,96,0,32,13,98,0,0,54,34,252,95,56,98,252,31,56,192,3,95,214,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,66,128,0,241,227,5,0,84,66,128,0,241,3,5,0,84,33,240,155,
      248,66,0,1,241,3,4,0,84,63,12,64,242,97,0,0,84,127,12,64,242,192,1,0,84,33,0,1,209,32,32,64,76,33,0,1,209,36,32,64,76,33,240,151,248,33,240,147,248,99,0,1,209,96,32,0,76,99,0,1,209,100,32,0,76,66,0,2,241,162,254,255,84,15,0,0,20,33,0,2,209,38,
      28,67,173,36,20,66,173,99,0,2,209,34,12,65,173,32,4,64,173,33,240,151,248,33,240,147,248,102,28,3,173,100,20,2,173,98,12,1,173,96,4,0,173,66,0,2,241,98,254,255,84,162,0,48,54,33,0,1,209,99,0,1,209,32,32,64,76,96,32,0,76,162,0,40,54,33,128,0,
      209,99,128,0,209,32,160,64,76,96,160,0,76,162,0,32,54,33,64,0,209,99,64,0,209,32,112,64,76,96,112,0,76,162,0,24,54,33,32,0,209,99,32,0,209,32,112,64,12,96,112,0,12,162,0,16,54,33,16,0,209,99,16,0,209,32,32,96,13,96,32,32,13,162,0,8,54,33,8,0,
      209,99,8,0,209,32,0,96,13,96,0,32,13,98,0,0,54,34,252,95,56,98,252,31,56,192,3,95,214,208,0,0,240,16,34,0,145,16,2,64,249,31,0,16,235,65,0,0,84,192,3,95,214,253,123,190,169,253,3,0,145,208,0,0,240,16,34,0,145,209,0,0,240,49,66,0,145,16,2,64,
      249,49,2,64,249,240,71,1,169,1,64,0,148,0,0,62,212,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,234,3,0,170,32,12,1,78,1,28,160,78,2,28,160,78,3,28,160,78,95,128,0,241,172,6,
      0,84,1,0,102,158,136,0,0,16,9,105,98,56,8,9,9,139,0,1,31,214,42,22,18,28,25,21,17,13,36,33,30,27,24,20,16,12,41,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64,113,159,12,64,33,191,13,64,33,0,13,192,3,95,214,
      64,113,159,12,64,33,191,13,64,1,32,13,192,3,95,214,64,113,159,12,64,33,191,13,65,1,0,57,192,3,95,214,64,113,159,12,64,33,32,13,192,3,95,214,64,113,159,12,64,33,0,13,192,3,95,214,64,113,159,12,64,1,32,13,192,3,95,214,64,113,159,12,65,1,0,57,192,
      3,95,214,64,113,0,12,192,3,95,214,72,64,0,209,8,1,10,139,0,113,0,76,64,113,0,76,192,3,95,214,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,192,128,82,95,0,8,235,234,1,0,84,73,252,69,147,41,5,0,241,64,161,159,76,204,255,255,84,66,16,64,146,130,248,255,181,
      192,3,95,214,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,72,13,64,242,160,0,0,84,8,65,0,209,232,3,8,203,66,0,8,203,64,113,136,76,73,252,70,147,31,32,3,213,31,32,3,213,31,32,3,213,31,32,3,213,31,32,3,213,41,5,0,241,64,33,159,76,204,
      255,255,84,66,20,64,146,66,251,255,180,95,128,0,241,43,245,255,84,64,161,159,76,66,128,0,241,193,244,255,84,192,3,95,214,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,95,64,0,241,108,24,0,84,67,30,0,16,104,104,
      98,56,99,8,8,203,96,0,31,214,0,0,0,0,0,0,0,0,3,40,193,168,40,36,193,168,31,1,3,235,1,28,0,84,74,13,192,218,41,13,192,218,64,1,9,235,224,7,159,26,0,36,128,90,192,3,95,214,3,132,64,248,40,132,64,248,31,1,3,235,224,0,0,84,99,12,192,218,8,13,192,
      218,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,214,3,68,64,184,40,68,64,184,31,1,3,235,224,0,0,84,99,8,192,218,8,9,192,218,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,214,3,20,64,56,40,20,64,56,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,
      214,3,132,64,248,40,132,64,248,31,1,3,235,224,0,0,84,99,12,192,218,8,13,192,218,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,214,3,68,64,184,40,68,64,184,31,1,3,235,224,0,0,84,99,8,192,218,8,9,192,218,96,0,8,235,224,7,159,26,0,36,128,90,192,
      3,95,214,3,36,64,120,40,36,64,120,127,0,8,235,224,0,0,84,99,4,192,218,8,5,192,218,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,214,3,20,64,56,40,20,64,56,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,214,3,132,64,248,40,132,64,248,31,1,3,235,
      224,0,0,84,99,12,192,218,8,13,192,218,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,214,3,68,64,184,40,68,64,184,31,1,3,235,224,0,0,84,99,8,192,218,8,9,192,218,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,214,3,36,64,120,40,36,64,120,99,4,192,
      218,8,5,192,218,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,214,0,0,128,210,192,3,95,214,3,132,64,248,40,132,64,248,31,1,3,235,224,0,0,84,99,12,192,218,8,13,192,218,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,214,3,68,64,184,40,68,64,184,99,
      8,192,218,8,9,192,218,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,214,3,132,64,248,40,132,64,248,99,12,192,218,8,13,192,218,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,214,3,132,64,248,40,132,64,248,31,1,3,235,224,0,0,84,99,12,192,218,8,13,
      192,218,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,214,3,20,64,56,40,20,64,56,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,214,3,132,64,248,40,132,64,248,31,1,3,235,224,0,0,84,99,12,192,218,8,13,192,218,96,0,8,235,224,7,159,26,0,36,128,90,
      192,3,95,214,3,36,64,120,40,36,64,120,99,4,192,218,8,5,192,218,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,214,3,132,64,248,40,132,64,248,31,1,3,235,224,0,0,84,99,12,192,218,8,13,192,218,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,214,3,36,
      64,120,40,36,64,120,127,0,8,235,224,0,0,84,99,4,192,218,8,5,192,218,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,214,3,20,64,56,40,20,64,56,96,0,8,235,224,7,159,26,0,36,128,90,192,3,95,214,95,128,0,241,43,2,0,84,66,128,0,209,3,40,193,168,40,
      36,193,168,31,1,3,235,33,4,0,84,95,1,9,235,33,3,0,84,3,40,193,168,40,36,193,168,31,1,3,235,97,3,0,84,95,1,9,235,97,2,0,84,66,128,0,241,106,254,255,84,66,128,0,145,95,64,0,241,109,229,255,84,66,32,0,209,3,132,64,248,40,132,64,248,31,1,3,235,225,
      1,0,84,66,32,0,241,108,255,255,84,32,244,255,84,66,32,0,145,99,2,0,16,104,104,98,56,99,8,8,203,96,0,31,214,74,13,192,218,41,13,192,218,64,1,9,235,224,7,159,26,0,36,128,90,192,3,95,214,99,12,192,218,8,13,192,218,96,0,8,235,224,7,159,26,0,36,128,
      90,192,3,95,214,0,0,0,0,0,0,0,0,0,0,0,0,136,206,144,180,124,216,154,190,116,108,92,74,134,226,164,200,236,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,81,10,64,249,240,115,47,235,240,51,144,154,31,2,17,235,67,0,0,84,192,3,95,214,
      16,206,116,146,49,6,64,209,63,2,64,249,63,2,16,235,161,255,255,84,192,3,95,214,32,0,128,82,192,3,95,214,204,204,204,204,204,204,204,204,253,123,191,169,253,3,0,145,134,2,0,180,63,4,0,113,128,0,0,84,1,1,0,53,223,0,0,185,6,0,0,20,72,28,0,114,9,
      32,128,82,40,5,136,26,195,16,1,169,200,0,0,185,199,20,64,249,7,1,0,180,232,0,0,208,239,3,7,170,198,24,64,249,8,1,64,249,0,1,63,214,253,123,193,168,224,0,31,214,253,123,193,168,192,3,95,214,192,3,95,214,243,83,189,169,245,11,0,249,253,251,1,169,
      253,99,0,145,168,0,0,240,8,1,63,145,244,3,1,170,19,217,96,248,224,3,19,170,154,252,255,151,245,3,0,170,1,168,0,145,224,3,20,170,14,8,0,148,161,0,0,240,33,192,63,145,224,3,20,170,66,5,128,82,68,8,0,148,224,3,20,170,225,3,19,170,226,3,21,170,253,
      251,65,169,245,11,64,249,243,83,195,168,129,8,0,20,243,15,30,248,253,251,0,169,253,35,0,145,193,0,0,144,33,224,2,145,243,3,0,170,106,8,0,148,161,0,0,240,33,64,55,145,224,3,19,170,102,8,0,148,224,3,19,170,173,8,0,148,200,0,0,176,8,77,66,249,0,
      1,63,214,31,4,0,49,224,7,159,26,253,251,64,169,243,7,66,248,192,3,95,214,243,83,188,169,245,91,1,169,247,19,0,249,253,251,2,169,253,163,0,145,243,3,1,170,244,3,0,170,224,3,1,170,129,32,128,82,227,7,0,148,224,3,19,170,145,8,0,148,245,3,0,170,
      224,3,19,170,150,8,0,148,215,0,0,176,225,3,0,170,224,3,20,170,226,3,21,42,247,90,66,249,224,2,63,214,245,3,0,42,224,3,19,170,133,8,0,148,214,0,0,176,31,64,53,235,214,86,66,249,33,2,0,84,192,2,63,214,31,232,1,113,193,1,0,84,224,3,19,170,1,0,130,
      82,203,7,0,148,224,3,19,170,121,8,0,148,245,3,0,170,224,3,19,170,126,8,0,148,225,3,0,170,224,3,20,170,226,3,21,42,224,2,63,214,245,3,0,42,85,1,0,52,224,3,19,170,110,8,0,148,31,64,53,235,201,0,0,84,225,3,21,42,224,3,19,170,30,8,0,148,224,3,31,
      42,6,0,0,20,192,2,63,214,232,0,176,82,31,4,0,113,8,60,0,51,0,176,136,26,253,251,66,169,247,19,64,249,245,91,65,169,243,83,196,168,192,3,95,214,243,83,189,169,245,11,0,249,253,251,1,169,253,99,0,145,255,131,0,209,200,0,0,240,243,3,1,170,244,3,
      0,170,224,3,1,170,129,32,128,82,8,5,64,249,232,15,0,249,160,7,0,148,224,3,19,170,225,3,20,170,200,7,0,148,224,3,19,170,83,8,0,148,245,3,0,170,224,3,19,170,74,8,0,148,31,12,0,241,67,2,0,84,168,6,64,121,31,233,0,113,65,1,0,84,168,10,64,121,31,
      113,1,113,129,1,0,84,168,2,64,121,8,121,26,18,8,5,1,81,31,105,0,113,226,0,0,84,46,0,0,20,169,2,64,121,63,113,1,113,97,0,0,84,31,113,1,113,32,5,0,84,232,243,1,178,224,3,0,145,232,163,0,169,232,3,0,249,99,7,0,148,225,3,0,145,224,3,31,170,145,255,
      255,151,160,4,248,55,224,3,0,145,44,8,0,148,245,3,0,170,224,3,0,145,47,8,0,148,225,3,0,170,224,3,19,170,226,3,21,170,174,7,0,148,224,3,0,145,41,8,0,148,129,11,128,82,99,18,0,148,192,5,0,180,245,3,0,170,224,3,0,145,35,8,0,148,168,2,0,203,224,
      3,0,145,8,9,0,145,21,253,65,147,30,8,0,148,225,3,0,170,224,3,19,170,226,3,21,170,157,7,0,148,224,3,19,170,225,3,20,170,206,7,0,148,224,3,0,145,67,7,0,148,224,3,19,170,90,255,255,151,64,1,0,54,245,3,31,42,28,0,0,20,200,0,0,144,245,3,0,42,160,
      0,0,240,0,0,3,145,8,193,66,249,0,1,63,214,19,0,0,20,212,0,0,144,160,0,0,240,0,192,3,145,148,194,66,249,128,2,63,214,224,3,19,170,2,8,0,148,200,0,0,144,8,197,66,249,0,1,63,214,160,0,0,240,0,160,4,145,128,2,63,214,85,0,128,82,245,0,176,114,5,0,
      0,20,181,0,136,82,21,0,176,114,224,3,0,145,35,7,0,148,224,15,64,249,123,253,255,151,224,3,21,42,255,131,0,145,253,251,65,169,245,11,64,249,243,83,195,168,192,3,95,214,243,83,187,169,245,91,1,169,247,99,2,169,249,107,3,169,253,123,4,169,253,3,
      1,145,255,3,1,209,200,0,0,208,245,3,0,170,160,0,0,240,0,192,4,145,243,3,1,170,249,0,176,82,8,5,64,249,232,31,0,249,93,0,0,148,248,3,0,170,160,0,0,240,0,32,5,145,89,0,0,148,246,3,0,170,160,0,0,240,0,128,5,145,85,0,0,148,72,6,128,82,55,3,8,42,
      24,9,0,180,246,8,0,180,244,3,0,170,160,8,0,180,250,0,0,176,239,3,24,170,255,55,0,185,72,3,64,249,0,1,63,214,225,211,0,145,224,3,21,170,0,3,63,214,160,6,0,52,247,3,0,42,232,243,1,178,224,99,0,145,232,35,2,169,232,15,0,249,235,6,0,148,225,3,23,
      42,224,99,0,145,178,7,0,148,32,4,0,54,224,99,0,145,186,7,0,148,72,3,64,249,239,3,22,170,225,55,64,185,227,3,0,170,0,1,63,214,224,3,21,170,226,3,23,42,192,2,63,214,192,2,0,52,224,99,0,145,255,11,0,249,255,15,0,185,173,7,0,148,72,3,64,249,239,
      3,20,170,0,1,63,214,161,0,0,240,33,192,5,145,226,67,0,145,227,51,0,145,128,2,63,214,32,1,0,52,225,11,64,249,225,0,0,180,224,3,19,170,19,7,0,148,224,99,0,145,204,6,0,148,247,3,31,42,18,0,0,20,200,0,0,144,8,85,66,249,0,1,63,214,8,60,0,18,31,4,
      0,113,8,1,25,42,23,176,136,26,224,99,0,145,193,6,0,148,8,0,0,20,200,0,0,144,8,85,66,249,0,1,63,214,8,60,0,18,31,4,0,113,8,1,25,42,23,176,136,26,224,31,64,249,17,253,255,151,224,3,23,42,255,3,1,145,253,123,68,169,249,107,67,169,247,99,66,169,
      245,91,65,169,243,83,197,168,192,3,95,214,243,15,30,248,253,251,0,169,253,35,0,145,200,0,0,208,73,46,64,249,225,3,0,170,211,0,0,240,8,49,75,185,40,121,104,248,201,0,0,240,41,121,75,185,8,1,64,145,8,5,64,185,63,1,8,107,108,1,0,84,96,186,69,249,
      192,0,0,180,194,0,0,144,66,104,66,249,253,251,64,169,243,7,66,248,64,0,31,214,253,251,64,169,243,7,66,248,192,3,95,214,211,0,0,240,115,226,45,145,224,3,19,170,161,11,0,249,103,8,0,148,104,2,64,185,211,0,0,240,161,11,64,249,31,5,0,49,161,253,
      255,84,2,3,0,148,96,186,5,249,192,0,0,240,0,224,45,145,147,8,0,148,161,11,64,249,230,255,255,23,204,204,204,204,204,204,204,204,243,83,188,169,245,91,1,169,247,99,2,169,253,123,3,169,253,195,0,145,255,3,3,209,200,0,0,208,8,5,64,249,168,131,28,
      248,35,9,0,180,64,229,5,79,232,243,1,178,224,7,7,169,224,131,0,145,244,131,0,145,245,3,3,170,226,35,8,169,147,194,0,145,255,139,0,185,232,51,0,249,224,3,2,173,224,3,1,173,105,6,0,148,148,98,0,145,224,3,20,170,102,6,0,148,224,3,19,170,100,6,0,
      148,224,195,1,145,225,131,0,145,211,4,0,148,129,0,0,176,33,160,0,145,0,8,128,82,38,8,0,148,246,3,0,170,32,6,0,180,224,135,67,173,56,0,128,82,200,0,0,240,247,0,0,176,160,7,61,173,216,14,0,185,0,181,69,249,168,0,0,208,8,65,54,145,200,2,0,249,224,
      0,0,180,8,0,64,249,233,2,64,249,8,5,64,249,239,3,8,170,32,1,63,214,0,1,63,214,160,7,125,173,168,0,0,208,8,129,53,145,213,26,0,249,200,2,0,249,168,2,64,249,192,134,0,173,233,2,64,249,8,5,64,249,239,3,8,170,32,1,63,214,224,3,21,170,0,1,63,214,
      224,135,67,173,224,3,0,145,225,3,22,170,216,58,0,185,224,7,0,173,34,0,0,148,200,2,64,249,245,3,0,42,233,2,64,249,8,9,64,249,239,3,8,170,32,1,63,214,224,3,22,170,0,1,63,214,10,0,0,20,117,0,136,82,21,0,176,114,13,0,0,20,224,135,67,173,224,3,0,
      145,225,3,22,170,224,7,0,173,17,0,0,148,245,3,0,42,224,3,19,170,38,6,0,148,224,3,20,170,36,6,0,148,224,131,0,145,34,6,0,148,160,131,92,248,122,252,255,151,224,3,21,42,255,3,3,145,253,123,67,169,247,99,66,169,245,91,65,169,243,83,196,168,192,
      3,95,214,243,83,188,169,245,91,1,169,247,99,2,169,253,123,3,169,253,195,0,145,255,3,3,209,200,0,0,208,245,3,0,170,233,243,1,178,224,67,0,145,244,3,1,170,8,5,64,249,168,131,28,248,233,167,1,169,233,11,0,249,6,6,0,148,160,2,64,249,32,1,0,180,8,
      0,64,121,232,0,0,52,225,67,0,145,113,254,255,151,246,3,0,42,248,3,31,42,55,0,128,82,9,0,0,20,160,26,64,185,225,67,0,145,226,3,31,170,227,3,31,170,25,1,0,148,246,3,0,42,247,3,31,42,56,0,128,82,243,3,22,42,118,0,0,52,243,15,248,54,12,0,0,20,181,
      206,64,169,224,67,0,145,194,6,0,148,33,0,128,82,226,3,23,42,227,3,21,170,228,3,19,170,229,3,20,170,48,7,0,148,243,3,0,42,115,14,248,54,200,0,0,208,73,46,64,249,223,2,0,113,214,0,0,240,245,23,159,26,8,49,75,185,40,121,104,248,201,0,0,240,41,137,
      75,185,8,1,64,145,8,5,64,185,63,1,8,107,44,14,0,84,192,194,69,249,128,12,0,180,200,0,0,144,161,0,0,240,33,64,9,145,8,105,66,249,0,1,63,214,192,11,0,180,214,0,0,208,214,194,38,145,200,2,64,249,201,14,64,249,10,33,127,169,234,163,4,169,169,14,
      0,181,212,0,0,208,148,162,38,145,200,0,0,208,131,130,0,145,225,255,255,240,33,64,52,145,159,254,2,169,224,35,1,145,8,97,69,249,226,3,20,170,0,1,63,214,96,9,0,53,130,6,64,249,200,0,0,208,128,18,64,249,65,0,128,82,67,0,64,121,8,101,69,249,0,1,
      63,214,136,2,64,185,31,13,0,113,99,7,0,84,200,0,0,208,8,245,102,57,8,7,48,54,200,0,0,208,8,225,68,249,31,249,81,242,129,6,0,84,8,32,160,82,12,96,161,210,234,35,1,145,235,227,0,145,76,0,192,242,237,3,1,145,232,35,0,249,8,1,128,82,235,71,0,249,
      43,0,128,82,233,243,1,178,243,43,0,185,72,177,5,169,8,0,200,210,236,195,0,145,75,181,4,169,233,167,5,169,161,35,1,209,72,53,0,249,232,163,0,145,138,0,128,82,236,175,7,169,203,2,64,249,172,0,0,240,140,97,52,145,248,227,0,57,232,171,6,169,170,
      0,0,240,74,209,49,145,235,167,4,169,104,1,64,121,73,0,128,82,235,11,128,210,245,195,0,57,43,0,192,242,229,35,1,145,232,39,10,41,232,243,1,50,169,0,0,240,41,97,49,145,137,1,9,75,234,175,5,169,232,15,0,185,200,0,0,208,226,3,31,170,227,3,31,170,
      233,15,0,185,196,0,128,82,192,14,64,249,8,109,69,249,0,1,63,214,200,0,0,208,128,18,64,249,159,2,0,185,159,18,0,249,8,105,69,249,0,1,63,214,224,67,0,145,116,5,0,148,160,131,92,248,204,251,255,151,224,3,19,42,255,3,3,145,253,123,67,169,247,99,
      66,169,245,91,65,169,243,83,196,168,192,3,95,214,212,0,0,240,148,34,46,145,224,3,20,170,61,7,0,148,136,2,64,185,31,5,0,49,65,241,255,84,200,0,0,144,160,0,0,240,0,192,8,145,225,3,31,170,2,0,129,82,8,181,66,249,0,1,63,214,192,194,5,249,192,0,0,
      240,0,32,46,145,101,7,0,148,126,255,255,23,160,0,128,82,96,0,62,212,32,0,32,212,204,204,204,204,204,204,204,204,243,83,189,169,245,91,1,169,253,123,2,169,253,131,0,145,255,3,3,209,200,0,0,208,8,5,64,249,168,131,29,248,33,6,0,180,64,229,5,79,
      246,243,1,178,160,255,59,169,224,67,1,145,245,67,1,145,243,3,1,170,191,219,60,169,180,194,0,145,191,3,29,184,246,75,0,249,224,131,3,173,224,131,2,173,58,5,0,148,181,98,0,145,224,3,21,170,55,5,0,148,224,3,20,170,53,5,0,148,160,35,1,209,225,67,
      1,145,164,3,0,148,224,227,0,145,246,91,4,169,246,31,0,249,46,5,0,148,224,131,0,145,246,219,2,169,246,19,0,249,42,5,0,148,224,35,0,145,246,91,1,169,246,7,0,249,38,5,0,148,160,131,91,248,0,1,0,180,8,0,64,121,200,0,0,52,225,227,0,145,145,253,255,
      151,128,1,0,52,246,3,0,42,16,0,0,20,160,3,93,184,225,227,0,145,226,131,0,145,227,35,0,145,58,0,0,148,8,0,0,20,118,0,136,82,22,0,176,114,46,0,0,20,224,227,0,145,231,5,0,148,225,131,0,145,249,253,255,151,246,3,0,42,96,0,0,52,224,3,31,170,25,0,
      0,20,224,35,0,145,219,5,0,148,192,1,0,55,161,0,0,240,33,0,7,145,224,131,0,145,145,5,0,148,224,35,0,145,210,5,0,148,246,3,0,170,224,35,0,145,213,5,0,148,225,3,0,170,224,131,0,145,226,3,22,170,152,5,0,148,224,131,0,145,201,5,0,148,246,3,0,170,
      224,131,0,145,204,5,0,148,225,3,22,170,204,5,0,148,246,3,31,42,96,2,0,249,224,35,0,145,244,4,0,148,224,131,0,145,242,4,0,148,224,227,0,145,240,4,0,148,224,3,20,170,238,4,0,148,224,3,21,170,236,4,0,148,224,67,1,145,234,4,0,148,160,131,93,248,
      66,251,255,151,224,3,22,42,255,3,3,145,253,123,66,169,245,91,65,169,243,83,195,168,192,3,95,214,243,83,186,169,245,91,1,169,247,99,2,169,249,107,3,169,251,115,4,169,253,123,5,169,253,67,1,145,255,131,1,209,200,0,0,208,244,3,2,170,245,3,1,170,
      246,3,0,42,247,3,31,42,147,0,128,82,8,5,64,249,248,243,1,178,218,0,0,240,220,0,0,208,217,0,0,240,227,7,0,249,232,47,0,249,224,67,0,145,104,2,23,75,223,6,0,113,248,227,1,169,27,1,151,26,248,11,0,249,195,4,0,148,225,67,0,145,224,3,27,42,194,252,
      255,151,224,67,0,145,147,5,0,148,225,3,31,42,226,3,20,170,227,3,21,170,72,1,0,148,128,20,0,55,224,67,0,145,140,5,0,148,33,0,128,82,226,3,20,170,227,3,21,170,65,1,0,148,160,19,0,55,136,51,75,185,73,46,64,249,40,121,104,248,73,155,75,185,8,1,64,
      145,8,5,64,185,63,1,8,107,204,15,0,84,40,203,69,249,104,5,0,180,233,0,0,176,234,243,1,50,239,3,8,170,41,1,64,249,255,43,5,41,32,1,63,214,225,163,0,145,227,179,0,145,32,0,128,82,226,3,31,170,0,1,63,214,31,232,1,113,193,3,0,84,232,43,64,185,136,
      3,0,52,224,3,1,145,248,227,4,169,248,35,0,249,149,4,0,148,225,43,64,185,224,3,1,145,92,5,0,148,224,1,0,54,200,0,0,240,224,3,1,145,28,201,69,249,98,5,0,148,232,0,0,176,239,3,28,170,226,3,0,170,8,1,64,249,0,1,63,214,225,163,0,145,227,179,0,145,
      32,0,128,82,128,3,63,214,128,1,0,52,224,3,1,145,132,4,0,148,147,0,128,82,220,0,0,208,217,0,0,240,224,67,0,145,127,4,0,148,247,6,0,17,255,22,0,113,1,246,255,84,109,0,0,20,224,3,1,145,75,5,0,148,232,47,64,185,72,254,255,52,168,0,0,240,8,225,18,
      145,249,3,31,42,19,184,0,145,28,89,123,248,96,162,94,248,225,3,28,170,81,21,0,148,224,0,0,52,232,47,64,185,57,7,0,17,115,66,1,145,63,3,8,107,3,255,255,84,227,255,255,23,124,2,64,121,224,3,21,170,104,226,95,120,105,194,95,120,106,162,95,120,97,
      162,93,248,252,35,6,41,233,43,7,41,164,4,0,148,224,3,1,145,93,4,0,148,224,195,0,145,225,3,21,170,63,1,0,148,52,2,0,180,0,2,0,54,224,3,20,170,225,1,128,82,111,4,0,148,225,3,1,145,224,3,28,42,98,1,128,82,67,1,128,82,248,227,4,248,248,99,4,169,
      218,20,0,148,217,0,0,240,32,1,0,52,147,0,128,82,220,0,0,208,201,255,255,23,147,0,128,82,220,0,0,208,217,0,0,240,160,248,7,54,44,0,0,20,225,3,1,145,224,3,20,170,134,4,0,148,115,0,128,82,225,3,1,145,224,3,28,42,98,1,128,82,67,1,128,82,199,20,0,
      148,224,253,255,53,224,3,20,170,161,0,0,240,33,32,21,145,191,4,0,148,225,3,1,145,224,3,20,170,188,4,0,148,115,6,0,113,65,254,255,84,24,0,0,20,217,0,0,240,57,99,46,145,224,3,25,170,2,6,0,148,40,3,64,185,217,0,0,240,31,5,0,49,129,239,255,84,200,
      0,0,144,160,0,0,240,0,160,21,145,8,97,66,249,0,1,63,214,200,0,0,144,161,0,0,240,33,64,21,145,8,105,66,249,0,1,63,214,32,203,5,249,192,0,0,240,0,96,46,145,38,6,0,148,109,255,255,23,224,7,64,249,160,0,0,180,168,0,0,240,8,65,2,145,1,89,123,248,
      88,4,0,148,224,67,0,145,17,4,0,148,243,3,31,42,8,0,0,20,200,0,0,144,160,0,0,240,0,128,9,145,8,193,66,249,0,1,63,214,83,0,128,82,243,0,176,114,224,47,64,249,96,250,255,151,224,3,19,42,255,131,1,145,253,123,69,169,251,115,68,169,249,107,67,169,
      247,99,66,169,245,91,65,169,243,83,198,168,192,3,95,214,204,204,204,204,204,204,204,204,243,83,189,169,245,11,0,249,253,251,1,169,253,99,0,145,255,195,0,209,200,0,0,208,8,5,64,249,168,3,30,248,194,3,0,180,244,10,128,82,244,0,176,114,160,3,0,
      180,245,3,1,170,97,3,0,180,232,243,1,178,225,99,0,145,243,3,2,170,232,163,1,169,232,163,0,169,35,0,0,148,128,2,0,54,225,35,0,145,224,3,21,170,245,35,0,145,30,0,0,148,224,1,0,54,232,3,31,170,233,99,0,145,42,105,104,184,171,106,104,184,95,1,11,
      107,40,2,0,84,67,2,0,84,8,17,0,145,31,65,0,241,33,255,255,84,232,3,31,42,14,0,0,20,116,0,136,82,20,0,176,114,160,3,94,248,43,250,255,151,224,3,20,42,255,195,0,145,253,251,65,169,245,11,64,249,243,83,195,168,192,3,95,214,40,0,128,82,2,0,0,20,
      8,0,128,18,244,3,31,42,104,2,0,185,243,255,255,23,243,83,189,169,245,91,1,169,253,123,2,169,253,131,0,145,255,67,0,209,201,0,0,208,243,3,1,170,244,3,0,170,245,3,31,170,54,0,128,82,40,0,128,82,41,5,64,249,233,7,0,249,255,3,0,249,8,2,0,54,225,
      3,0,145,224,3,20,170,66,1,128,82,62,21,0,148,233,3,64,249,96,122,53,184,63,1,20,235,96,1,0,84,73,1,0,180,40,1,64,121,31,185,0,113,225,0,0,84,52,9,0,145,40,0,128,82,8,0,0,20,232,3,31,42,127,122,53,184,5,0,0,20,232,3,31,42,117,0,0,181,63,1,20,
      235,224,0,0,84,169,6,0,145,191,14,0,241,246,39,159,26,245,3,9,170,63,17,0,241,129,252,255,84,224,7,64,249,232,3,54,42,19,1,0,18,240,249,255,151,224,3,19,42,255,67,0,145,253,123,66,169,245,91,65,169,243,83,195,168,192,3,95,214,204,204,204,204,
      227,3,0,170,224,3,31,170,225,3,31,170,226,3,31,170,8,253,255,23,253,123,191,169,253,3,0,145,195,0,0,144,160,0,0,240,0,64,7,145,225,3,31,170,2,0,129,82,99,180,66,249,96,0,63,214,96,0,0,180,253,123,193,168,192,3,95,214,195,0,0,144,160,0,0,240,
      0,96,8,145,225,3,31,170,2,0,129,82,99,180,66,249,253,123,193,168,96,0,31,214,243,83,188,169,245,91,1,169,252,19,0,249,253,251,2,169,253,163,0,145,255,195,8,209,200,0,0,208,244,3,2,170,245,3,1,42,246,3,0,170,224,131,0,145,65,21,128,82,8,5,64,
      249,2,65,128,82,243,3,3,170,168,3,29,248,215,249,255,151,136,32,128,82,233,243,1,178,191,2,0,114,234,135,97,178,35,67,128,82,64,21,138,154,232,15,0,185,200,0,0,176,233,3,0,249,228,3,0,145,225,3,22,170,226,3,31,42,8,121,69,249,67,0,160,114,0,
      1,63,214,232,3,0,42,224,3,31,42,104,1,0,52,19,0,0,18,160,3,93,248,170,249,255,151,224,3,19,42,255,195,8,145,253,251,66,169,252,19,64,249,245,91,65,169,243,83,196,168,192,3,95,214,200,0,0,176,224,3,64,249,161,0,0,176,33,224,54,145,228,131,0,145,
      229,51,0,145,8,125,69,249,226,3,31,170,227,3,31,170,0,1,63,214,200,0,0,176,245,3,0,42,224,3,64,249,8,113,69,249,0,1,63,214,224,3,31,42,213,252,255,53,232,15,64,185,31,13,0,113,99,252,255,84,225,131,0,145,224,3,19,170,119,3,0,148,224,3,19,170,
      2,4,0,148,129,11,128,82,60,14,0,148,96,251,255,180,21,8,0,145,232,243,1,178,225,67,0,145,224,3,21,170,232,35,1,169,99,255,255,151,32,1,0,54,148,0,0,180,224,3,20,170,225,3,21,170,103,3,0,148,224,67,0,145,225,3,19,170,4,0,0,148,204,255,255,23,
      224,3,31,42,202,255,255,23,243,83,190,169,253,123,1,169,253,67,0,145,243,3,1,170,232,3,31,170,169,0,0,208,41,129,19,145,10,104,104,184,43,105,104,184,95,1,11,107,168,0,0,84,3,1,0,84,8,17,0,145,31,65,0,241,33,255,255,84,224,3,19,170,253,123,65,
      169,243,83,194,168,34,251,255,23,180,0,0,240,160,0,0,208,0,192,19,145,148,198,66,249,128,2,63,214,224,3,19,170,212,3,0,148,128,2,63,214,160,0,0,208,0,16,21,145,128,2,63,214,224,3,31,42,253,123,65,169,243,83,194,168,192,3,95,214,204,204,204,204,
      204,204,204,204,204,204,204,204,166,0,0,20,204,204,204,204,204,204,204,204,204,204,204,204,8,48,0,145,9,1,64,185,0,0,176,18,63,1,0,107,64,1,0,84,32,5,0,17,10,125,95,136,95,1,9,107,129,0,0,84,0,125,10,136,138,255,255,53,3,0,0,20,95,63,3,213,244,
      255,255,23,192,3,95,214,204,204,204,204,243,15,30,248,253,251,0,169,253,35,0,145,9,0,176,18,10,48,0,145,75,1,64,185,127,1,9,107,96,4,0,84,104,5,0,81,76,125,95,136,159,1,11,107,129,0,0,84,72,253,12,136,140,255,255,53,3,0,0,20,95,63,3,213,245,
      255,255,23,168,2,0,53,243,0,0,144,191,59,3,213,0,1,0,180,8,0,64,249,105,2,64,249,8,17,64,249,239,3,8,170,32,1,63,214,33,0,128,82,0,1,63,214,200,0,0,208,0,181,69,249,224,0,0,180,8,0,64,249,105,2,64,249,8,9,64,249,239,3,8,170,32,1,63,214,0,1,63,
      214,232,3,31,42,224,3,8,42,253,251,64,169,243,7,66,248,192,3,95,214,232,119,31,50,251,255,255,23,243,83,190,169,253,123,1,169,253,67,0,145,255,3,1,209,201,0,0,176,232,3,2,170,243,3,0,170,41,5,64,249,169,131,30,248,33,2,248,55,244,0,0,144,255,
      7,0,249,104,4,0,180,9,1,64,249,35,1,64,249,137,2,64,249,239,3,3,170,32,1,63,214,161,0,0,208,33,32,22,145,226,35,0,145,224,3,8,170,96,0,63,214,225,3,0,42,226,7,64,249,23,0,0,20,105,58,64,185,41,5,0,113,203,253,255,84,96,134,64,173,224,67,0,145,
      225,3,19,170,105,58,0,185,224,135,0,173,124,252,255,151,160,3,248,54,225,3,0,42,96,26,64,249,233,0,0,144,8,0,64,249,41,1,64,249,8,13,64,249,239,3,8,170,32,1,63,214,226,3,31,170,0,1,63,214,18,0,0,20,226,3,31,170,96,26,64,249,137,2,64,249,8,0,
      64,249,8,13,64,249,239,3,8,170,32,1,63,214,0,1,63,214,224,7,64,249,0,1,0,180,255,7,0,249,137,2,64,249,8,0,64,249,8,9,64,249,239,3,8,170,32,1,63,214,0,1,63,214,160,131,94,248,207,248,255,151,224,3,31,42,255,3,1,145,253,123,65,169,243,83,194,168,
      192,3,95,214,204,204,204,204,243,83,190,169,253,123,1,169,253,67,0,145,243,3,0,170,0,24,64,249,244,3,1,42,168,0,0,176,8,129,53,145,104,2,0,249,32,1,0,180,127,26,0,249,233,0,0,144,8,0,64,249,41,1,64,249,8,9,64,249,239,3,8,170,32,1,63,214,0,1,
      63,214,232,11,2,50,104,14,0,185,116,0,0,52,224,3,19,170,40,4,0,148,224,3,19,170,253,123,65,169,243,83,194,168,192,3,95,214,204,204,204,204,32,0,32,212,32,0,32,212,253,123,191,169,253,3,0,145,95,0,0,249,44,44,64,41,42,36,65,41,236,0,0,53,203,
      0,0,53,95,1,3,113,129,0,0,84,8,192,168,82,63,1,8,107,96,2,0,84,72,0,136,82,45,113,134,82,8,0,176,114,77,209,169,114,159,1,13,107,193,2,0,84,12,59,153,82,76,122,169,114,127,1,12,107,65,2,0,84,203,182,150,82,75,226,169,114,95,1,11,107,193,1,0,
      84,202,157,141,82,42,184,169,114,63,1,10,107,65,1,0,84,64,0,0,249,233,0,0,144,8,0,64,249,41,1,64,249,8,5,64,249,239,3,8,170,32,1,63,214,0,1,63,214,232,3,31,42,224,3,8,42,253,123,193,168,192,3,95,214,243,83,189,169,245,91,1,169,253,123,2,169,
      253,131,0,145,255,67,0,209,200,0,0,176,213,0,0,176,243,3,0,170,212,0,0,208,8,5,64,249,232,7,0,249,168,50,75,185,73,46,64,249,40,121,104,248,201,0,0,208,41,169,75,185,8,1,64,145,8,5,64,185,63,1,8,107,108,9,0,84,136,210,69,249,246,0,0,144,40,2,
      0,180,224,3,19,170,97,16,128,82,39,2,0,148,224,3,19,170,213,2,0,148,224,3,0,185,224,3,19,170,148,210,69,249,217,2,0,148,200,2,64,249,239,3,20,170,225,3,0,170,0,1,63,214,224,3,0,145,128,2,63,214,32,5,0,52,168,50,75,185,213,0,0,208,73,46,64,249,
      40,121,104,248,201,0,0,208,41,185,75,185,8,1,64,145,8,5,64,185,63,1,8,107,204,8,0,84,168,218,69,249,245,0,176,82,40,3,0,180,201,2,64,249,239,3,8,170,255,3,0,249,32,1,63,214,224,3,0,145,0,1,63,214,244,3,0,42,96,3,248,55,225,3,64,249,224,3,19,
      170,45,2,0,148,0,1,0,55,168,0,0,240,8,85,66,249,0,1,63,214,8,60,0,18,31,4,0,113,8,1,21,42,20,176,136,26,200,0,0,176,224,3,64,249,8,137,69,249,0,1,63,214,11,0,0,20,8,146,128,82,180,2,8,42,8,0,0,20,232,3,64,185,224,3,19,170,1,5,0,81,83,2,0,148,
      232,255,175,18,31,0,0,114,244,19,136,26,224,7,64,249,41,248,255,151,224,3,20,42,255,67,0,145,253,123,66,169,245,91,65,169,243,83,195,168,192,3,95,214,246,3,20,170,212,0,0,208,148,162,46,145,224,3,20,170,154,3,0,148,136,2,64,185,244,3,22,170,
      31,5,0,49,193,245,255,84,168,0,0,240,160,0,0,208,0,0,23,145,8,97,66,249,0,1,63,214,168,0,0,240,161,0,0,208,33,96,22,145,8,105,66,249,0,1,63,214,128,210,5,249,192,0,0,208,0,160,46,145,190,3,0,148,159,255,255,23,212,0,0,208,148,226,46,145,224,
      3,20,170,131,3,0,148,136,2,64,185,31,5,0,49,161,246,255,84,6,0,0,148,160,218,5,249,192,0,0,208,0,224,46,145,177,3,0,148,175,255,255,23,253,123,191,169,253,3,0,145,168,0,0,240,160,0,0,208,0,224,28,145,225,3,31,170,2,0,129,82,8,181,66,249,0,1,
      63,214,224,0,0,180,162,0,0,240,161,0,0,208,33,64,29,145,66,104,66,249,253,123,193,168,64,0,31,214,253,123,193,168,192,3,95,214,243,83,190,169,253,123,1,169,253,67,0,145,255,195,0,209,200,0,0,176,243,3,1,170,244,3,0,170,160,0,0,208,0,64,24,145,
      161,0,0,208,33,128,23,145,8,5,64,249,226,3,20,170,227,3,19,170,228,3,31,170,37,0,128,82,168,131,30,248,32,0,0,148,99,98,0,145,130,34,0,145,160,0,0,208,0,224,25,145,161,0,0,208,33,96,25,145,228,3,31,170,229,3,31,42,23,0,0,148,232,243,1,178,224,
      35,0,145,232,255,1,169,232,163,0,169,111,1,0,148,132,98,0,145,160,0,0,208,0,192,27,145,161,0,0,208,33,192,26,145,226,131,0,145,227,35,0,145,229,3,31,42,9,0,0,148,224,35,0,145,103,1,0,148,160,131,94,248,191,247,255,151,255,195,0,145,253,123,65,
      169,243,83,194,168,192,3,95,214,243,83,188,169,245,91,1,169,247,99,2,169,253,123,3,169,253,195,0,145,247,3,5,42,243,3,4,170,245,3,3,170,244,3,2,170,246,3,1,170,133,0,0,54,200,0,0,176,41,0,128,82,9,129,39,57,225,3,21,170,66,2,0,148,128,1,0,54,
      224,3,21,170,32,2,0,148,128,2,0,249,147,5,0,180,32,5,0,180,225,3,31,170,66,1,128,82,212,18,0,148,31,4,0,113,232,23,159,26,36,0,0,20,216,0,0,176,119,0,0,55,8,131,103,57,40,4,0,52,224,135,97,178,36,0,0,148,128,0,0,54,40,0,128,82,8,131,39,57,7,
      0,0,20,160,255,159,146,0,0,176,242,29,0,0,148,8,0,0,18,8,131,39,57,160,2,0,54,160,255,159,146,225,3,22,170,0,0,176,242,226,3,20,170,227,3,21,170,228,3,19,170,50,0,0,148,160,1,0,55,224,135,97,178,225,3,22,170,226,3,20,170,227,3,21,170,228,3,19,
      170,253,123,67,169,247,99,66,169,245,91,65,169,243,83,196,168,39,0,0,20,232,3,31,42,104,2,0,185,253,123,67,169,247,99,66,169,245,91,65,169,243,83,196,168,192,3,95,214,243,15,30,248,253,251,0,169,253,35,0,145,255,67,0,209,200,0,0,176,35,3,128,
      82,161,0,0,208,33,0,30,145,228,3,0,145,226,3,31,42,8,5,64,249,67,0,160,114,232,7,0,249,200,0,0,176,255,3,0,249,8,121,69,249,0,1,63,214,200,0,0,176,243,3,0,42,224,3,64,249,8,113,69,249,0,1,63,214,224,7,64,249,127,2,0,113,243,23,159,26,92,247,
      255,151,224,3,19,42,255,67,0,145,253,251,64,169,243,7,66,248,192,3,95,214,243,83,188,169,245,91,1,169,247,99,2,169,253,123,3,169,253,195,0,145,255,131,1,209,200,0,0,176,246,3,0,170,248,243,1,178,224,163,0,145,243,3,4,170,244,3,3,170,8,5,64,249,
      245,3,2,170,247,3,1,170,168,131,28,248,248,99,3,169,248,23,0,249,231,0,0,148,224,163,0,145,194,254,255,151,224,67,0,145,248,227,1,169,248,11,0,249,225,0,0,148,224,3,1,145,248,227,4,169,248,35,0,249,221,0,0,148,225,3,1,145,224,3,31,170,11,249,
      255,151,128,1,248,55,224,3,1,145,172,1,0,148,129,11,128,82,230,11,0,148,96,0,0,181,224,3,1,145,167,1,0,148,1,8,0,145,224,67,0,145,23,1,0,148,3,0,0,20,224,67,0,145,73,1,0,148,224,3,1,145,205,0,0,148,255,7,0,249,183,6,0,180,232,2,64,121,104,6,
      0,52,232,243,1,178,224,3,23,170,232,163,4,169,232,35,0,249,102,245,255,151,1,168,0,145,224,3,1,145,208,0,0,148,161,0,0,208,33,0,30,145,224,3,1,145,70,1,0,148,224,3,1,145,225,3,23,170,67,1,0,148,224,3,1,145,138,1,0,148,200,0,0,176,225,3,0,170,
      228,35,0,145,224,3,22,170,226,3,31,42,35,0,128,82,8,121,69,249,0,1,63,214,247,3,0,42,224,3,1,145,173,0,0,148,246,3,31,42,215,2,0,53,224,163,0,145,123,1,0,148,225,3,0,170,224,7,64,249,226,3,21,170,227,3,20,170,228,3,19,170,41,0,0,148,64,1,0,55,
      224,67,0,145,114,1,0,148,225,3,0,170,224,7,64,249,226,3,21,170,227,3,20,170,228,3,19,170,32,0,0,148,64,2,0,54,54,0,128,82,24,0,0,20,246,3,31,42,224,67,0,145,148,0,0,148,224,163,0,145,146,0,0,148,160,131,92,248,211,2,0,18,233,246,255,151,224,
      3,19,42,255,131,1,145,253,123,67,169,247,99,66,169,245,91,65,169,243,83,196,168,192,3,95,214,224,7,64,249,161,0,0,208,33,96,31,145,226,3,21,170,227,3,20,170,228,3,19,170,7,0,0,148,246,3,0,42,200,0,0,176,224,7,64,249,8,113,69,249,0,1,63,214,230,
      255,255,23,243,83,188,169,245,91,1,169,247,19,0,249,253,251,2,169,253,163,0,145,255,67,0,209,200,0,0,176,245,3,3,170,244,3,2,170,246,3,1,170,247,3,0,170,8,5,64,249,232,7,0,249,132,3,0,180,136,0,128,82,243,3,4,170,229,19,0,145,230,3,0,145,224,
      3,23,170,225,3,31,170,232,127,0,41,200,0,0,176,226,3,22,170,3,2,128,82,228,3,31,170,8,117,69,249,0,1,63,214,64,3,0,52,224,3,23,170,225,3,22,170,226,3,20,170,227,3,21,170,39,0,0,148,64,3,0,54,128,2,64,249,64,4,0,180,225,3,31,170,66,1,128,82,224,
      17,0,148,31,4,0,113,15,0,0,20,224,7,64,249,170,246,255,151,224,3,23,170,225,3,22,170,226,3,20,170,227,3,21,170,255,67,0,145,253,251,66,169,247,19,64,249,245,91,65,169,243,83,196,168,19,0,0,20,232,7,64,185,31,5,0,113,232,23,159,26,52,0,128,82,
      104,2,0,185,2,0,0,20,244,3,31,42,224,7,64,249,151,246,255,151,224,3,20,42,255,67,0,145,253,251,66,169,247,19,64,249,245,91,65,169,243,83,196,168,192,3,95,214,232,3,31,42,242,255,255,23,243,83,188,169,245,91,1,169,252,19,0,249,253,251,2,169,253,
      163,0,145,255,131,8,209,200,0,0,176,243,3,2,170,245,3,1,170,246,3,0,170,224,67,0,145,225,3,31,42,8,5,64,249,2,65,128,82,244,3,3,170,168,3,29,248,149,246,255,151,136,32,128,82,229,67,0,145,230,51,0,145,224,3,22,170,225,3,31,170,226,3,21,170,232,
      15,0,185,200,0,0,176,67,0,128,82,228,3,31,170,8,117,69,249,0,1,63,214,245,3,0,42,224,0,0,53,225,67,0,145,224,3,20,170,87,0,0,148,224,3,20,170,226,0,0,148,96,2,0,249,160,3,93,248,191,2,0,113,243,23,159,26,101,246,255,151,224,3,19,42,255,131,8,
      145,253,251,66,169,252,19,64,249,245,91,65,169,243,83,196,168,192,3,95,214,31,124,0,169,31,8,0,249,192,3,95,214,243,15,30,248,253,251,0,169,253,35,0,145,8,8,64,249,31,0,0,249,232,0,0,180,31,1,0,121,243,3,0,170,0,8,64,249,96,0,0,180,202,1,0,148,
      127,254,0,169,253,251,64,169,243,7,66,248,192,3,95,214,243,15,30,248,253,251,0,169,253,35,0,145,243,3,0,170,31,124,0,169,31,8,0,249,5,0,0,148,224,3,19,170,253,251,64,169,243,7,66,248,192,3,95,214,243,83,189,169,245,91,1,169,253,123,2,169,253,
      131,0,145,8,4,64,249,31,1,1,235,66,3,0,84,244,3,1,170,63,4,0,177,32,3,0,84,136,6,0,145,243,3,0,170,8,1,8,171,0,49,159,218,170,1,0,148,245,3,0,170,160,1,0,180,104,2,64,249,168,2,0,180,118,10,64,249,8,249,127,211,2,9,0,145,224,3,21,170,225,3,22,
      170,112,245,255,151,118,0,0,180,224,3,22,170,159,1,0,148,116,214,0,169,191,2,0,241,224,7,159,26,4,0,0,20,32,0,128,82,2,0,0,20,224,3,31,42,253,123,66,169,245,91,65,169,243,83,195,168,192,3,95,214,118,10,64,249,191,2,0,121,54,254,255,181,242,255,
      255,23,243,83,190,169,253,123,1,169,253,67,0,145,243,3,1,170,244,3,0,170,161,0,0,180,224,3,19,170,86,244,255,151,226,3,0,170,2,0,0,20,226,3,31,170,224,3,20,170,225,3,19,170,253,123,65,169,243,83,194,168,1,0,0,20,243,83,189,169,245,11,0,249,253,
      251,1,169,253,99,0,145,243,3,2,170,244,3,0,170,8,8,64,249,245,3,1,170,31,0,0,249,72,0,0,180,31,1,0,121,115,2,0,180,224,3,20,170,225,3,19,170,183,255,255,151,32,2,0,54,128,10,64,249,98,250,127,211,225,3,21,170,58,245,255,151,136,6,64,249,31,1,
      19,235,163,0,0,84,137,10,64,249,147,2,0,249,73,0,0,180,63,121,51,120,31,1,19,235,224,55,159,26,4,0,0,20,32,0,128,82,2,0,0,20,224,3,31,42,253,251,65,169,245,11,64,249,243,83,195,168,192,3,95,214,8,8,64,249,31,0,0,249,72,0,0,180,31,1,0,121,192,
      3,95,214,8,4,64,249,31,1,1,235,163,0,0,84,9,8,64,249,1,0,0,249,73,0,0,180,63,121,33,120,31,1,1,235,224,55,159,26,192,3,95,214,243,83,190,169,253,123,1,169,253,67,0,145,243,3,1,170,244,3,0,170,161,0,0,180,224,3,19,170,18,244,255,151,226,3,0,170,
      2,0,0,20,226,3,31,170,224,3,20,170,225,3,19,170,253,123,65,169,243,83,194,168,1,0,0,20,243,83,189,169,245,91,1,169,253,123,2,169,253,131,0,145,161,3,0,180,8,0,64,249,246,3,2,170,244,3,0,170,19,1,2,171,194,2,0,84,245,3,1,170,224,3,20,170,225,
      3,19,170,116,255,255,151,32,2,0,54,136,10,64,249,194,250,127,211,137,2,64,249,225,3,21,170,0,5,9,139,245,244,255,151,136,6,64,249,31,1,19,235,163,0,0,84,137,10,64,249,147,2,0,249,73,0,0,180,63,121,51,120,31,1,19,235,224,55,159,26,4,0,0,20,224,
      3,31,42,2,0,0,20,32,0,128,82,253,123,66,169,245,91,65,169,243,83,195,168,192,3,95,214,8,0,64,249,31,1,2,235,105,0,0,84,9,8,64,249,33,121,34,120,31,1,2,235,224,151,159,26,192,3,95,214,40,252,65,211,1,5,0,145,81,255,255,23,0,4,64,249,192,3,95,
      214,0,0,64,249,192,3,95,214,8,0,64,249,31,1,0,241,224,23,159,26,192,3,95,214,0,8,64,249,192,3,95,214,243,83,189,169,245,11,0,249,253,251,1,169,253,99,0,145,168,0,0,176,8,33,1,145,31,0,0,241,41,248,127,211,21,1,128,154,51,9,0,145,127,2,1,235,
      137,1,0,84,200,0,0,144,224,3,19,170,8,133,69,249,0,1,63,214,244,3,0,170,224,0,0,180,224,3,20,170,225,3,21,170,226,3,19,170,185,244,255,151,2,0,0,20,244,3,31,170,224,3,20,170,253,251,65,169,245,11,64,249,243,83,195,168,192,3,95,214,243,83,189,
      169,245,91,1,169,253,123,2,169,253,131,0,145,182,0,0,208,243,3,1,170,225,3,31,170,226,3,31,42,244,3,0,170,214,74,66,249,192,2,63,214,96,2,0,52,245,3,0,42,225,3,0,42,224,3,19,170,26,255,255,151,192,1,0,54,224,3,19,170,207,255,255,151,225,3,0,
      170,224,3,20,170,226,3,21,42,192,2,63,214,232,3,0,42,224,3,19,170,225,3,8,42,253,123,66,169,245,91,65,169,243,83,195,168,113,255,255,23,224,3,31,42,253,123,66,169,245,91,65,169,243,83,195,168,192,3,95,214,243,83,186,169,245,91,1,169,247,99,2,
      169,249,107,3,169,251,35,0,249,253,251,4,169,253,35,1,145,56,248,127,211,247,3,1,170,245,3,0,170,224,3,3,170,225,3,24,170,243,3,3,170,244,3,2,42,248,254,255,151,246,3,0,42,224,3,19,170,225,3,24,170,89,255,255,151,247,2,0,180,248,3,31,170,247,
      6,0,209,185,0,0,176,57,131,31,145,186,106,120,56,159,2,0,114,251,18,152,154,224,3,19,170,98,251,127,211,72,255,68,211,33,107,232,56,140,255,255,151,72,15,64,146,34,0,128,82,98,251,127,179,224,3,19,170,33,107,232,56,134,255,255,151,24,7,0,145,
      247,6,0,209,255,6,0,177,225,253,255,84,192,2,0,18,253,251,68,169,251,35,64,249,249,107,67,169,247,99,66,169,245,91,65,169,243,83,198,168,192,3,95,214,243,83,187,169,245,91,1,169,247,99,2,169,249,27,0,249,253,251,3,169,253,227,0,145,255,195,0,
      209,200,0,0,144,243,3,5,170,246,3,4,170,247,3,3,170,248,3,2,42,249,3,1,42,8,5,64,249,245,3,0,170,232,23,0,249,168,0,0,208,8,185,66,249,0,1,63,214,192,2,0,180,181,0,0,208,161,0,0,176,33,224,31,145,244,3,0,170,181,106,66,249,160,2,63,214,128,6,
      0,180,201,0,0,240,239,3,0,170,232,3,0,170,41,1,64,249,32,1,63,214,32,3,0,18,225,3,24,42,226,3,23,170,227,3,22,170,228,3,19,170,0,1,63,214,243,3,0,42,224,39,0,185,69,0,0,20,168,0,0,208,8,85,66,249,0,1,63,214,232,0,176,82,31,4,0,113,8,60,0,51,
      233,243,1,178,19,176,136,26,224,35,0,145,233,39,1,169,243,39,0,185,233,7,0,249,130,254,255,151,224,147,0,145,227,35,0,145,129,0,128,82,34,0,128,82,148,255,255,151,180,0,0,208,160,0,0,176,0,64,36,145,148,198,66,249,128,2,63,214,224,35,0,145,75,
      255,255,151,128,2,63,214,160,0,0,176,0,160,38,145,128,2,63,214,224,3,21,170,128,2,63,214,160,0,0,176,0,16,21,145,128,2,63,214,224,35,0,145,110,254,255,151,41,0,0,20,168,0,0,208,8,85,66,249,0,1,63,214,232,0,176,82,31,4,0,113,8,60,0,51,233,243,
      1,178,19,176,136,26,224,35,0,145,233,39,1,169,243,39,0,185,233,7,0,249,93,254,255,151,224,147,0,145,227,35,0,145,129,0,128,82,34,0,128,82,111,255,255,151,182,0,0,208,160,0,0,176,0,192,32,145,214,198,66,249,192,2,63,214,224,35,0,145,38,255,255,
      151,192,2,63,214,160,0,0,176,0,16,21,145,192,2,63,214,224,35,0,145,78,254,255,151,161,0,0,176,33,224,35,145,224,3,20,170,160,2,63,214,160,0,0,180,168,0,0,208,224,3,20,170,8,29,66,249,0,1,63,214,224,23,64,249,157,244,255,151,224,3,19,42,255,195,
      0,145,253,251,67,169,249,27,64,249,247,99,66,169,245,91,65,169,243,83,197,168,192,3,95,214,253,123,190,169,253,3,0,145,48,0,128,146,176,11,0,249,102,0,0,148,2,0,0,20,0,0,128,210,253,123,194,168,192,3,95,214,97,0,0,212,166,15,0,20,204,204,204,
      204,94,0,0,20,204,204,204,204,252,255,255,23,204,204,204,204,243,83,190,169,245,11,0,249,253,123,191,169,253,3,0,145,200,0,0,144,20,129,44,145,169,0,0,208,40,225,14,145,9,253,223,200,243,3,0,170,128,34,0,145,32,1,63,214,104,2,64,185,136,0,0,
      53,9,0,128,18,105,2,0,185,23,0,0,20,31,5,0,49,161,1,0,84,168,0,0,208,21,97,23,145,168,254,223,200,3,0,128,82,130,12,128,82,129,34,0,145,224,3,20,170,0,1,63,214,104,2,64,185,136,2,0,52,31,5,0,49,224,254,255,84,200,0,0,144,9,49,75,185,72,46,64,
      249,9,89,105,248,200,0,0,144,8,1,64,185,42,1,64,145,72,5,0,185,168,0,0,208,8,129,22,145,9,253,223,200,128,34,0,145,32,1,63,214,253,123,193,168,245,11,64,249,243,83,194,168,192,3,95,214,8,0,128,18,104,2,0,185,169,0,0,208,40,129,22,145,245,255,
      255,23,204,204,204,204,243,83,191,169,253,123,191,169,253,3,0,145,200,0,0,144,20,129,44,145,169,0,0,208,40,225,14,145,9,253,223,200,243,3,0,170,128,34,0,145,32,1,63,214,204,0,0,144,136,1,64,185,128,34,0,145,9,5,0,17,200,0,0,144,10,49,75,185,
      72,46,64,249,137,1,0,185,105,2,0,185,9,89,106,248,136,1,64,185,43,1,64,145,104,5,0,185,169,0,0,208,40,129,22,145,9,253,223,200,32,1,63,214,224,3,20,170,168,0,0,208,8,225,23,145,9,253,223,200,32,1,63,214,253,123,193,168,243,83,193,168,192,3,95,
      214,243,15,31,248,253,123,191,169,253,3,0,145,243,3,0,170,4,0,0,20,224,3,19,170,30,10,0,148,224,0,0,52,224,3,19,170,67,15,0,148,96,255,255,180,253,123,193,168,243,7,65,248,192,3,95,214,127,6,0,177,64,0,0,84,104,0,0,148,111,0,0,148,243,15,31,
      248,253,123,191,169,253,3,0,145,104,0,0,240,8,225,0,145,243,3,0,170,233,3,1,170,104,2,0,249,97,34,0,145,32,33,0,145,127,254,0,169,53,8,0,148,224,3,19,170,253,123,193,168,243,7,65,248,192,3,95,214,9,4,64,249,104,0,0,240,8,33,1,145,63,1,0,241,
      32,17,136,154,192,3,95,214,204,204,204,204,204,204,204,204,243,83,191,169,253,123,191,169,253,3,0,145,104,0,0,240,8,225,0,145,244,3,0,170,128,34,0,145,136,2,0,249,243,3,1,42,69,8,0,148,147,0,0,54,1,3,128,210,224,3,20,170,109,255,255,151,224,
      3,20,170,253,123,193,168,243,83,193,168,192,3,95,214,104,0,0,240,8,225,1,145,8,252,0,169,106,0,0,240,73,161,1,145,9,0,0,249,192,3,95,214,204,204,204,204,104,0,0,240,8,225,0,145,8,132,0,248,49,8,0,20,243,15,31,248,253,123,191,169,253,3,0,145,
      104,0,0,240,8,225,0,145,243,3,0,170,233,3,1,170,104,2,0,249,97,34,0,145,32,33,0,145,127,254,0,169,255,7,0,148,104,0,0,240,8,161,1,145,104,2,0,249,224,3,19,170,253,123,193,168,243,7,65,248,192,3,95,214,204,204,204,204,104,0,0,240,8,129,2,145,
      8,252,0,169,106,0,0,240,73,65,2,145,9,0,0,249,192,3,95,214,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,104,0,0,240,8,225,0,145,243,3,0,170,233,3,1,170,104,2,0,249,97,34,0,145,32,33,0,145,127,254,0,169,227,7,0,148,104,0,0,240,8,
      65,2,145,104,2,0,249,224,3,19,170,253,123,193,168,243,7,65,248,192,3,95,214,204,204,204,204,253,123,189,169,253,3,0,145,224,67,0,145,193,255,255,151,168,0,0,240,1,33,42,145,224,67,0,145,231,4,0,148,253,123,189,169,253,3,0,145,224,67,0,145,217,
      255,255,151,168,0,0,240,1,65,44,145,224,67,0,145,223,4,0,148,204,204,204,204,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,104,0,0,240,8,1,3,145,243,3,0,170,104,2,0,249,97,0,0,54,1,3,128,210,17,255,255,151,224,3,19,170,253,123,193,
      168,243,7,65,248,192,3,95,214,243,83,189,169,245,91,1,169,247,19,0,249,253,123,190,169,253,3,0,145,245,3,0,170,246,3,1,170,0,0,128,82,198,3,0,148,8,28,0,83,72,7,0,52,153,3,0,148,19,28,0,83,179,67,0,57,52,0,128,82,215,0,0,144,232,146,75,185,40,
      7,0,53,40,0,128,82,232,146,11,185,44,4,0,148,8,28,0,83,168,2,0,52,119,4,0,148,56,3,0,148,65,3,0,148,168,0,0,176,1,97,57,145,169,0,0,176,32,1,57,145,250,12,0,148,128,1,0,53,42,4,0,148,8,28,0,83,40,1,0,52,168,0,0,176,1,225,56,145,169,0,0,176,32,
      193,56,145,219,12,0,148,72,0,128,82,232,146,11,185,20,0,128,82,224,3,19,42,148,3,0,148,244,2,0,53,90,4,0,148,243,3,0,170,104,2,64,249,168,1,0,180,62,3,0,148,8,28,0,83,72,1,0,52,115,2,64,249,239,3,19,170,200,0,0,240,8,1,64,249,0,1,63,214,226,
      3,22,170,65,0,128,82,224,3,21,170,96,2,63,214,202,0,0,144,72,53,75,185,9,5,0,17,73,53,11,185,32,0,128,82,2,0,0,20,0,0,128,82,253,123,194,168,247,19,64,249,245,91,65,169,243,83,195,168,192,3,95,214,224,0,128,82,65,4,0,148,243,83,190,169,245,11,
      0,249,253,123,190,169,253,3,0,145,20,28,0,83,201,0,0,144,40,53,75,185,31,1,0,113,204,0,0,84,0,0,128,82,253,123,194,168,245,11,64,249,243,83,194,168,192,3,95,214,8,5,0,81,40,53,11,185,72,3,0,148,19,28,0,83,179,67,0,57,213,0,0,144,168,146,75,185,
      31,9,0,113,1,2,0,84,247,3,0,148,242,2,0,148,67,4,0,148,191,146,11,185,224,3,19,42,88,3,0,148,1,0,128,82,224,3,20,42,123,3,0,148,8,28,0,83,31,1,0,113,243,7,159,26,249,3,0,148,224,3,19,42,229,255,255,23,224,0,128,82,25,4,0,148,204,204,204,204,
      204,204,204,204,253,123,191,169,253,3,0,145,33,2,0,52,63,4,0,113,128,1,0,84,63,8,0,113,224,0,0,84,63,12,0,113,96,0,0,84,32,0,128,82,12,0,0,20,255,3,0,148,2,0,0,20,237,3,0,148,0,28,0,83,7,0,0,20,225,3,2,170,121,255,255,151,4,0,0,20,95,0,0,241,
      224,7,159,26,193,255,255,151,253,123,193,168,192,3,95,214,243,83,190,169,245,91,1,169,253,123,191,169,253,3,0,145,104,0,0,240,19,101,64,249,244,3,0,170,245,3,1,42,246,3,2,170,115,0,0,181,32,0,128,82,9,0,0,20,200,0,0,240,8,1,64,249,239,3,19,170,
      0,1,63,214,226,3,22,170,225,3,21,42,224,3,20,170,96,2,63,214,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,243,83,190,169,245,91,1,169,253,123,189,169,253,3,0,145,246,3,0,170,182,19,0,249,244,3,1,42,180,23,0,185,245,3,2,170,181,15,
      0,249,244,0,0,53,200,0,0,144,8,53,75,185,31,1,0,113,108,0,0,84,0,0,128,82,58,0,0,20,136,6,0,81,31,5,0,113,232,1,0,84,226,3,21,170,225,3,20,42,224,3,22,170,209,255,255,151,243,3,0,42,179,19,0,185,147,5,0,52,226,3,21,170,225,3,20,42,224,3,22,170,
      178,255,255,151,243,3,0,42,179,19,0,185,179,4,0,52,226,3,21,170,225,3,20,42,224,3,22,170,79,244,255,151,243,3,0,42,179,19,0,185,159,6,0,113,96,10,64,122,129,1,0,84,226,3,21,170,1,0,128,82,224,3,22,170,70,244,255,151,191,2,0,241,224,7,159,26,
      117,255,255,151,226,3,21,170,1,0,128,82,224,3,22,170,179,255,255,151,159,2,0,113,132,26,67,122,193,1,0,84,226,3,21,170,225,3,20,42,224,3,22,170,148,255,255,151,243,3,0,42,179,19,0,185,243,0,0,52,226,3,21,170,225,3,20,42,224,3,22,170,165,255,
      255,151,243,3,0,42,179,19,0,185,3,0,0,20,19,0,128,82,191,19,0,185,224,3,19,42,253,123,195,168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,204,204,204,204,243,83,190,169,245,11,0,249,253,123,191,169,253,3,0,145,243,3,1,42,127,6,
      0,113,244,3,0,170,245,3,2,170,65,0,0,84,21,2,0,148,226,3,21,170,225,3,19,42,224,3,20,170,163,255,255,151,253,123,193,168,245,11,64,249,243,83,194,168,192,3,95,214,243,83,191,169,253,123,191,169,253,3,0,145,200,0,0,144,20,225,44,145,136,2,64,
      249,191,59,3,213,31,5,0,241,32,5,0,84,200,4,0,181,104,0,0,240,0,65,3,145,169,0,0,208,40,1,19,145,9,253,223,200,32,1,63,214,243,3,0,170,115,0,0,181,51,0,128,210,19,0,0,20,104,0,0,240,1,193,3,145,169,0,0,208,40,65,19,145,9,253,223,200,224,3,19,
      170,32,1,63,214,224,254,255,180,104,0,0,240,1,33,4,145,128,6,0,249,169,0,0,208,40,65,19,145,9,253,223,200,224,3,19,170,32,1,63,214,192,253,255,180,128,10,0,249,136,254,95,200,104,0,0,181,147,254,9,200,169,255,255,53,191,59,3,213,31,1,0,241,96,
      10,65,250,4,25,65,250,96,0,0,84,32,0,128,82,2,0,0,20,0,0,128,82,253,123,193,168,243,83,193,168,192,3,95,214,204,204,204,204,243,83,191,169,253,123,191,169,253,3,0,145,199,255,255,151,8,28,0,83,104,1,0,52,200,0,0,144,19,1,45,145,116,2,64,249,
      200,0,0,240,8,1,64,249,239,3,20,170,0,1,63,214,96,66,0,145,128,2,63,214,17,0,0,20,200,0,0,144,10,1,45,145,3,0,0,20,191,58,3,213,63,32,3,213,72,9,64,249,191,59,3,213,136,255,255,181,44,0,128,210,75,65,0,145,104,253,95,200,104,0,0,181,108,253,
      9,200,169,255,255,53,191,59,3,213,200,254,255,181,253,123,193,168,243,83,193,168,192,3,95,214,204,204,204,204,232,255,255,144,14,1,0,145,200,61,64,185,203,193,40,139,104,133,64,185,31,53,0,113,9,3,0,84,108,241,64,185,204,2,0,52,104,41,64,121,
      201,49,0,145,47,73,108,184,13,0,128,82,108,13,64,121,8,1,11,139,10,97,0,145,204,1,0,52,73,33,0,145,40,5,64,185,255,1,8,107,163,0,0,84,43,1,64,185,8,1,11,11,255,1,8,107,3,1,0,84,173,5,0,17,191,1,12,107,74,161,0,145,41,161,0,145,163,254,255,84,
      0,0,128,210,192,3,95,214,11,0,0,185,72,37,64,185,40,0,0,185,73,13,64,185,192,65,41,139,250,255,255,23,243,83,191,169,253,123,185,169,253,3,0,145,168,0,0,208,8,193,23,145,9,253,223,200,244,3,1,170,2,6,128,210,225,67,0,145,243,3,0,170,32,1,63,
      214,64,0,0,181,96,0,62,212,232,55,64,185,137,8,128,82,31,1,9,106,224,3,0,84,168,0,0,208,8,225,19,145,9,253,223,200,224,3,1,145,32,1,63,214,237,71,64,185,170,5,0,81,107,2,10,10,138,2,10,10,104,1,10,11,233,3,13,203,171,65,40,139,44,1,19,138,105,
      5,0,209,77,0,0,181,128,0,62,212,42,9,205,154,136,10,205,154,74,1,8,139,75,125,64,242,64,1,0,84,10,0,128,82,137,253,95,136,41,1,10,42,137,253,8,136,168,255,255,53,191,59,3,213,140,1,13,139,107,5,0,209,43,255,255,181,253,123,199,168,243,83,193,
      168,192,3,95,214,243,83,190,169,245,91,1,169,253,123,190,169,253,3,0,145,246,3,0,42,244,3,1,170,225,83,0,145,224,67,0,145,160,255,255,151,243,3,0,170,147,0,0,181,136,0,128,82,136,2,0,185,23,0,0,20,201,0,0,144,40,97,75,185,245,19,64,185,40,1,
      0,53,40,0,128,82,40,97,11,185,232,23,64,185,72,0,248,55,96,0,62,212,225,3,21,170,224,3,19,170,181,255,255,151,168,0,0,208,8,161,23,145,9,253,223,200,227,3,20,170,226,3,22,42,225,3,21,170,224,3,19,170,32,1,63,214,64,0,0,53,96,0,62,212,253,123,
      194,168,245,91,65,169,243,83,194,168,192,3,95,214,243,83,191,169,253,123,191,169,253,3,0,145,104,0,0,208,8,177,65,185,136,3,96,54,86,255,255,151,168,0,0,240,10,97,45,145,72,1,64,185,9,5,0,17,73,1,0,185,63,5,0,113,129,0,0,84,65,17,0,145,128,0,
      128,82,200,255,255,151,21,255,255,151,8,28,0,83,104,1,0,52,168,0,0,240,19,33,45,145,116,2,64,249,200,0,0,208,8,1,64,249,239,3,20,170,0,1,63,214,96,34,0,145,128,2,63,214,4,0,0,20,191,59,3,213,168,0,0,240,31,169,5,249,253,123,193,168,243,83,193,
      168,192,3,95,214,243,83,191,169,253,123,190,169,253,3,0,145,104,0,0,208,8,177,65,185,104,3,96,54,50,255,255,151,168,0,0,240,10,97,45,145,72,1,64,185,9,5,0,81,73,1,0,185,137,0,0,53,64,5,64,185,225,67,0,145,165,255,255,151,242,254,255,151,8,28,
      0,83,104,1,0,52,168,0,0,240,19,33,45,145,116,2,64,249,200,0,0,208,8,1,64,249,239,3,20,170,0,1,63,214,96,34,0,145,128,2,63,214,4,0,0,20,191,59,3,213,168,0,0,240,31,169,5,249,253,123,194,168,243,83,193,168,192,3,95,214,204,204,204,204,243,83,187,
      169,245,91,1,169,247,99,2,169,249,107,3,169,251,35,0,249,253,123,184,169,253,3,0,145,246,3,0,170,249,3,1,170,175,255,255,151,203,42,66,41,200,226,64,41,201,255,255,240,55,1,0,145,246,103,2,169,201,14,64,185,218,30,64,185,234,19,0,185,10,9,128,
      82,232,66,40,139,234,27,0,185,234,227,0,145,232,27,0,249,200,2,64,185,95,125,0,169,255,255,4,169,255,91,0,185,200,1,0,55,234,99,0,145,234,51,0,249,190,255,255,151,168,0,0,176,8,97,22,145,9,253,223,200,227,131,1,145,34,0,128,82,1,0,128,82,32,
      24,0,24,32,1,63,214,0,0,128,210,182,0,0,20,232,66,41,139,244,74,120,248,41,3,8,203,232,66,43,139,59,253,67,147,11,89,123,248,105,253,127,211,42,1,0,82,234,59,0,185,170,0,0,52,232,10,0,145,1,65,43,139,225,35,0,249,4,0,0,20,104,61,0,18,232,67,
      0,185,225,35,64,249,104,0,0,208,21,45,65,249,200,0,0,208,19,0,128,210,181,1,0,180,8,1,64,249,239,3,21,170,0,1,63,214,225,99,0,145,0,0,128,82,160,2,63,214,243,3,0,170,83,17,0,181,104,0,0,208,21,45,65,249,225,35,64,249,200,0,0,208,20,8,0,181,53,
      1,0,180,8,1,64,249,239,3,21,170,0,1,63,214,225,99,0,145,32,0,128,82,160,2,63,214,244,3,0,170,20,5,0,181,168,0,0,176,8,129,21,145,224,27,64,249,9,253,223,200,2,0,128,82,1,0,128,210,32,1,63,214,244,3,0,170,244,3,0,181,168,0,0,176,8,161,18,145,
      9,253,223,200,32,1,63,214,104,0,0,208,20,49,65,249,224,91,0,185,84,1,0,180,200,0,0,208,8,1,64,249,239,3,20,170,0,1,63,214,225,99,0,145,96,0,128,82,128,2,63,214,244,3,0,170,212,1,0,181,232,99,0,145,232,55,0,249,107,255,255,151,168,0,0,176,8,97,
      22,145,9,253,223,200,227,163,1,145,34,0,128,82,1,0,128,82,224,13,0,24,32,1,63,214,224,43,64,249,99,0,0,20,234,66,56,139,72,253,95,200,84,253,9,200,201,255,255,53,191,59,3,213,31,1,20,235,193,0,0,84,168,0,0,176,8,225,16,145,9,253,223,200,224,
      3,20,170,32,1,63,214,104,0,0,208,21,45,65,249,225,35,64,249,244,39,0,249,216,0,0,208,53,1,0,180,8,3,64,249,239,3,21,170,0,1,63,214,225,99,0,145,64,0,128,82,160,2,63,214,225,35,64,249,243,3,0,170,51,7,0,181,200,22,64,185,104,2,0,52,200,30,64,
      185,40,2,0,52,136,62,64,185,11,170,136,82,138,194,40,139,73,1,64,185,63,1,11,107,97,1,0,84,72,9,64,185,31,1,26,107,1,1,0,84,72,25,64,249,159,2,8,235,161,0,0,84,232,19,64,185,232,66,40,139,19,89,123,248,179,4,0,181,168,0,0,176,8,65,19,145,9,253,
      223,200,224,3,20,170,32,1,63,214,243,3,0,170,211,3,0,181,168,0,0,176,8,161,18,145,9,253,223,200,32,1,63,214,104,0,0,208,19,49,65,249,224,91,0,185,51,1,0,180,8,3,64,249,239,3,19,170,0,1,63,214,225,99,0,145,128,0,128,82,96,2,63,214,243,3,0,170,
      211,1,0,181,232,99,0,145,232,59,0,249,24,255,255,151,168,0,0,176,8,97,22,145,9,253,223,200,227,195,1,145,34,0,128,82,1,0,128,82,160,3,0,24,32,1,63,214,235,254,255,151,243,43,64,249,51,3,0,249,104,0,0,208,21,45,65,249,85,1,0,180,200,0,0,208,8,
      1,64,249,255,91,0,185,239,3,21,170,244,207,4,169,0,1,63,214,225,99,0,145,160,0,128,82,160,2,63,214,0,255,255,151,224,3,19,170,253,123,200,168,251,35,64,249,249,107,67,169,247,99,66,169,245,91,65,169,243,83,197,168,192,3,95,214,31,32,3,213,87,
      0,109,192,126,0,109,192,127,0,109,192,204,204,204,204,243,83,191,169,253,123,189,169,253,3,0,145,180,0,0,240,136,6,64,249,115,6,0,88,31,1,19,235,65,5,0,84,168,0,0,176,8,1,20,145,255,15,0,249,9,253,223,200,224,99,0,145,32,1,63,214,232,15,64,249,
      169,0,0,176,41,1,18,145,232,11,0,249,40,253,223,200,0,1,63,214,232,11,64,249,9,124,64,211,41,1,8,202,168,0,0,176,8,225,17,145,233,11,0,249,9,253,223,200,32,1,63,214,232,11,64,249,9,124,64,211,224,131,0,145,41,1,8,202,168,0,0,176,8,65,22,145,
      233,11,0,249,9,253,223,200,32,1,63,214,233,35,64,185,235,67,0,145,232,19,64,249,10,129,9,202,233,11,64,249,72,1,9,202,8,1,11,202,9,189,64,146,63,1,19,235,136,1,0,88,8,1,137,154,136,6,0,249,233,3,40,170,168,0,0,240,9,9,0,249,253,123,195,168,243,
      83,193,168,192,3,95,214,31,32,3,213,50,162,223,45,153,43,0,0,51,162,223,45,153,43,0,0,168,0,0,240,0,193,45,145,169,0,0,176,40,225,20,145,9,253,223,200,32,1,31,214,168,0,0,240,0,193,45,145,150,4,0,20,204,204,204,204,253,123,191,169,253,3,0,145,
      12,0,0,148,137,4,128,210,8,0,64,249,8,1,9,170,8,0,0,249,11,0,0,148,8,0,64,249,9,1,127,178,9,0,0,249,253,123,193,168,192,3,95,214,204,204,204,204,168,0,0,240,0,1,46,145,192,3,95,214,204,204,204,204,168,0,0,240,0,33,46,145,192,3,95,214,204,204,
      204,204,253,123,190,169,253,3,0,145,170,73,139,82,200,255,255,240,9,1,0,145,40,1,64,121,31,1,10,107,1,5,0,84,44,61,64,185,42,193,44,139,11,170,136,82,72,1,64,185,31,1,11,107,65,4,0,84,72,49,64,121,31,45,8,113,225,3,0,84,11,0,9,203,41,193,44,
      139,40,41,64,121,8,1,9,139,10,97,0,145,41,13,64,121,8,5,128,210,44,41,8,155,170,11,0,249,95,1,12,235,64,1,0,84,73,13,64,185,127,65,41,235,163,0,0,84,72,9,64,185,9,1,9,11,127,65,41,235,131,0,0,84,74,161,0,145,245,255,255,23,10,0,128,210,106,0,
      0,181,0,0,128,82,10,0,0,20,72,37,64,185,104,0,248,54,0,0,128,82,6,0,0,20,32,0,128,82,4,0,0,20,0,0,128,82,2,0,0,20,0,0,128,82,253,123,194,168,192,3,95,214,253,123,191,169,253,3,0,145,34,1,0,148,96,2,0,52,232,3,18,170,172,0,0,240,138,97,46,145,
      11,5,64,249,72,253,95,200,104,1,0,181,75,253,9,200,169,255,255,53,8,0,0,20,127,1,8,235,96,1,0,84,138,97,46,145,72,253,95,200,104,0,0,181,75,253,9,200,169,255,255,53,191,59,3,213,8,255,255,181,0,0,128,82,253,123,193,168,192,3,95,214,32,0,128,
      82,253,255,255,23,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,19,28,0,83,4,1,0,148,192,0,0,52,179,0,0,53,168,0,0,240,8,97,46,145,31,253,159,200,191,59,3,213,253,123,193,168,243,7,65,248,192,3,95,214,253,123,191,169,253,3,0,145,
      170,0,0,240,72,81,110,57,31,0,0,113,41,0,128,82,41,1,136,26,73,81,46,57,238,0,0,148,47,4,0,148,8,28,0,83,104,0,0,53,0,0,128,82,8,0,0,20,126,5,0,148,8,28,0,83,136,0,0,53,0,0,128,82,54,4,0,148,249,255,255,23,32,0,128,82,253,123,193,168,192,3,95,
      214,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,168,0,0,240,8,81,110,57,19,28,0,83,41,28,0,83,72,0,0,52,169,0,0,53,224,3,19,42,116,5,0,148,224,3,19,42,36,4,0,148,32,0,128,82,253,123,193,168,243,7,65,248,192,3,95,214,204,204,204,
      204,243,83,191,169,253,123,191,169,253,3,0,145,180,0,0,240,136,66,111,57,243,3,0,42,104,3,0,53,127,6,0,113,168,3,0,84,199,0,0,148,128,1,0,52,115,1,0,53,168,0,0,240,0,129,46,145,176,7,0,148,160,0,0,53,168,0,0,240,0,225,46,145,172,7,0,148,128,
      1,0,52,0,0,128,82,13,0,0,20,168,0,0,240,8,129,46,145,169,0,0,240,41,225,46,145,10,0,128,146,10,41,0,169,42,41,0,169,10,9,0,249,42,9,0,249,40,0,128,82,136,66,47,57,32,0,128,82,253,123,193,168,243,83,193,168,192,3,95,214,160,0,128,82,110,0,0,148,
      204,204,204,204,243,83,189,169,245,91,1,169,247,99,2,169,253,123,191,169,253,3,0,145,247,3,0,170,243,3,1,42,248,3,2,170,246,3,3,170,244,3,4,42,245,3,5,170,157,0,0,148,31,0,0,113,96,10,65,122,33,1,0,84,200,0,0,208,8,1,64,249,239,3,22,170,0,1,
      63,214,226,3,24,170,1,0,128,82,224,3,23,170,192,2,63,214,225,3,21,170,224,3,20,42,247,8,0,148,253,123,193,168,247,99,66,169,245,91,65,169,243,83,195,168,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,0,0,128,82,181,255,255,151,8,28,
      0,83,31,1,0,113,224,7,159,26,253,123,193,168,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,124,0,0,148,96,0,0,52,118,0,0,148,7,0,0,20,118,0,0,148,81,6,0,148,96,0,0,52,0,0,128,82,3,0,0,20,229,7,0,148,32,0,128,82,253,123,193,168,192,
      3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,108,0,0,148,160,0,0,52,168,0,0,240,0,129,46,145,100,7,0,148,4,0,0,20,84,5,0,148,64,0,0,53,90,5,0,148,253,123,193,168,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,0,0,128,82,11,
      5,0,148,180,3,0,148,253,123,193,168,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,180,3,0,148,8,28,0,83,104,0,0,53,0,0,128,82,7,0,0,20,7,5,0,148,8,28,0,83,104,0,0,53,180,3,0,148,250,255,255,23,32,0,128,82,253,123,193,168,192,3,95,
      214,204,204,204,204,253,123,191,169,253,3,0,145,4,5,0,148,171,3,0,148,32,0,128,82,253,123,193,168,192,3,95,214,204,204,204,204,168,0,0,240,0,97,47,145,192,3,95,214,204,204,204,204,96,0,62,212,192,3,95,214,243,83,190,169,245,91,1,169,253,123,
      191,169,253,3,0,145,168,0,0,176,8,97,47,145,169,0,0,176,53,129,47,145,19,33,0,145,8,33,0,145,31,1,21,235,66,1,0,84,214,0,0,208,116,134,64,248,180,0,0,180,200,2,64,249,239,3,20,170,0,1,63,214,128,2,63,214,127,2,21,235,35,255,255,84,253,123,193,
      168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,243,83,190,169,245,91,1,169,253,123,191,169,253,3,0,145,168,0,0,176,8,161,47,145,169,0,0,176,53,193,47,145,19,33,0,145,8,33,0,145,31,1,21,235,66,1,0,84,214,0,0,208,116,134,64,248,
      180,0,0,180,200,2,64,249,239,3,20,170,0,1,63,214,128,2,63,214,127,2,21,235,35,255,255,84,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,192,3,95,214,204,204,204,204,0,0,128,82,192,3,95,214,32,0,128,82,192,3,95,214,
      168,0,0,240,8,233,75,185,31,1,0,113,224,7,159,26,192,3,95,214,204,204,204,204,243,83,189,169,245,91,1,169,247,19,0,249,253,123,188,169,253,3,0,145,247,3,0,170,245,3,1,170,54,5,0,88,181,1,0,180,168,2,64,185,104,1,32,54,244,2,64,249,200,0,0,208,
      8,1,64,249,137,130,95,248,51,33,64,249,53,25,64,249,239,3,19,170,0,1,63,214,128,34,0,209,96,2,63,214,168,0,0,176,8,193,22,145,9,253,223,200,225,67,0,145,224,3,21,170,32,1,63,214,224,11,0,249,181,0,0,180,168,2,64,185,72,0,24,55,64,0,0,181,86,
      2,0,88,227,99,0,145,245,131,2,169,130,0,128,82,246,223,1,169,33,0,128,82,192,1,0,24,168,0,0,176,8,97,22,145,9,253,223,200,32,1,63,214,253,123,196,168,247,19,64,249,245,91,65,169,243,83,195,168,192,3,95,214,32,5,147,25,0,0,0,0,0,64,153,1,0,0,
      0,0,99,115,109,224,204,204,204,204,243,83,187,169,245,91,1,169,247,99,2,169,249,107,3,169,251,35,0,249,253,123,189,169,253,3,0,145,247,3,3,170,232,102,64,169,235,3,1,170,245,30,64,249,248,3,0,170,235,11,0,249,10,1,25,203,168,2,64,185,104,0,248,
      54,9,121,64,146,53,1,25,139,233,50,65,57,72,17,0,209,244,74,64,185,63,1,0,113,90,1,136,154,8,7,64,185,201,12,128,82,31,1,9,106,193,8,0,84,248,139,1,169,168,2,64,185,159,2,8,107,66,16,0,84,168,0,0,176,27,225,22,145,115,0,0,208,182,82,52,139,200,
      6,64,185,95,67,40,235,131,6,0,84,200,10,64,185,95,67,40,235,34,6,0,84,200,18,64,185,232,5,0,52,200,14,64,185,31,5,0,113,32,1,0,84,227,42,64,249,34,67,40,139,225,3,11,170,224,99,0,145,52,237,255,151,96,5,248,55,31,0,0,113,109,4,0,84,8,3,64,185,
      41,14,0,24,31,1,9,107,193,1,0,84,104,134,65,249,136,1,0,180,96,34,12,145,35,12,0,148,32,1,0,52,115,134,65,249,200,0,0,208,8,1,64,249,239,3,19,170,0,1,63,214,33,0,128,82,224,3,24,170,96,2,63,214,200,18,64,185,34,0,128,82,243,11,64,249,32,67,40,
      139,225,3,19,170,18,237,255,151,200,18,64,185,226,3,24,170,3,3,64,185,224,3,19,170,229,34,64,249,228,22,64,249,33,67,40,139,104,255,223,200,0,1,63,214,15,237,255,151,115,0,0,208,235,11,64,249,168,2,64,185,148,6,0,17,159,2,8,107,130,8,0,84,197,
      255,255,23,0,0,128,82,66,0,0,20,232,18,64,249,172,2,64,185,19,1,25,203,159,2,12,107,130,7,0,84,234,3,12,42,174,82,52,139,233,3,10,42,200,5,64,185,95,67,40,235,35,6,0,84,200,9,64,185,95,67,40,235,194,5,0,84,8,7,64,185,6,1,27,114,96,3,0,84,11,
      0,128,82,170,2,0,52,175,82,52,139,167,82,52,139,173,82,43,139,168,5,64,185,127,66,40,235,131,1,0,84,168,9,64,185,127,66,40,235,34,1,0,84,233,17,64,185,168,17,64,185,31,1,9,107,161,0,0,84,233,12,64,185,168,13,64,185,31,1,9,107,128,0,0,84,107,
      5,0,17,127,1,10,107,227,253,255,84,233,3,12,42,127,1,12,107,225,2,0,84,235,11,64,249,200,17,64,185,168,0,0,52,127,66,40,235,196,8,64,122,33,2,0,84,11,0,0,20,136,6,0,17,227,42,64,249,232,74,0,185,201,13,64,185,225,3,11,170,32,0,128,82,34,67,41,
      139,227,236,255,151,172,2,64,185,233,3,12,42,148,6,0,17,235,11,64,249,159,2,9,107,234,3,9,42,227,248,255,84,32,0,128,82,253,123,195,168,251,35,64,249,249,107,67,169,247,99,66,169,245,91,65,169,243,83,197,168,192,3,95,214,99,115,109,224,204,204,
      204,204,243,83,190,169,245,91,1,169,253,123,191,169,253,3,0,145,245,3,0,170,179,14,64,185,244,3,1,42,243,1,0,52,150,2,128,210,115,6,0,81,2,12,0,148,169,18,64,185,8,48,64,249,106,34,22,155,64,193,41,139,8,4,64,185,159,2,8,107,141,0,0,84,8,8,64,
      185,159,2,8,107,109,0,0,84,147,254,255,53,0,0,128,210,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,243,3,1,170,225,3,0,170,224,3,19,170,94,12,0,148,225,3,0,42,224,3,19,170,
      219,255,255,151,31,0,0,241,224,7,159,26,253,123,193,168,243,7,65,248,192,3,95,214,204,204,204,204,243,83,190,169,245,11,0,249,253,123,190,169,253,3,0,145,227,67,0,145,244,3,1,170,243,3,2,170,21,0,0,148,245,3,0,170,224,3,19,170,225,3,20,170,73,
      12,0,148,225,3,0,42,224,3,19,170,198,255,255,151,96,0,0,181,3,0,128,18,2,0,0,20,3,4,64,185,226,3,19,170,225,3,20,170,224,3,21,170,140,12,0,148,253,123,194,168,245,11,64,249,243,83,194,168,192,3,95,214,204,204,204,204,243,83,188,169,245,91,1,
      169,247,99,2,169,249,107,3,169,253,123,190,169,253,3,0,145,247,3,2,170,243,3,0,170,245,14,64,185,224,3,23,170,248,3,1,170,244,3,3,170,44,12,0,148,104,2,64,249,246,3,0,42,136,2,0,249,85,5,0,52,168,0,0,144,25,161,22,145,154,2,128,210,181,6,0,81,
      233,18,64,185,170,126,26,155,8,7,64,249,75,193,41,139,115,1,8,139,105,6,64,185,223,2,9,107,45,3,0,84,104,10,64,185,223,2,8,107,204,2,0,84,0,3,64,249,2,0,128,210,40,255,223,200,225,67,0,145,0,1,63,214,108,166,65,41,11,0,128,82,232,11,64,249,13,
      0,64,185,14,193,41,139,44,1,0,52,104,125,64,211,9,57,26,155,42,13,128,185,95,65,45,235,0,1,0,84,107,5,0,17,127,1,12,107,35,255,255,84,127,1,12,107,99,0,0,84,181,0,0,52,222,255,255,23,136,2,64,249,9,1,64,249,137,2,0,249,224,3,20,170,253,123,194,
      168,249,107,67,169,247,99,66,169,245,91,65,169,243,83,196,168,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,92,236,255,151,255,67,2,209,104,0,0,176,10,193,9,145,237,95,64,249,76,65,2,145,235,3,0,145,72,37,193,168,104,37,129,168,95,
      1,12,235,161,255,255,84,0,0,64,249,9,0,0,240,41,65,20,145,72,1,64,249,104,1,0,249,200,124,64,147,228,35,3,169,164,21,64,249,232,3,67,57,233,15,2,169,3,0,128,210,73,2,0,88,229,135,4,169,165,33,64,249,161,1,64,249,226,35,0,249,226,3,0,145,232,
      167,5,169,8,0,128,146,232,59,0,249,168,0,0,144,8,225,22,145,9,253,223,200,32,1,63,214,255,67,2,145,64,236,255,151,253,123,193,168,192,3,95,214,31,32,3,213,32,5,147,25,0,0,0,0,243,83,189,169,245,91,1,169,247,99,2,169,253,123,189,169,253,3,0,145,
      243,3,4,170,244,3,3,170,117,14,64,185,247,3,0,170,248,3,1,170,225,3,20,170,224,3,19,170,246,3,2,42,189,11,0,148,53,9,0,52,143,6,64,249,14,0,128,18,103,18,64,185,6,0,128,18,235,3,21,42,133,2,128,210,109,5,0,81,169,125,5,155,42,193,39,139,76,1,
      15,139,136,5,64,185,31,0,8,107,141,0,0,84,136,9,64,185,31,0,8,107,173,0,0,84,235,3,13,42,232,3,11,42,139,254,255,53,200,0,0,52,104,5,0,81,10,125,5,155,75,193,39,139,109,1,15,139,2,0,0,20,13,0,128,210,11,0,128,82,104,125,64,211,9,125,5,155,42,
      193,39,139,76,1,15,139,45,1,0,180,169,5,64,185,136,1,64,185,31,1,9,107,205,1,0,84,169,9,64,185,136,5,64,185,31,1,9,107,76,1,0,84,136,1,64,185,223,2,8,107,235,0,0,84,136,5,64,185,223,2,8,107,140,0,0,84,223,5,0,49,206,17,139,26,230,3,11,42,107,
      5,0,17,127,1,21,107,3,253,255,84,223,5,0,49,248,11,0,249,200,17,159,26,248,19,0,249,223,5,0,49,232,27,0,185,232,7,134,26,233,67,0,145,232,43,0,185,40,37,64,169,234,131,0,145,224,3,23,170,232,38,0,169,72,37,64,169,232,38,1,169,253,123,195,168,
      247,99,66,169,245,91,65,169,243,83,195,168,192,3,95,214,195,10,0,148,253,123,191,169,253,3,0,145,252,10,0,148,0,48,64,249,253,123,193,168,192,3,95,214,243,15,31,248,253,123,191,169,253,3,0,145,243,3,0,170,244,10,0,148,19,48,0,249,253,123,193,
      168,243,7,65,248,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,236,10,0,148,0,52,64,249,253,123,193,168,192,3,95,214,243,15,31,248,253,123,191,169,253,3,0,145,243,3,0,170,228,10,0,148,19,52,0,249,253,123,193,168,243,7,65,248,192,3,
      95,214,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,243,3,0,170,97,2,0,249,217,10,0,148,8,44,64,249,127,2,8,235,130,0,0,84,213,10,0,148,8,44,64,249,2,0,0,20,8,0,128,210,104,6,0,249,208,10,0,148,19,44,0,249,224,3,19,170,253,123,193,
      168,243,7,65,248,192,3,95,214,243,83,191,169,253,123,191,169,253,3,0,145,244,3,0,170,198,10,0,148,8,44,64,249,159,2,8,235,225,1,0,84,194,10,0,148,8,44,64,249,136,1,0,180,19,5,64,249,159,2,8,235,128,0,0,84,232,3,19,170,232,0,0,180,251,255,255,
      23,185,10,0,148,19,44,0,249,253,123,193,168,243,83,193,168,192,3,95,214,120,10,0,148,204,204,204,204,243,83,190,169,245,91,1,169,253,123,190,169,253,3,0,145,244,3,3,170,225,11,0,249,147,6,64,249,246,3,0,170,245,3,2,170,169,10,0,148,19,48,0,249,
      211,30,64,249,166,10,0,148,19,52,0,249,164,10,0,148,7,0,128,82,6,0,128,210,136,30,64,249,5,0,128,82,9,48,64,249,227,3,20,170,226,3,21,170,225,67,0,145,10,1,64,185,224,3,22,170,36,65,42,139,16,13,0,148,253,123,194,168,245,91,65,169,243,83,194,
      168,192,3,95,214,204,204,204,204,243,83,191,169,253,123,186,169,253,3,0,145,244,3,0,170,180,19,0,249,161,35,0,249,162,31,0,249,163,27,0,249,164,23,0,249,191,19,0,185,180,39,0,249,162,43,0,249,134,10,0,148,19,8,64,249,239,3,19,170,200,0,0,176,
      8,1,64,249,0,1,63,214,161,35,1,145,128,2,64,185,96,2,63,214,177,67,0,145,63,254,159,136,1,0,0,20,160,19,64,185,253,123,198,168,243,83,193,168,192,3,95,214,204,204,204,204,204,204,204,204,243,83,191,169,253,123,189,169,253,3,0,145,161,67,0,57,
      128,4,0,180,233,4,0,24,8,0,64,185,31,1,9,107,1,4,0,84,8,24,64,185,31,17,0,113,161,3,0,84,8,32,64,185,9,4,0,24,8,1,9,75,31,9,0,113,8,3,0,84,8,24,64,249,200,2,0,180,9,5,64,185,233,0,0,52,8,28,64,249,1,193,41,139,0,20,64,249,24,0,0,148,15,0,0,20,
      14,0,0,20,8,1,64,185,136,1,32,54,9,20,64,249,52,1,64,249,52,1,0,180,136,2,64,249,19,9,64,249,239,3,19,170,200,0,0,176,8,1,64,249,0,1,63,214,224,3,20,170,96,2,63,214,253,123,195,168,243,83,193,168,192,3,95,214,31,32,3,213,99,115,109,224,32,5,
      147,25,66,7,0,20,204,204,204,204,32,0,31,214,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,243,3,0,170,62,10,0,148,9,44,64,249,201,0,0,180,40,1,64,249,31,1,19,235,224,0,0,84,41,5,64,249,137,255,255,181,32,0,128,82,253,123,193,168,
      243,7,65,248,192,3,95,214,0,0,128,82,252,255,255,23,40,44,64,41,234,3,0,170,64,193,40,139,203,0,248,55,41,8,64,185,72,201,107,248,10,201,169,184,73,193,43,139,32,1,0,139,192,3,95,214,243,83,191,169,253,123,191,169,253,3,0,145,243,3,0,170,116,
      2,64,249,169,3,0,24,136,2,64,185,31,1,9,107,0,1,0,84,73,3,0,24,31,1,9,107,160,0,0,84,9,3,0,24,31,1,9,107,192,1,0,84,9,0,0,20,22,10,0,148,8,48,64,185,31,1,0,113,173,0,0,84,18,10,0,148,8,48,64,185,9,5,0,81,9,48,0,185,0,0,128,82,253,123,193,168,
      243,83,193,168,192,3,95,214,10,10,0,148,20,16,0,249,115,6,64,249,7,10,0,148,19,20,0,249,1,7,0,148,82,67,67,224,77,79,67,224,99,115,109,224,204,204,204,204,243,83,190,169,245,91,1,169,253,123,191,169,253,3,0,145,244,3,0,170,136,34,64,57,245,3,
      1,170,8,3,0,52,137,2,64,249,201,2,0,180,40,1,192,57,234,3,9,170,104,0,0,52,72,29,192,56,232,255,255,53,86,1,9,203,192,6,0,145,233,6,0,148,243,3,0,170,51,1,0,180,130,2,64,249,193,6,0,145,62,16,0,148,232,3,19,170,41,0,128,82,168,2,0,249,169,34,
      0,57,19,0,128,210,224,3,19,170,215,6,0,148,4,0,0,20,136,2,64,249,191,34,0,57,168,2,0,249,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,243,15,31,248,253,123,191,169,253,3,0,145,243,3,0,170,104,34,64,57,104,0,0,52,96,2,64,249,199,
      6,0,148,127,34,0,57,127,2,0,249,253,123,193,168,243,7,65,248,192,3,95,214,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,168,0,0,144,8,1,21,145,9,253,223,200,32,1,63,214,160,0,0,180,19,0,64,249,183,6,0,148,224,3,19,170,179,255,255,
      181,253,123,193,168,243,7,65,248,192,3,95,214,204,204,204,204,31,0,1,235,97,0,0,84,0,0,128,82,4,0,0,20,33,36,0,145,0,36,0,145,250,15,0,20,192,3,95,214,253,123,191,169,253,3,0,145,38,16,0,148,8,28,0,83,104,0,0,53,0,0,128,82,7,0,0,20,125,9,0,148,
      8,28,0,83,104,0,0,53,52,16,0,148,250,255,255,23,32,0,128,82,253,123,193,168,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,8,28,0,83,104,0,0,53,142,9,0,148,41,16,0,148,32,0,128,82,253,123,193,168,192,3,95,214,204,204,204,204,253,123,
      191,169,253,3,0,145,134,9,0,148,32,0,128,82,253,123,193,168,192,3,95,214,253,123,191,169,253,3,0,145,154,9,0,148,31,0,0,241,224,7,159,26,253,123,193,168,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,0,0,128,210,215,9,0,148,32,0,128,
      82,253,123,193,168,192,3,95,214,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,9,0,0,148,243,3,0,170,211,0,0,180,200,0,0,176,8,1,64,249,239,3,19,170,0,1,63,214,96,2,63,214,61,9,0,148,168,0,0,208,0,69,71,249,191,59,3,213,192,3,95,214,
      40,60,0,83,15,0,128,210,8,6,0,53,8,0,64,121,168,20,0,52,8,236,124,146,16,1,192,61,12,65,0,145,10,12,0,18,13,0,128,146,16,154,96,78,17,10,96,78,43,62,24,78,170,2,24,55,41,62,8,78,8,12,29,83,168,37,200,154,42,1,8,234,96,0,0,84,73,17,192,218,3,
      0,0,20,104,17,192,218,9,1,1,17,233,2,56,54,144,5,193,60,17,154,96,78,48,58,176,110,8,62,8,78,136,255,255,180,49,10,96,78,40,62,8,78,136,1,0,180,9,17,192,218,13,0,0,20,72,33,0,81,9,113,29,83,169,37,201,154,40,1,11,234,64,254,255,84,8,17,192,218,
      10,1,1,17,137,13,74,139,6,0,0,20,40,62,24,78,9,17,192,218,41,1,1,17,40,125,64,211,137,13,72,139,32,65,0,209,120,0,0,20,22,13,2,78,13,236,124,146,176,1,192,61,9,12,0,18,6,14,128,82,10,0,128,146,18,154,96,78,17,142,118,110,82,10,96,78,51,10,96,
      78,71,62,24,78,108,62,24,78,137,1,24,55,8,12,29,83,73,62,8,78,74,37,200,154,104,62,8,78,46,1,10,234,11,1,10,138,97,0,0,84,171,1,0,181,10,0,0,20,202,17,192,218,12,0,0,20,40,33,0,81,9,113,29,83,74,37,201,154,76,1,12,138,71,1,7,138,11,0,128,210,
      14,0,128,210,71,0,0,181,204,2,0,180,232,16,192,218,10,1,1,17,107,0,0,180,105,17,192,218,3,0,0,20,136,17,192,218,9,1,1,17,95,1,9,107,98,0,0,84,0,0,128,210,76,0,0,20,44,62,8,78,43,62,24,78,106,6,56,54,107,0,0,180,105,17,192,218,3,0,0,20,136,17,
      192,218,9,1,1,17,200,0,9,75,175,13,72,139,176,13,193,60,19,142,118,110,20,154,96,78,112,30,180,78,17,58,176,110,40,62,8,78,72,255,255,180,114,10,96,78,149,10,96,78,75,62,8,78,174,62,8,78,76,62,24,78,110,0,0,180,202,17,192,218,4,0,0,20,168,62,
      24,78,9,17,192,218,42,1,1,17,107,0,0,180,105,17,192,218,3,0,0,20,136,17,192,218,9,1,1,17,95,1,9,107,35,2,0,84,108,62,8,78,107,62,24,78,202,251,63,55,41,0,128,210,14,2,0,181,72,1,1,81,40,33,200,154,9,5,0,209,42,1,11,234,96,0,0,84,73,17,192,218,
      3,0,0,20,136,17,192,218,9,1,1,17,201,0,9,75,23,0,0,20,224,3,15,170,23,0,0,20,41,0,128,210,14,1,0,180,40,33,202,154,9,5,0,209,42,1,12,138,75,17,192,218,9,6,128,82,41,1,11,75,12,0,0,20,72,1,1,81,40,33,200,154,9,5,0,209,42,1,11,234,96,0,0,84,73,
      17,192,218,3,0,0,20,136,17,192,218,9,1,1,17,8,14,128,82,9,1,9,75,40,125,64,211,160,13,72,139,192,3,95,214,204,204,204,204,204,204,204,204,168,0,0,208,8,193,21,145,169,0,0,240,40,117,3,249,32,0,128,82,192,3,95,214,204,204,204,204,204,204,204,
      204,253,123,191,169,253,3,0,145,168,0,0,208,0,97,52,145,184,2,0,148,168,0,0,208,0,193,52,145,181,2,0,148,32,0,128,82,253,123,193,168,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,52,3,0,148,32,0,128,82,253,123,193,168,192,3,95,214,
      204,204,204,204,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,168,0,0,208,19,5,64,249,224,3,19,170,232,22,0,148,224,3,19,170,138,0,0,148,224,3,19,170,78,23,0,148,224,3,19,170,38,24,0,148,224,3,19,170,166,0,0,148,32,0,128,82,253,123,
      193,168,243,7,65,248,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,0,0,128,82,241,254,255,151,0,28,0,83,253,123,193,168,192,3,95,214,204,204,204,204,243,83,191,169,253,123,191,169,253,3,0,145,180,0,0,240,138,86,67,249,72,253,95,136,
      8,5,0,81,72,253,9,136,169,255,255,53,191,59,3,213,8,1,0,53,128,86,67,249,168,0,0,208,19,193,0,145,31,0,19,235,96,0,0,84,92,22,0,148,147,86,3,249,32,0,128,82,253,123,193,168,243,83,193,168,192,3,95,214,204,204,204,204,204,204,204,204,243,15,31,
      248,253,123,191,169,253,3,0,145,179,0,0,240,96,130,67,249,79,22,0,148,127,130,3,249,179,0,0,240,96,134,67,249,75,22,0,148,127,134,3,249,179,0,0,240,96,70,67,249,71,22,0,148,127,70,3,249,179,0,0,240,96,74,67,249,67,22,0,148,127,74,3,249,32,0,
      128,82,253,123,193,168,243,7,65,248,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,104,0,0,176,0,65,12,145,1,0,4,145,79,22,0,148,0,28,0,83,253,123,193,168,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,8,28,0,83,232,0,0,52,
      168,0,0,240,8,125,67,249,72,0,0,180,217,23,0,148,32,0,128,82,6,0,0,20,104,0,0,176,0,65,12,145,1,0,4,145,109,22,0,148,0,28,0,83,253,123,193,168,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,0,28,0,83,197,16,0,148,32,0,128,82,253,123,
      193,168,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,54,17,0,148,31,0,0,241,224,7,159,26,253,123,193,168,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,104,17,0,148,32,0,128,82,253,123,193,168,192,3,95,214,243,83,191,169,
      253,123,191,169,253,3,0,145,244,3,0,170,20,0,0,148,243,3,0,170,83,1,0,180,200,0,0,176,8,1,64,249,239,3,19,170,0,1,63,214,224,3,20,170,96,2,63,214,96,0,0,52,32,0,128,82,2,0,0,20,0,0,128,82,253,123,193,168,243,83,193,168,192,3,95,214,168,0,0,208,
      0,253,5,249,192,3,95,214,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,0,0,128,82,94,16,0,148,168,0,0,208,9,5,64,249,43,21,0,18,170,0,0,208,72,253,69,249,42,1,8,202,83,45,203,154,0,0,128,82,95,16,0,148,224,3,19,170,253,123,193,168,
      243,7,65,248,192,3,95,214,168,0,0,208,0,9,76,185,191,59,3,213,192,3,95,214,2,0,128,82,65,0,128,82,90,0,0,20,204,204,204,204,34,0,128,82,1,0,128,82,0,0,128,82,85,0,0,20,168,0,0,208,0,1,6,249,192,3,95,214,204,204,204,204,243,83,191,169,253,123,
      190,169,253,3,0,145,104,0,0,176,1,65,16,145,48,0,128,146,169,0,0,144,40,225,18,145,191,67,1,169,9,253,223,200,244,3,0,42,162,67,0,145,0,0,128,82,32,1,63,214,0,2,0,52,104,0,0,176,1,161,16,145,160,11,64,249,168,0,0,144,8,65,19,145,9,253,223,200,
      32,1,63,214,243,3,0,170,243,0,0,180,200,0,0,144,8,1,64,249,239,3,19,170,0,1,63,214,224,3,20,42,96,2,63,214,160,11,64,249,160,0,0,180,136,0,0,240,8,225,16,145,9,253,223,200,32,1,63,214,253,123,194,168,243,83,193,168,192,3,95,214,204,204,204,204,
      253,123,191,169,253,3,0,145,176,24,0,148,31,4,0,113,0,1,0,84,232,3,18,170,8,49,64,249,9,189,64,185,42,33,8,83,106,0,0,53,32,0,128,82,2,0,0,20,0,0,128,82,253,123,193,168,192,3,95,214,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,243,
      3,0,42,236,255,255,151,8,28,0,83,72,1,0,52,136,0,0,240,8,193,17,145,9,253,223,200,32,1,63,214,136,0,0,240,8,129,23,145,9,253,223,200,225,3,19,42,32,1,63,214,224,3,19,42,183,255,255,151,136,0,0,240,8,161,15,145,9,253,223,200,224,3,19,42,32,1,
      63,214,204,204,204,204,243,15,31,248,253,123,187,169,253,3,0,145,48,0,128,146,176,35,0,249,243,3,0,42,162,135,2,41,98,3,0,53,136,0,0,240,8,1,19,145,9,253,223,200,0,0,128,210,32,1,63,214,160,2,0,180,8,0,64,121,169,73,139,82,31,1,9,107,33,2,0,
      84,8,60,64,185,11,170,136,82,10,192,40,139,73,1,64,185,63,1,11,107,97,1,0,84,72,49,64,121,31,45,8,113,1,1,0,84,72,133,64,185,31,57,0,113,169,0,0,84,72,249,64,185,104,0,0,52,224,3,19,42,143,255,255,151,168,99,0,145,191,67,0,57,168,23,0,249,169,
      83,0,145,168,67,0,145,169,35,3,169,72,0,128,82,73,0,128,82,168,167,3,41,163,115,0,145,162,163,0,145,161,131,0,145,160,71,0,145,77,0,0,148,31,32,3,213,168,23,64,185,136,0,0,52,253,123,197,168,243,7,65,248,192,3,95,214,224,3,19,42,177,255,255,
      151,243,83,190,169,245,11,0,249,253,123,189,169,253,3,0,145,244,3,0,170,181,0,0,176,168,50,112,57,8,7,0,53,41,0,128,82,168,0,0,176,8,33,48,145,9,253,159,136,191,59,3,213,136,2,64,249,9,1,64,185,201,2,0,53,168,0,0,176,10,5,64,249,75,21,0,18,170,
      15,0,249,168,0,0,176,8,1,70,249,31,1,10,235,96,1,0,84,73,1,8,202,51,45,203,154,239,3,19,170,200,0,0,144,8,1,64,249,0,1,63,214,2,0,128,210,1,0,128,82,0,0,128,210,96,2,63,214,168,0,0,176,0,97,52,145,5,0,0,20,63,5,0,113,129,0,0,84,168,0,0,176,0,
      193,52,145,77,1,0,148,136,2,64,249,9,1,64,185,201,0,0,53,136,0,0,208,1,65,58,145,137,0,0,208,32,193,57,145,131,2,0,148,136,0,0,208,1,129,58,145,137,0,0,208,32,97,58,145,126,2,0,148,136,6,64,249,9,1,64,185,201,0,0,53,40,0,128,82,168,50,48,57,
      136,10,64,249,41,0,128,82,9,1,0,57,253,123,195,168,245,11,64,249,243,83,194,168,192,3,95,214,37,4,0,148,243,83,191,169,253,123,190,169,253,3,0,145,244,3,2,170,243,3,3,170,179,11,0,249,32,0,64,185,109,15,0,148,224,3,20,170,179,255,255,151,96,
      2,64,185,115,15,0,148,253,123,194,168,243,83,193,168,192,3,95,214,204,204,204,204,243,83,190,169,245,91,1,169,253,123,189,169,253,3,0,145,245,3,0,42,117,11,0,52,168,6,0,81,31,5,0,113,233,0,0,84,7,24,0,148,200,2,128,82,8,0,0,185,72,21,0,148,211,
      2,128,82,83,0,0,20,85,18,0,148,180,0,0,176,129,66,48,145,130,32,128,82,0,0,128,210,68,27,0,148,136,66,48,145,169,0,0,208,40,61,3,249,169,0,0,208,51,53,67,249,115,0,0,180,104,2,192,57,72,0,0,53,147,66,48,145,228,131,0,145,255,255,1,169,227,99,
      0,145,2,0,128,210,1,0,128,210,224,3,19,170,94,0,0,148,246,135,65,169,34,0,128,210,224,3,22,170,62,0,0,148,244,3,0,170,20,1,0,181,229,23,0,148,136,1,128,82,8,0,0,185,0,0,128,210,197,20,0,148,147,1,128,82,48,0,0,20,130,14,22,139,228,131,0,145,
      227,99,0,145,225,3,20,170,224,3,19,170,75,0,0,148,191,6,0,113,33,1,0,84,232,15,64,249,171,0,0,208,116,69,3,249,0,0,128,210,8,5,0,81,170,0,0,208,72,129,6,185,30,0,0,20,225,67,0,145,255,11,0,249,224,3,20,170,75,24,0,148,243,3,0,42,243,0,0,52,224,
      11,64,249,171,20,0,148,224,3,20,170,255,11,0,249,168,20,0,148,20,0,0,20,234,11,64,249,9,0,128,210,72,1,64,249,235,3,10,170,136,0,0,180,104,141,64,248,41,5,0,145,200,255,255,181,171,0,0,208,105,129,6,185,255,11,0,249,168,0,0,208,10,69,3,249,0,
      0,128,210,152,20,0,148,255,11,0,249,224,3,20,170,149,20,0,148,19,0,128,82,224,3,19,42,253,123,195,168,245,91,65,169,243,83,194,168,192,3,95,214,243,15,31,248,253,123,191,169,253,3,0,145,8,0,252,146,31,0,8,235,98,2,0,84,8,0,128,146,66,0,0,181,
      128,0,62,212,8,9,194,154,63,0,8,235,162,1,0,84,41,124,2,155,232,15,32,170,31,1,9,235,41,1,0,84,32,13,0,139,33,0,128,210,236,23,0,148,243,3,0,170,0,0,128,210,121,20,0,148,224,3,19,170,2,0,0,20,0,0,128,210,253,123,193,168,243,7,65,248,192,3,95,
      214,243,83,188,169,245,91,1,169,247,99,2,169,249,27,0,249,253,123,191,169,253,3,0,145,248,3,4,170,249,3,3,170,31,3,0,249,40,0,128,210,40,3,0,249,244,3,0,170,246,3,1,170,243,3,2,170,118,0,0,180,211,2,0,249,214,34,0,145,23,0,128,82,136,22,192,
      56,31,137,0,113,161,0,0,84,255,2,0,113,247,23,159,26,85,4,128,82,21,0,0,20,8,3,64,249,9,5,0,145,9,3,0,249,147,0,0,180,136,242,223,56,104,2,0,57,115,6,0,145,149,242,223,56,224,3,21,42,66,27,0,148,32,1,0,52,8,3,64,249,9,5,0,145,9,3,0,249,147,0,
      0,180,136,2,192,57,104,2,0,57,115,6,0,145,148,6,0,145,21,1,0,52,183,252,255,53,191,130,0,113,164,26,73,122,65,252,255,84,147,0,0,180,127,242,31,56,2,0,0,20,148,6,0,209,21,0,128,82,136,2,192,57,40,1,0,52,31,129,0,113,96,0,0,84,31,37,0,113,161,
      0,0,84,136,6,0,145,244,3,8,170,8,1,192,57,249,255,255,23,232,8,0,52,118,0,0,180,211,2,0,249,214,34,0,145,40,3,64,249,151,11,128,82,9,5,0,145,41,3,0,249,136,2,192,57,43,0,128,82,10,0,128,82,3,0,0,20,136,30,192,56,74,5,0,17,31,113,1,113,160,255,
      255,84,31,137,0,113,129,1,0,84,74,1,0,55,213,0,0,52,136,6,192,57,31,137,0,113,97,0,0,84,148,6,0,145,4,0,0,20,191,2,0,113,245,23,159,26,11,0,128,82,74,125,1,83,42,1,0,52,74,5,0,81,115,0,0,180,119,2,0,57,115,6,0,145,8,3,64,249,9,5,0,145,9,3,0,
      249,42,255,255,53,128,2,192,57,32,3,0,52,149,0,0,53,31,128,0,113,4,24,73,122,160,2,0,84,75,2,0,52,147,0,0,180,96,2,0,57,128,2,192,57,115,6,0,145,243,26,0,148,32,1,0,52,8,3,64,249,148,6,0,145,9,5,0,145,9,3,0,249,147,0,0,180,136,2,192,57,104,2,
      0,57,115,6,0,145,8,3,64,249,9,5,0,145,9,3,0,249,148,6,0,145,201,255,255,23,115,0,0,180,127,2,0,57,115,6,0,145,8,3,64,249,9,5,0,145,9,3,0,249,176,255,255,23,86,0,0,180,223,2,0,249,40,3,64,249,9,5,0,145,41,3,0,249,253,123,193,168,249,27,64,249,
      247,99,66,169,245,91,65,169,243,83,196,168,192,3,95,214,96,0,0,181,0,0,128,18,10,0,0,20,9,8,64,249,8,0,64,249,31,1,9,235,161,0,0,84,168,0,0,176,8,5,64,249,8,32,0,169,8,8,0,249,0,0,128,82,192,3,95,214,204,204,204,204,253,123,188,169,253,3,0,145,
      48,0,128,146,168,131,0,145,168,195,2,169,160,19,0,249,72,0,128,82,73,0,128,82,168,167,2,41,163,83,0,145,162,163,0,145,161,99,0,145,160,67,0,145,87,0,0,148,31,32,3,213,253,123,196,168,192,3,95,214,204,204,204,204,243,83,187,169,245,91,1,169,247,
      99,2,169,249,107,3,169,251,35,0,249,253,123,191,169,253,3,0,145,246,3,0,170,200,2,64,249,11,1,64,249,107,0,0,181,0,0,128,18,63,0,0,20,186,0,0,176,76,7,64,249,105,1,64,249,42,1,12,202,105,5,64,249,142,21,0,18,85,45,206,154,42,1,12,202,169,6,0,
      209,63,13,0,177,84,45,206,154,72,6,0,84,249,3,12,170,247,3,21,170,248,3,20,170,219,0,0,144,148,34,0,209,159,2,21,235,163,3,0,84,136,2,64,249,31,1,25,235,96,255,255,84,9,1,12,202,153,2,0,249,104,3,64,249,51,45,206,154,239,3,19,170,0,1,63,214,
      96,2,63,214,201,2,64,249,76,7,64,249,43,1,64,249,142,21,0,18,104,1,64,249,10,1,12,202,104,5,64,249,77,45,206,154,191,1,23,235,10,1,12,202,75,45,206,154,96,1,88,250,224,252,255,84,247,3,13,170,245,3,13,170,248,3,11,170,244,3,11,170,226,255,255,
      23,191,6,0,177,128,0,0,84,224,3,21,170,129,19,0,148,76,7,64,249,200,2,64,249,9,1,64,249,44,1,0,249,202,2,64,249,72,1,64,249,12,5,0,249,201,2,64,249,42,1,64,249,76,9,0,249,0,0,128,82,253,123,193,168,251,35,64,249,249,107,67,169,247,99,66,169,
      245,91,65,169,243,83,197,168,192,3,95,214,243,83,191,169,253,123,190,169,253,3,0,145,243,3,2,170,244,3,3,170,180,11,0,249,32,0,64,185,215,13,0,148,224,3,19,170,165,255,255,151,243,3,0,42,128,2,64,185,220,13,0,148,224,3,19,42,253,123,194,168,
      243,83,193,168,192,3,95,214,204,204,204,204,42,0,0,20,204,204,204,204,243,83,191,169,253,123,190,169,253,3,0,145,48,0,128,146,176,11,0,249,168,0,0,176,19,33,53,145,224,3,19,170,66,0,0,148,31,32,3,213,96,34,0,145,85,0,0,148,31,32,3,213,168,0,
      0,176,19,177,70,249,83,1,0,180,104,2,64,249,244,3,19,170,168,0,0,180,224,3,8,170,70,19,0,148,136,142,64,248,168,255,255,181,224,3,19,170,66,19,0,148,168,0,0,176,19,173,70,249,83,1,0,180,104,2,64,249,244,3,19,170,168,0,0,180,224,3,8,170,58,19,
      0,148,136,142,64,248,168,255,255,181,224,3,19,170,54,19,0,148,253,123,194,168,243,83,193,168,192,3,95,214,243,83,191,169,253,123,191,169,253,3,0,145,180,0,0,176,136,166,70,249,104,0,0,180,0,0,128,82,23,0,0,20,154,16,0,148,189,26,0,148,243,3,
      0,170,179,0,0,181,0,0,128,210,37,19,0,148,0,0,128,18,15,0,0,20,224,3,19,170,61,0,0,148,96,0,0,181,20,0,128,18,5,0,0,20,168,0,0,176,0,177,6,249,128,166,6,249,20,0,128,82,0,0,128,210,24,19,0,148,224,3,19,170,22,19,0,148,224,3,20,42,253,123,193,
      168,243,83,193,168,192,3,95,214,204,204,204,204,243,83,191,169,253,123,191,169,253,3,0,145,168,0,0,176,8,177,70,249,19,0,64,249,127,2,8,235,96,1,0,84,83,1,0,180,104,2,64,249,244,3,19,170,168,0,0,180,224,3,8,170,3,19,0,148,136,142,64,248,168,
      255,255,181,224,3,19,170,255,18,0,148,253,123,193,168,243,83,193,168,192,3,95,214,204,204,204,204,243,83,191,169,253,123,191,169,253,3,0,145,168,0,0,176,8,173,70,249,19,0,64,249,127,2,8,235,96,1,0,84,83,1,0,180,104,2,64,249,244,3,19,170,168,
      0,0,180,224,3,8,170,237,18,0,148,136,142,64,248,168,255,255,181,224,3,19,170,233,18,0,148,253,123,193,168,243,83,193,168,192,3,95,214,204,204,204,204,243,83,189,169,245,91,1,169,247,19,0,249,253,123,191,169,253,3,0,145,244,3,0,170,136,2,192,
      57,234,3,20,170,9,0,128,210,40,1,0,52,31,245,0,113,41,5,137,154,75,5,0,145,72,29,192,56,200,255,255,53,106,5,0,145,72,1,192,57,40,255,255,53,1,1,128,210,32,5,0,145,64,22,0,148,243,3,0,170,179,0,0,181,0,0,128,210,204,18,0,148,0,0,128,210,42,0,
      0,20,246,3,19,170,137,2,192,57,137,4,0,52,234,3,20,170,105,0,0,52,72,29,192,56,232,255,255,53,72,1,20,203,23,5,0,145,63,245,0,113,192,1,0,84,33,0,128,210,224,3,23,170,44,22,0,148,245,3,0,170,117,1,0,180,226,3,20,170,225,3,23,170,47,11,0,148,
      96,3,0,53,213,2,0,249,0,0,128,210,214,34,0,145,178,18,0,148,244,2,20,139,232,255,255,23,104,2,64,249,244,3,19,170,168,0,0,180,224,3,8,170,171,18,0,148,136,142,64,248,168,255,255,181,224,3,19,170,167,18,0,148,0,0,128,210,165,18,0,148,215,255,
      255,23,0,0,128,210,162,18,0,148,224,3,19,170,253,123,193,168,247,19,64,249,245,91,65,169,243,83,195,168,192,3,95,214,4,0,128,210,3,0,128,82,2,0,128,210,1,0,128,210,0,0,128,210,140,247,255,151,204,204,204,204,243,83,190,169,245,91,1,169,253,123,
      191,169,253,3,0,145,243,3,0,170,244,3,1,170,127,2,20,235,64,1,0,84,214,0,0,144,117,134,64,248,181,0,0,180,200,2,64,249,239,3,21,170,0,1,63,214,160,2,63,214,127,2,20,235,33,255,255,84,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,
      204,204,204,204,243,83,190,169,245,91,1,169,253,123,191,169,253,3,0,145,243,3,0,170,244,3,1,170,127,2,20,235,96,1,0,84,214,0,0,144,117,134,64,248,213,0,0,180,200,2,64,249,239,3,21,170,0,1,63,214,160,2,63,214,128,0,0,53,127,2,20,235,1,255,255,
      84,0,0,128,82,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,8,1,0,24,31,0,8,107,96,0,0,84,0,0,128,82,3,0,0,20,224,3,8,42,4,0,0,20,192,3,95,214,99,115,109,224,204,204,204,204,243,83,189,169,245,91,1,169,247,19,0,249,
      253,123,191,169,253,3,0,145,244,3,0,42,246,3,1,170,117,13,0,148,243,3,0,170,115,1,0,180,106,2,64,249,75,1,3,145,95,1,11,235,233,3,10,170,192,0,0,84,40,5,65,184,31,1,20,107,32,1,0,84,63,1,11,235,129,255,255,84,0,0,128,82,253,123,193,168,247,19,
      64,249,245,91,65,169,243,83,195,168,192,3,95,214,40,65,0,209,40,255,255,180,21,5,64,249,245,254,255,180,191,22,0,241,129,0,0,84,31,5,0,249,32,0,128,82,243,255,255,23,191,6,0,241,97,0,0,84,0,0,128,18,239,255,255,23,119,6,64,249,118,6,0,249,20,
      5,64,185,159,34,0,113,129,9,0,84,73,193,0,145,43,65,2,145,3,0,0,20,63,5,0,249,41,65,0,145,63,1,11,235,161,255,255,84,8,1,64,185,137,9,0,24,118,18,64,185,31,1,9,107,200,3,0,84,64,3,0,84,9,9,0,24,31,1,9,107,128,2,0,84,201,8,0,24,31,1,9,107,192,
      1,0,84,137,8,0,24,31,1,9,107,0,1,0,84,73,8,0,24,31,1,9,107,244,3,22,42,33,5,0,84,40,16,128,82,52,16,128,82,37,0,0,20,200,16,128,82,212,16,128,82,34,0,0,20,104,16,128,82,116,16,128,82,31,0,0,20,72,16,128,82,84,16,128,82,28,0,0,20,136,16,128,82,
      148,16,128,82,25,0,0,20,9,6,0,24,31,1,9,107,128,2,0,84,201,5,0,24,31,1,9,107,192,1,0,84,137,5,0,24,31,1,9,107,0,1,0,84,73,5,0,24,31,1,9,107,244,3,22,42,161,1,0,84,168,17,128,82,180,17,128,82,9,0,0,20,200,17,128,82,212,17,128,82,6,0,0,20,168,
      16,128,82,180,16,128,82,3,0,0,20,72,17,128,82,84,17,128,82,104,18,0,185,200,0,0,144,8,1,64,249,239,3,21,170,0,1,63,214,225,3,20,42,0,1,128,82,160,2,63,214,118,18,0,185,8,0,0,20,31,5,0,249,200,0,0,144,8,1,64,249,239,3,21,170,0,1,63,214,224,3,
      20,42,160,2,63,214,119,6,0,249,166,255,255,23,145,0,0,192,141,0,0,192,142,0,0,192,143,0,0,192,144,0,0,192,146,0,0,192,147,0,0,192,180,2,0,192,181,2,0,192,204,204,204,204,4,0,128,82,1,0,0,20,243,15,31,248,253,123,191,169,253,3,0,145,225,0,0,181,
      232,20,0,148,211,2,128,82,19,0,0,185,41,18,0,148,192,2,128,82,21,0,0,20,130,1,0,180,136,28,64,211,63,0,0,121,9,5,0,145,95,0,9,235,136,0,0,84,220,20,0,148,83,4,128,82,6,0,0,20,104,8,0,81,31,137,0,113,233,0,0,84,214,20,0,148,211,2,128,82,19,0,
      0,185,23,18,0,148,224,3,19,42,3,0,0,20,132,28,0,83,5,0,0,148,253,123,193,168,243,7,65,248,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,136,28,0,83,238,3,1,170,13,0,128,210,200,0,0,52,168,5,128,82,40,0,0,121,46,8,0,145,45,0,128,210,
      224,3,0,75,235,3,14,170,7,6,128,82,230,10,128,82,236,3,14,170,67,0,0,53,128,0,62,212,8,8,195,26,9,129,3,27,224,3,8,42,207,9,0,145,173,5,0,145,63,37,0,113,232,144,134,26,9,33,41,11,201,1,0,121,128,0,0,52,238,3,15,170,191,1,2,235,35,254,255,84,
      191,1,2,235,3,1,0,84,63,0,0,121,169,20,0,148,72,4,128,82,8,0,0,185,234,17,0,148,64,4,128,82,9,0,0,20,255,1,0,121,137,229,95,120,104,37,64,120,136,5,0,121,127,1,12,235,105,225,31,120,99,255,255,84,0,0,128,82,253,123,193,168,192,3,95,214,204,204,
      204,204,253,123,191,169,253,3,0,145,168,0,0,176,8,17,71,185,104,1,0,53,224,0,0,181,146,20,0,148,200,2,128,82,8,0,0,185,211,17,0,148,0,0,176,18,6,0,0,20,65,255,255,180,79,0,0,148,3,0,0,20,2,0,128,210,4,0,0,148,253,123,193,168,192,3,95,214,204,
      204,204,204,243,83,190,169,245,91,1,169,253,123,189,169,253,3,0,145,243,3,0,170,245,3,1,170,243,0,0,181,125,20,0,148,200,2,128,82,8,0,0,185,190,17,0,148,0,0,176,18,55,0,0,20,85,255,255,180,225,3,2,170,224,67,0,145,72,0,0,148,235,15,64,249,104,
      157,64,249,168,0,0,181,225,3,21,170,224,3,19,170,50,0,0,148,38,0,0,20,104,0,0,176,22,1,32,145,96,38,64,120,31,0,4,113,66,1,0,84,12,28,64,211,200,10,0,145,9,121,108,120,137,0,0,54,104,137,64,249,20,105,108,56,7,0,0,20,20,28,0,83,5,0,0,20,225,
      99,0,145,35,25,0,148,235,15,64,249,20,60,0,83,160,38,64,120,31,0,4,113,66,1,0,84,12,28,64,211,200,10,0,145,9,121,108,120,137,0,0,54,104,137,64,249,9,105,108,56,7,0,0,20,9,28,0,83,5,0,0,20,225,99,0,145,19,25,0,148,235,15,64,249,9,60,0,83,128,
      2,9,75,64,0,0,53,212,251,255,53,232,163,64,57,168,0,0,52,234,11,64,249,72,169,67,185,9,121,30,18,73,169,3,185,253,123,195,168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,237,3,0,170,170,37,64,120,43,36,64,120,72,5,1,81,31,101,0,
      113,73,129,0,17,104,5,1,81,76,129,137,26,105,129,0,17,31,101,0,113,104,129,137,26,128,1,8,75,64,0,0,53,140,254,255,53,192,3,95,214,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,243,3,0,170,127,98,0,57,129,0,0,180,40,36,64,169,104,
      166,0,169,27,0,0,20,168,0,0,176,8,17,71,185,200,0,0,53,168,0,0,144,8,33,27,145,9,41,64,169,105,170,0,169,19,0,0,20,181,11,0,148,97,34,0,145,96,2,0,249,8,72,64,249,104,6,0,249,9,68,64,249,105,10,0,249,28,25,0,148,96,2,64,249,97,66,0,145,45,25,
      0,148,105,2,64,249,40,169,67,185,168,0,8,55,8,1,31,50,42,0,128,82,40,169,3,185,106,98,0,57,224,3,19,170,253,123,193,168,243,7,65,248,192,3,95,214,204,204,204,204,255,67,0,209,255,255,159,136,232,255,223,136,255,67,0,145,228,16,0,20,204,204,204,
      204,140,25,0,20,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,147,11,0,148,19,12,64,249,243,0,0,180,239,3,19,170,168,0,0,240,8,1,64,249,0,1,63,214,96,2,63,214,1,0,0,20,188,2,0,148,204,204,204,204,243,83,191,169,253,123,186,169,253,
      3,0,145,168,0,0,176,8,17,71,185,255,19,0,249,255,195,0,57,255,35,1,57,255,67,1,57,255,99,1,57,8,1,0,53,168,0,0,144,8,33,27,145,9,41,64,169,235,227,0,145,105,41,0,169,43,0,128,82,235,35,1,57,224,7,1,169,65,0,0,180,32,0,0,249,35,0,128,82,225,67,
      0,145,224,131,0,145,168,0,0,148,232,35,65,57,244,3,0,42,31,9,0,113,161,0,0,84,234,19,64,249,72,169,67,185,9,121,30,18,73,169,3,185,232,67,65,57,168,0,0,52,224,131,0,145,243,79,64,185,39,0,0,148,19,32,0,185,232,99,65,57,168,0,0,52,224,131,0,145,
      243,87,64,185,33,0,0,148,19,36,0,185,224,3,20,42,253,123,198,168,243,83,193,168,192,3,95,214,204,204,204,204,243,83,191,169,253,123,191,169,253,3,0,145,244,3,0,170,136,162,64,57,31,9,0,113,161,0,0,84,138,2,64,249,72,169,67,185,9,121,30,18,73,
      169,3,185,136,194,64,57,168,0,0,52,224,3,20,170,147,46,64,185,11,0,0,148,19,32,0,185,136,226,64,57,168,0,0,52,224,3,20,170,147,54,64,185,5,0,0,148,19,36,0,185,253,123,193,168,243,83,193,168,192,3,95,214,243,83,191,169,253,123,190,169,253,3,0,
      145,244,3,0,170,136,2,64,249,8,3,0,181,136,0,0,208,8,161,18,145,9,253,223,200,32,1,63,214,136,66,64,57,224,19,0,185,200,0,0,53,40,0,128,82,159,6,0,249,136,66,0,57,1,0,128,210,2,0,0,20,129,6,64,249,224,67,0,145,224,11,0,148,136,0,0,208,8,33,23,
      145,243,3,0,170,224,19,64,185,147,2,0,249,9,253,223,200,32,1,63,214,179,0,0,180,128,2,64,249,253,123,194,168,243,83,193,168,192,3,95,214,77,2,0,148,243,83,191,169,253,123,190,169,253,3,0,145,244,3,0,170,136,2,64,249,8,3,0,181,136,0,0,208,8,161,
      18,145,9,253,223,200,32,1,63,214,136,66,64,57,224,19,0,185,200,0,0,53,40,0,128,82,159,6,0,249,136,66,0,57,1,0,128,210,2,0,0,20,129,6,64,249,224,67,0,145,190,11,0,148,136,0,0,208,8,33,23,145,243,3,0,170,224,19,64,185,147,2,0,249,9,253,223,200,
      32,1,63,214,211,2,0,180,147,10,64,169,129,98,0,145,104,74,64,249,224,3,19,170,136,14,0,249,105,70,64,249,137,18,0,249,140,24,0,148,130,6,64,249,129,130,0,145,224,3,19,170,156,24,0,148,104,170,67,185,168,0,8,55,8,1,31,50,73,0,128,82,104,170,3,
      185,137,162,0,57,253,123,194,168,243,83,193,168,192,3,95,214,26,2,0,148,204,204,204,204,243,83,191,169,253,123,190,169,253,3,0,145,136,0,0,208,8,161,18,145,9,253,223,200,244,3,0,170,32,1,63,214,136,66,64,57,224,19,0,185,200,0,0,53,40,0,128,82,
      159,6,0,249,136,66,0,57,1,0,128,210,2,0,0,20,129,6,64,249,224,67,0,145,140,11,0,148,136,0,0,208,8,33,23,145,243,3,0,170,224,19,64,185,147,2,0,249,9,253,223,200,32,1,63,214,224,3,19,170,253,123,194,168,243,83,193,168,192,3,95,214,243,83,188,169,
      245,91,1,169,247,99,2,169,249,107,3,169,253,123,191,169,253,3,0,145,244,3,1,170,154,2,64,249,249,3,0,170,245,3,2,42,118,28,0,83,218,0,0,181,34,19,0,148,214,2,128,82,22,0,0,185,99,16,0,148,16,0,0,20,149,2,0,52,168,10,0,81,31,137,0,113,41,2,0,
      84,40,0,128,82,214,2,128,82,40,195,0,57,229,3,25,170,54,47,0,185,4,0,128,210,3,0,128,82,2,0,128,210,1,0,128,210,0,0,128,210,93,16,0,148,137,6,64,249,105,44,0,180,136,2,64,249,40,1,0,249,96,1,0,20,72,11,0,145,83,3,64,121,136,2,0,249,23,0,128,
      82,41,163,64,57,233,0,0,53,224,3,25,170,130,255,255,151,4,0,0,20,136,2,64,249,19,37,64,120,136,2,0,249,224,3,19,42,1,1,128,82,203,24,0,148,64,255,255,53,200,2,31,50,127,182,0,113,216,18,136,26,104,174,0,81,169,255,159,82,31,1,9,106,129,0,0,84,
      136,2,64,249,19,37,64,120,136,2,0,249,191,122,27,114,214,2,128,82,225,19,0,84,127,194,0,113,131,14,0,84,127,234,0,113,98,0,0,84,104,194,0,81,110,0,0,20,9,226,159,82,127,2,9,107,194,12,0,84,127,130,25,113,99,13,0,84,127,170,25,113,98,0,0,84,104,
      130,25,81,101,0,0,20,127,194,27,113,163,12,0,84,127,234,27,113,98,0,0,84,104,194,27,81,95,0,0,20,127,154,37,113,227,11,0,84,127,194,37,113,98,0,0,84,104,154,37,81,89,0,0,20,127,154,39,113,35,11,0,84,127,194,39,113,98,0,0,84,104,154,39,81,83,
      0,0,20,127,154,41,113,99,10,0,84,127,194,41,113,98,0,0,84,104,154,41,81,77,0,0,20,127,154,43,113,163,9,0,84,127,194,43,113,98,0,0,84,104,154,43,81,71,0,0,20,127,154,45,113,227,8,0,84,127,194,45,113,98,0,0,84,104,154,45,81,65,0,0,20,127,154,49,
      113,35,8,0,84,127,194,49,113,98,0,0,84,104,154,49,81,59,0,0,20,127,154,51,113,99,7,0,84,127,194,51,113,98,0,0,84,104,154,51,81,53,0,0,20,127,154,53,113,163,6,0,84,127,194,53,113,98,0,0,84,104,154,53,81,47,0,0,20,127,66,57,113,227,5,0,84,127,
      106,57,113,98,0,0,84,104,66,57,81,41,0,0,20,127,66,59,113,35,5,0,84,127,106,59,113,98,0,0,84,104,66,59,81,35,0,0,20,127,130,60,113,99,4,0,84,127,170,60,113,98,0,0,84,104,130,60,81,29,0,0,20,9,8,130,82,127,2,9,107,131,3,0,84,72,9,130,82,127,2,
      8,107,98,0,0,84,8,8,130,82,20,0,0,20,9,252,130,82,127,2,9,107,131,2,0,84,72,253,130,82,127,2,8,107,98,0,0,84,8,252,130,82,12,0,0,20,8,254,156,82,104,2,8,11,9,61,0,83,63,37,0,113,72,1,0,84,8,2,131,82,5,0,0,20,73,227,159,82,127,2,9,107,162,0,0,
      84,8,226,159,82,104,2,8,75,31,5,0,49,97,1,0,84,104,6,1,81,31,101,0,113,104,134,1,81,105,0,0,84,31,101,0,113,40,4,0,84,31,101,0,113,104,130,0,81,104,130,136,26,8,221,0,81,136,3,0,53,138,2,64,249,233,251,159,82,75,1,64,121,76,9,0,145,140,2,0,249,
      104,97,1,81,31,1,9,106,160,1,0,84,191,2,0,113,138,2,0,249,8,1,128,82,181,18,136,26,75,2,0,52,72,1,64,121,31,1,11,107,224,1,0,84,92,18,0,148,22,0,0,185,158,15,0,148,11,0,0,20,136,9,0,145,147,1,64,121,191,2,0,113,136,2,0,249,9,2,128,82,181,18,
      137,26,4,0,0,20,191,2,0,113,72,1,128,82,181,18,136,26,8,0,128,18,85,0,0,53,128,0,62,212,13,9,213,26,127,194,0,113,131,14,0,84,127,234,0,113,98,0,0,84,105,194,0,81,110,0,0,20,8,226,159,82,127,2,8,107,194,12,0,84,127,130,25,113,99,13,0,84,127,
      170,25,113,98,0,0,84,105,130,25,81,101,0,0,20,127,194,27,113,163,12,0,84,127,234,27,113,98,0,0,84,105,194,27,81,95,0,0,20,127,154,37,113,227,11,0,84,127,194,37,113,98,0,0,84,105,154,37,81,89,0,0,20,127,154,39,113,35,11,0,84,127,194,39,113,98,
      0,0,84,105,154,39,81,83,0,0,20,127,154,41,113,99,10,0,84,127,194,41,113,98,0,0,84,105,154,41,81,77,0,0,20,127,154,43,113,163,9,0,84,127,194,43,113,98,0,0,84,105,154,43,81,71,0,0,20,127,154,45,113,227,8,0,84,127,194,45,113,98,0,0,84,105,154,45,
      81,65,0,0,20,127,154,49,113,35,8,0,84,127,194,49,113,98,0,0,84,105,154,49,81,59,0,0,20,127,154,51,113,99,7,0,84,127,194,51,113,98,0,0,84,105,154,51,81,53,0,0,20,127,154,53,113,163,6,0,84,127,194,53,113,98,0,0,84,105,154,53,81,47,0,0,20,127,66,
      57,113,227,5,0,84,127,106,57,113,98,0,0,84,105,66,57,81,41,0,0,20,127,66,59,113,35,5,0,84,127,106,59,113,98,0,0,84,105,66,59,81,35,0,0,20,127,130,60,113,99,4,0,84,127,170,60,113,98,0,0,84,105,130,60,81,29,0,0,20,8,8,130,82,127,2,8,107,131,3,
      0,84,72,9,130,82,127,2,8,107,98,0,0,84,8,8,130,82,20,0,0,20,8,252,130,82,127,2,8,107,131,2,0,84,72,253,130,82,127,2,8,107,98,0,0,84,8,252,130,82,12,0,0,20,8,254,156,82,104,2,8,11,9,61,0,83,63,37,0,113,72,1,0,84,8,2,131,82,5,0,0,20,72,227,159,
      82,127,2,8,107,162,0,0,84,8,226,159,82,105,2,8,75,63,5,0,49,161,1,0,84,104,6,1,81,31,101,0,113,104,134,1,81,105,0,0,84,31,101,0,113,200,0,0,84,31,101,0,113,104,130,0,81,104,130,136,26,9,221,0,81,2,0,0,20,9,0,128,18,140,2,64,249,63,1,21,107,226,
      1,0,84,232,126,21,27,147,1,64,121,11,1,9,11,127,1,8,107,233,39,159,26,255,2,13,107,232,151,159,26,41,1,8,42,10,11,9,42,136,9,0,145,247,3,11,42,136,2,0,249,88,1,29,50,111,255,255,23,136,9,0,209,136,2,0,249,243,0,0,52,8,1,64,121,31,1,19,107,128,
      0,0,84,178,17,0,148,22,0,0,185,244,14,0,148,248,0,24,55,136,6,64,249,154,2,0,249,72,0,0,180,26,1,0,249,0,0,128,82,32,0,0,20,225,3,23,42,224,3,24,42,36,0,0,148,8,28,0,83,136,2,0,52,40,0,128,82,73,4,128,82,40,195,0,57,41,47,0,185,120,0,0,55,23,
      0,128,18,15,0,0,20,137,6,64,249,216,0,8,54,105,0,0,180,136,2,64,249,40,1,0,249,0,0,176,82,13,0,0,20,105,0,0,180,136,2,64,249,40,1,0,249,0,0,176,18,8,0,0,20,88,0,8,54,247,3,23,75,137,6,64,249,105,0,0,180,136,2,64,249,40,1,0,249,224,3,23,42,253,
      123,193,168,249,107,67,169,247,99,66,169,245,91,65,169,243,83,196,168,192,3,95,214,204,204,204,204,192,0,16,55,32,1,0,54,192,0,8,54,8,0,176,82,63,0,8,107,169,0,0,84,32,0,128,82,192,3,95,214,8,0,176,18,251,255,255,23,0,0,128,82,252,255,255,23,
      8,0,64,121,169,73,139,82,31,1,9,107,97,1,0,84,8,60,64,185,11,170,136,82,10,192,40,139,73,1,64,185,63,1,11,107,161,0,0,84,72,49,64,121,32,0,128,82,31,45,8,113,64,0,0,84,0,0,128,82,192,3,95,214,8,60,64,185,11,0,128,82,10,192,40,139,73,41,64,121,
      76,13,64,121,40,1,10,139,0,97,0,145,204,1,0,52,10,32,0,145,73,5,64,185,63,64,41,235,163,0,0,84,72,1,64,185,9,1,9,11,63,64,41,235,227,0,0,84,107,5,0,17,127,1,12,107,0,160,0,145,74,161,0,145,163,254,255,84,0,0,128,210,192,3,95,214,204,204,204,
      204,243,83,191,169,253,123,191,169,253,3,0,145,243,3,0,170,212,255,255,144,128,2,0,145,210,255,255,151,96,0,0,53,0,0,128,82,13,0,0,20,136,2,0,145,97,2,8,203,128,2,0,145,219,255,255,151,96,0,0,181,0,0,128,82,6,0,0,20,8,36,64,185,233,127,104,42,
      32,1,0,18,2,0,0,20,0,0,128,82,253,123,193,168,243,83,193,168,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,236,14,0,148,96,0,0,180,192,2,128,82,245,14,0,148,168,0,0,144,8,25,64,185,72,0,8,54,96,0,62,212,96,0,128,82,49,248,255,151,
      243,15,31,248,253,123,191,169,253,3,0,145,8,0,0,176,0,193,4,145,137,0,0,208,40,33,16,145,9,253,223,200,32,1,63,214,168,0,0,144,0,29,0,185,31,4,0,49,192,1,0,84,168,0,0,144,19,193,53,145,136,0,0,208,8,129,16,145,9,253,223,200,225,3,19,170,32,1,
      63,214,160,0,0,52,40,0,128,18,104,122,0,185,32,0,128,82,3,0,0,20,5,0,0,148,0,0,128,82,253,123,193,168,243,7,65,248,192,3,95,214,243,15,31,248,253,123,191,169,253,3,0,145,179,0,0,144,96,30,64,185,31,4,0,49,224,0,0,84,136,0,0,208,8,65,16,145,9,
      253,223,200,32,1,63,214,8,0,128,18,104,30,0,185,32,0,128,82,253,123,193,168,243,7,65,248,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,6,0,0,148,96,0,0,180,253,123,193,168,192,3,95,214,190,255,255,151,204,204,204,204,243,83,190,169,
      245,91,1,169,253,123,191,169,253,3,0,145,182,0,0,144,200,30,64,185,31,5,0,49,97,0,0,84,0,0,128,210,56,0,0,20,136,0,0,208,8,161,18,145,9,253,223,200,32,1,63,214,136,0,0,208,8,97,16,145,244,3,0,42,192,30,64,185,9,253,223,200,32,1,63,214,245,3,
      0,170,191,6,0,177,32,1,0,84,149,4,0,181,136,0,0,176,8,129,16,145,192,30,64,185,9,253,223,200,1,0,128,146,32,1,63,214,96,0,0,53,21,0,128,210,27,0,0,20,1,16,128,210,32,0,128,210,187,22,0,148,136,0,0,176,8,129,16,145,243,3,0,170,192,30,64,185,9,
      253,223,200,179,1,0,180,225,3,19,170,32,1,63,214,192,0,0,52,40,0,128,18,104,122,0,185,245,3,19,170,19,0,128,210,8,0,0,20,136,0,0,176,8,129,16,145,192,30,64,185,9,253,223,200,1,0,128,210,32,1,63,214,21,0,128,210,224,3,19,170,178,252,255,151,136,
      0,0,176,8,33,23,145,9,253,223,200,224,3,20,42,32,1,63,214,224,3,21,170,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,243,83,191,169,253,123,191,169,253,3,0,145,243,3,0,170,148,0,0,240,128,30,64,185,31,4,0,49,128,2,
      0,84,243,0,0,181,136,0,0,176,8,97,16,145,9,253,223,200,32,1,63,214,243,3,0,170,128,30,64,185,136,0,0,176,8,129,16,145,9,253,223,200,1,0,128,210,32,1,63,214,243,0,0,180,136,0,0,240,8,193,53,145,127,2,8,235,96,0,0,84,224,3,19,170,140,252,255,151,
      253,123,193,168,243,83,193,168,192,3,95,214,192,0,0,180,136,0,0,240,8,193,53,145,31,0,8,235,64,0,0,84,131,252,255,23,192,3,95,214,204,204,204,204,34,0,64,249,1,0,0,20,40,48,65,57,73,16,0,209,31,1,0,113,71,0,137,154,32,3,0,180,15,24,64,185,45,
      4,64,249,168,193,47,139,168,2,0,180,14,20,64,185,12,0,128,82,14,2,0,52,136,125,125,211,9,193,47,139,42,105,109,184,171,193,42,139,255,0,11,235,131,0,0,84,140,5,0,17,159,1,14,107,3,255,255,84,204,0,0,52,136,5,0,81,169,77,40,139,42,193,47,139,
      64,5,64,185,192,3,95,214,0,0,128,18,254,255,255,23,49,255,255,23,234,3,2,170,8,0,64,249,73,29,64,185,0,201,105,184,31,8,0,49,129,0,0,84,34,0,64,249,224,3,10,170,218,255,255,23,192,3,95,214,41,28,64,185,8,0,64,249,2,201,41,184,192,3,95,214,243,
      83,191,169,253,123,190,169,253,3,0,145,244,3,3,42,227,67,0,145,243,3,2,170,148,243,255,151,104,30,64,185,9,0,64,249,42,193,40,139,233,11,0,249,73,5,64,185,159,2,9,107,77,0,0,84,84,5,0,185,253,123,194,168,243,83,193,168,192,3,95,214,243,15,31,
      248,253,123,190,169,253,3,0,145,227,67,0,145,243,3,2,170,131,243,255,151,104,30,64,185,9,0,64,249,233,11,0,249,41,193,40,139,32,5,64,185,253,123,194,168,243,7,65,248,192,3,95,214,243,83,188,169,245,91,1,169,247,99,2,169,249,27,0,249,251,31,0,
      249,253,123,187,169,253,3,0,145,248,3,0,170,184,31,0,249,243,3,1,170,179,19,0,249,179,27,0,249,245,3,2,170,181,23,0,249,247,3,3,42,183,19,0,185,48,244,255,151,251,3,0,170,187,35,0,249,226,3,21,170,225,3,19,170,224,3,24,170,188,255,255,151,244,
      3,0,42,38,255,255,151,8,48,64,185,9,5,0,17,9,48,0,185,159,6,0,49,142,18,87,122,77,8,0,84,159,6,0,49,205,10,0,84,168,6,64,185,159,2,8,107,106,10,0,84,150,126,125,147,27,244,255,151,168,10,64,185,9,192,40,139,52,105,118,184,180,23,0,185,22,244,
      255,151,168,10,64,185,9,192,40,139,42,1,22,139,75,5,64,185,75,1,0,52,16,244,255,151,168,10,64,185,9,192,40,139,42,1,22,139,83,5,64,185,11,244,255,151,8,192,51,139,179,19,64,249,2,0,0,20,8,0,128,210,136,3,0,180,226,3,20,42,225,3,21,170,224,3,
      24,170,158,255,255,151,121,42,64,249,0,244,255,151,168,10,64,185,9,192,40,139,42,1,22,139,75,5,64,185,75,1,0,52,250,243,255,151,168,10,64,185,9,192,40,139,42,1,22,139,83,5,64,185,245,243,255,151,0,192,51,139,179,19,64,249,2,0,0,20,0,0,128,210,
      99,32,128,82,226,3,25,170,225,3,24,170,225,223,255,151,224,3,27,170,241,243,255,151,8,0,0,20,183,19,64,185,181,23,64,249,179,27,64,249,179,19,0,249,184,31,64,249,187,35,64,249,180,23,64,185,180,27,0,185,189,255,255,23,222,254,255,151,8,48,64,
      185,31,1,0,113,173,0,0,84,218,254,255,151,8,48,64,185,9,5,0,81,9,48,0,185,159,6,0,49,142,18,87,122,172,1,0,84,226,3,20,42,225,3,21,170,224,3,24,170,110,255,255,151,253,123,197,168,251,31,64,249,249,27,64,249,247,99,66,169,245,91,65,169,243,83,
      196,168,192,3,95,214,140,254,255,151,139,254,255,151,243,83,190,169,245,91,1,169,253,123,190,169,253,3,0,145,244,3,2,170,246,3,0,170,224,3,20,170,245,3,1,170,50,255,255,151,243,3,0,42,224,3,22,170,227,67,0,145,226,3,20,170,225,3,21,170,244,242,
      255,151,226,3,20,170,225,3,21,170,224,3,22,170,104,255,255,151,127,2,0,107,109,1,0,84,226,3,19,42,225,3,20,170,224,67,0,145,76,255,255,151,227,3,19,42,226,3,20,170,225,3,21,170,224,3,22,170,75,255,255,151,6,0,0,20,226,3,20,170,225,3,21,170,224,
      3,22,170,88,255,255,151,243,3,0,42,224,3,19,42,253,123,194,168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,243,83,188,169,245,91,1,169,247,99,2,169,249,27,0,249,251,31,0,249,253,123,184,169,253,3,0,145,245,3,0,170,181,39,0,249,
      22,0,128,82,191,19,0,185,191,23,0,185,191,23,0,249,191,19,0,249,142,254,255,151,24,20,64,249,184,35,0,249,139,254,255,151,25,16,64,249,185,31,0,249,180,42,64,249,180,15,0,249,187,38,64,249,187,47,0,249,179,34,64,249,168,26,64,249,168,43,0,249,
      181,22,64,249,181,51,0,249,224,3,19,170,102,21,0,148,125,254,255,151,20,16,0,249,123,254,255,151,19,20,0,249,121,254,255,151,8,16,64,249,1,21,64,249,160,195,1,145,151,243,255,151,247,3,0,170,183,27,0,249,179,39,64,249,104,46,64,249,200,0,0,180,
      40,0,128,82,168,23,0,185,109,254,255,151,8,56,64,249,168,19,0,249,3,32,128,82,98,58,64,249,225,3,21,170,160,43,64,249,92,223,255,151,243,3,0,170,179,23,0,249,35,0,0,20,40,0,128,82,177,67,0,145,40,254,159,136,95,254,255,151,31,64,0,185,168,23,
      64,185,180,15,64,249,136,1,0,52,33,0,128,82,224,3,20,170,228,243,255,151,168,19,64,249,3,129,0,145,2,25,64,185,1,5,64,185,0,1,64,185,137,0,0,176,40,97,22,145,7,0,0,20,131,130,0,145,130,26,64,185,129,6,64,185,128,2,64,185,136,0,0,176,8,97,22,
      145,9,253,223,200,32,1,63,214,182,19,64,185,179,23,64,249,184,35,64,249,185,31,64,249,187,47,64,249,181,51,64,249,183,27,64,249,224,3,23,170,117,243,255,151,118,2,0,53,105,4,0,24,136,2,64,185,31,1,9,107,225,1,0,84,136,26,64,185,31,17,0,113,129,
      1,0,84,136,34,64,185,137,3,0,24,8,1,9,75,31,9,0,113,232,0,0,84,128,22,64,249,238,243,255,151,128,0,0,52,33,0,128,82,224,3,20,170,184,243,255,151,43,254,255,151,25,16,0,249,41,254,255,151,24,20,0,249,105,31,64,185,168,2,64,249,42,0,128,146,10,
      201,41,248,224,3,19,170,253,123,200,168,251,31,64,249,249,27,64,249,247,99,66,169,245,91,65,169,243,83,196,168,192,3,95,214,31,32,3,213,99,115,109,224,32,5,147,25,232,3,1,170,225,3,2,170,0,1,31,214,204,204,204,204,232,3,1,170,225,3,2,170,226,
      3,3,42,0,1,31,214,72,0,0,208,8,193,23,145,8,252,0,169,74,0,0,208,73,129,23,145,9,0,0,249,192,3,95,214,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,72,0,0,208,8,225,0,145,243,3,0,170,233,3,1,170,104,2,0,249,97,34,0,145,32,33,0,145,
      127,254,0,169,253,243,255,151,72,0,0,208,8,129,23,145,104,2,0,249,224,3,19,170,253,123,193,168,243,7,65,248,192,3,95,214,204,204,204,204,243,83,191,169,253,123,191,169,253,3,0,145,10,0,64,249,243,3,2,170,127,2,0,185,12,5,0,24,11,5,0,24,52,0,
      128,82,72,1,64,185,31,1,12,107,193,3,0,84,72,25,64,185,31,17,0,113,161,1,0,84,72,33,64,185,8,1,11,75,31,9,0,113,40,1,0,84,41,20,64,249,72,21,64,249,31,1,9,235,161,0,0,84,116,2,0,185,72,1,64,185,31,1,12,107,225,1,0,84,72,25,64,185,31,17,0,113,
      129,1,0,84,72,33,64,185,8,1,11,75,31,9,0,113,8,1,0,84,72,25,64,249,200,0,0,181,208,253,255,151,20,64,0,185,32,0,128,82,116,2,0,185,2,0,0,20,0,0,128,82,253,123,193,168,243,83,193,168,192,3,95,214,31,32,3,213,99,115,109,224,32,5,147,25,243,83,
      187,169,245,91,1,169,247,99,2,169,249,107,3,169,251,35,0,249,253,123,190,169,253,3,0,145,250,3,0,170,247,3,1,170,87,6,0,180,232,2,64,185,25,0,128,82,22,0,128,82,31,1,0,113,173,4,0,84,199,242,255,151,72,27,64,249,9,13,64,185,10,192,41,139,83,
      17,0,145,194,242,255,151,72,27,64,249,9,13,64,185,24,200,105,184,31,3,0,113,205,2,0,84,219,126,64,147,187,242,255,151,116,70,64,184,245,3,0,170,243,11,0,249,83,27,64,249,166,242,255,151,232,6,64,185,161,194,52,139,226,3,19,170,9,192,40,139,136,
      2,128,210,96,39,8,155,183,0,0,148,192,0,0,53,24,7,0,81,243,11,64,249,31,3,0,113,236,253,255,84,2,0,0,20,57,0,128,82,232,2,64,185,214,6,0,17,223,2,8,107,171,251,255,84,224,3,25,42,253,123,194,168,251,35,64,249,249,107,67,169,247,99,66,169,245,
      91,65,169,243,83,197,168,192,3,95,214,77,253,255,151,243,83,187,169,245,91,1,169,247,99,2,169,249,107,3,169,251,35,0,249,253,123,191,169,253,3,0,145,249,3,2,170,244,3,0,170,224,3,25,170,247,3,1,170,245,3,3,170,243,3,4,170,248,3,5,42,250,3,6,
      170,251,28,0,83,96,20,0,148,119,253,255,151,73,17,0,24,8,64,64,185,42,17,0,24,43,17,0,24,54,17,0,24,168,2,0,53,136,2,64,185,31,1,9,107,64,2,0,84,31,1,10,107,1,1,0,84,136,26,64,185,31,61,0,113,225,0,0,84,136,50,64,249,41,15,0,88,31,1,9,235,2,
      0,0,20,31,1,11,107,224,0,0,84,104,2,64,185,9,113,0,18,63,1,22,107,99,0,0,84,104,38,64,185,168,12,0,55,136,6,64,185,201,12,128,82,31,1,9,106,128,4,0,84,105,6,64,185,233,11,0,52,216,11,0,53,104,3,40,54,136,2,64,185,31,1,11,107,1,2,0,84,162,18,
      64,249,225,3,21,170,224,3,19,170,196,253,255,151,31,4,0,49,139,11,0,84,104,6,64,185,31,0,8,107,42,11,0,84,227,3,0,42,224,3,23,170,225,3,21,170,226,3,19,170,6,254,255,151,75,0,0,20,31,1,10,107,225,0,0,84,131,30,64,249,127,4,0,49,100,160,73,122,
      170,9,0,84,128,22,64,249,245,255,255,23,226,3,19,170,225,3,21,170,224,3,23,170,87,241,255,151,62,0,0,20,104,14,64,185,104,2,0,53,104,2,64,185,74,9,0,24,9,113,0,18,63,1,10,107,227,0,0,84,104,34,64,185,168,0,0,52,46,242,255,151,104,34,64,185,9,
      192,40,139,9,1,0,181,104,2,64,185,9,113,0,18,63,1,22,107,163,5,0,84,104,38,64,185,9,9,2,83,73,5,0,52,136,2,64,185,137,6,0,24,31,1,9,107,161,3,0,84,136,26,64,185,31,13,0,113,67,3,0,84,136,34,64,185,31,1,22,107,233,2,0,84,136,26,64,249,9,9,64,
      185,137,2,0,52,38,242,255,151,136,26,64,249,9,9,64,185,22,192,41,139,246,1,0,180,168,0,0,208,8,1,64,249,239,3,22,170,0,1,63,214,231,3,27,42,230,3,26,170,229,3,24,42,228,3,19,170,227,3,21,170,226,3,25,170,225,3,23,170,224,3,20,170,192,2,63,214,
      11,0,0,20,231,3,26,170,230,3,24,42,229,3,27,42,228,3,19,170,227,3,21,170,226,3,25,170,225,3,23,170,224,3,20,170,13,1,0,148,32,0,128,82,253,123,193,168,251,35,64,249,249,107,67,169,247,99,66,169,245,91,65,169,243,83,197,168,192,3,95,214,180,252,
      255,151,31,32,3,213,32,5,147,25,0,0,0,0,99,115,109,224,41,0,0,128,38,0,0,128,34,5,147,25,33,5,147,25,204,204,204,204,243,83,190,169,245,91,1,169,253,123,191,169,253,3,0,145,243,3,0,170,104,6,64,185,245,3,1,170,246,3,2,170,104,7,0,52,244,3,8,
      42,222,241,255,151,8,192,52,139,232,6,0,180,180,0,0,52,116,6,64,185,217,241,255,151,8,192,52,139,3,0,0,20,8,0,128,210,20,0,128,82,8,65,192,57,200,5,0,52,104,2,64,185,104,0,56,54,169,2,64,185,73,5,32,55,180,0,0,52,205,241,255,151,104,6,64,185,
      20,192,40,139,2,0,0,20,20,0,128,210,216,241,255,151,168,6,64,185,9,192,40,139,159,2,9,235,32,2,0,84,104,6,64,185,168,0,0,52,193,241,255,151,104,6,64,185,20,192,40,139,2,0,0,20,20,0,128,210,204,241,255,151,168,6,64,185,9,192,40,139,128,66,0,145,
      33,65,0,145,249,2,0,148,96,0,0,52,0,0,128,82,16,0,0,20,168,2,64,185,104,0,8,54,105,2,64,185,105,255,31,54,202,2,64,185,106,0,0,54,104,2,64,185,232,254,7,54,106,0,16,54,104,2,64,185,136,254,23,54,106,0,8,54,104,2,64,185,40,254,15,54,32,0,128,
      82,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,243,83,189,169,245,91,1,169,247,99,2,169,253,123,190,169,253,3,0,145,247,3,0,170,244,3,1,170,243,3,2,170,245,3,3,170,24,0,128,82,104,6,64,185,168,0,0,52,246,3,8,42,147,241,255,151,
      8,192,54,139,3,0,0,20,8,0,128,210,22,0,128,82,136,11,0,180,182,0,0,52,140,241,255,151,104,6,64,185,9,192,40,139,2,0,0,20,9,0,128,210,40,65,192,57,136,10,0,52,106,10,64,185,106,0,0,53,104,2,64,185,8,10,248,54,105,2,64,185,105,0,248,55,136,2,64,
      249,20,193,42,139,201,1,56,54,168,2,64,185,136,1,32,54,138,0,0,240,83,249,69,249,51,1,0,180,239,3,19,170,168,0,0,208,8,1,64,249,0,1,63,214,96,2,63,214,192,8,0,180,180,8,0,180,5,0,0,20,201,0,24,54,224,22,64,249,64,8,0,180,52,8,0,180,128,2,0,249,
      14,0,0,20,168,2,64,185,8,2,0,54,225,22,64,249,129,7,0,180,116,7,0,180,162,22,128,185,224,3,20,170,128,221,255,151,168,22,64,185,31,33,0,113,97,5,0,84,128,2,64,249,32,5,0,180,161,34,0,145,41,242,255,151,128,2,0,249,37,0,0,20,168,26,64,185,168,
      0,0,52,243,3,8,42,101,241,255,151,9,192,51,139,3,0,0,20,9,0,128,210,19,0,128,82,232,22,64,249,137,1,0,181,168,4,0,180,148,4,0,180,179,22,128,185,161,34,0,145,224,3,8,170,23,242,255,151,226,3,19,170,225,3,0,170,224,3,20,170,99,221,255,151,16,
      0,0,20,104,3,0,180,84,3,0,180,179,0,0,52,80,241,255,151,168,26,64,185,9,192,40,139,2,0,0,20,9,0,128,210,105,2,0,180,168,2,64,185,31,1,30,114,74,0,128,82,41,0,128,82,88,17,137,26,184,19,0,185,224,3,24,42,2,0,0,20,0,0,128,82,253,123,194,168,247,
      99,66,169,245,91,65,169,243,83,195,168,192,3,95,214,238,251,255,151,237,251,255,151,236,251,255,151,235,251,255,151,234,251,255,151,233,251,255,151,243,83,190,169,245,11,0,249,253,123,191,169,253,3,0,145,245,3,0,170,243,3,3,170,72,0,64,185,104,
      0,248,54,244,3,1,170,4,0,0,20,73,8,64,185,40,0,64,249,20,193,41,139,227,3,19,170,224,3,21,170,119,255,255,151,31,4,0,113,96,2,0,84,31,8,0,113,225,3,0,84,97,34,0,145,160,22,64,249,222,241,255,151,245,3,0,170,104,26,64,185,168,0,0,52,28,241,255,
      151,104,26,64,185,1,192,40,139,2,0,0,20,1,0,128,210,35,0,128,82,226,3,21,170,224,3,20,170,238,253,255,151,15,0,0,20,97,34,0,145,160,22,64,249,206,241,255,151,245,3,0,170,104,26,64,185,168,0,0,52,12,241,255,151,104,26,64,185,1,192,40,139,2,0,
      0,20,1,0,128,210,226,3,21,170,224,3,20,170,219,253,255,151,253,123,193,168,245,11,64,249,243,83,194,168,192,3,95,214,178,251,255,151,204,204,204,204,253,123,186,169,243,83,1,169,245,91,2,169,247,99,3,169,249,107,4,169,251,43,0,249,253,3,0,145,
      193,220,255,151,255,67,3,209,247,3,1,170,231,43,0,249,245,3,3,170,247,35,0,249,243,3,4,170,255,131,0,57,246,3,2,170,244,3,0,170,246,23,0,249,184,28,0,83,250,3,6,42,248,135,0,57,226,3,19,170,250,51,0,185,225,3,21,170,224,3,23,170,13,253,255,151,
      249,3,0,42,63,7,0,49,235,35,0,84,104,6,64,185,63,3,8,107,138,35,0,84,136,2,64,185,105,37,0,24,123,37,0,24,31,1,9,107,225,9,0,84,136,26,64,185,31,17,0,113,129,9,0,84,136,34,64,185,8,1,27,75,31,9,0,113,8,9,0,84,136,26,64,249,200,8,0,181,190,251,
      255,151,8,16,64,249,72,32,0,180,187,251,255,151,20,16,64,249,185,251,255,151,40,0,128,82,22,20,64,249,232,131,0,57,128,30,64,249,246,23,0,249,203,240,255,151,136,2,64,185,41,34,0,24,31,1,9,107,65,1,0,84,136,26,64,185,31,17,0,113,225,0,0,84,136,
      34,64,185,8,1,27,75,31,9,0,113,104,0,0,84,136,26,64,249,168,30,0,180,165,251,255,151,8,28,64,249,72,5,0,180,162,251,255,151,22,28,64,249,160,251,255,151,225,3,22,170,31,28,0,249,224,3,20,170,216,253,255,151,8,28,0,83,8,4,0,53,200,2,64,185,21,
      0,128,82,31,1,0,113,205,28,0,84,152,0,0,208,151,2,128,210,179,126,64,147,148,240,255,151,200,6,64,185,9,192,40,139,106,38,23,155,75,5,64,185,43,1,0,52,142,240,255,151,200,6,64,185,9,192,40,139,106,38,23,155,83,5,64,185,137,240,255,151,8,192,
      51,139,2,0,0,20,8,0,128,210,1,195,41,145,0,33,0,145,197,241,255,151,32,26,0,52,200,2,64,185,181,6,0,17,191,2,8,107,138,25,0,84,232,255,255,23,246,23,64,249,168,6,64,249,243,163,10,169,232,26,0,24,137,2,64,185,63,1,8,107,97,21,0,84,136,26,64,
      185,31,17,0,113,1,21,0,84,136,34,64,185,8,1,27,75,31,9,0,113,136,20,0,84,104,14,64,185,8,14,0,52,229,3,26,42,228,3,19,170,227,3,21,170,226,3,25,42,225,163,2,145,224,35,2,145,14,240,255,151,232,35,2,145,236,163,64,185,8,37,64,169,234,227,1,145,
      72,37,0,169,232,147,64,185,31,1,12,107,34,12,0,84,237,71,64,249,143,2,128,210,232,63,64,249,248,131,64,185,237,39,0,249,8,1,64,249,9,127,64,211,238,131,1,145,10,17,64,185,40,125,15,155,11,193,42,139,170,5,64,249,107,1,10,139,104,37,64,169,106,
      17,64,185,200,37,0,169,202,17,0,185,232,99,64,185,31,1,25,107,12,9,0,84,232,103,64,185,63,3,8,107,172,8,0,84,168,6,64,249,22,0,128,82,233,115,64,185,9,193,41,139,232,111,64,185,233,47,0,249,200,7,0,52,200,126,64,211,10,37,15,155,235,227,2,145,
      72,37,64,169,104,37,0,169,74,17,64,185,106,17,0,185,72,240,255,151,136,26,64,249,9,13,64,185,10,192,41,139,75,17,0,145,235,31,0,249,66,240,255,151,136,26,64,249,9,13,64,185,23,200,105,184,12,0,0,20,61,240,255,151,232,31,64,249,250,3,0,170,130,
      26,64,249,224,227,2,145,27,69,64,184,65,195,59,139,232,31,0,249,61,254,255,151,96,1,0,53,247,6,0,81,255,2,0,113,140,254,255,84,232,111,64,185,214,6,0,17,143,2,128,210,223,2,8,107,224,2,0,84,233,47,64,249,219,255,255,23,232,135,64,57,70,195,59,
      139,231,131,1,145,247,35,64,249,229,227,2,145,226,23,64,249,228,3,19,170,227,3,21,170,232,99,0,57,232,131,64,57,224,3,20,170,225,3,23,170,232,67,0,57,232,43,64,249,232,7,0,249,232,51,64,185,232,3,0,185,106,0,0,148,143,2,128,210,2,0,0,20,247,
      35,64,249,237,39,64,249,236,163,64,185,24,7,0,17,232,63,64,249,31,3,12,107,227,244,255,84,248,135,64,57,104,2,64,185,138,11,0,24,9,113,0,18,63,1,10,107,67,7,0,84,104,34,64,185,168,0,0,52,246,239,255,151,104,34,64,185,9,192,40,139,41,1,0,181,
      104,38,64,185,9,9,2,83,41,6,0,52,225,3,19,170,224,3,21,170,251,238,255,151,8,28,0,83,136,5,0,53,104,38,64,185,9,9,2,83,9,8,0,53,104,34,64,185,168,0,0,52,229,239,255,151,104,34,64,185,1,192,40,139,2,0,0,20,1,0,128,210,224,3,20,170,25,253,255,
      151,8,28,0,83,200,3,0,53,227,99,1,145,226,3,19,170,225,3,21,170,224,3,23,170,18,239,255,151,226,23,64,249,7,0,128,18,227,3,0,170,248,67,0,57,6,0,128,18,255,87,0,169,229,3,19,170,4,0,128,210,225,3,20,170,224,3,23,170,73,239,255,151,13,0,0,20,
      104,14,64,185,104,1,0,52,216,2,0,53,231,43,64,249,230,3,26,42,229,3,25,42,228,3,19,170,227,3,21,170,226,3,22,170,225,3,23,170,224,3,20,170,86,0,0,148,189,250,255,151,8,28,64,249,72,1,0,181,255,67,3,145,157,219,255,151,251,43,64,249,249,107,68,
      169,247,99,67,169,245,91,66,169,243,83,65,169,253,123,198,168,192,3,95,214,117,250,255,151,172,247,255,151,33,0,128,82,224,3,20,170,57,240,255,151,224,227,2,145,155,252,255,151,136,0,0,176,1,225,45,145,224,227,2,145,187,237,255,151,166,250,255,
      151,20,16,0,249,164,250,255,151,232,23,64,249,8,20,0,249,157,247,255,151,99,115,109,224,32,5,147,25,33,5,147,25,204,204,204,204,243,83,187,169,245,91,1,169,247,99,2,169,249,107,3,169,251,35,0,249,253,123,190,169,253,3,0,145,255,131,0,209,248,
      3,1,170,226,27,0,249,249,3,3,170,250,3,4,170,246,3,0,170,226,3,26,170,225,3,25,170,224,3,24,170,227,227,0,145,245,3,5,170,243,3,6,170,251,3,7,170,196,238,255,151,247,3,0,170,211,0,0,180,227,3,19,170,226,3,21,170,225,3,23,170,224,3,22,170,93,
      254,255,151,116,11,64,185,115,3,64,185,128,239,255,151,231,3,20,42,226,27,64,249,230,3,19,42,245,103,0,169,229,3,26,170,169,14,64,185,227,3,23,170,232,163,66,57,225,3,22,170,4,192,41,139,224,3,24,170,232,67,0,57,239,238,255,151,255,131,0,145,
      253,123,194,168,251,35,64,249,249,107,67,169,247,99,66,169,245,91,65,169,243,83,197,168,192,3,95,214,243,83,187,169,245,91,1,169,247,99,2,169,249,107,3,169,251,35,0,249,253,123,185,169,253,3,0,145,255,131,0,209,248,3,0,170,8,3,64,185,250,3,1,
      170,251,3,2,170,250,111,3,169,233,11,1,50,247,3,3,170,31,1,9,107,245,3,4,170,249,3,5,42,244,3,6,42,246,3,7,170,224,14,0,84,83,250,255,151,8,8,64,249,136,3,0,180,136,0,0,144,8,97,15,145,9,253,223,200,0,0,128,210,32,1,63,214,243,3,0,170,74,250,
      255,151,8,8,64,249,31,1,19,235,64,2,0,84,8,3,64,185,41,14,0,24,31,1,9,107,192,1,0,84,233,13,0,24,31,1,9,107,96,1,0,84,231,3,25,42,230,3,22,170,229,3,20,42,228,3,21,170,227,3,23,170,226,3,27,170,225,3,26,170,224,3,24,170,165,239,255,151,32,11,
      0,53,232,6,64,249,245,163,4,169,169,14,64,185,169,11,0,52,229,3,20,42,228,3,21,170,227,3,23,170,226,3,25,42,225,35,1,145,224,195,1,145,213,238,255,151,232,195,1,145,236,139,64,185,8,37,64,169,234,99,1,145,72,37,0,169,232,123,64,185,31,1,12,107,
      194,8,0,84,250,59,64,249,142,2,128,210,251,47,64,249,246,99,64,185,104,3,64,249,201,126,64,211,237,99,1,145,10,17,64,185,40,125,14,155,11,193,42,139,74,7,64,249,107,1,10,139,104,37,64,169,106,17,64,185,168,37,0,169,170,17,0,185,232,91,64,185,
      31,1,25,107,236,5,0,84,232,95,64,185,63,3,8,107,140,5,0,84,233,107,64,185,232,6,64,249,10,193,41,139,233,103,64,185,41,41,14,155,52,81,0,209,138,6,64,185,202,1,0,52,243,3,10,42,5,239,255,151,8,192,51,139,40,1,0,180,179,0,0,52,1,239,255,151,136,
      6,64,185,9,192,40,139,2,0,0,20,9,0,128,210,40,65,192,57,232,2,0,53,236,139,64,185,136,2,64,185,168,2,48,55,224,27,64,249,227,3,1,145,226,3,21,170,225,3,23,170,45,238,255,151,243,3,0,170,241,238,255,151,136,14,64,185,231,99,64,185,229,3,21,170,
      255,67,0,57,230,91,64,185,227,3,19,170,244,95,0,169,226,31,64,249,225,3,24,170,4,192,40,139,224,27,64,249,97,238,255,151,236,139,64,185,214,6,0,17,223,2,12,107,142,2,128,210,3,248,255,84,255,131,0,145,253,123,199,168,251,35,64,249,249,107,67,
      169,247,99,66,169,245,91,65,169,243,83,197,168,192,3,95,214,153,249,255,151,77,79,67,224,82,67,67,224,72,0,64,185,10,117,30,18,8,200,106,248,73,125,64,147,41,1,8,203,42,1,0,139,64,33,0,209,163,219,255,23,253,123,191,169,253,3,0,145,104,28,64,
      249,9,1,64,185,43,117,30,18,40,200,107,248,106,125,64,147,73,1,8,203,42,1,1,139,64,33,0,209,152,219,255,151,32,0,128,82,253,123,193,168,192,3,95,214,41,0,64,57,8,0,64,57,10,1,9,75,10,1,0,53,11,0,1,203,40,29,0,19,168,0,0,52,41,28,64,56,104,105,
      97,56,10,1,9,75,106,255,255,52,232,3,10,75,9,125,31,83,32,125,74,75,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,31,0,0,241,36,24,64,250,105,0,0,84,34,1,0,181,31,0,0,57,157,10,0,148,200,2,128,82,8,0,0,185,222,7,0,148,192,2,128,82,
      253,123,193,168,192,3,95,214,233,3,0,170,74,0,0,203,72,105,233,56,40,21,0,56,104,1,0,52,33,4,0,209,129,255,255,181,1,1,0,181,31,0,0,57,141,10,0,148,72,4,128,82,8,0,0,185,206,7,0,148,64,4,128,82,240,255,255,23,0,0,128,82,238,255,255,23,204,204,
      204,204,253,123,191,169,253,3,0,145,136,0,0,208,0,97,57,145,137,0,0,144,40,193,20,145,9,253,223,200,2,0,128,82,1,244,129,82,32,1,63,214,224,0,0,52,138,0,0,208,72,129,78,185,32,0,128,82,9,5,0,17,73,129,14,185,3,0,0,20,5,0,0,148,0,0,128,82,253,
      123,193,168,192,3,95,214,204,204,204,204,243,83,189,169,245,91,1,169,247,19,0,249,253,123,191,169,253,3,0,145,150,0,0,208,211,130,78,185,211,1,0,52,136,0,0,208,20,97,57,145,136,0,0,144,21,65,15,145,23,5,128,210,115,6,0,81,169,254,223,200,96,
      82,23,155,32,1,63,214,200,130,78,185,9,5,0,81,201,130,14,185,51,255,255,53,32,0,128,82,253,123,193,168,247,19,64,249,245,91,65,169,243,83,195,168,192,3,95,214,204,204,204,204,204,204,204,204,204,204,204,204,136,0,0,208,9,5,64,249,138,0,0,208,
      72,193,60,145,10,193,3,145,9,37,129,168,31,1,10,235,193,255,255,84,32,0,128,82,192,3,95,214,204,204,204,204,204,204,204,204,243,83,190,169,245,11,0,249,253,123,191,169,253,3,0,145,8,28,0,83,232,1,0,53,136,0,0,208,19,65,58,145,137,0,0,144,53,
      225,16,145,116,98,2,145,96,134,64,248,192,0,0,180,31,4,0,177,96,0,0,84,168,254,223,200,0,1,63,214,127,130,31,248,127,2,20,235,1,255,255,84,32,0,128,82,253,123,193,168,245,11,64,249,243,83,194,168,192,3,95,214,204,204,204,204,243,15,31,248,253,
      123,191,169,253,3,0,145,72,0,0,208,2,161,36,145,72,0,0,208,1,193,36,145,67,16,0,145,0,0,128,82,185,0,0,148,243,3,0,170,243,0,0,180,168,0,0,176,8,1,64,249,239,3,19,170,0,1,63,214,96,2,63,214,2,0,0,20,32,0,128,82,253,123,193,168,243,7,65,248,192,
      3,95,214,136,0,0,144,8,33,16,145,9,253,223,200,32,1,31,214,136,0,0,144,8,65,16,145,9,253,223,200,32,1,31,214,136,0,0,144,8,97,16,145,9,253,223,200,32,1,31,214,136,0,0,144,8,129,16,145,9,253,223,200,32,1,31,214,243,83,190,169,245,91,1,169,253,
      123,191,169,253,3,0,145,246,3,2,42,72,0,0,208,2,1,37,145,243,3,0,170,244,3,1,42,72,0,0,208,1,33,37,145,67,32,0,145,192,1,128,82,143,0,0,148,245,3,0,170,85,1,0,180,168,0,0,176,8,1,64,249,239,3,21,170,0,1,63,214,226,3,22,42,225,3,20,42,224,3,19,
      170,160,2,63,214,7,0,0,20,136,0,0,144,8,161,20,145,9,253,223,200,225,3,20,42,224,3,19,170,32,1,63,214,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,243,83,187,169,245,91,1,169,247,99,2,169,249,107,3,169,251,35,0,249,
      253,123,191,169,253,3,0,145,255,67,0,209,245,3,2,170,72,0,0,208,2,161,37,145,243,3,0,170,244,3,1,42,72,0,0,208,1,193,37,145,246,3,3,42,67,32,0,145,0,2,128,82,247,3,4,170,248,3,5,42,250,3,6,170,251,3,7,170,98,0,0,148,249,3,0,170,57,2,0,180,168,
      0,0,176,8,1,64,249,239,3,25,170,0,1,63,214,232,59,64,249,231,3,27,170,230,3,26,170,229,3,24,42,228,3,23,170,227,3,22,42,232,3,0,249,226,3,21,170,225,3,20,42,224,3,19,170,32,3,63,214,13,0,0,20,1,0,128,82,224,3,19,170,19,0,0,148,136,0,0,144,8,
      65,21,145,9,253,223,200,229,3,24,42,228,3,23,170,227,3,22,42,226,3,21,170,225,3,20,42,32,1,63,214,255,67,0,145,253,123,193,168,251,35,64,249,249,107,67,169,247,99,66,169,245,91,65,169,243,83,197,168,192,3,95,214,204,204,204,204,243,83,190,169,
      245,11,0,249,253,123,191,169,253,3,0,145,72,0,0,208,2,1,38,145,243,3,0,170,245,3,1,42,72,0,0,208,1,33,38,145,67,32,0,145,64,2,128,82,46,0,0,148,244,3,0,170,52,1,0,180,168,0,0,176,8,1,64,249,239,3,20,170,0,1,63,214,225,3,21,42,224,3,19,170,128,
      2,63,214,3,0,0,20,224,3,19,170,156,15,0,148,253,123,193,168,245,11,64,249,243,83,194,168,192,3,95,214,204,204,204,204,243,83,191,169,253,123,191,169,253,3,0,145,72,0,0,208,2,113,38,145,72,0,0,208,1,129,38,145,244,3,0,170,67,16,0,145,0,3,128,
      82,18,0,0,148,243,3,0,170,51,1,0,180,168,0,0,176,8,1,64,249,239,3,19,170,0,1,63,214,225,3,20,170,160,0,128,146,96,2,63,214,2,0,0,20,160,0,0,24,253,123,193,168,243,83,193,168,192,3,95,214,31,32,3,213,37,2,0,192,204,204,204,204,243,83,187,169,
      245,91,1,169,247,99,2,169,249,107,3,169,251,35,0,249,253,123,189,169,253,3,0,145,136,0,0,208,22,65,58,145,225,19,0,249,219,130,2,145,26,124,64,211,245,3,2,170,105,123,122,248,248,3,3,170,191,59,3,213,151,0,0,208,235,6,64,249,106,21,0,18,105,
      1,9,202,32,45,202,154,31,4,0,177,160,8,0,84,160,8,0,181,191,2,24,235,32,7,0,84,72,0,0,208,8,1,15,145,137,0,0,144,41,161,21,145,232,39,1,169,138,0,0,144,89,161,18,145,180,70,64,184,211,122,116,248,191,59,3,213,147,0,0,180,127,6,0,177,1,9,0,84,
      39,0,0,20,23,121,116,248,2,0,129,82,40,253,223,200,1,0,128,210,224,3,23,170,0,1,63,214,243,3,0,170,147,6,0,181,40,255,223,200,0,1,63,214,31,92,1,113,193,2,0,84,72,0,0,208,1,33,36,145,226,0,128,210,224,3,23,170,48,15,0,148,0,2,0,52,72,0,0,208,
      1,97,36,145,226,0,128,210,224,3,23,170,42,15,0,148,64,1,0,52,137,0,0,144,40,161,21,145,8,253,223,200,2,0,128,82,1,0,128,210,224,3,23,170,0,1,63,214,243,3,0,170,115,3,0,181,200,14,20,139,9,0,128,146,9,253,159,200,191,59,3,213,232,39,65,169,191,
      2,24,235,65,250,255,84,136,0,0,208,11,5,64,249,105,21,0,18,10,8,128,82,74,1,9,75,105,15,26,139,8,0,128,146,8,45,202,154,11,1,11,202,43,253,159,200,191,59,3,213,0,0,128,210,253,123,195,168,251,35,64,249,249,107,67,169,247,99,66,169,245,91,65,
      169,243,83,197,168,192,3,95,214,202,14,20,139,72,253,95,200,83,253,9,200,201,255,255,53,191,59,3,213,200,0,0,180,136,0,0,144,8,225,16,145,9,253,223,200,224,3,19,170,32,1,63,214,136,0,0,144,8,65,19,145,225,19,64,249,9,253,223,200,224,3,19,170,
      32,1,63,214,128,251,255,180,136,0,0,208,10,5,64,249,11,8,128,82,73,21,0,18,105,1,9,75,8,44,201,154,105,15,26,139,10,1,10,202,42,253,159,200,191,59,3,213,221,255,255,23,243,83,190,169,245,91,1,169,253,123,191,169,253,3,0,145,136,0,0,240,20,129,
      0,145,19,0,128,82,149,0,0,240,22,5,128,210,104,126,64,211,0,81,22,155,2,0,128,82,1,244,129,82,215,254,255,151,32,1,0,52,168,82,66,185,115,6,0,17,127,58,0,113,9,5,0,17,169,82,2,185,163,254,255,84,32,0,128,82,4,0,0,20,0,0,128,82,8,0,0,148,0,0,
      128,82,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,204,204,204,204,243,83,189,169,245,91,1,169,247,19,0,249,253,123,191,169,253,3,0,145,150,0,0,208,211,82,66,185,211,1,0,52,136,0,0,208,20,129,0,145,104,0,0,240,21,
      65,15,145,23,5,128,210,115,6,0,81,169,254,223,200,96,82,23,155,32,1,63,214,200,82,66,185,9,5,0,81,201,82,2,185,51,255,255,53,32,0,128,82,253,123,193,168,247,19,64,249,245,91,65,169,243,83,195,168,192,3,95,214,204,204,204,204,136,0,0,208,8,129,
      0,145,9,124,64,147,10,5,128,210,32,33,10,155,106,0,0,240,73,129,15,145,40,253,223,200,0,1,31,214,204,204,204,204,136,0,0,208,8,129,0,145,9,124,64,147,10,5,128,210,32,33,10,155,106,0,0,240,73,97,21,145,40,253,223,200,0,1,31,214,204,204,204,204,
      253,123,191,169,253,3,0,145,104,0,0,240,8,97,19,145,9,253,223,200,32,1,63,214,136,0,0,208,0,45,1,249,31,0,0,241,224,7,159,26,253,123,193,168,192,3,95,214,136,0,0,208,31,45,1,249,32,0,128,82,192,3,95,214,253,123,191,169,253,3,0,145,8,0,0,144,
      0,193,21,145,112,254,255,151,136,0,0,176,0,33,0,185,31,4,0,49,97,0,0,84,0,0,128,82,7,0,0,20,129,0,0,148,128,0,0,181,0,0,128,82,6,0,0,148,250,255,255,23,32,0,128,82,253,123,193,168,192,3,95,214,204,204,204,204,243,15,31,248,253,123,191,169,253,
      3,0,145,147,0,0,176,96,34,64,185,31,4,0,49,128,0,0,84,93,254,255,151,8,0,128,18,104,34,0,185,32,0,128,82,253,123,193,168,243,7,65,248,192,3,95,214,243,83,190,169,245,11,0,249,253,123,191,169,253,3,0,145,104,0,0,240,8,161,18,145,9,253,223,200,
      32,1,63,214,243,3,0,42,149,0,0,176,160,34,64,185,31,4,0,49,224,0,0,84,77,254,255,151,128,0,0,180,31,4,0,177,244,3,128,154,30,0,0,20,160,34,64,185,1,0,128,146,74,254,255,151,96,0,0,53,20,0,128,210,24,0,0,20,1,121,128,210,32,0,128,210,160,8,0,
      148,244,3,0,170,160,34,64,185,212,0,0,181,1,0,128,210,63,254,255,151,0,0,128,210,41,5,0,148,244,255,255,23,225,3,20,170,58,254,255,151,192,0,0,53,160,34,64,185,1,0,128,210,54,254,255,151,224,3,20,170,247,255,255,23,224,3,20,170,230,0,0,148,0,
      0,128,210,28,5,0,148,104,0,0,240,8,33,23,145,9,253,223,200,224,3,19,42,32,1,63,214,212,0,0,180,224,3,20,170,253,123,193,168,245,11,64,249,243,83,194,168,192,3,95,214,248,246,255,151,204,204,204,204,243,83,191,169,253,123,191,169,253,3,0,145,
      148,0,0,176,128,34,64,185,31,4,0,49,0,1,0,84,23,254,255,151,243,3,0,170,147,0,0,180,127,6,0,177,32,4,0,84,28,0,0,20,128,34,64,185,1,0,128,146,19,254,255,151,128,3,0,52,1,121,128,210,32,0,128,210,107,8,0,148,243,3,0,170,128,34,64,185,211,0,0,
      181,1,0,128,210,10,254,255,151,0,0,128,210,244,4,0,148,17,0,0,20,225,3,19,170,5,254,255,151,192,0,0,53,128,34,64,185,1,0,128,210,1,254,255,151,224,3,19,170,247,255,255,23,224,3,19,170,177,0,0,148,0,0,128,210,231,4,0,148,224,3,19,170,253,123,
      193,168,243,83,193,168,192,3,95,214,202,246,255,151,204,204,204,204,243,83,190,169,245,11,0,249,253,123,191,169,253,3,0,145,104,0,0,240,8,161,18,145,9,253,223,200,32,1,63,214,243,3,0,42,149,0,0,176,160,34,64,185,31,4,0,49,224,0,0,84,227,253,
      255,151,128,0,0,180,31,4,0,177,244,3,128,154,30,0,0,20,160,34,64,185,1,0,128,146,224,253,255,151,96,0,0,53,20,0,128,210,24,0,0,20,1,121,128,210,32,0,128,210,54,8,0,148,244,3,0,170,160,34,64,185,212,0,0,181,1,0,128,210,213,253,255,151,0,0,128,
      210,191,4,0,148,244,255,255,23,225,3,20,170,208,253,255,151,192,0,0,53,160,34,64,185,1,0,128,210,204,253,255,151,224,3,20,170,247,255,255,23,224,3,20,170,124,0,0,148,0,0,128,210,178,4,0,148,104,0,0,240,8,33,23,145,9,253,223,200,224,3,19,42,32,
      1,63,214,224,3,20,170,253,123,193,168,245,11,64,249,243,83,194,168,192,3,95,214,204,204,204,204,243,83,191,169,253,123,191,169,253,3,0,145,148,0,0,176,128,34,64,185,31,4,0,49,96,1,0,84,175,253,255,151,243,3,0,170,19,1,0,180,128,34,64,185,1,0,
      128,210,174,253,255,151,224,3,19,170,146,0,0,148,224,3,19,170,150,4,0,148,253,123,193,168,243,83,193,168,192,3,95,214,243,83,190,169,245,11,0,249,253,123,191,169,253,3,0,145,149,0,0,176,160,34,64,185,244,3,1,170,31,4,0,49,128,1,0,84,153,253,
      255,151,243,3,0,170,19,1,0,180,127,6,0,177,33,4,0,84,0,0,128,210,253,123,193,168,245,11,64,249,243,83,194,168,192,3,95,214,160,34,64,185,1,0,128,146,145,253,255,151,0,255,255,52,1,121,128,210,32,0,128,210,233,7,0,148,243,3,0,170,160,34,64,185,
      211,0,0,181,1,0,128,210,136,253,255,151,0,0,128,210,114,4,0,148,237,255,255,23,225,3,19,170,131,253,255,151,192,0,0,53,160,34,64,185,1,0,128,210,127,253,255,151,224,3,19,170,247,255,255,23,224,3,19,170,47,0,0,148,0,0,128,210,101,4,0,148,8,121,
      128,210,128,78,8,155,223,255,255,23,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,243,3,0,170,147,0,0,180,85,0,0,148,224,3,19,170,89,4,0,148,253,123,193,168,243,7,65,248,192,3,95,214,204,204,204,204,243,83,191,169,253,123,191,169,
      253,3,0,145,244,3,0,170,128,74,64,249,243,3,1,170,192,1,0,180,229,13,0,148,136,0,0,208,8,117,67,249,128,74,64,249,31,0,8,235,0,1,0,84,136,0,0,176,8,193,21,145,31,0,8,235,128,0,0,84,8,16,64,185,72,0,0,53,25,14,0,148,147,74,0,249,115,0,0,180,224,
      3,19,170,151,13,0,148,253,123,193,168,243,83,193,168,192,3,95,214,204,204,204,204,253,123,187,169,253,3,0,145,232,99,0,145,224,15,0,249,232,31,0,249,168,0,128,82,232,35,0,185,232,99,0,145,232,35,0,249,136,0,128,82,169,0,128,82,233,163,4,41,136,
      0,0,208,8,161,27,145,233,195,0,145,232,27,0,249,43,0,128,82,233,39,0,249,137,0,128,82,11,40,0,185,227,131,0,145,233,47,0,185,72,0,0,144,9,1,20,145,232,15,64,249,226,227,0,145,225,147,0,145,224,67,0,145,9,1,0,249,234,15,64,249,136,0,0,176,8,193,
      0,145,75,169,3,185,233,15,64,249,40,69,0,249,234,15,64,249,105,8,128,82,73,121,1,121,232,15,64,249,9,133,3,121,232,15,64,249,31,209,1,249,68,0,0,148,227,163,0,145,226,3,1,145,225,179,0,145,224,67,0,145,85,0,0,148,253,123,197,168,192,3,95,214,
      253,123,188,169,253,3,0,145,232,163,0,145,224,163,2,169,168,0,128,82,232,23,0,185,169,0,128,82,232,163,0,145,233,27,0,185,232,31,0,249,137,0,128,82,136,0,128,82,232,167,3,41,9,0,64,249,72,0,0,144,8,1,20,145,63,1,8,235,128,0,0,84,224,3,9,170,
      243,3,0,148,224,23,64,249,0,56,64,249,240,3,0,148,232,23,64,249,0,45,64,249,237,3,0,148,232,23,64,249,0,49,64,249,234,3,0,148,232,23,64,249,0,53,64,249,231,3,0,148,232,23,64,249,0,37,64,249,228,3,0,148,232,23,64,249,0,41,64,249,225,3,0,148,232,
      23,64,249,0,61,64,249,222,3,0,148,232,23,64,249,0,65,64,249,219,3,0,148,232,23,64,249,0,225,65,249,216,3,0,148,227,83,0,145,226,195,0,145,225,99,0,145,224,67,0,145,51,0,0,148,227,115,0,145,226,227,0,145,225,131,0,145,224,67,0,145,74,0,0,148,
      253,123,196,168,192,3,95,214,204,204,204,204,243,83,191,169,253,123,190,169,253,3,0,145,244,3,2,170,243,3,3,170,179,11,0,249,32,0,64,185,51,254,255,151,136,2,64,249,9,1,64,249,42,69,64,249,73,253,95,136,41,5,0,17,73,253,8,136,168,255,255,53,
      191,59,3,213,96,2,64,185,51,254,255,151,253,123,194,168,243,83,193,168,192,3,95,214,204,204,204,204,243,83,191,169,253,123,190,169,253,3,0,145,244,3,2,170,243,3,3,170,179,11,0,249,32,0,64,185,29,254,255,151,136,6,64,249,9,1,64,249,138,2,64,249,
      33,1,64,249,64,1,64,249,83,255,255,151,96,2,64,185,31,254,255,151,253,123,194,168,243,83,193,168,192,3,95,214,204,204,204,204,243,83,191,169,253,123,190,169,253,3,0,145,244,3,2,170,243,3,3,170,179,11,0,249,32,0,64,185,9,254,255,151,136,2,64,
      249,9,1,64,249,32,69,64,249,128,1,0,180,8,252,95,136,8,5,0,81,8,252,9,136,169,255,255,53,191,59,3,213,200,0,0,53,136,0,0,176,8,193,0,145,31,0,8,235,64,0,0,84,138,3,0,148,96,2,64,185,2,254,255,151,253,123,194,168,243,83,193,168,192,3,95,214,243,
      83,191,169,253,123,190,169,253,3,0,145,243,3,2,170,244,3,3,170,180,11,0,249,32,0,64,185,237,253,255,151,104,2,64,249,19,1,64,249,96,74,64,249,192,1,0,180,16,13,0,148,96,74,64,249,136,0,0,208,8,117,67,249,31,0,8,235,0,1,0,84,136,0,0,176,8,193,
      21,145,31,0,8,235,128,0,0,84,8,16,64,185,72,0,0,53,68,13,0,148,127,74,0,249,128,2,64,185,227,253,255,151,253,123,194,168,243,83,193,168,192,3,95,214,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,224,0,128,82,208,253,255,151,19,0,
      128,82,0,0,128,82,163,14,0,148,128,0,0,53,29,0,0,148,92,0,0,148,51,0,128,82,224,0,128,82,209,253,255,151,224,3,19,42,253,123,193,168,243,7,65,248,192,3,95,214,204,204,204,204,204,204,204,204,243,83,191,169,253,123,191,169,253,3,0,145,136,0,0,
      208,20,129,9,145,19,0,128,210,96,106,116,248,96,0,0,180,116,14,0,148,127,106,52,248,115,34,0,145,127,2,16,241,67,255,255,84,32,0,128,82,253,123,193,168,243,83,193,168,192,3,95,214,204,204,204,204,243,83,188,169,245,91,1,169,247,99,2,169,249,
      27,0,249,253,123,184,169,253,3,0,145,104,0,0,240,8,129,19,145,9,253,223,200,224,67,0,145,32,1,63,214,232,167,64,121,168,5,0,52,232,47,64,249,104,5,0,180,20,1,64,185,9,0,132,82,21,17,0,145,159,10,64,113,147,178,137,26,224,3,19,42,111,14,0,148,
      136,0,0,208,8,97,70,185,180,194,52,139,127,2,8,107,22,193,147,26,214,3,0,52,137,0,0,208,56,129,9,145,104,0,0,240,23,129,18,145,19,0,128,210,25,9,128,210,128,134,64,248,31,4,0,177,32,2,0,84,31,8,0,177,224,1,0,84,169,2,64,57,169,1,0,54,137,0,24,
      55,232,254,223,200,0,1,63,214,32,1,0,52,104,254,70,147,8,123,104,248,105,22,64,146,42,33,25,155,137,130,95,248,73,21,0,249,168,2,64,57,72,225,0,57,115,6,0,145,181,6,0,145,214,6,0,81,86,253,255,53,253,123,200,168,249,27,64,249,247,99,66,169,245,
      91,65,169,243,83,196,168,192,3,95,214,204,204,204,204,243,83,187,169,245,91,1,169,247,99,2,169,249,107,3,169,251,35,0,249,253,123,191,169,253,3,0,145,136,0,0,208,23,129,9,145,104,0,0,240,25,161,19,145,105,0,0,240,56,129,18,145,20,0,128,82,58,
      0,128,146,10,9,128,210,59,16,128,82,150,126,64,147,200,254,70,147,232,122,104,248,201,22,64,146,51,33,10,155,105,22,64,249,42,9,0,145,95,5,0,241,169,0,0,84,104,226,64,57,8,1,25,50,104,226,0,57,40,0,0,20,123,226,0,57,244,0,0,52,159,6,0,113,96,
      0,0,84,96,1,128,18,4,0,0,20,64,1,128,18,2,0,0,20,32,1,128,18,40,255,223,200,0,1,63,214,245,3,0,170,168,6,0,145,31,5,0,241,9,2,0,84,8,255,223,200,0,1,63,214,160,1,0,52,8,28,0,18,117,22,0,249,31,9,0,113,129,0,0,84,104,226,64,57,8,1,26,50,230,255,
      255,23,31,13,0,113,161,1,0,84,104,226,64,57,8,1,29,50,225,255,255,23,104,226,64,57,122,22,0,249,8,1,26,50,104,226,0,57,136,0,0,208,9,125,67,249,105,0,0,180,40,121,118,248,26,25,0,185,148,6,0,17,159,14,0,113,10,9,128,210,33,249,255,84,253,123,
      193,168,251,35,64,249,249,107,67,169,247,99,66,169,245,91,65,169,243,83,197,168,192,3,95,214,204,204,204,204,204,204,204,204,253,123,191,169,253,3,0,145,104,0,0,240,8,65,17,145,9,253,223,200,32,1,63,214,136,0,0,208,0,53,3,249,104,0,0,240,8,97,
      17,145,9,253,223,200,32,1,63,214,136,0,0,208,0,57,3,249,32,0,128,82,253,123,193,168,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,78,253,255,151,136,0,0,208,1,161,26,145,15,1,0,148,253,123,193,168,192,3,95,214,204,204,204,204,204,
      204,204,204,243,83,191,169,253,123,191,169,253,3,0,145,148,0,0,208,136,210,90,57,104,2,0,53,136,0,0,208,19,97,26,145,137,0,0,176,40,193,0,145,104,10,0,249,137,0,0,176,41,193,13,145,136,0,0,176,8,129,9,145,104,38,0,169,114,253,255,151,226,3,0,
      170,64,0,128,18,99,66,0,145,33,0,128,82,39,1,0,148,40,0,128,82,136,210,26,57,32,0,128,82,253,123,193,168,243,83,193,168,192,3,95,214,253,123,189,169,243,83,1,169,245,91,2,169,253,3,0,145,112,213,255,151,255,67,0,209,243,3,1,170,157,0,0,148,244,
      3,0,42,52,18,0,52,136,0,0,176,22,1,18,145,10,0,128,82,11,6,128,210,85,125,64,211,168,126,11,155,9,105,118,184,63,1,20,107,96,10,0,84,74,5,0,17,95,21,0,113,35,255,255,84,8,189,159,82,159,2,8,107,96,9,0,84,104,0,0,240,8,33,21,145,9,253,223,200,
      128,62,0,83,32,1,63,214,160,8,0,52,40,189,159,82,159,2,8,107,97,1,0,84,104,6,0,185,127,18,1,249,127,26,0,185,127,58,0,121,127,10,0,185,127,194,0,248,127,22,0,185,224,3,19,170,112,1,0,148,113,0,0,20,104,0,0,240,8,33,17,145,9,253,223,200,225,3,
      0,145,224,3,20,42,32,1,63,214,192,5,0,52,34,32,128,82,1,0,128,82,96,98,0,145,82,214,255,151,232,3,64,185,116,6,0,185,127,18,1,249,31,9,0,113,65,4,0,84,232,27,64,57,234,27,0,145,72,2,0,52,72,5,64,57,8,2,0,52,73,1,64,57,63,1,8,107,104,1,0,84,8,
      1,9,75,12,5,0,17,43,5,0,17,105,66,43,139,140,5,0,81,107,5,0,17,40,97,64,57,8,1,30,50,40,97,0,57,76,255,255,53,72,45,64,56,8,254,255,53,42,0,128,82,74,5,0,17,105,66,42,139,95,253,3,113,40,97,64,57,8,1,29,50,40,97,0,57,67,255,255,84,96,6,64,185,
      45,1,0,148,41,0,128,82,96,18,1,249,2,0,0,20,9,0,128,82,105,10,0,185,200,255,255,23,136,0,0,208,8,177,70,185,8,7,0,53,0,0,128,18,57,0,0,20,34,32,128,82,1,0,128,82,96,98,0,145,32,214,255,151,136,0,0,176,6,193,17,145,13,0,128,210,142,0,128,82,197,
      0,128,210,199,66,0,145,168,54,5,155,236,12,8,139,139,1,64,57,139,2,0,52,136,5,64,57,72,2,0,52,127,1,8,107,200,1,0,84,111,5,0,17,255,5,4,113,98,1,0,84,106,66,47,139,168,105,102,56,107,5,0,17,239,5,0,17,73,97,64,57,40,1,8,42,72,97,0,57,136,5,64,
      57,127,1,8,107,169,254,255,84,139,45,64,56,203,253,255,53,173,5,0,145,206,5,0,81,238,252,255,53,40,0,128,82,116,162,0,41,224,3,20,42,250,0,0,148,10,0,128,210,96,18,1,249,108,50,0,145,203,0,128,82,14,3,128,210,205,18,0,145,168,42,14,155,107,5,
      0,81,74,5,0,145,169,121,104,120,137,37,0,120,107,255,255,53,144,255,255,23,224,3,19,170,51,0,0,148,0,0,128,82,255,67,0,145,222,212,255,151,245,91,66,169,243,83,65,169,253,123,195,168,192,3,95,214,204,204,204,204,243,15,31,248,253,123,189,169,
      253,3,0,145,243,3,0,42,224,67,0,145,1,0,128,210,186,240,255,151,137,0,0,208,63,177,6,185,127,10,0,49,193,0,0,84,40,0,128,82,40,177,6,185,105,0,0,240,40,33,19,145,7,0,0,20,127,14,0,49,33,1,0,84,40,0,128,82,40,177,6,185,105,0,0,208,40,1,17,145,
      9,253,223,200,32,1,63,214,243,3,0,42,7,0,0,20,127,18,0,49,161,0,0,84,40,0,128,82,40,177,6,185,232,15,64,249,19,13,64,185,232,163,64,57,168,0,0,52,234,11,64,249,72,169,67,185,9,121,30,18,73,169,3,185,224,3,19,42,253,123,195,168,243,7,65,248,192,
      3,95,214,243,15,31,248,253,123,191,169,253,3,0,145,243,3,0,170,96,98,0,145,34,32,128,82,1,0,128,82,179,213,255,151,136,0,0,144,12,193,0,145,127,66,0,248,138,97,0,145,127,18,1,249,107,98,0,145,127,194,0,248,41,32,128,82,127,22,0,185,72,21,64,
      56,41,5,0,81,104,21,0,56,169,255,255,53,138,101,4,145,107,102,4,145,9,32,128,82,72,21,64,56,41,5,0,81,104,21,0,56,169,255,255,53,253,123,193,168,243,7,65,248,192,3,95,214,204,204,204,204,243,83,190,169,245,11,0,249,253,123,191,169,253,3,0,145,
      244,3,0,170,245,3,1,170,137,170,67,185,136,0,0,144,8,253,71,185,63,1,8,106,160,0,0,84,136,74,64,249,104,0,0,180,147,70,64,249,29,0,0,20,160,0,128,82,230,251,255,151,147,70,64,249,168,2,64,249,127,2,8,235,160,2,0,84,179,1,0,180,104,254,95,136,
      8,5,0,81,104,254,9,136,169,255,255,53,191,59,3,213,232,0,0,53,136,0,0,144,8,193,0,145,127,2,8,235,96,0,0,84,224,3,19,170,101,1,0,148,179,2,64,249,147,70,0,249,105,254,95,136,41,5,0,17,105,254,8,136,168,255,255,53,191,59,3,213,160,0,128,82,214,
      251,255,151,211,0,0,180,224,3,19,170,253,123,193,168,245,11,64,249,243,83,194,168,192,3,95,214,61,243,255,151,243,83,190,169,245,11,0,249,255,195,9,209,253,123,0,169,253,3,0,145,243,3,0,42,226,15,0,249,52,28,0,83,227,23,0,249,225,3,3,170,224,
      3,2,170,195,255,255,151,224,3,19,42,119,255,255,151,232,15,64,249,245,3,0,42,8,69,64,249,9,5,64,185,191,2,9,107,97,0,0,84,0,0,128,82,76,0,0,20,0,69,128,210,231,9,0,148,243,3,0,170,179,0,0,181,0,0,128,210,57,1,0,148,0,0,128,18,68,0,0,20,232,15,
      64,249,224,3,1,145,2,69,128,82,1,69,64,249,118,212,255,151,225,3,1,145,2,69,128,82,224,3,19,170,114,212,255,151,225,3,19,170,127,2,0,185,224,3,21,42,182,254,255,151,245,3,0,42,191,6,0,49,193,0,0,84,66,4,0,148,200,2,128,82,8,0,0,185,224,3,19,
      170,233,255,255,23,84,0,0,53,148,9,0,148,232,15,64,249,10,69,64,249,72,253,95,136,8,5,0,81,72,253,9,136,169,255,255,53,191,59,3,213,8,1,0,53,232,15,64,249,0,69,64,249,137,0,0,144,40,193,0,145,31,0,8,235,64,0,0,84,17,1,0,148,40,0,128,82,104,2,
      0,185,232,15,64,249,19,69,0,249,233,15,64,249,136,0,0,144,8,253,71,185,42,169,67,185,95,1,8,106,33,2,0,84,232,99,0,145,233,163,0,145,232,39,3,169,168,0,128,82,169,0,128,82,232,39,4,41,227,131,0,145,226,195,0,145,225,147,0,145,224,67,0,145,166,
      0,0,148,180,0,0,52,232,23,64,249,9,1,64,249,138,0,0,144,73,105,3,249,0,0,128,210,245,0,0,148,224,3,21,42,253,123,64,169,255,195,9,145,245,11,64,249,243,83,194,168,192,3,95,214,31,144,14,113,0,2,0,84,31,160,14,113,96,1,0,84,31,212,14,113,192,
      0,0,84,72,0,0,144,8,225,39,145,31,216,14,113,224,19,136,154,9,0,0,20,72,0,0,144,0,161,39,145,6,0,0,20,72,0,0,144,0,97,39,145,3,0,0,20,72,0,0,144,0,33,39,145,192,3,95,214,253,123,190,169,243,11,0,249,253,3,0,145,215,211,255,151,255,131,28,209,
      243,3,0,170,96,6,64,185,40,189,159,82,31,0,8,107,192,10,0,84,104,0,0,208,8,33,17,145,9,253,223,200,225,67,0,145,32,1,63,214,0,10,0,52,8,0,128,82,233,163,0,145,40,21,0,56,8,5,0,17,31,1,4,113,163,255,255,84,11,4,128,82,232,91,64,57,235,163,0,57,
      233,91,0,145,136,1,0,52,236,163,0,145,42,5,64,57,5,0,0,20,31,1,4,113,162,0,0,84,139,73,40,56,8,5,0,17,31,1,10,107,105,255,255,84,40,45,64,56,232,254,255,53,101,6,64,185,6,0,128,82,228,163,12,145,3,32,128,82,226,163,0,145,33,0,128,82,0,0,128,
      210,57,12,0,148,103,6,64,185,6,32,128,82,97,18,65,249,229,163,4,145,255,3,0,185,4,32,128,82,227,163,0,145,2,32,128,82,0,0,128,210,161,12,0,148,103,6,64,185,6,32,128,82,97,18,65,249,229,163,8,145,255,3,0,185,4,32,128,82,227,163,0,145,2,64,128,
      82,0,0,128,210,151,12,0,148,44,0,128,82,10,0,128,210,238,163,12,145,13,32,128,82,239,163,4,145,231,163,8,145,201,37,64,120,233,0,0,54,105,66,44,139,75,105,111,56,40,97,64,57,8,1,28,50,40,97,0,57,9,0,0,20,233,0,8,54,105,66,44,139,75,105,103,56,
      40,97,64,57,8,1,27,50,40,97,0,57,2,0,0,20,11,0,128,82,72,1,19,139,11,101,4,57,74,5,0,145,140,5,0,17,173,5,0,81,109,253,255,53,32,0,0,20,12,0,128,210,11,4,128,82,13,32,128,82,104,133,1,81,31,101,0,113,8,1,0,84,104,125,0,81,106,66,40,139,110,29,
      0,83,73,97,64,57,40,1,28,50,72,97,0,57,13,0,0,20,104,5,2,81,31,101,0,113,40,1,0,84,104,125,0,81,106,66,40,139,73,97,64,57,40,1,27,50,105,1,3,17,72,97,0,57,46,29,0,83,2,0,0,20,14,0,128,82,104,2,12,139,14,101,4,57,107,5,0,17,140,5,0,145,173,5,
      0,81,173,252,255,53,255,131,28,145,99,211,255,151,243,11,64,249,253,123,194,168,192,3,95,214,204,204,204,204,243,83,190,169,245,91,1,169,253,123,190,169,253,3,0,145,244,3,2,170,246,3,3,170,182,11,0,249,32,0,64,185,190,250,255,151,136,2,64,249,
      9,1,64,249,42,69,64,249,65,97,0,145,136,0,0,176,21,97,26,145,160,2,64,249,32,1,0,180,161,0,0,180,34,32,128,82,135,211,255,151,211,2,128,82,8,0,0,20,34,32,128,210,1,0,128,82,82,212,255,151,89,3,0,148,211,2,128,82,19,0,0,185,154,0,0,148,136,2,
      64,249,9,1,64,249,42,69,64,249,76,101,4,145,171,6,64,249,139,1,0,180,236,0,0,180,138,1,4,145,136,37,193,168,104,37,129,168,159,1,10,235,161,255,255,84,8,0,0,20,104,1,4,145,127,125,129,168,127,1,8,235,193,255,255,84,68,3,0,148,19,0,0,185,134,
      0,0,148,136,6,64,249,9,1,64,249,42,1,64,249,72,253,95,136,8,5,0,81,72,253,9,136,169,255,255,53,191,59,3,213,40,1,0,53,136,6,64,249,9,1,64,249,32,1,64,249,136,0,0,144,8,193,0,145,31,0,8,235,64,0,0,84,21,0,0,148,136,2,64,249,11,1,64,249,137,6,
      64,249,42,1,64,249,104,69,64,249,72,1,0,249,137,2,64,249,42,1,64,249,75,69,64,249,105,253,95,136,41,5,0,17,105,253,8,136,168,255,255,53,191,59,3,213,192,2,64,185,127,250,255,151,253,123,194,168,245,91,65,169,243,83,194,168,192,3,95,214,243,15,
      31,248,253,123,191,169,253,3,0,145,226,3,0,170,34,2,0,180,136,0,0,176,0,45,65,249,104,0,0,208,8,65,20,145,9,253,223,200,1,0,128,82,32,1,63,214,32,1,0,53,104,0,0,208,8,161,18,145,9,253,223,200,32,1,63,214,55,3,0,148,243,3,0,42,9,3,0,148,19,0,
      0,185,253,123,193,168,243,7,65,248,192,3,95,214,243,83,189,169,245,91,1,169,247,19,0,249,253,123,191,169,253,3,0,145,244,3,0,170,245,3,1,170,159,2,21,235,64,4,0,84,243,3,20,170,151,0,0,240,118,2,64,249,246,0,0,180,232,2,64,249,239,3,22,170,0,
      1,63,214,192,2,63,214,8,28,0,83,136,0,0,52,115,66,0,145,127,2,21,235,193,254,255,84,127,2,21,235,96,2,0,84,127,2,20,235,224,1,0,84,115,34,0,209,104,130,95,248,8,1,0,180,117,2,64,249,213,0,0,180,232,2,64,249,239,3,21,170,0,1,63,214,0,0,128,82,
      160,2,63,214,115,66,0,209,104,34,0,145,31,1,20,235,129,254,255,84,0,0,128,82,2,0,0,20,32,0,128,82,253,123,193,168,247,19,64,249,245,91,65,169,243,83,195,168,192,3,95,214,243,83,190,169,245,91,1,169,253,123,191,169,253,3,0,145,244,3,0,170,243,
      3,1,170,159,2,19,235,128,1,0,84,150,0,0,240,117,130,95,248,213,0,0,180,200,2,64,249,239,3,21,170,0,1,63,214,0,0,128,82,160,2,63,214,115,66,0,209,127,2,20,235,225,254,255,84,32,0,128,82,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,
      4,0,128,210,3,0,128,82,2,0,128,210,1,0,128,210,0,0,128,210,83,0,0,20,136,0,0,176,0,93,3,249,192,3,95,214,204,204,204,204,243,83,188,169,245,91,1,169,247,99,2,169,249,27,0,249,253,123,191,169,253,3,0,145,244,3,5,170,245,3,0,170,128,2,64,249,246,
      3,1,170,247,3,2,170,248,3,3,42,249,3,4,170,128,0,0,181,224,3,20,170,87,239,255,151,96,2,0,180,19,220,65,249,51,2,0,180,136,0,0,240,8,1,64,249,239,3,19,170,0,1,63,214,228,3,25,170,227,3,24,42,226,3,23,170,225,3,22,170,224,3,21,170,96,2,63,214,
      253,123,193,168,249,27,64,249,247,99,66,169,245,91,65,169,243,83,196,168,192,3,95,214,136,0,0,176,0,225,26,145,225,3,20,170,14,0,0,148,136,0,0,144,10,5,64,249,9,0,64,249,75,21,0,18,42,1,10,202,83,45,203,154,211,252,255,181,228,3,25,170,227,3,
      24,42,226,3,23,170,225,3,22,170,224,3,21,170,89,228,255,151,243,83,191,169,253,123,191,169,253,3,0,145,243,3,1,170,104,66,64,57,244,3,0,170,200,1,0,53,104,0,0,208,8,161,18,145,9,253,223,200,32,1,63,214,40,0,128,82,127,6,0,249,104,66,0,57,105,
      0,0,208,40,33,23,145,9,253,223,200,32,1,63,214,8,0,128,210,2,0,0,20,104,6,64,249,128,14,8,139,253,123,193,168,243,83,193,168,192,3,95,214,204,204,204,204,253,123,187,169,253,3,0,145,136,0,0,176,8,17,71,185,255,11,0,249,255,131,0,57,255,227,0,
      57,255,3,1,57,255,35,1,57,8,1,0,53,136,0,0,144,8,33,27,145,9,41,64,169,235,163,0,145,105,41,0,169,43,0,128,82,235,227,0,57,229,67,0,145,160,255,255,151,224,67,0,145,148,238,255,151,253,123,197,168,192,3,95,214,204,204,204,204,136,0,0,176,8,1,
      27,145,0,1,0,169,0,1,1,169,192,3,95,214,204,204,204,204,253,123,190,169,253,3,0,145,227,83,0,145,226,67,0,145,225,99,0,145,224,67,0,145,104,0,128,82,105,0,128,82,232,167,2,41,183,0,0,148,253,123,194,168,192,3,95,214,243,83,188,169,245,91,1,169,
      247,99,2,169,249,27,0,249,251,31,0,249,253,123,189,169,253,3,0,145,244,3,0,42,21,0,128,210,27,0,128,82,51,0,128,82,179,67,0,57,159,34,0,113,44,1,0,84,96,2,0,84,159,10,0,113,160,1,0,84,159,18,0,113,224,1,0,84,159,26,0,113,32,1,0,84,36,0,0,20,
      159,46,0,113,64,1,0,84,159,62,0,113,128,0,0,84,136,86,0,81,31,5,0,113,168,3,0,84,224,3,20,42,124,0,0,148,247,3,0,170,33,0,0,20,29,250,255,151,245,3,0,170,53,1,0,181,0,0,128,18,253,123,195,168,251,31,64,249,249,27,64,249,247,99,66,169,245,91,
      65,169,243,83,196,168,192,3,95,214,169,2,64,249,40,0,0,240,8,225,66,249,42,17,8,139,5,0,0,20,40,5,64,185,31,1,20,107,160,0,0,84,41,65,0,145,63,1,10,235,97,255,255,84,9,0,128,210,201,0,0,181,1,2,0,148,200,2,128,82,8,0,0,185,66,255,255,151,231,
      255,255,23,55,33,0,145,19,0,128,82,191,67,0,57,24,0,128,210,115,0,0,52,96,0,128,82,74,249,255,151,246,2,64,249,136,0,0,144,25,33,0,145,179,0,0,52,40,3,64,249,11,21,0,18,202,2,8,202,86,45,203,154,223,6,0,241,96,4,0,84,214,8,0,180,159,46,0,113,
      232,1,0,84,8,34,129,82,8,37,212,26,136,1,0,54,184,6,64,249,184,19,0,249,191,6,0,249,159,34,0,113,193,2,0,84,122,249,255,151,27,16,64,185,187,23,0,185,119,249,255,151,136,17,128,82,8,16,0,185,159,34,0,113,193,1,0,84,40,0,0,240,9,229,66,249,168,
      2,64,249,10,17,9,139,43,0,0,240,105,233,66,249,72,17,9,139,170,15,0,249,95,1,8,235,192,0,0,84,95,5,0,249,74,65,0,145,251,255,255,23,40,3,64,249,232,2,0,249,115,0,0,52,96,0,128,82,37,249,255,151,223,6,0,241,97,0,0,84,0,0,128,82,174,255,255,23,
      159,34,0,113,97,1,0,84,90,249,255,151,243,3,0,170,239,3,22,170,136,0,0,240,8,1,64,249,0,1,63,214,97,18,64,185,0,1,128,82,192,2,63,214,7,0,0,20,239,3,22,170,136,0,0,240,8,1,64,249,0,1,63,214,224,3,20,42,192,2,63,214,159,46,0,113,104,253,255,84,
      8,34,129,82,8,37,212,26,8,253,7,54,184,6,0,249,159,34,0,113,161,252,255,84,66,249,255,151,27,16,0,185,226,255,255,23,115,0,0,52,96,0,128,82,1,249,255,151,96,0,128,82,169,232,255,151,31,8,0,113,192,2,0,84,31,24,0,113,32,2,0,84,31,60,0,113,96,
      1,0,84,31,84,0,113,160,0,0,84,31,88,0,113,96,1,0,84,0,0,128,210,14,0,0,20,136,0,0,176,8,1,27,145,0,33,0,145,10,0,0,20,136,0,0,176,8,1,27,145,0,97,0,145,6,0,0,20,136,0,0,176,0,65,27,145,3,0,0,20,136,0,0,176,0,1,27,145,192,3,95,214,243,83,191,
      169,253,123,190,169,253,3,0,145,243,3,3,170,179,11,0,249,32,0,64,185,212,248,255,151,136,0,0,144,9,5,64,249,43,21,0,18,138,0,0,176,72,105,67,249,42,1,8,202,84,45,203,154,96,2,64,185,213,248,255,151,224,3,20,170,253,123,194,168,243,83,193,168,
      192,3,95,214,136,0,0,176,0,113,3,249,192,3,95,214,204,204,204,204,32,0,128,82,97,0,0,20,243,15,31,248,253,123,187,169,253,3,0,145,136,0,0,176,8,17,71,185,255,11,0,249,255,131,0,57,243,3,0,170,255,227,0,57,255,3,1,57,255,35,1,57,8,1,0,53,136,
      0,0,144,8,33,27,145,9,41,64,169,235,163,0,145,105,41,0,169,43,0,128,82,235,227,0,57,179,0,0,181,0,0,128,82,75,0,0,148,243,3,0,42,16,0,0,20,225,67,0,145,224,3,19,170,20,0,0,148,96,0,0,52,19,0,128,18,10,0,0,20,104,22,64,185,191,59,3,213,8,45,11,
      83,168,0,0,52,224,3,19,170,119,11,0,148,32,11,0,148,224,254,255,53,19,0,128,82,224,67,0,145,136,237,255,151,224,3,19,42,253,123,197,168,243,7,65,248,192,3,95,214,204,204,204,204,243,83,190,169,245,91,1,169,253,123,191,169,253,3,0,145,243,3,0,
      170,105,22,64,185,246,3,1,170,191,59,3,213,40,5,0,18,31,9,0,113,65,4,0,84,63,5,26,114,0,4,0,84,104,82,64,169,21,1,20,75,116,2,0,249,191,2,0,113,127,18,0,185,77,3,0,84,89,11,0,148,227,3,22,170,226,3,21,42,225,3,20,170,65,12,0,148,191,2,0,107,
      32,1,0,84,106,82,0,145,73,253,95,136,41,1,28,50,73,253,8,136,168,255,255,53,191,59,3,213,0,0,128,18,12,0,0,20,104,22,64,185,191,59,3,213,8,9,2,83,232,0,0,52,106,82,0,145,73,253,95,136,41,121,30,18,73,253,8,136,168,255,255,53,191,59,3,213,0,0,
      128,82,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,253,123,188,169,253,3,0,145,232,83,0,145,224,67,0,57,232,23,0,249,233,67,0,145,232,99,0,145,233,35,3,169,8,1,128,82,255,255,2,41,9,1,128,82,233,163,3,41,227,115,
      0,145,226,163,0,145,225,131,0,145,224,71,0,145,58,0,0,148,232,67,64,57,233,23,64,185,31,1,0,113,232,27,64,185,32,17,136,26,253,123,196,168,192,3,95,214,243,83,191,169,253,123,190,169,253,3,0,145,243,3,2,170,244,3,3,170,180,11,0,249,32,0,64,249,
      103,0,0,148,106,6,64,249,104,2,64,249,9,1,64,249,41,4,0,180,41,21,64,185,191,59,3,213,40,53,13,83,168,3,0,52,40,5,0,18,31,9,0,113,97,0,0,84,63,5,26,114,65,0,0,84,9,2,88,54,104,10,64,249,9,1,64,57,233,0,0,53,104,2,64,249,8,1,64,249,9,21,64,185,
      191,59,3,213,40,5,1,83,200,1,0,52,104,2,64,249,0,1,64,249,103,255,255,151,31,4,0,49,192,0,0,84,106,6,64,249,72,1,64,185,9,5,0,17,73,1,0,185,4,0,0,20,104,14,64,249,9,0,128,18,9,1,0,185,128,2,64,249,71,0,0,148,253,123,194,168,243,83,193,168,192,
      3,95,214,204,204,204,204,243,83,190,169,245,91,1,169,253,123,186,169,253,3,0,145,245,3,2,170,244,3,3,170,180,31,0,249,32,0,64,185,14,248,255,151,136,0,0,144,19,125,67,249,137,0,0,144,40,241,134,185,118,14,8,139,179,27,0,249,127,2,22,235,160,
      4,0,84,104,2,64,249,168,15,0,249,170,2,64,249,200,1,0,180,9,21,64,185,191,59,3,213,40,53,13,83,72,1,0,52,40,5,0,18,31,9,0,113,97,0,0,84,63,5,26,114,225,0,0,84,201,0,88,55,72,1,64,185,9,5,0,17,73,1,0,185,115,34,0,145,235,255,255,23,170,10,64,
      249,169,6,64,249,168,2,64,249,171,99,0,145,171,35,0,249,168,39,0,249,169,43,0,249,170,47,0,249,168,15,64,249,168,19,0,249,168,23,0,249,163,131,0,145,162,3,1,145,161,163,0,145,160,67,0,145,155,255,255,151,238,255,255,23,128,2,64,185,234,247,255,
      151,253,123,198,168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,104,0,0,176,8,129,15,145,9,253,223,200,0,192,0,145,32,1,31,214,204,204,204,204,104,0,0,176,8,97,21,145,9,253,223,200,0,192,0,145,32,1,31,214,204,204,204,204,204,204,
      204,204,204,204,204,204,243,83,188,169,245,91,1,169,247,99,2,169,249,107,3,169,253,123,191,169,253,3,0,145,147,0,0,144,104,242,70,185,104,0,0,53,8,64,128,82,4,0,0,20,31,13,0,113,105,0,128,82,40,177,136,26,1,1,128,210,104,242,6,185,0,125,64,147,
      187,0,0,148,152,0,0,144,0,127,3,249,0,0,128,210,71,253,255,151,8,127,67,249,168,1,0,181,104,0,128,82,1,1,128,210,104,242,6,185,96,0,128,210,176,0,0,148,0,127,3,249,0,0,128,210,61,253,255,151,8,127,67,249,104,0,0,181,0,0,128,18,31,0,0,20,104,
      0,0,240,19,129,27,145,137,0,0,144,55,129,9,145,21,0,128,210,22,0,128,210,116,0,128,82,26,9,128,210,57,0,128,18,2,0,128,82,1,244,129,82,96,194,0,145,68,246,255,151,8,127,67,249,169,254,70,147,170,22,64,146,211,106,40,248,232,122,105,248,73,33,
      26,155,43,21,64,249,104,9,0,145,31,9,0,241,72,0,0,84,121,26,0,185,181,6,0,145,214,34,0,145,115,98,1,145,148,6,0,81,180,253,255,53,0,0,128,82,253,123,193,168,249,107,67,169,247,99,66,169,245,91,65,169,243,83,196,168,192,3,95,214,243,83,190,169,
      245,91,1,169,253,123,191,169,253,3,0,145,190,254,255,151,199,13,0,148,104,0,0,176,21,65,15,145,19,0,128,210,116,0,128,82,150,0,0,144,200,126,67,249,96,106,104,248,245,13,0,148,200,126,67,249,105,106,104,248,170,254,223,200,32,193,0,145,64,1,
      63,214,115,34,0,145,148,6,0,81,212,254,255,53,192,126,67,249,253,252,255,151,223,126,3,249,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,253,123,190,169,253,3,0,145,232,3,18,170,255,19,0,185,9,49,64,249,42,17,64,249,
      75,9,64,185,235,0,248,55,224,67,0,145,133,246,255,151,232,19,64,185,0,0,128,82,31,5,0,113,64,0,0,84,32,0,128,82,253,123,194,168,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,2,248,255,151,128,0,0,181,104,0,0,240,0,161,31,145,2,0,0,
      20,0,128,0,145,253,123,193,168,192,3,95,214,253,123,191,169,253,3,0,145,248,247,255,151,128,0,0,181,104,0,0,240,0,177,31,145,2,0,0,20,0,144,0,145,253,123,193,168,192,3,95,214,243,15,31,248,253,123,191,169,253,3,0,145,243,3,0,42,236,247,255,151,
      128,0,0,181,104,0,0,240,8,177,31,145,2,0,0,20,8,144,0,145,224,3,19,42,19,1,0,185,12,0,0,148,243,3,0,42,226,247,255,151,128,0,0,181,104,0,0,240,8,161,31,145,2,0,0,20,8,128,0,145,19,1,0,185,253,123,193,168,243,7,65,248,192,3,95,214,72,0,0,144,
      11,65,2,145,10,0,128,82,76,125,64,211,136,241,125,211,9,105,107,184,31,0,9,107,224,1,0,84,74,5,0,17,95,181,0,113,35,255,255,84,8,76,0,81,31,69,0,113,104,0,0,84,160,1,128,82,10,0,0,20,8,240,2,81,31,57,0,113,10,1,128,82,201,2,128,82,64,145,137,
      26,4,0,0,20,137,241,125,211,104,17,0,145,32,105,104,184,192,3,95,214,243,83,191,169,253,123,191,169,253,3,0,145,243,3,1,170,52,0,128,82,96,54,0,185,116,226,0,57,223,255,255,151,96,46,0,185,116,194,0,57,253,123,193,168,243,83,193,168,192,3,95,
      214,204,204,204,204,243,83,190,169,245,11,0,249,253,123,191,169,253,3,0,145,160,0,0,180,232,3,128,146,8,9,192,154,31,1,1,235,131,3,0,84,8,124,1,155,41,0,128,210,149,0,0,144,160,46,65,249,31,1,0,241,51,1,136,154,104,0,0,176,8,33,20,145,9,253,
      223,200,226,3,19,170,1,1,128,82,32,1,63,214,96,2,0,181,104,0,0,176,20,33,20,145,160,13,0,148,96,1,0,52,224,3,19,170,107,230,255,151,0,1,0,52,160,46,65,249,226,3,19,170,136,254,223,200,1,1,128,82,0,1,63,214,192,254,255,180,5,0,0,20,136,255,255,
      151,136,1,128,82,8,0,0,185,0,0,128,210,253,123,193,168,245,11,64,249,243,83,194,168,192,3,95,214,50,0,0,20,204,204,204,204,243,83,191,169,253,123,188,169,253,3,0,145,243,3,0,170,244,3,1,170,1,0,128,210,224,99,0,145,75,235,255,151,232,19,64,249,
      41,189,159,82,8,13,64,185,31,1,9,107,33,1,0,84,232,195,64,57,168,0,0,52,234,15,64,249,72,169,67,185,9,121,30,18,73,169,3,185,35,189,159,82,17,0,0,20,63,245,255,151,232,195,64,57,0,1,0,53,168,0,0,52,234,15,64,249,72,169,67,185,9,121,30,18,73,
      169,3,185,35,0,128,82,7,0,0,20,168,0,0,52,234,15,64,249,72,169,67,185,9,121,30,18,73,169,3,185,3,0,128,82,226,67,0,145,225,3,19,170,224,3,20,170,162,0,0,148,96,0,0,52,0,0,128,210,2,0,0,20,96,10,64,249,253,123,196,168,243,83,193,168,192,3,95,
      214,243,83,187,169,245,91,1,169,247,99,2,169,249,107,3,169,251,35,0,249,253,123,188,169,253,3,0,145,246,3,0,170,248,3,1,170,248,0,0,181,68,255,255,151,200,2,128,82,8,0,0,185,133,252,255,151,192,2,128,82,126,0,0,20,31,3,0,249,19,0,128,210,200,
      2,64,249,255,127,2,169,20,0,128,210,255,27,0,249,200,3,0,180,83,5,128,82,244,7,128,82,225,67,0,145,243,67,0,57,224,3,8,170,244,71,0,57,255,75,0,57,130,14,0,148,200,2,64,249,96,1,0,181,227,131,0,145,2,0,128,210,1,0,128,210,224,3,8,170,63,1,0,
      148,245,3,0,42,85,1,0,52,243,91,66,169,244,3,19,170,18,0,0,20,225,3,0,170,224,3,8,170,226,131,0,145,154,1,0,148,247,3,0,42,247,1,0,53,200,142,64,248,232,252,255,181,243,83,66,169,136,2,19,203,9,253,67,147,53,5,0,145,233,3,19,170,25,0,128,210,
      24,0,0,20,128,134,64,248,247,251,255,151,159,2,22,235,161,255,255,84,36,0,0,20,243,87,66,169,244,3,19,170,3,0,0,20,128,134,64,248,239,251,255,151,159,2,21,235,161,255,255,84,245,3,23,42,27,0,0,20,43,133,64,248,104,1,192,57,234,3,11,170,104,0,
      0,52,72,29,192,56,232,255,255,53,72,1,11,203,8,1,25,139,25,5,0,145,63,1,20,235,193,254,255,84,34,0,128,210,225,3,25,170,224,3,21,170,78,231,255,151,246,3,0,170,214,1,0,181,0,0,128,210,216,251,255,151,245,3,19,170,3,0,0,20,160,134,64,248,212,
      251,255,151,191,2,20,235,161,255,255,84,21,0,128,18,224,3,19,170,207,251,255,151,224,3,21,42,40,0,0,20,218,14,21,139,245,3,19,170,191,2,20,235,247,3,26,170,224,2,0,84,219,2,19,203,162,2,64,249,72,0,192,57,233,3,2,170,104,0,0,52,40,29,192,56,
      232,255,255,53,40,1,2,203,8,5,0,145,73,3,23,203,232,15,0,249,227,3,8,170,33,1,25,139,224,3,23,170,235,13,0,148,64,3,0,53,232,15,64,249,119,107,53,248,181,34,0,145,191,2,20,235,23,1,23,139,129,253,255,84,0,0,128,210,22,3,0,249,175,251,255,151,
      245,3,19,170,3,0,0,20,160,134,64,248,171,251,255,151,191,2,20,235,161,255,255,84,224,3,19,170,167,251,255,151,0,0,128,82,253,123,196,168,251,35,64,249,249,107,67,169,247,99,66,169,245,91,65,169,243,83,197,168,192,3,95,214,4,0,128,210,3,0,128,
      82,2,0,128,210,1,0,128,210,0,0,128,210,143,224,255,151,243,83,190,169,245,11,0,249,247,15,0,249,253,123,191,169,253,3,0,145,244,3,0,170,243,3,1,170,247,3,3,42,52,1,0,181,104,162,64,57,136,0,0,52,96,10,64,249,140,251,255,151,127,162,0,57,127,
      126,1,169,127,18,0,249,78,0,0,20,136,2,192,57,8,3,0,53,104,14,64,249,104,2,0,181,104,162,64,57,136,0,0,52,96,10,64,249,128,251,255,151,127,162,0,57,64,0,128,210,39,4,0,148,136,1,128,82,31,0,0,241,96,10,0,249,0,1,159,26,31,0,0,113,233,23,159,
      154,31,0,0,113,232,23,159,26,104,162,0,57,105,14,0,249,32,7,0,53,104,10,64,249,31,1,0,121,230,255,255,23,5,0,128,82,4,0,128,210,3,0,128,18,226,3,20,170,33,1,128,82,224,3,23,42,172,2,0,148,21,124,64,147,53,1,0,181,104,0,0,176,8,161,18,145,9,253,
      223,200,32,1,63,214,145,254,255,151,124,254,255,151,0,0,64,185,37,0,0,20,105,14,64,249,191,2,9,235,105,2,0,84,104,162,64,57,136,0,0,52,96,10,64,249,87,251,255,151,127,162,0,57,160,250,127,211,254,3,0,148,136,1,128,82,31,0,0,241,96,10,0,249,0,
      1,159,26,31,0,0,113,169,2,159,154,31,0,0,113,232,23,159,26,104,162,0,57,105,14,0,249,0,2,0,53,100,10,64,249,229,3,9,42,3,0,128,18,226,3,20,170,33,1,128,82,224,3,23,42,134,2,0,148,8,124,64,147,136,0,0,181,105,0,0,176,40,161,18,145,218,255,255,
      23,8,5,0,209,104,18,0,249,0,0,128,82,253,123,193,168,247,15,64,249,245,11,64,249,243,83,194,168,192,3,95,214,243,83,190,169,246,95,1,169,253,123,191,169,253,3,0,145,244,3,0,170,243,3,1,170,247,3,3,42,52,1,0,181,104,162,64,57,136,0,0,52,96,10,
      64,249,41,251,255,151,127,162,0,57,127,126,1,169,127,18,0,249,82,0,0,20,136,2,64,121,8,3,0,53,104,14,64,249,104,2,0,181,104,162,64,57,136,0,0,52,96,10,64,249,29,251,255,151,127,162,0,57,32,0,128,210,196,3,0,148,136,1,128,82,31,0,0,241,96,10,
      0,249,0,1,159,26,31,0,0,113,233,23,159,154,31,0,0,113,232,23,159,26,104,162,0,57,105,14,0,249,160,7,0,53,104,10,64,249,31,1,0,57,230,255,255,23,7,0,128,210,6,0,128,210,5,0,128,82,4,0,128,210,3,0,128,18,226,3,20,170,1,0,128,82,224,3,23,42,105,
      2,0,148,22,124,64,147,54,1,0,181,104,0,0,176,8,161,18,145,9,253,223,200,32,1,63,214,44,254,255,151,23,254,255,151,0,0,64,185,39,0,0,20,105,14,64,249,223,2,9,235,105,2,0,84,104,162,64,57,136,0,0,52,96,10,64,249,242,250,255,151,127,162,0,57,224,
      3,22,170,153,3,0,148,136,1,128,82,31,0,0,241,96,10,0,249,0,1,159,26,31,0,0,113,201,2,159,154,31,0,0,113,232,23,159,26,104,162,0,57,105,14,0,249,64,2,0,53,100,10,64,249,229,3,9,42,7,0,128,210,6,0,128,210,3,0,128,18,226,3,20,170,1,0,128,82,224,
      3,23,42,65,2,0,148,8,124,64,147,136,0,0,181,105,0,0,176,40,161,18,145,216,255,255,23,8,5,0,209,104,18,0,249,0,0,128,82,253,123,193,168,246,95,65,169,243,83,194,168,192,3,95,214,243,83,188,169,245,91,1,169,247,99,2,169,249,27,0,249,253,123,191,
      169,253,3,0,145,246,3,0,170,200,2,192,57,233,3,22,170,249,3,1,170,244,3,2,170,243,3,3,170,104,0,0,52,40,29,192,56,232,255,255,53,40,1,22,203,23,5,0,145,232,3,52,170,255,2,8,235,9,1,0,84,128,1,128,82,253,123,193,168,249,27,64,249,247,99,66,169,
      245,91,65,169,243,83,196,168,192,3,95,214,232,2,20,139,24,5,0,145,224,3,24,170,33,0,128,210,31,254,255,151,245,3,0,170,244,0,0,180,227,3,20,170,226,3,25,170,225,3,24,170,224,3,21,170,218,12,0,148,224,6,0,53,1,3,20,203,227,3,23,170,226,3,22,170,
      160,2,20,139,212,12,0,148,32,6,0,53,105,162,64,169,63,1,8,235,1,5,0,84,96,2,64,249,128,1,0,181,1,1,128,210,128,0,128,210,9,254,255,151,96,2,0,249,0,0,128,210,150,250,255,151,105,2,64,249,9,2,0,180,40,129,0,145,105,162,0,169,27,0,0,20,8,1,0,203,
      20,253,67,147,9,0,240,146,159,2,9,235,8,1,0,84,150,250,127,211,225,3,22,170,2,1,128,210,164,1,0,148,64,1,0,181,0,0,128,210,133,250,255,151,224,3,21,170,131,250,255,151,147,1,128,82,0,0,128,210,128,250,255,151,224,3,19,42,197,255,255,23,8,12,
      20,139,96,34,0,169,9,12,22,139,0,0,128,210,105,10,0,249,120,250,255,151,105,6,64,249,53,1,0,249,104,6,64,249,9,33,0,145,105,6,0,249,19,0,128,82,240,255,255,23,4,0,128,210,3,0,128,82,2,0,128,210,1,0,128,210,0,0,128,210,97,223,255,151,253,123,
      187,169,243,83,1,169,245,91,2,169,247,99,3,169,249,35,0,249,253,3,0,145,100,205,255,151,255,67,11,209,243,3,0,170,224,3,1,170,244,3,2,170,53,23,0,88,11,0,0,20,8,0,192,57,8,189,0,81,9,29,0,83,63,181,0,113,104,0,0,84,169,38,200,154,201,0,0,55,
      225,3,0,170,224,3,19,170,238,12,0,148,31,0,19,235,161,254,255,84,9,0,192,57,63,233,0,113,65,1,0,84,104,6,0,145,31,0,8,235,224,0,0,84,227,3,20,170,2,0,128,210,1,0,128,210,224,3,19,170,121,255,255,151,152,0,0,20,40,189,0,81,9,29,0,83,63,181,0,
      113,168,0,0,84,168,38,200,154,104,0,0,54,41,0,128,82,2,0,0,20,9,0,128,82,8,0,19,203,255,255,0,169,63,1,0,113,255,255,1,169,224,35,0,145,255,23,0,249,225,3,19,170,255,195,0,57,249,7,136,154,209,253,255,151,104,0,0,176,8,225,15,145,9,253,223,200,
      5,0,128,82,4,0,128,210,3,0,128,82,226,35,2,145,1,0,128,82,32,1,63,214,247,3,0,170,255,6,0,177,161,1,0,84,227,3,20,170,2,0,128,210,1,0,128,210,224,3,19,170,84,255,255,151,232,195,64,57,243,3,0,42,104,0,0,52,224,15,64,249,29,250,255,151,224,3,
      19,42,109,0,0,20,136,38,64,169,41,1,8,203,56,253,67,147,1,0,128,210,255,255,5,169,224,227,0,145,255,255,6,169,255,63,0,249,255,3,2,57,1,233,255,151,232,35,64,249,41,189,159,82,8,13,64,185,31,1,9,107,33,1,0,84,232,67,65,57,168,0,0,52,234,31,64,
      249,72,169,67,185,9,121,30,18,73,169,3,185,35,189,159,82,17,0,0,20,245,242,255,151,232,67,65,57,0,1,0,53,168,0,0,52,234,31,64,249,72,169,67,185,9,121,30,18,73,169,3,185,35,0,128,82,7,0,0,20,168,0,0,52,234,31,64,249,72,169,67,185,9,121,30,18,
      73,169,3,185,3,0,128,82,226,3,0,145,225,99,1,145,224,211,2,145,188,254,255,151,245,55,64,249,31,0,0,113,224,19,149,154,8,0,192,57,31,185,0,113,225,0,0,84,8,4,192,57,104,1,0,52,31,185,0,113,97,0,0,84,8,8,192,57,232,0,0,52,227,3,20,170,226,3,25,
      170,225,3,19,170,18,255,255,151,246,3,0,42,86,4,0,53,232,3,66,57,104,0,0,52,224,3,21,170,218,249,255,151,104,0,0,176,8,1,16,145,9,253,223,200,225,35,2,145,224,3,23,170,32,1,63,214,128,247,255,53,138,34,64,169,9,1,10,203,43,253,67,147,31,3,11,
      235,224,0,0,84,8,0,0,176,3,129,1,145,64,13,24,139,97,1,24,203,2,1,128,210,244,10,0,148,104,0,0,176,8,193,15,145,9,253,223,200,224,3,23,170,32,1,63,214,232,195,64,57,104,0,0,52,224,15,64,249,191,249,255,151,0,0,128,82,15,0,0,20,232,3,66,57,104,
      0,0,52,224,3,21,170,185,249,255,151,104,0,0,144,8,193,15,145,9,253,223,200,224,3,23,170,32,1,63,214,232,195,64,57,104,0,0,52,224,15,64,249,176,249,255,151,224,3,22,42,255,67,11,145,181,204,255,151,249,35,64,249,247,99,67,169,245,91,66,169,243,
      83,65,169,253,123,197,168,192,3,95,214,1,8,0,0,0,32,0,0,31,0,1,235,98,0,0,84,0,0,128,18,3,0,0,20,31,0,1,235,224,151,159,26,192,3,95,214,204,204,204,204,253,123,190,169,243,83,1,169,253,3,0,145,153,204,255,151,255,131,9,209,104,0,0,144,8,193,
      18,145,9,253,223,200,243,3,1,170,84,124,64,211,162,32,128,82,225,99,1,145,32,1,63,214,0,1,0,53,104,0,0,144,8,161,18,145,9,253,223,200,32,1,63,214,186,252,255,151,0,0,128,82,42,0,0,20,1,0,128,210,243,211,2,169,224,35,0,145,243,211,3,169,255,39,
      0,249,255,67,1,57,113,232,255,151,232,11,64,249,41,189,159,82,8,13,64,185,31,1,9,107,33,1,0,84,232,131,64,57,168,0,0,52,234,7,64,249,72,169,67,185,9,121,30,18,73,169,3,185,35,189,159,82,17,0,0,20,101,242,255,151,232,131,64,57,0,1,0,53,168,0,
      0,52,234,7,64,249,72,169,67,185,9,121,30,18,73,169,3,185,35,0,128,82,7,0,0,20,168,0,0,52,234,7,64,249,72,169,67,185,9,121,30,18,73,169,3,185,3,0,128,82,226,3,0,145,225,163,0,145,224,99,1,145,8,0,0,148,224,39,64,249,255,131,9,145,101,204,255,
      151,243,83,65,169,253,123,194,168,192,3,95,214,204,204,204,204,243,83,190,169,246,11,0,249,253,123,191,169,253,3,0,145,244,3,0,170,243,3,1,170,246,3,3,42,244,0,0,181,104,162,64,57,72,0,0,52,127,162,0,57,127,126,1,169,127,18,0,249,56,0,0,20,136,
      2,64,121,8,2,0,53,104,14,64,249,104,1,0,181,104,162,64,57,72,0,0,52,127,162,0,57,95,252,255,151,72,4,128,82,8,0,0,185,64,4,128,82,127,162,0,57,127,14,0,249,43,0,0,20,104,10,64,249,31,1,0,57,238,255,255,23,7,0,128,210,6,0,128,210,5,0,128,82,4,
      0,128,210,3,0,128,18,226,3,20,170,1,0,128,82,224,3,22,42,151,0,0,148,9,124,64,147,41,1,0,181,104,0,0,144,8,161,18,145,9,253,223,200,32,1,63,214,90,252,255,151,69,252,255,151,0,0,64,185,21,0,0,20,104,14,64,249,63,1,8,235,200,251,255,84,100,10,
      64,249,229,3,8,42,7,0,128,210,6,0,128,210,3,0,128,18,226,3,20,170,1,0,128,82,224,3,22,42,129,0,0,148,8,124,64,147,136,0,0,181,105,0,0,144,40,161,18,145,234,255,255,23,8,5,0,209,104,18,0,249,0,0,128,82,253,123,193,168,246,11,64,249,243,83,194,
      168,192,3,95,214,225,3,0,42,0,0,128,210,131,0,128,82,2,0,128,82,2,0,0,20,204,204,204,204,243,83,190,169,245,11,0,249,253,123,189,169,253,3,0,145,243,3,1,42,225,3,0,170,224,67,0,145,245,3,2,42,244,3,3,42,239,231,255,151,232,19,64,249,106,30,0,
      83,8,193,42,139,9,101,64,57,63,1,20,106,33,1,0,84,213,0,0,52,232,15,64,249,8,1,64,249,9,217,106,120,63,1,21,106,97,0,0,84,0,0,128,82,2,0,0,20,32,0,128,82,232,163,64,57,168,0,0,52,234,11,64,249,72,169,67,185,9,121,30,18,73,169,3,185,253,123,195,
      168,245,11,64,249,243,83,194,168,192,3,95,214,204,204,204,204,243,83,190,169,245,91,1,169,253,123,191,169,253,3,0,145,244,3,0,170,245,3,1,170,246,3,2,170,85,1,0,180,232,3,128,146,8,9,213,154,31,1,22,235,194,0,0,84,244,251,255,151,136,1,128,82,
      8,0,0,185,0,0,128,210,20,0,0,20,180,0,0,180,224,3,20,170,153,11,0,148,243,3,0,170,2,0,0,20,19,0,128,210,181,126,22,155,224,3,20,170,225,3,21,170,166,11,0,148,244,3,0,170,159,2,0,241,110,18,85,250,162,0,0,84,162,2,19,203,1,0,128,82,128,2,19,139,
      214,204,255,151,224,3,20,170,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,168,134,152,82,31,0,8,107,136,1,0,84,136,133,152,82,8,0,8,75,31,37,0,113,136,0,0,84,233,84,128,82,40,37,200,26,104,0,0,55,31,168,0,113,97,2,0,84,1,0,128,82,
      17,0,0,20,8,211,154,82,31,0,8,107,160,1,0,84,40,213,155,82,31,0,8,107,105,1,0,84,104,214,155,82,31,0,8,107,201,254,255,84,8,189,159,82,31,0,8,107,96,254,255,84,40,189,159,82,31,0,8,107,65,0,0,84,33,0,29,18,104,0,0,144,8,225,21,145,9,253,223,
      200,32,1,31,214,8,189,159,82,8,0,8,75,31,5,0,113,168,134,152,82,233,135,159,26,31,0,8,107,72,1,0,84,136,133,152,82,8,0,8,75,31,37,0,113,136,0,0,84,234,84,128,82,72,37,200,26,136,2,0,55,31,168,0,113,15,0,0,20,8,211,154,82,31,0,8,107,224,1,0,84,
      40,213,155,82,31,0,8,107,73,1,0,84,104,214,155,82,31,0,8,107,41,1,0,84,8,189,159,82,31,0,8,107,192,0,0,84,40,189,159,82,31,0,8,107,96,0,0,84,33,120,24,18,2,0,0,20,1,0,128,82,63,1,0,113,232,19,135,154,63,1,0,113,230,19,134,154,105,0,0,52,71,0,
      0,180,255,0,0,185,231,3,8,170,104,0,0,144,8,1,24,145,9,253,223,200,32,1,31,214,243,83,190,169,245,91,1,169,253,123,191,169,253,3,0,145,104,0,0,144,8,33,18,145,9,253,223,200,32,1,63,214,243,3,0,170,115,0,0,181,0,0,128,210,63,0,0,20,104,2,64,121,
      235,3,19,170,104,1,0,52,233,3,11,170,42,9,0,145,40,45,192,120,200,255,255,53,72,1,11,203,9,253,65,147,42,5,0,145,107,5,10,139,104,1,64,121,232,254,255,53,104,1,19,203,9,9,0,145,54,253,65,147,227,3,22,42,7,0,128,210,6,0,128,210,5,0,128,82,4,0,
      128,210,226,3,19,170,1,0,128,82,0,0,128,82,174,255,255,151,21,124,64,147,245,0,0,181,104,0,0,144,8,193,16,145,9,253,223,200,224,3,19,170,32,1,63,214,222,255,255,23,224,3,21,170,232,0,0,148,244,3,0,170,148,0,0,181,0,0,128,210,58,248,255,151,244,
      255,255,23,229,3,21,42,7,0,128,210,6,0,128,210,228,3,20,170,227,3,22,42,226,3,19,170,1,0,128,82,0,0,128,82,150,255,255,151,160,0,0,53,224,3,20,170,45,248,255,151,20,0,128,210,3,0,0,20,0,0,128,210,41,248,255,151,104,0,0,144,8,193,16,145,9,253,
      223,200,224,3,19,170,32,1,63,214,224,3,20,170,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,253,123,188,169,253,3,0,145,8,60,0,83,233,255,159,82,232,35,0,121,31,1,9,107,97,0,0,84,224,255,159,82,56,0,0,20,224,99,0,145,4,231,255,151,
      235,19,64,249,41,189,159,82,104,13,64,185,31,1,9,107,33,2,0,84,236,35,64,121,159,1,2,113,130,1,0,84,40,0,0,240,8,1,32,145,9,9,0,145,141,29,64,211,42,121,109,120,138,0,0,54,104,137,64,249,0,105,109,56,31,0,0,20,128,29,0,83,29,0,0,20,104,157,64,
      249,16,0,0,20,224,35,64,121,31,0,4,113,98,1,0,84,40,0,0,240,8,9,32,145,12,28,64,211,9,121,108,120,137,0,0,54,104,137,64,249,0,105,108,56,16,0,0,20,0,28,0,83,14,0,0,20,104,157,64,249,136,1,0,180,37,0,128,82,228,75,0,145,35,0,128,82,226,67,0,145,
      1,32,128,82,224,3,8,170,247,10,0,148,96,0,0,53,224,35,64,121,2,0,0,20,224,39,64,121,232,195,64,57,168,0,0,52,234,15,64,249,72,169,67,185,9,121,30,18,73,169,3,185,253,123,196,168,192,3,95,214,243,15,31,248,253,123,191,169,253,3,0,145,243,3,1,
      170,104,0,0,240,8,117,67,249,105,2,64,249,63,1,8,235,0,1,0,84,104,0,0,208,8,253,71,185,9,168,67,185,63,1,8,106,97,0,0,84,44,2,0,148,96,2,0,249,253,123,193,168,243,7,65,248,192,3,95,214,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,
      243,3,1,170,104,0,0,240,8,85,67,249,105,2,64,249,63,1,8,235,0,1,0,84,104,0,0,208,8,253,71,185,9,168,67,185,63,1,8,106,97,0,0,84,32,245,255,151,96,2,0,249,253,123,193,168,243,7,65,248,192,3,95,214,204,204,204,204,243,15,31,248,253,123,191,169,
      253,3,0,145,104,0,0,240,8,161,27,145,243,3,1,170,10,121,98,248,105,2,64,249,63,1,10,235,0,1,0,84,104,0,0,208,8,253,71,185,9,168,67,185,63,1,8,106,97,0,0,84,3,2,0,148,96,2,0,249,253,123,193,168,243,7,65,248,192,3,95,214,243,15,31,248,253,123,
      191,169,253,3,0,145,104,0,0,240,8,161,26,145,243,3,1,170,10,121,98,248,105,2,64,249,63,1,10,235,0,1,0,84,104,0,0,208,8,253,71,185,9,168,67,185,63,1,8,106,97,0,0,84,247,244,255,151,96,2,0,249,253,123,193,168,243,7,65,248,192,3,95,214,104,0,0,
      240,8,65,28,145,41,0,128,82,9,253,159,136,191,59,3,213,192,3,95,214,204,204,204,204,204,204,204,204,253,123,190,169,253,3,0,145,227,83,0,145,226,67,0,145,225,99,0,145,224,67,0,145,136,0,128,82,137,0,128,82,232,167,2,41,3,0,0,148,253,123,194,
      168,192,3,95,214,243,83,190,169,245,91,1,169,253,123,189,169,253,3,0,145,245,3,3,170,181,19,0,249,32,0,64,185,225,241,255,151,191,67,0,57,104,0,0,240,20,161,27,145,243,3,20,170,118,0,0,208,179,15,0,249,136,34,0,145,127,2,8,235,96,1,0,84,201,
      194,21,145,104,2,64,249,31,1,9,235,160,0,0,84,193,194,21,145,224,3,19,170,219,1,0,148,96,2,0,249,115,34,0,145,243,255,255,23,160,2,64,185,214,241,255,151,253,123,195,168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,243,83,190,169,
      245,11,0,249,253,123,191,169,253,3,0,145,31,128,0,177,104,3,0,84,31,0,0,241,40,0,128,210,19,1,128,154,104,0,0,144,8,33,20,145,117,0,0,240,160,46,65,249,9,253,223,200,226,3,19,170,1,0,128,82,32,1,63,214,96,2,0,181,104,0,0,144,20,33,20,145,106,
      8,0,148,96,1,0,52,224,3,19,170,53,225,255,151,0,1,0,52,160,46,65,249,226,3,19,170,136,254,223,200,1,0,128,82,0,1,63,214,192,254,255,180,5,0,0,20,82,250,255,151,136,1,128,82,8,0,0,185,0,0,128,210,253,123,193,168,245,11,64,249,243,83,194,168,192,
      3,95,214,243,83,191,169,253,123,191,169,253,3,0,145,243,3,0,170,244,3,2,170,151,220,255,151,104,6,64,185,201,12,128,82,31,1,9,106,193,0,0,84,104,2,64,185,169,1,0,24,31,1,9,107,0,8,65,122,128,0,0,84,253,123,193,168,243,83,193,168,192,3,95,214,
      64,233,255,151,19,16,0,249,62,233,255,151,20,20,0,249,56,230,255,151,31,32,3,213,99,115,109,224,204,204,204,204,243,15,31,248,253,123,190,169,253,3,0,145,9,60,0,83,232,255,159,82,63,1,8,107,51,60,0,83,96,2,0,84,63,1,4,113,194,0,0,84,104,0,0,
      208,8,249,67,249,9,121,105,120,32,1,19,10,13,0,0,20,227,67,0,145,233,39,0,121,34,0,128,82,255,35,0,121,225,75,0,145,32,0,128,82,17,11,0,148,128,0,0,52,232,35,64,121,0,1,19,10,2,0,0,20,0,0,128,82,253,123,194,168,243,7,65,248,192,3,95,214,102,
      250,255,23,204,204,204,204,136,0,0,176,9,1,64,249,170,255,255,240,72,129,46,145,63,1,8,235,64,1,0,84,233,3,18,170,10,128,64,249,40,9,64,249,95,1,8,235,131,0,0,84,40,5,64,249,95,1,8,235,73,0,0,84,96,0,62,212,192,3,95,214,98,0,0,181,0,0,128,82,
      15,0,0,20,73,4,0,209,73,1,0,180,10,0,64,121,10,1,0,52,40,0,64,121,95,1,8,107,161,0,0,84,0,8,0,145,33,8,0,145,41,5,0,209,9,255,255,181,9,0,64,121,40,0,64,121,32,1,8,75,192,3,95,214,243,83,189,169,245,91,1,169,247,99,2,169,253,123,191,169,253,
      3,0,145,245,3,0,170,181,2,0,180,72,0,0,176,23,193,17,145,19,0,128,82,116,28,128,82,136,2,19,11,9,125,72,11,162,10,128,210,224,3,21,170,54,125,1,19,216,126,64,147,8,239,124,211,1,105,119,248,47,11,0,148,160,1,0,52,96,0,248,54,212,6,0,81,2,0,0,
      20,211,6,0,17,127,2,20,107,45,254,255,84,0,0,128,82,253,123,193,168,247,99,66,169,245,91,65,169,243,83,195,168,192,3,95,214,9,239,124,211,232,34,0,145,42,105,104,184,234,254,255,55,95,145,3,113,162,254,255,84,40,0,0,240,8,65,40,145,73,125,124,
      147,32,105,104,184,241,255,255,23,10,64,0,145,73,253,95,136,41,5,0,17,73,253,8,136,168,255,255,53,191,59,3,213,10,112,64,249,202,0,0,180,73,253,95,136,41,5,0,17,73,253,8,136,168,255,255,53,191,59,3,213,10,120,64,249,202,0,0,180,73,253,95,136,
      41,5,0,17,73,253,8,136,168,255,255,53,191,59,3,213,10,116,64,249,202,0,0,180,73,253,95,136,41,5,0,17,73,253,8,136,168,255,255,53,191,59,3,213,10,128,64,249,202,0,0,180,73,253,95,136,41,5,0,17,73,253,8,136,168,255,255,53,191,59,3,213,104,0,0,
      208,13,97,27,145,10,224,0,145,203,0,128,82,72,1,95,248,31,1,13,235,0,1,0,84,76,1,64,249,204,0,0,180,137,253,95,136,41,5,0,17,137,253,8,136,168,255,255,53,191,59,3,213,72,129,94,248,8,1,0,180,76,129,95,248,204,0,0,180,137,253,95,136,41,5,0,17,
      137,253,8,136,168,255,255,53,191,59,3,213,74,129,0,145,107,5,0,81,107,253,255,53,0,144,64,249,149,0,0,20,224,7,0,180,10,64,0,145,73,253,95,136,41,5,0,81,73,253,8,136,168,255,255,53,191,59,3,213,10,112,64,249,202,0,0,180,73,253,95,136,41,5,0,
      81,73,253,8,136,168,255,255,53,191,59,3,213,10,120,64,249,202,0,0,180,73,253,95,136,41,5,0,81,73,253,8,136,168,255,255,53,191,59,3,213,10,116,64,249,202,0,0,180,73,253,95,136,41,5,0,81,73,253,8,136,168,255,255,53,191,59,3,213,10,128,64,249,202,
      0,0,180,73,253,95,136,41,5,0,81,73,253,8,136,168,255,255,53,191,59,3,213,104,0,0,208,13,97,27,145,10,224,0,145,203,0,128,82,72,1,95,248,31,1,13,235,0,1,0,84,76,1,64,249,204,0,0,180,137,253,95,136,41,5,0,81,137,253,8,136,168,255,255,53,191,59,
      3,213,72,129,94,248,8,1,0,180,76,129,95,248,204,0,0,180,137,253,95,136,41,5,0,81,137,253,8,136,168,255,255,53,191,59,3,213,74,129,0,145,107,5,0,81,107,253,255,53,0,144,64,249,102,0,0,20,192,3,95,214,243,83,189,169,245,91,1,169,247,19,0,249,253,
      123,191,169,253,3,0,145,243,3,0,170,105,126,64,249,105,3,0,180,104,0,0,208,8,1,32,145,63,1,8,235,224,2,0,84,104,114,64,249,168,2,0,180,8,1,64,185,104,2,0,53,96,122,64,249,192,0,0,180,8,0,64,185,136,0,0,53,20,246,255,151,96,126,64,249,68,9,0,
      148,96,118,64,249,192,0,0,180,8,0,64,185,136,0,0,53,13,246,255,151,96,126,64,249,137,9,0,148,96,114,64,249,9,246,255,151,96,126,64,249,7,246,255,151,104,130,64,249,200,1,0,180,8,1,64,185,136,1,0,53,104,134,64,249,0,249,3,209,0,246,255,151,104,
      138,64,249,0,1,2,209,253,245,255,151,104,142,64,249,0,1,2,209,250,245,255,151,96,130,64,249,248,245,255,151,96,146,64,249,66,0,0,148,104,0,0,208,23,97,27,145,118,162,4,145,116,226,0,145,213,0,128,82,136,2,95,248,31,1,23,235,0,1,0,84,128,2,64,
      249,192,0,0,180,8,0,64,185,136,0,0,53,233,245,255,151,192,2,64,249,231,245,255,151,136,130,94,248,200,0,0,180,128,130,95,248,128,0,0,180,8,0,64,185,72,0,0,53,224,245,255,151,214,34,0,145,148,130,0,145,181,6,0,81,149,253,255,53,224,3,19,170,218,
      245,255,151,253,123,193,168,247,19,64,249,245,91,65,169,243,83,195,168,192,3,95,214,160,1,0,180,40,0,0,208,8,65,40,145,31,0,8,235,32,1,0,84,10,112,5,145,72,253,95,136,8,5,0,17,72,253,9,136,169,255,255,53,191,59,3,213,224,3,8,42,2,0,0,20,0,0,
      176,18,192,3,95,214,204,204,204,204,160,1,0,180,40,0,0,208,8,65,40,145,31,0,8,235,32,1,0,84,10,112,5,145,72,253,95,136,8,5,0,81,72,253,9,136,169,255,255,53,191,59,3,213,224,3,8,42,2,0,0,20,0,0,176,18,192,3,95,214,204,204,204,204,243,15,31,248,
      253,123,191,169,253,3,0,145,243,3,0,170,115,1,0,180,40,0,0,176,8,65,40,145,127,2,8,235,224,0,0,84,104,94,65,185,191,59,3,213,136,0,0,53,74,9,0,148,224,3,19,170,166,245,255,151,253,123,193,168,243,7,65,248,192,3,95,214,243,83,191,169,253,123,
      191,169,253,3,0,145,85,240,255,151,244,3,0,170,137,170,67,185,104,0,0,176,8,253,71,185,63,1,8,106,96,0,0,84,147,74,64,249,83,1,0,181,128,0,128,82,5,240,255,151,104,0,0,208,1,117,67,249,128,66,2,145,11,0,0,148,243,3,0,170,128,0,128,82,8,240,255,
      151,179,0,0,180,224,3,19,170,253,123,193,168,243,83,193,168,192,3,95,214,112,231,255,151,204,204,204,204,243,83,191,169,253,123,191,169,253,3,0,145,243,3,1,170,211,2,0,180,160,2,0,180,20,0,64,249,159,2,19,235,97,0,0,84,224,3,19,170,17,0,0,20,
      19,0,0,249,224,3,19,170,211,254,255,151,116,255,255,180,224,3,20,170,14,255,255,151,136,18,64,185,232,254,255,53,104,0,0,176,8,193,21,145,159,2,8,235,96,254,255,84,224,3,20,170,70,255,255,151,240,255,255,23,0,0,128,210,253,123,193,168,243,83,
      193,168,192,3,95,214,253,123,191,169,253,3,0,145,31,8,0,49,225,0,0,84,138,248,255,151,31,0,0,185,126,248,255,151,40,1,128,82,8,0,0,185,24,0,0,20,32,2,248,55,104,0,0,208,8,97,70,185,31,0,8,107,162,1,0,84,9,124,64,147,104,0,0,208,8,129,9,145,42,
      253,70,211,41,21,64,146,8,121,106,248,11,9,128,210,43,33,11,155,105,225,64,57,105,0,0,54,96,21,64,249,8,0,0,20,115,248,255,151,31,0,0,185,103,248,255,151,40,1,128,82,8,0,0,185,168,245,255,151,0,0,128,146,253,123,193,168,192,3,95,214,243,83,189,
      169,245,91,1,169,247,19,0,249,253,123,191,169,253,3,0,145,243,3,0,42,83,5,248,55,104,0,0,208,8,97,70,185,127,2,8,107,194,4,0,84,104,126,64,147,105,0,0,208,53,129,9,145,20,253,70,211,22,21,64,146,168,122,116,248,23,9,128,210,202,34,23,155,73,
      225,64,57,137,3,0,54,72,21,64,249,31,5,0,177,32,3,0,84,176,9,0,148,31,4,0,113,1,2,0,84,51,1,0,52,127,6,0,113,160,0,0,84,127,10,0,113,97,1,0,84,96,1,128,18,4,0,0,20,64,1,128,18,2,0,0,20,32,1,128,18,72,0,0,240,8,65,23,145,9,253,223,200,1,0,128,
      210,32,1,63,214,168,122,116,248,10,0,128,146,0,0,128,82,201,34,23,155,42,21,0,249,7,0,0,20,48,248,255,151,40,1,128,82,8,0,0,185,55,248,255,151,31,0,0,185,0,0,128,18,253,123,193,168,247,19,64,249,245,91,65,169,243,83,195,168,192,3,95,214,204,
      204,204,204,243,83,189,169,245,91,1,169,247,99,2,169,253,123,191,169,253,3,0,145,1,9,128,210,0,8,128,210,113,248,255,151,243,3,0,170,115,0,0,181,19,0,128,210,32,0,0,20,8,64,130,210,116,2,8,139,127,2,20,235,128,3,0,84,117,194,0,145,87,1,128,82,
      24,0,128,146,182,194,0,209,224,3,22,170,2,0,128,82,1,244,129,82,9,238,255,151,184,130,31,248,10,0,128,82,191,134,4,248,168,82,92,56,191,2,28,120,183,34,28,56,183,50,28,56,183,66,28,56,8,17,29,18,168,82,28,56,200,194,42,139,73,5,0,17,234,3,9,
      42,31,249,0,57,63,21,0,113,99,255,255,84,168,194,0,209,31,1,20,235,33,253,255,84,0,0,128,210,220,244,255,151,224,3,19,170,253,123,193,168,247,99,66,169,245,91,65,169,243,83,195,168,192,3,95,214,204,204,204,204,243,83,190,169,245,91,1,169,253,
      123,191,169,253,3,0,145,243,3,0,170,19,2,0,180,8,64,130,210,116,2,8,139,127,2,20,235,245,3,19,170,32,1,0,84,72,0,0,240,22,65,15,145,200,254,223,200,224,3,21,170,0,1,63,214,181,34,1,145,191,2,20,235,97,255,255,84,224,3,19,170,192,244,255,151,
      253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,243,83,189,169,245,91,1,169,247,99,2,169,253,123,190,169,253,3,0,145,245,3,0,42,191,10,64,113,99,1,0,84,206,247,255,151,40,1,128,82,8,0,0,185,15,245,255,151,32,1,128,82,
      253,123,194,168,247,99,66,169,245,91,65,169,243,83,195,168,192,3,95,214,20,0,128,82,224,0,128,82,22,239,255,151,19,0,128,210,191,11,0,249,120,0,0,208,119,0,0,208,233,98,70,185,191,2,9,107,43,1,0,84,22,131,9,145,200,122,115,248,72,0,0,180,12,
      0,0,20,146,255,255,151,192,122,51,248,192,0,0,181,148,1,128,82,224,0,128,82,15,239,255,151,224,3,20,42,230,255,255,23,232,98,70,185,9,1,1,17,233,98,6,185,115,6,0,145,179,11,0,249,237,255,255,23,9,124,64,147,104,0,0,208,8,129,9,145,42,253,70,
      147,41,21,64,146,8,121,106,248,11,9,128,210,32,33,11,155,75,0,0,240,105,129,15,145,40,253,223,200,0,1,31,214,9,124,64,147,104,0,0,208,8,129,9,145,42,253,70,147,41,21,64,146,8,121,106,248,11,9,128,210,32,33,11,155,75,0,0,240,105,97,21,145,40,
      253,223,200,0,1,31,214,253,123,186,169,243,83,1,169,245,91,2,169,247,99,3,169,249,107,4,169,251,43,0,249,253,3,0,145,109,199,255,151,255,131,0,209,250,3,0,145,249,3,1,42,225,3,0,170,224,3,26,170,247,3,2,170,248,3,3,42,251,3,4,170,244,3,5,42,
      243,3,6,42,82,227,255,151,116,0,0,53,72,7,64,249,20,13,64,185,127,2,0,113,41,1,128,82,40,0,128,82,33,17,136,26,5,0,128,82,4,0,128,210,227,3,24,42,226,3,23,170,224,3,20,42,153,251,255,151,246,3,0,42,118,0,0,53,20,0,128,82,61,0,0,20,213,126,64,
      147,168,250,127,211,9,65,0,145,63,1,8,235,32,129,159,154,192,5,0,180,31,0,16,241,136,1,0,84,8,60,0,145,31,1,0,235,76,0,0,84,232,223,124,178,15,253,68,211,187,201,255,151,255,115,47,203,243,3,0,145,51,1,0,180,136,153,153,82,5,0,0,20,231,252,255,
      151,243,3,0,170,147,0,0,180,168,187,155,82,104,2,0,185,115,66,0,145,233,3,19,170,83,3,0,180,162,250,127,211,1,0,128,82,224,3,19,170,70,200,255,151,229,3,22,42,228,3,19,170,227,3,24,42,226,3,23,170,33,0,128,82,224,3,20,42,111,251,255,151,233,
      3,19,170,160,1,0,52,72,0,0,240,8,193,19,145,9,253,223,200,226,3,0,42,227,3,27,170,225,3,19,170,224,3,25,42,32,1,63,214,233,3,19,170,244,3,0,42,4,0,0,20,9,0,128,210,20,0,128,82,233,0,0,180,40,1,95,184,170,187,155,82,31,1,10,107,97,0,0,84,32,65,
      0,209,21,244,255,151,72,99,64,57,168,0,0,52,74,3,64,249,72,169,67,185,9,121,30,18,73,169,3,185,224,3,20,42,95,3,0,145,255,131,0,145,19,199,255,151,251,43,64,249,249,107,68,169,247,99,67,169,245,91,66,169,243,83,65,169,253,123,198,168,192,3,95,
      214,204,204,204,204,243,83,188,169,245,91,1,169,247,99,2,169,249,27,0,249,253,123,189,169,253,3,0,145,255,67,0,209,249,3,1,170,225,3,0,170,224,131,0,145,248,3,2,42,247,3,3,170,246,3,4,42,245,3,5,170,244,3,6,42,243,3,7,42,226,226,255,151,232,
      131,64,185,231,3,19,42,230,3,20,42,229,3,21,170,228,3,22,42,232,3,0,185,227,3,23,170,226,3,24,42,225,3,25,170,224,163,0,145,15,0,0,148,232,227,64,57,168,0,0,52,234,19,64,249,72,169,67,185,9,121,30,18,73,169,3,185,255,67,0,145,253,123,195,168,
      249,27,64,249,247,99,66,169,245,91,65,169,243,83,196,168,192,3,95,214,204,204,204,204,253,123,186,169,243,83,1,169,245,91,2,169,247,99,3,169,249,107,4,169,251,43,0,249,253,3,0,145,209,198,255,151,255,131,0,209,250,67,0,145,245,3,4,42,69,7,0,
      169,191,2,0,113,243,3,0,170,251,3,2,42,248,3,3,170,246,3,6,42,247,3,7,42,237,0,0,84,161,126,64,147,224,3,24,170,75,8,0,148,31,0,21,107,21,4,0,17,21,160,149,26,119,0,0,53,104,2,64,249,23,13,64,185,72,131,64,185,42,1,128,82,41,0,128,82,5,0,128,
      82,4,0,128,210,31,1,0,113,65,17,137,26,227,3,21,42,226,3,24,170,224,3,23,42,246,250,255,151,249,3,0,42,121,0,0,53,0,0,128,82,144,0,0,20,40,127,127,147,9,65,0,145,63,1,8,235,32,129,159,154,32,16,0,180,180,187,155,82,137,153,153,82,31,0,16,241,
      136,1,0,84,8,60,0,145,31,1,0,235,76,0,0,84,232,223,124,178,15,253,68,211,23,201,255,151,255,115,47,203,243,67,0,145,19,1,0,180,105,2,0,185,5,0,0,20,67,252,255,151,243,3,0,170,115,0,0,180,116,2,0,185,115,66,0,145,244,3,19,170,147,13,0,180,229,
      3,25,42,228,3,19,170,227,3,21,42,226,3,24,170,33,0,128,82,224,3,23,42,208,250,255,151,128,12,0,52,88,7,64,249,7,0,128,210,255,3,0,249,6,0,128,210,5,0,128,82,4,0,128,210,224,3,24,170,227,3,25,42,226,3,19,170,225,3,27,42,188,236,255,151,245,3,
      0,42,53,2,0,52,91,2,80,54,22,9,0,52,191,2,22,107,172,1,0,84,68,3,64,249,7,0,128,210,255,3,0,249,6,0,128,210,229,3,22,42,227,3,25,42,226,3,19,170,225,3,27,42,224,3,24,170,172,236,255,151,245,3,0,42,85,7,0,53,244,3,19,170,69,0,0,20,168,126,127,
      147,9,65,0,145,63,1,8,235,32,129,159,154,64,6,0,180,31,0,16,241,136,1,0,84,8,60,0,145,31,1,0,235,76,0,0,84,232,223,124,178,15,253,68,211,215,200,255,151,255,115,47,203,244,67,0,145,52,1,0,180,136,153,153,82,5,0,0,20,3,252,255,151,244,3,0,170,
      148,0,0,180,168,187,155,82,136,2,0,185,148,66,0,145,212,3,0,180,7,0,128,210,255,3,0,249,6,0,128,210,229,3,21,42,228,3,20,170,227,3,25,42,226,3,19,170,225,3,27,42,224,3,24,170,133,236,255,151,160,1,0,52,7,0,128,210,6,0,128,210,227,3,21,42,226,
      3,20,170,1,0,128,82,224,3,23,42,246,1,0,53,5,0,128,82,4,0,128,210,164,250,255,151,245,3,0,42,181,1,0,53,136,2,95,184,169,187,155,82,31,1,9,107,97,0,0,84,128,66,0,209,54,243,255,151,21,0,128,82,244,3,19,170,14,0,0,20,68,3,64,249,229,3,22,42,242,
      255,255,23,136,2,95,184,169,187,155,82,31,1,9,107,1,255,255,84,128,66,0,209,42,243,255,151,245,255,255,23,20,0,128,210,21,0,128,82,244,0,0,180,136,2,95,184,169,187,155,82,31,1,9,107,97,0,0,84,128,66,0,209,32,243,255,151,224,3,21,42,95,67,0,209,
      255,131,0,145,36,198,255,151,251,43,64,249,249,107,68,169,247,99,67,169,245,91,66,169,243,83,65,169,253,123,198,168,192,3,95,214,253,123,191,169,253,3,0,145,130,240,255,151,8,28,0,83,31,1,0,113,224,23,159,26,253,123,193,168,192,3,95,214,253,
      123,189,169,253,3,0,145,224,23,0,185,31,8,0,49,161,0,0,84,35,246,255,151,40,1,128,82,8,0,0,185,29,0,0,20,0,3,248,55,104,0,0,208,8,97,70,185,31,0,8,107,130,2,0,84,9,124,64,147,104,0,0,208,8,129,9,145,42,253,70,147,41,21,64,146,8,121,106,248,11,
      9,128,210,41,33,11,155,42,225,64,57,74,1,0,54,232,83,0,145,224,3,3,41,224,67,0,145,232,19,0,249,227,99,0,145,226,131,0,145,225,115,0,145,9,0,0,148,6,0,0,20,7,246,255,151,40,1,128,82,8,0,0,185,72,243,255,151,0,0,128,18,253,123,195,168,192,3,95,
      214,243,83,191,169,253,123,190,169,253,3,0,145,243,3,2,170,244,3,3,170,180,11,0,249,32,0,64,185,81,254,255,151,104,2,64,249,0,1,64,185,9,124,64,147,42,253,70,147,104,0,0,208,8,129,9,145,41,21,64,146,11,9,128,210,8,121,106,248,41,33,11,155,42,
      225,64,57,10,2,0,54,104,253,255,151,72,0,0,240,8,161,16,145,9,253,223,200,32,1,63,214,96,0,0,52,19,0,128,82,12,0,0,20,72,0,0,240,8,161,18,145,9,253,223,200,32,1,63,214,243,3,0,42,233,245,255,151,19,0,0,185,221,245,255,151,40,1,128,82,8,0,0,185,
      19,0,128,18,128,2,64,185,60,254,255,151,224,3,19,42,253,123,194,168,243,83,193,168,192,3,95,214,204,204,204,204,253,123,191,169,253,3,0,145,224,0,0,181,207,245,255,151,200,2,128,82,8,0,0,185,16,243,255,151,0,0,128,18,3,0,0,20,0,24,64,185,191,
      59,3,213,253,123,193,168,192,3,95,214,204,204,204,204,253,123,186,169,243,83,1,169,245,91,2,169,247,99,3,169,249,107,4,169,251,43,0,249,253,3,0,145,161,197,255,151,255,3,1,209,247,3,0,42,245,3,1,170,246,3,2,42,243,3,3,170,246,24,0,52,21,2,0,
      181,40,0,128,82,104,226,0,57,201,2,128,82,127,54,0,185,229,3,19,170,104,194,0,57,4,0,128,210,3,0,128,82,105,46,0,185,2,0,128,210,1,0,128,210,0,0,128,210,247,242,255,151,0,0,128,18,184,0,0,20,232,126,64,147,105,0,0,208,56,129,9,145,27,253,70,
      147,248,27,0,249,25,21,64,146,8,123,123,248,26,9,128,210,249,19,0,249,42,35,26,155,84,229,192,57,136,254,3,17,9,29,0,83,63,5,0,113,72,0,0,84,86,252,7,55,72,225,64,57,200,0,40,54,227,3,19,170,66,0,128,82,1,0,128,210,224,3,23,42,6,7,0,148,224,
      3,23,42,122,3,0,148,64,9,0,52,8,123,123,248,41,35,26,155,42,225,192,57,202,8,248,54,104,162,64,57,104,0,0,53,224,3,19,170,5,226,255,151,104,14,64,249,9,157,64,249,169,0,0,181,8,123,123,248,41,35,26,155,42,229,192,57,106,7,0,52,8,123,123,248,
      225,163,0,145,41,35,26,155,72,0,0,240,8,129,17,145,32,21,64,249,9,253,223,200,32,1,63,214,64,6,0,52,116,5,0,52,159,2,0,113,128,202,66,122,44,14,0,84,186,66,54,139,255,11,0,249,20,0,128,82,23,0,128,82,255,27,0,185,191,2,26,235,248,3,21,170,130,
      3,0,84,22,39,64,120,224,3,22,42,146,7,0,148,223,34,32,107,33,2,0,84,249,3,20,42,52,11,0,17,244,23,0,185,223,42,0,113,33,1,0,84,160,1,128,82,137,7,0,148,8,60,0,83,31,53,0,113,225,0,0,84,52,15,0,17,247,6,0,17,244,223,2,41,31,3,26,235,226,0,0,84,
      236,255,255,23,72,0,0,240,8,161,18,145,9,253,223,200,32,1,63,214,224,19,0,185,249,19,64,249,248,27,64,249,233,67,0,145,26,9,128,210,51,0,0,20,227,3,19,170,226,3,22,42,225,3,21,170,224,3,23,42,177,0,0,148,24,0,0,20,8,123,123,248,42,35,26,155,
      73,225,192,57,201,2,248,54,244,1,0,52,159,6,0,113,0,1,0,84,159,10,0,113,97,7,0,84,226,3,22,42,225,3,21,170,224,3,23,42,39,2,0,148,10,0,0,20,226,3,22,42,225,3,21,170,224,3,23,42,110,2,0,148,5,0,0,20,226,3,22,42,225,3,21,170,224,3,23,42,207,1,
      0,148,224,3,0,249,26,0,0,20,232,227,0,145,64,21,64,249,31,1,0,249,4,0,128,210,31,9,0,185,72,0,0,240,8,65,24,145,9,253,223,200,227,243,0,145,226,3,22,42,225,3,21,170,32,1,63,214,192,0,0,53,72,0,0,240,8,161,18,145,9,253,223,200,32,1,63,214,224,
      59,0,185,233,227,0,145,40,1,64,249,234,3,0,145,41,9,64,185,72,1,0,249,73,9,0,185,225,11,64,185,232,7,64,185,200,3,0,53,224,3,64,185,192,1,0,52,31,20,0,113,33,1,0,84,41,1,128,82,105,46,0,185,40,0,128,82,169,0,128,82,104,194,0,57,105,54,0,185,
      104,226,0,57,92,255,255,23,225,3,19,170,72,245,255,151,89,255,255,23,8,123,123,248,41,35,26,155,42,225,64,57,138,0,48,54,168,2,192,57,31,105,0,113,64,1,0,84,40,0,128,82,127,54,0,185,137,3,128,82,104,194,0,57,105,46,0,185,104,226,0,57,75,255,
      255,23,0,1,1,75,2,0,0,20,0,0,128,82,255,3,1,145,218,196,255,151,251,43,64,249,249,107,68,169,247,99,67,169,245,91,66,169,243,83,65,169,253,123,198,168,192,3,95,214,243,83,188,169,245,91,1,169,247,99,2,169,249,27,0,249,251,31,0,249,253,123,190,
      169,253,3,0,145,244,3,0,42,180,19,0,185,246,3,1,170,247,3,2,42,243,3,3,170,159,10,0,49,1,1,0,84,40,0,128,82,104,226,0,57,127,54,0,185,104,194,0,57,41,1,128,82,105,46,0,185,54,0,0,20,20,5,248,55,104,0,0,176,8,97,70,185,159,2,8,107,130,4,0,84,
      136,126,64,147,24,253,70,211,105,0,0,176,57,129,9,145,27,21,64,146,9,9,128,210,40,123,120,248,105,35,9,155,42,225,64,57,74,3,0,54,224,3,20,42,25,253,255,151,21,0,128,18,40,123,120,248,9,9,128,210,105,35,9,155,42,225,64,57,10,1,0,55,40,0,128,
      82,104,194,0,57,41,1,128,82,105,46,0,185,104,226,0,57,127,54,0,185,7,0,0,20,227,3,19,170,226,3,23,42,225,3,22,170,224,3,20,42,235,254,255,151,245,3,0,42,224,3,20,42,16,253,255,151,224,3,21,42,15,0,0,20,40,0,128,82,41,1,128,82,104,226,0,57,127,
      54,0,185,104,194,0,57,105,46,0,185,229,3,19,170,4,0,128,210,3,0,128,82,2,0,128,210,1,0,128,210,0,0,128,210,235,241,255,151,0,0,128,18,253,123,194,168,251,31,64,249,249,27,64,249,247,99,66,169,245,91,65,169,243,83,196,168,192,3,95,214,253,123,
      186,169,243,83,1,169,245,91,2,169,247,99,3,169,249,107,4,169,251,43,0,249,253,3,0,145,113,196,255,151,255,131,2,209,250,3,0,145,246,3,0,42,200,126,64,147,106,0,0,176,91,129,9,145,9,253,70,147,8,21,64,146,48,0,128,146,73,23,0,249,10,9,128,210,
      80,67,0,249,243,3,1,170,24,125,10,155,104,123,105,248,249,3,3,170,119,66,34,139,83,95,3,169,9,3,8,139,40,21,64,249,72,55,0,249,104,66,34,139,72,35,0,249,73,0,0,208,40,161,17,145,9,253,223,200,32,1,63,214,40,163,64,57,64,35,0,185,104,0,0,53,224,
      3,25,170,237,224,255,151,40,15,64,249,95,7,0,249,20,0,128,82,21,0,128,82,95,19,0,185,10,13,64,185,72,27,64,249,74,39,0,185,31,1,23,235,226,26,0,84,200,126,64,147,107,0,0,144,110,129,34,145,9,253,70,147,78,39,7,169,104,2,192,57,55,0,128,82,95,
      7,0,185,72,3,0,57,40,189,159,82,95,1,8,107,161,10,0,84,107,123,105,248,13,0,128,82,8,67,45,139,9,1,11,139,42,249,192,57,138,0,0,52,173,5,0,17,191,21,0,113,75,255,255,84,72,35,64,249,12,1,19,203,173,6,0,52,71,23,64,249,104,123,103,248,9,3,8,139,
      42,249,64,57,200,201,234,56,14,5,0,17,214,1,13,75,159,193,54,235,203,20,0,84,8,3,11,139,11,249,0,145,74,35,2,145,233,3,13,42,104,21,192,56,41,5,0,81,72,21,0,56,169,255,255,53,10,0,128,82,223,2,0,113,45,1,0,84,235,3,19,170,76,35,2,145,104,21,
      192,56,73,1,13,11,74,5,0,17,95,1,22,107,136,201,41,56,107,255,255,84,10,0,128,210,104,123,103,248,9,3,10,139,74,5,0,145,173,5,0,81,41,1,8,139,63,249,0,57,77,255,255,53,72,35,2,145,95,163,4,169,72,0,128,82,223,17,0,113,41,0,128,82,55,17,136,26,
      226,126,64,211,228,3,25,170,67,35,1,145,65,67,1,145,64,19,0,145,217,5,0,148,31,4,0,177,160,17,0,84,214,6,0,81,19,0,0,20,104,2,64,57,214,201,232,56,200,6,0,17,159,193,40,235,43,18,0,84,31,17,0,113,95,207,5,169,42,0,128,82,73,0,128,82,87,17,137,
      26,226,126,64,211,228,3,25,170,67,99,1,145,65,131,1,145,64,19,0,145,197,5,0,148,31,4,0,177,32,15,0,84,115,194,54,139,44,0,0,20,77,23,64,249,108,123,109,248,10,3,12,139,75,245,64,57,104,1,30,18,31,17,0,113,99,1,0,84,72,249,192,57,66,0,128,210,
      105,2,192,57,65,67,2,145,72,67,2,57,104,31,128,82,104,1,8,10,73,71,2,57,72,245,0,57,22,0,0,20,40,15,64,249,107,2,192,57,9,1,64,249,106,29,0,83,42,217,234,120,202,1,248,54,72,31,64,249,118,6,0,145,223,2,8,235,194,14,0,84,227,3,25,170,66,0,128,
      210,225,3,19,170,64,19,0,145,221,4,0,148,31,4,0,49,160,10,0,84,243,3,22,170,8,0,0,20,34,0,128,210,225,3,19,170,227,3,25,170,64,19,0,145,212,4,0,148,31,4,0,49,128,9,0,84,64,35,64,185,7,0,128,210,6,0,128,210,165,0,128,82,68,99,2,145,227,3,23,42,
      66,19,0,145,1,0,128,82,115,6,0,145,24,248,255,151,246,3,0,42,22,8,0,52,72,0,0,208,8,65,24,145,87,55,64,249,9,253,223,200,4,0,128,210,67,115,0,145,226,3,22,42,65,99,2,145,224,3,23,170,32,1,63,214,96,11,0,52,72,27,64,249,85,19,64,185,106,2,8,75,
      72,31,64,185,84,1,21,11,84,15,0,185,31,1,22,107,163,5,0,84,72,3,192,57,31,41,0,113,129,2,0,84,168,1,128,82,72,51,0,121,72,0,0,208,8,65,24,145,9,253,223,200,4,0,128,210,67,115,0,145,34,0,128,82,65,99,0,145,224,3,23,170,32,1,63,214,128,8,0,52,
      72,31,64,185,8,8,0,52,72,19,64,185,148,6,0,17,84,15,0,185,21,5,0,17,85,19,0,185,72,31,64,249,127,2,8,235,130,2,0,84,78,39,71,169,74,39,64,185,64,255,255,23,14,0,128,82,159,1,0,241,141,1,0,84,15,0,128,210,200,1,13,11,105,123,103,248,10,195,40,
      139,232,105,243,56,206,5,0,17,207,125,64,147,255,1,12,235,75,1,9,139,104,249,0,57,235,254,255,84,136,1,20,11,72,15,0,185,225,3,21,42,64,7,64,249,255,131,2,145,117,195,255,151,251,43,64,249,249,107,68,169,247,99,67,169,245,91,66,169,243,83,65,
      169,253,123,198,168,192,3,95,214,13,0,128,82,159,1,0,241,45,254,255,84,78,23,64,249,10,0,128,210,104,123,110,248,173,5,0,17,9,3,8,139,43,1,10,139,74,105,243,56,106,249,0,57,170,125,64,147,95,1,12,235,11,255,255,84,229,255,255,23,8,3,12,139,11,
      249,0,57,105,123,109,248,10,3,9,139,72,245,64,57,137,6,0,17,73,15,0,185,8,1,30,50,72,245,0,57,221,255,255,23,85,19,64,185,219,255,255,23,72,0,0,208,8,161,18,145,9,253,223,200,32,1,63,214,65,19,64,185,64,11,0,185,213,255,255,23,204,204,204,204,
      253,123,188,169,243,83,1,169,245,91,2,169,247,27,0,249,253,3,0,145,61,195,255,151,47,40,128,210,179,197,255,151,255,115,47,203,9,124,64,147,255,3,0,249,104,0,0,176,8,129,9,145,255,11,0,185,42,253,70,147,41,21,64,146,8,121,106,248,11,9,128,210,
      243,3,1,170,1,0,128,82,116,66,34,139,41,33,11,155,127,2,20,235,55,21,64,249,162,5,0,84,246,99,0,145,232,99,0,145,171,1,128,82,241,130,130,210,234,99,49,139,127,2,20,235,98,1,0,84,105,2,192,57,115,6,0,145,63,41,0,113,129,0,0,84,11,1,0,57,33,4,
      0,17,8,5,0,145,9,21,0,56,31,1,10,235,163,254,255,84,21,1,22,75,225,11,0,185,72,0,0,208,8,65,24,145,9,253,223,200,4,0,128,210,227,67,0,145,226,3,21,42,225,99,0,145,224,3,23,170,32,1,63,214,64,1,0,52,232,135,64,41,233,19,64,185,8,1,9,11,232,7,
      0,185,63,1,21,107,67,1,0,84,127,2,20,235,163,251,255,84,7,0,0,20,72,0,0,208,8,161,18,145,9,253,223,200,32,1,63,214,225,11,64,185,224,3,0,185,224,3,64,249,47,40,128,210,255,115,47,139,2,195,255,151,247,27,64,249,245,91,66,169,243,83,65,169,253,
      123,196,168,192,3,95,214,253,123,189,169,243,83,1,169,245,91,2,169,253,3,0,145,240,194,255,151,47,40,128,210,102,197,255,151,255,115,47,203,9,124,64,147,255,3,0,249,104,0,0,176,8,129,9,145,255,11,0,185,42,253,70,147,41,21,64,146,8,121,106,248,
      11,9,128,210,243,3,1,170,1,0,128,82,116,66,34,139,41,33,11,155,54,21,64,249,37,0,0,20,232,99,0,145,171,1,128,82,209,130,130,210,234,99,49,139,127,2,20,235,98,1,0,84,105,2,64,121,115,10,0,145,63,41,0,113,129,0,0,84,11,1,0,121,33,8,0,17,8,9,0,
      145,9,37,0,120,31,1,10,235,163,254,255,84,233,99,0,145,225,11,0,185,8,1,9,203,21,253,65,147,72,0,0,208,8,65,24,145,9,253,223,200,4,0,128,210,227,67,0,145,162,122,31,83,225,99,0,145,224,3,22,170,32,1,63,214,64,1,0,52,232,135,64,41,233,19,64,185,
      8,1,9,11,63,5,21,107,232,7,0,185,67,1,0,84,127,2,20,235,99,251,255,84,7,0,0,20,72,0,0,208,8,161,18,145,9,253,223,200,32,1,63,214,225,11,64,185,224,3,0,185,224,3,64,249,47,40,128,210,255,115,47,139,181,194,255,151,245,91,66,169,243,83,65,169,
      253,123,195,168,192,3,95,214,253,123,186,169,243,83,1,169,245,91,2,169,247,99,3,169,249,107,4,169,251,43,0,249,253,3,0,145,161,194,255,151,79,40,128,210,23,197,255,151,255,115,47,203,9,124,64,147,104,0,0,176,8,129,9,145,42,253,70,147,41,21,64,
      146,8,121,106,248,11,9,128,210,243,3,1,170,118,66,34,139,41,33,11,155,232,3,0,145,31,1,0,249,127,2,22,235,31,9,0,185,57,21,64,249,34,7,0,84,72,0,0,208,24,65,24,145,250,99,0,145,187,1,128,82,247,3,19,42,235,99,0,145,234,3,27,145,232,3,26,170,
      127,2,22,235,66,1,0,84,105,2,64,121,115,10,0,145,63,41,0,113,97,0,0,84,27,1,0,121,8,9,0,145,9,37,0,120,31,1,10,235,195,254,255,84,8,1,11,203,3,253,65,147,7,0,128,210,6,0,128,210,165,170,129,82,228,35,27,145,226,99,0,145,1,0,128,82,32,189,159,
      82,215,246,255,151,245,3,0,42,181,2,0,52,20,0,128,82,213,1,0,52,232,35,27,145,1,65,52,139,8,255,223,200,162,2,20,75,4,0,128,210,227,67,0,145,224,3,25,170,0,1,63,214,64,1,0,52,232,19,64,185,20,1,20,11,159,2,21,107,131,254,255,84,105,2,23,75,233,
      7,0,185,127,2,22,235,226,0,0,84,211,255,255,23,72,0,0,208,8,161,18,145,9,253,223,200,32,1,63,214,224,3,0,185,224,3,64,249,225,11,64,185,79,40,128,210,255,115,47,139,89,194,255,151,251,43,64,249,249,107,68,169,247,99,67,169,245,91,66,169,243,
      83,65,169,253,123,198,168,192,3,95,214,204,204,204,204,243,83,190,169,245,91,1,169,253,123,190,169,253,3,0,145,20,0,128,82,191,19,0,185,0,1,128,82,177,233,255,151,115,0,128,82,117,0,0,176,118,0,0,176,179,23,0,185,200,242,70,185,127,2,8,107,0,
      4,0,84,168,126,67,249,9,217,115,248,73,0,0,181,26,0,0,20,169,15,0,249,40,21,64,185,191,59,3,213,8,53,13,83,8,1,0,52,168,126,67,249,0,217,115,248,170,4,0,148,31,4,0,49,96,0,0,84,136,6,0,17,168,19,0,185,168,126,67,249,9,217,115,248,32,193,0,145,
      74,0,0,208,72,65,15,145,9,253,223,200,32,1,63,214,168,126,67,249,0,217,115,248,32,239,255,151,168,126,67,249,31,217,51,248,180,19,64,185,115,6,0,17,222,255,255,23,0,1,128,82,147,233,255,151,224,3,20,42,253,123,194,168,245,91,65,169,243,83,194,
      168,192,3,95,214,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,243,3,0,170,104,22,64,185,191,59,3,213,8,53,13,83,8,2,0,52,104,22,64,185,191,59,3,213,8,25,6,83,136,1,0,52,96,6,64,249,5,239,255,151,106,82,0,145,11,40,128,18,73,253,
      95,136,41,1,11,10,73,253,8,136,168,255,255,53,191,59,3,213,127,126,0,169,127,18,0,185,253,123,193,168,243,7,65,248,192,3,95,214,253,123,191,169,253,3,0,145,31,8,0,49,161,0,0,84,16,242,255,151,40,1,128,82,8,0,0,185,21,0,0,20,0,2,248,55,104,0,
      0,176,8,97,70,185,31,0,8,107,130,1,0,84,9,124,64,147,104,0,0,176,8,129,9,145,42,253,70,211,41,21,64,146,8,121,106,248,11,9,128,210,41,33,11,155,42,225,64,57,64,1,26,18,6,0,0,20,252,241,255,151,40,1,128,82,8,0,0,185,61,239,255,151,0,0,128,82,
      253,123,193,168,192,3,95,214,204,204,204,204,104,0,0,176,0,29,71,185,191,59,3,213,192,3,95,214,253,123,186,169,243,83,1,169,245,91,2,169,247,99,3,169,249,107,4,169,251,43,0,249,253,3,0,145,205,193,255,151,255,195,15,209,250,3,0,170,246,3,2,170,
      249,3,3,170,250,1,0,181,193,1,0,180,226,241,255,151,200,2,128,82,8,0,0,185,35,239,255,151,255,195,15,145,201,193,255,151,251,43,64,249,249,107,68,169,247,99,67,169,245,91,66,169,243,83,65,169,253,123,198,168,192,3,95,214,118,254,255,180,89,254,
      255,180,63,8,0,241,131,254,255,84,40,4,0,209,255,3,0,249,251,99,0,145,24,105,22,155,245,35,8,145,245,239,0,169,11,0,128,210,119,0,0,240,8,3,26,203,9,9,214,154,42,5,0,145,95,33,0,241,72,4,0,84,31,3,26,235,233,25,0,84,211,2,26,139,127,2,24,235,
      244,3,26,170,136,1,0,84,232,2,64,249,239,3,25,170,0,1,63,214,225,3,20,170,224,3,19,170,32,3,63,214,31,0,0,113,116,194,148,154,211,2,19,139,127,2,24,235,201,254,255,84,235,3,22,170,234,3,24,170,159,2,24,235,0,1,0,84,140,2,24,203,72,1,192,57,107,
      5,0,209,137,105,234,56,136,105,42,56,73,21,0,56,107,255,255,181,24,3,22,203,31,3,26,235,136,252,255,84,235,3,64,249,176,0,0,20,72,253,65,211,19,125,22,155,232,2,64,249,239,3,25,170,116,2,26,139,0,1,63,214,225,3,20,170,224,3,26,170,32,3,63,214,
      31,0,0,113,141,1,0,84,236,3,22,170,235,3,20,170,95,3,20,235,0,1,0,84,106,1,19,203,104,21,192,56,73,1,192,57,140,5,0,209,72,1,0,57,105,241,31,56,76,255,255,181,232,2,64,249,239,3,25,170,0,1,63,214,225,3,24,170,224,3,26,170,32,3,63,214,31,0,0,
      113,141,1,0,84,235,3,22,170,234,3,24,170,95,3,24,235,0,1,0,84,76,3,24,203,72,1,192,57,107,5,0,209,137,105,234,56,136,105,42,56,73,21,0,56,107,255,255,181,232,2,64,249,239,3,25,170,0,1,63,214,225,3,24,170,224,3,20,170,32,3,63,214,31,0,0,113,141,
      1,0,84,235,3,22,170,234,3,24,170,159,2,24,235,0,1,0,84,140,2,24,203,72,1,192,57,107,5,0,209,137,105,234,56,136,105,42,56,73,21,0,56,107,255,255,181,243,3,26,170,245,3,24,170,159,2,19,235,169,1,0,84,211,2,19,139,127,2,20,235,66,1,0,84,232,2,64,
      249,239,3,25,170,0,1,63,214,225,3,20,170,224,3,19,170,32,3,63,214,31,0,0,113,205,254,255,84,12,0,0,20,211,2,19,139,127,2,24,235,40,1,0,84,232,2,64,249,239,3,25,170,0,1,63,214,225,3,20,170,224,3,19,170,32,3,63,214,31,0,0,113,205,254,255,84,123,
      0,0,240,247,3,21,170,181,2,22,203,191,2,20,235,41,1,0,84,104,3,64,249,239,3,25,170,0,1,63,214,225,3,20,170,224,3,21,170,32,3,63,214,31,0,0,113,172,254,255,84,191,2,19,235,3,2,0,84,235,3,22,170,234,3,21,170,127,2,21,235,0,1,0,84,108,2,21,203,
      72,1,192,57,107,5,0,209,137,105,234,56,136,105,42,56,73,21,0,56,107,255,255,181,159,2,21,235,148,18,147,154,119,0,0,240,202,255,255,23,251,11,64,249,159,2,23,235,162,1,0,84,247,2,22,203,255,2,20,235,73,1,0,84,104,0,0,240,8,1,64,249,239,3,25,
      170,0,1,63,214,225,3,20,170,224,3,23,170,32,3,63,214,192,254,255,52,13,0,0,20,123,0,0,240,247,2,22,203,255,2,26,235,9,1,0,84,104,3,64,249,239,3,25,170,0,1,63,214,225,3,20,170,224,3,23,170,32,3,63,214,224,254,255,52,251,11,64,249,9,3,19,203,235,
      87,64,169,232,2,26,203,31,1,9,235,203,1,0,84,95,3,23,235,226,0,0,84,119,3,0,249,107,5,0,145,123,35,0,145,186,134,0,248,245,239,0,169,235,3,0,249,127,2,24,235,119,0,0,240,2,2,0,84,250,3,19,170,57,255,255,23,127,2,24,235,226,0,0,84,120,3,0,249,
      107,5,0,145,123,35,0,145,179,134,0,248,245,239,0,169,235,3,0,249,95,3,23,235,98,0,0,84,248,3,23,170,44,255,255,23,119,0,0,240,107,5,0,209,181,34,0,209,235,87,0,169,123,35,0,209,251,11,0,249,43,226,255,183,186,2,64,249,120,3,64,249,35,255,255,
      23,204,204,204,204,253,123,191,169,253,3,0,145,163,0,0,181,160,0,0,181,65,1,0,181,0,0,128,82,13,0,0,20,224,0,0,180,193,0,0,180,99,0,0,181,31,0,0,57,250,255,255,23,34,1,0,181,31,0,0,57,220,240,255,151,200,2,128,82,8,0,0,185,29,238,255,151,192,
      2,128,82,253,123,193,168,192,3,95,214,234,3,0,170,233,3,1,170,235,3,3,170,77,0,0,203,127,4,0,177,225,0,0,84,168,105,234,56,72,21,0,56,8,253,255,52,41,5,0,209,137,255,255,181,14,0,0,20,168,105,234,56,236,3,11,170,72,21,0,56,40,252,255,52,41,5,
      0,209,105,0,0,180,107,5,0,209,43,255,255,181,136,5,0,209,63,1,0,241,136,145,136,154,72,0,0,181,95,1,0,57,233,250,255,181,127,4,0,177,161,0,0,84,8,0,1,139,31,241,31,56,0,10,128,82,223,255,255,23,31,0,0,57,180,240,255,151,72,4,128,82,8,0,0,185,
      245,237,255,151,64,4,128,82,216,255,255,23,253,123,191,169,253,3,0,145,144,192,255,151,255,131,0,209,8,0,128,82,234,3,0,145,9,125,64,211,63,129,0,241,66,4,0,84,8,5,0,17,63,105,42,56,31,129,0,113,75,255,255,84,41,0,64,57,105,1,0,52,236,3,0,145,
      45,0,128,82,43,253,67,211,40,9,0,18,105,105,108,56,170,33,200,26,72,1,9,42,41,28,64,56,104,105,44,56,41,255,255,53,9,0,64,57,105,1,0,52,43,0,128,82,236,3,0,145,40,9,0,18,41,253,67,211,106,33,200,26,40,105,108,56,95,1,8,106,129,0,0,84,9,28,64,
      56,41,255,255,53,0,0,128,210,255,131,0,145,115,192,255,151,253,123,193,168,192,3,95,214,110,1,0,148,204,204,204,204,2,0,128,210,1,0,0,20,243,83,191,169,253,123,189,169,253,3,0,145,244,3,0,170,243,3,1,170,244,0,0,181,122,240,255,151,200,2,128,
      82,8,0,0,185,187,237,255,151,0,0,128,210,30,0,0,20,83,255,255,180,159,2,19,235,130,255,255,84,225,3,2,170,224,67,0,145,67,220,255,151,236,19,64,249,136,9,64,185,104,0,0,53,96,6,0,209,13,0,0,20,107,6,0,209,107,5,0,209,159,2,11,235,168,0,0,84,
      104,1,64,57,8,1,12,139,9,101,64,57,73,255,23,55,104,2,11,203,9,1,64,146,106,2,9,203,64,5,0,209,232,163,64,57,168,0,0,52,234,11,64,249,72,169,67,185,9,121,30,18,73,169,3,185,253,123,195,168,243,83,193,168,192,3,95,214,253,123,191,169,253,3,0,
      145,224,0,0,181,81,240,255,151,200,2,128,82,8,0,0,185,146,237,255,151,0,0,128,146,9,0,0,20,226,3,0,170,104,0,0,144,0,45,65,249,73,0,0,176,40,129,20,145,9,253,223,200,1,0,128,82,32,1,63,214,253,123,193,168,192,3,95,214,204,204,204,204,243,83,
      190,169,245,91,1,169,253,123,191,169,253,3,0,145,244,3,0,170,243,3,1,170,148,0,0,181,224,3,19,170,198,245,255,151,36,0,0,20,147,0,0,181,224,3,20,170,24,237,255,151,31,0,0,20,127,130,0,177,72,3,0,84,72,0,0,176,8,97,20,145,118,0,0,144,192,46,65,
      249,9,253,223,200,227,3,19,170,226,3,20,170,1,0,128,82,32,1,63,214,128,2,0,181,72,0,0,176,21,97,20,145,48,254,255,151,128,1,0,52,224,3,19,170,251,214,255,151,32,1,0,52,192,46,65,249,227,3,19,170,168,254,223,200,226,3,20,170,1,0,128,82,0,1,63,
      214,160,254,255,180,5,0,0,20,23,240,255,151,136,1,128,82,8,0,0,185,0,0,128,210,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,243,83,189,169,245,91,1,169,247,99,2,169,253,123,191,169,253,3,0,145,255,67,0,209,243,3,
      3,42,127,2,0,113,245,3,0,170,246,3,1,42,244,3,2,170,247,3,4,170,248,3,5,42,237,0,0,84,97,126,64,147,224,3,20,170,102,191,255,151,31,0,19,107,19,4,0,17,19,160,147,26,7,0,128,210,255,3,0,249,6,0,128,210,229,3,24,42,228,3,23,170,227,3,19,42,226,
      3,20,170,225,3,22,42,224,3,21,170,17,230,255,151,255,67,0,145,253,123,193,168,247,99,66,169,245,91,65,169,243,83,195,168,192,3,95,214,243,83,191,169,253,123,191,169,253,3,0,145,243,3,0,170,147,8,0,180,72,0,0,240,20,1,32,145,96,14,64,249,136,
      14,64,249,31,0,8,235,64,0,0,84,195,236,255,151,96,18,64,249,136,18,64,249,31,0,8,235,64,0,0,84,190,236,255,151,96,22,64,249,136,22,64,249,31,0,8,235,64,0,0,84,185,236,255,151,96,26,64,249,136,26,64,249,31,0,8,235,64,0,0,84,180,236,255,151,96,
      30,64,249,136,30,64,249,31,0,8,235,64,0,0,84,175,236,255,151,96,34,64,249,136,34,64,249,31,0,8,235,64,0,0,84,170,236,255,151,96,38,64,249,136,38,64,249,31,0,8,235,64,0,0,84,165,236,255,151,96,54,64,249,136,54,64,249,31,0,8,235,64,0,0,84,160,
      236,255,151,96,58,64,249,136,58,64,249,31,0,8,235,64,0,0,84,155,236,255,151,96,62,64,249,136,62,64,249,31,0,8,235,64,0,0,84,150,236,255,151,96,66,64,249,136,66,64,249,31,0,8,235,64,0,0,84,145,236,255,151,96,70,64,249,136,70,64,249,31,0,8,235,
      64,0,0,84,140,236,255,151,96,74,64,249,136,74,64,249,31,0,8,235,64,0,0,84,135,236,255,151,253,123,193,168,243,83,193,168,192,3,95,214,204,204,204,204,243,83,191,169,253,123,191,169,253,3,0,145,243,3,0,170,147,3,0,180,72,0,0,240,20,1,32,145,96,
      2,64,249,136,2,64,249,31,0,8,235,64,0,0,84,119,236,255,151,96,6,64,249,136,6,64,249,31,0,8,235,64,0,0,84,114,236,255,151,96,10,64,249,136,10,64,249,31,0,8,235,64,0,0,84,109,236,255,151,96,46,64,249,136,46,64,249,31,0,8,235,64,0,0,84,104,236,
      255,151,96,50,64,249,136,50,64,249,31,0,8,235,64,0,0,84,99,236,255,151,253,123,193,168,243,83,193,168,192,3,95,214,204,204,204,204,243,83,190,169,245,11,0,249,253,123,191,169,253,3,0,145,243,3,0,170,179,10,0,180,116,226,0,145,127,2,20,235,245,
      3,19,170,160,0,0,84,160,134,64,248,83,236,255,151,191,2,20,235,161,255,255,84,149,226,0,145,3,0,0,20,128,134,64,248,77,236,255,151,159,2,21,235,161,255,255,84,116,194,1,145,117,66,3,145,3,0,0,20,128,134,64,248,70,236,255,151,159,2,21,235,161,
      255,255,84,116,66,3,145,117,194,4,145,3,0,0,20,128,134,64,248,63,236,255,151,159,2,21,235,161,255,255,84,116,194,4,145,117,2,5,145,3,0,0,20,128,134,64,248,56,236,255,151,159,2,21,235,161,255,255,84,96,162,64,249,52,236,255,151,96,166,64,249,
      50,236,255,151,96,170,64,249,48,236,255,151,116,130,5,145,117,98,6,145,3,0,0,20,128,134,64,248,43,236,255,151,159,2,21,235,161,255,255,84,116,98,6,145,117,66,7,145,3,0,0,20,128,134,64,248,36,236,255,151,159,2,21,235,161,255,255,84,116,66,7,145,
      117,194,8,145,3,0,0,20,128,134,64,248,29,236,255,151,159,2,21,235,161,255,255,84,116,194,8,145,117,66,10,145,3,0,0,20,128,134,64,248,22,236,255,151,159,2,21,235,161,255,255,84,116,66,10,145,117,130,10,145,3,0,0,20,128,134,64,248,15,236,255,151,
      159,2,21,235,161,255,255,84,96,82,65,249,11,236,255,151,96,86,65,249,9,236,255,151,96,90,65,249,7,236,255,151,96,94,65,249,5,236,255,151,253,123,193,168,245,11,64,249,243,83,194,168,192,3,95,214,255,67,0,209,224,3,0,185,224,3,64,185,96,0,62,
      212,255,67,0,145,192,3,95,214,253,123,191,169,253,3,0,145,0,1,128,82,247,255,255,151,253,123,193,168,192,3,95,214,255,67,0,209,224,3,0,249,64,0,128,82,96,0,62,212,255,67,0,145,192,3,95,214,72,0,0,176,8,193,19,145,9,253,223,200,32,1,31,214,243,
      83,190,169,245,91,1,169,253,123,191,169,253,3,0,145,244,3,0,170,245,3,1,170,246,3,2,170,243,3,3,170,85,9,0,180,54,9,0,180,168,2,192,57,136,0,0,53,20,9,0,180,159,2,0,121,70,0,0,20,104,162,64,57,104,0,0,53,224,3,19,170,116,219,255,151,106,14,64,
      249,40,189,159,82,64,13,64,185,31,0,8,107,129,1,0,84,104,0,0,144,3,65,45,145,228,3,19,170,226,3,22,170,225,3,21,170,224,3,20,170,148,0,0,148,9,0,128,18,31,0,0,113,32,177,128,26,51,0,0,20,72,157,64,249,168,0,0,181,116,5,0,180,168,2,64,57,136,
      2,0,121,40,0,0,20,169,2,64,57,72,1,64,249,9,217,233,120,137,3,248,54,67,9,64,185,127,4,0,113,77,1,0,84,223,2,3,107,11,1,0,84,159,2,0,241,229,7,159,26,228,3,20,170,226,3,21,170,33,1,128,82,247,242,255,151,224,0,0,53,104,14,64,249,9,9,64,185,223,
      194,41,235,195,0,0,84,168,6,192,57,136,0,0,52,104,14,64,249,0,9,64,185,20,0,0,20,40,0,128,82,73,5,128,82,104,194,0,57,105,46,0,185,0,0,128,18,14,0,0,20,159,2,0,241,229,7,159,26,228,3,20,170,35,0,128,82,226,3,21,170,33,1,128,82,224,242,255,151,
      96,254,255,52,32,0,128,82,4,0,0,20,104,0,0,144,31,169,5,249,0,0,128,82,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,237,3,0,170,98,0,0,181,0,0,128,82,16,0,0,20,170,37,64,120,43,36,64,120,72,5,1,81,31,101,0,113,73,
      129,0,17,104,5,1,81,76,129,137,26,105,129,0,17,31,101,0,113,104,129,137,26,128,1,8,75,128,0,0,53,108,0,0,52,66,4,0,209,66,254,255,181,192,3,95,214,104,0,0,144,0,89,75,185,192,3,95,214,204,204,204,204,233,3,0,170,40,1,192,57,0,0,128,210,200,0,
      0,52,31,0,1,235,128,0,0,84,0,4,0,145,8,104,233,56,136,255,255,53,192,3,95,214,2,0,0,20,204,204,204,204,243,83,190,169,245,91,1,169,253,123,190,169,253,3,0,145,244,3,0,42,245,3,1,170,246,3,2,42,243,3,3,170,252,245,255,151,31,4,0,177,225,0,0,84,
      40,0,128,82,41,1,128,82,104,194,0,57,105,46,0,185,0,0,128,146,31,0,0,20,72,0,0,176,8,1,23,145,9,253,223,200,227,3,22,42,226,67,0,145,225,3,21,170,32,1,63,214,0,1,0,53,72,0,0,176,8,161,18,145,9,253,223,200,32,1,63,214,225,3,19,170,176,238,255,
      151,240,255,255,23,224,11,64,249,31,4,0,177,160,253,255,84,137,126,64,147,104,0,0,144,8,129,9,145,42,253,70,147,41,21,64,146,8,121,106,248,11,9,128,210,43,33,11,155,168,31,128,82,106,225,64,57,72,1,8,10,104,225,0,57,253,123,194,168,245,91,65,
      169,243,83,194,168,192,3,95,214,204,204,204,204,243,15,31,248,253,123,190,169,253,3,0,145,243,3,0,170,224,67,0,145,35,1,0,148,31,16,0,241,8,1,0,84,232,19,64,185,233,255,159,82,31,1,9,107,169,255,159,82,41,129,136,26,83,0,0,180,105,2,0,121,253,
      123,194,168,243,7,65,248,192,3,95,214,243,83,187,169,245,91,1,169,247,99,2,169,249,107,3,169,251,35,0,249,253,123,190,169,253,3,0,145,248,3,1,170,19,3,64,249,247,3,0,170,246,3,2,170,249,3,3,170,245,3,4,170,119,8,0,180,244,3,23,170,182,5,0,180,
      122,0,128,210,155,0,128,210,104,2,192,57,104,0,0,53,34,0,128,210,8,0,0,20,104,6,192,57,104,0,0,53,66,0,128,210,4,0,0,20,104,10,192,57,31,1,0,113,66,3,155,154,228,3,21,170,227,3,25,170,225,3,19,170,224,67,0,145,245,0,0,148,31,4,0,177,0,4,0,84,
      128,3,0,180,232,19,64,185,233,255,159,82,31,1,9,107,169,1,0,84,223,6,0,241,73,2,0,84,10,65,64,81,8,0,155,82,214,6,0,209,8,41,74,42,234,19,0,185,136,2,0,121,232,6,128,82,10,85,22,51,138,46,0,120,2,0,0,20,136,2,0,121,136,10,0,145,148,10,0,145,
      19,0,19,139,214,6,0,209,22,251,255,181,244,3,8,170,136,2,23,203,19,3,0,249,0,253,65,147,10,0,0,20,19,0,128,210,159,2,0,121,250,255,255,23,19,3,0,249,73,5,128,82,40,0,128,82,169,46,0,185,168,194,0,57,0,0,128,146,253,123,194,168,251,35,64,249,
      249,107,67,169,247,99,66,169,245,91,65,169,243,83,197,168,192,3,95,214,20,0,128,210,118,0,128,210,151,0,128,210,104,2,192,57,104,0,0,53,34,0,128,210,8,0,0,20,104,6,192,57,104,0,0,53,66,0,128,210,4,0,0,20,104,10,192,57,31,1,0,113,194,2,151,154,
      228,3,21,170,227,3,25,170,225,3,19,170,0,0,128,210,180,0,0,148,31,4,0,177,0,252,255,84,224,0,0,180,31,16,0,241,65,0,0,84,148,6,0,145,19,0,19,139,148,6,0,145,232,255,255,23,224,3,20,170,220,255,255,23,253,123,190,169,253,3,0,145,224,35,0,121,
      23,1,0,148,0,1,0,52,226,83,0,145,33,0,128,82,224,67,0,145,44,1,0,148,96,0,0,52,224,35,64,121,2,0,0,20,224,255,159,82,253,123,194,168,192,3,95,214,204,204,204,204,243,83,189,169,245,91,1,169,247,19,0,249,253,123,191,169,253,3,0,145,244,3,3,170,
      147,30,64,249,246,3,1,170,245,3,0,170,247,3,2,170,225,3,20,170,98,18,0,145,224,3,22,170,237,226,255,151,168,6,64,185,201,12,128,82,43,0,128,82,74,0,128,82,31,1,9,106,104,6,64,185,105,1,138,26,63,1,8,106,224,0,0,84,227,3,20,170,226,3,23,170,225,
      3,22,170,224,3,21,170,255,209,255,151,2,0,0,20,32,0,128,82,253,123,193,168,247,19,64,249,245,91,65,169,243,83,195,168,192,3,95,214,204,204,204,204,243,15,31,248,253,123,187,169,253,3,0,145,104,0,0,144,8,17,71,185,255,11,0,249,255,131,0,57,255,
      227,0,57,255,3,1,57,255,35,1,57,8,1,0,53,72,0,0,240,8,33,27,145,9,41,64,169,235,163,0,145,105,41,0,169,43,0,128,82,235,227,0,57,225,67,0,145,59,0,0,148,243,3,0,42,224,67,0,145,206,217,255,151,224,3,19,42,253,123,197,168,243,7,65,248,192,3,95,
      214,204,204,204,204,243,83,190,169,245,11,0,249,253,123,191,169,253,3,0,145,243,3,0,170,245,3,1,170,211,1,0,181,40,0,128,82,201,2,128,82,168,194,0,57,229,3,21,170,169,46,0,185,4,0,128,210,3,0,128,82,2,0,128,210,1,0,128,210,0,0,128,210,193,234,
      255,151,0,0,128,18,26,0,0,20,104,22,64,185,20,0,128,18,191,59,3,213,8,53,13,83,72,2,0,52,225,3,21,170,224,3,19,170,43,236,255,151,244,3,0,42,224,3,19,170,56,251,255,151,224,3,19,170,146,247,255,151,225,3,21,170,74,1,0,148,96,0,248,54,20,0,128,
      18,5,0,0,20,96,22,64,249,96,0,0,180,64,234,255,151,127,22,0,249,224,3,19,170,153,1,0,148,224,3,20,42,253,123,193,168,245,11,64,249,243,83,194,168,192,3,95,214,204,204,204,204,243,83,191,169,253,123,190,169,253,3,0,145,243,3,0,170,179,11,0,249,
      244,3,1,170,19,2,0,181,40,0,128,82,136,194,0,57,201,2,128,82,137,46,0,185,229,3,20,170,4,0,128,210,3,0,128,82,2,0,128,210,1,0,128,210,0,0,128,210,143,234,255,151,0,0,128,18,253,123,194,168,243,83,193,168,192,3,95,214,104,22,64,185,191,59,3,213,
      8,49,12,83,224,3,19,170,104,0,0,52,119,1,0,148,246,255,255,23,175,236,255,151,225,3,20,170,224,3,19,170,174,255,255,151,244,3,0,42,224,3,19,170,175,236,255,151,224,3,20,42,238,255,255,23,253,123,191,169,253,3,0,145,14,189,255,151,255,67,0,209,
      104,0,0,144,8,129,45,145,127,0,0,241,106,16,136,154,63,0,0,241,40,0,128,210,79,16,136,154,8,0,0,208,8,73,55,145,63,0,0,241,46,16,136,154,63,0,0,241,7,16,159,154,111,0,0,181,32,0,128,146,90,0,0,20,72,13,64,121,232,3,0,53,203,1,64,57,206,5,0,145,
      203,0,56,55,71,0,0,180,235,0,0,185,127,1,0,113,224,7,159,154,80,0,0,20,104,9,27,18,31,1,3,113,97,0,0,84,76,0,128,82,10,0,0,20,104,13,28,18,31,129,3,113,97,0,0,84,108,0,128,82,5,0,0,20,104,17,29,18,31,193,3,113,1,8,0,84,140,0,128,82,232,0,128,
      82,41,0,128,82,8,1,12,75,40,33,200,26,9,5,0,81,43,1,11,10,237,3,12,42,11,0,0,20,76,17,64,57,13,29,0,83,75,1,64,185,136,249,3,17,9,29,0,83,63,9,0,113,160,153,65,122,227,5,0,84,191,1,12,107,162,5,0,84,191,1,15,235,233,33,141,154,207,1,1,203,8,
      0,0,20,198,21,192,56,239,5,0,145,200,4,26,18,31,1,2,113,129,4,0,84,200,20,0,18,11,25,11,42,255,1,9,235,3,255,255,84,63,1,13,235,226,0,0,84,168,1,41,75,75,1,0,185,76,9,0,121,9,29,0,83,73,13,0,121,192,255,255,23,8,0,155,82,104,1,8,75,31,253,31,
      113,137,2,0,84,127,65,68,113,66,2,0,84,8,16,128,82,232,3,0,185,9,0,129,82,40,0,160,82,233,163,0,41,136,9,0,209,233,3,0,145,40,121,104,184,127,1,8,107,3,1,0,84,71,0,0,180,235,0,0,185,127,1,0,113,224,3,141,154,225,3,10,170,22,1,0,148,4,0,0,20,
      225,3,4,170,224,3,10,170,10,1,0,148,255,67,0,145,170,188,255,151,253,123,193,168,192,3,95,214,204,204,204,204,243,15,31,248,253,123,191,169,253,3,0,145,83,0,0,208,96,210,68,249,31,8,0,177,193,1,0,84,40,0,0,208,0,33,53,145,73,0,0,144,40,33,15,
      145,9,253,223,200,6,0,128,210,5,0,128,82,100,0,128,82,3,0,128,210,98,0,128,82,1,0,168,82,32,1,63,214,96,210,4,249,31,4,0,177,224,7,159,26,253,123,193,168,243,7,65,248,192,3,95,214,204,204,204,204,243,83,189,169,245,91,1,169,247,19,0,249,253,
      123,191,169,253,3,0,145,72,0,0,144,8,33,24,145,244,3,0,170,87,0,0,208,224,210,68,249,9,253,223,200,245,3,1,42,246,3,2,170,4,0,128,210,227,3,22,170,226,3,21,42,225,3,20,170,32,1,63,214,243,3,0,42,147,4,0,53,72,0,0,144,8,161,18,145,9,253,223,200,
      32,1,63,214,31,24,0,113,193,3,0,84,224,210,68,249,31,12,0,177,168,0,0,84,72,0,0,144,8,1,15,145,9,253,223,200,32,1,63,214,40,0,0,208,0,33,53,145,73,0,0,144,40,33,15,145,9,253,223,200,6,0,128,210,5,0,128,82,100,0,128,82,3,0,128,210,98,0,128,82,
      1,0,168,82,32,1,63,214,72,0,0,144,8,33,24,145,4,0,128,210,224,210,4,249,9,253,223,200,227,3,22,170,226,3,21,42,225,3,20,170,32,1,63,214,243,3,0,42,224,3,19,42,253,123,193,168,247,19,64,249,245,91,65,169,243,83,195,168,192,3,95,214,204,204,204,
      204,204,204,204,204,204,204,204,204,72,0,0,208,0,209,68,249,31,12,0,177,168,0,0,84,72,0,0,144,8,1,15,145,9,253,223,200,32,1,31,214,192,3,95,214,204,204,204,204,243,83,190,169,245,91,1,169,253,123,191,169,253,3,0,145,244,3,0,42,245,3,1,170,204,
      243,255,151,31,4,0,177,86,0,0,240,97,0,0,84,19,0,128,82,29,0,0,20,202,50,65,249,159,6,0,113,97,0,0,84,72,33,67,57,168,0,0,55,159,10,0,113,65,1,0,84,72,1,66,57,8,1,0,54,32,0,128,82,188,243,255,151,243,3,0,170,64,0,128,82,185,243,255,151,127,2,
      0,235,224,253,255,84,224,3,20,42,181,243,255,151,72,0,0,144,8,1,15,145,9,253,223,200,32,1,63,214,0,253,255,53,72,0,0,144,8,161,18,145,9,253,223,200,32,1,63,214,243,3,0,42,224,3,20,42,205,243,255,151,136,126,64,147,201,130,9,145,11,253,70,147,
      10,21,64,146,40,121,107,248,12,9,128,210,73,33,12,155,63,225,0,57,211,0,0,52,225,3,21,170,224,3,19,42,103,236,255,151,0,0,128,18,2,0,0,20,0,0,128,82,253,123,193,168,245,91,65,169,243,83,194,168,192,3,95,214,204,204,204,204,253,123,189,169,253,
      3,0,145,224,23,0,185,31,8,0,49,1,1,0,84,40,0,128,82,63,52,0,185,41,1,128,82,40,224,0,57,40,192,0,57,41,44,0,185,38,0,0,20,0,3,248,55,72,0,0,240,8,97,70,185,31,0,8,107,130,2,0,84,9,124,64,147,72,0,0,240,8,129,9,145,42,253,70,147,41,21,64,146,
      8,121,106,248,11,9,128,210,41,33,11,155,42,225,64,57,74,1,0,54,232,83,0,145,232,7,2,169,224,3,3,41,225,115,0,145,224,67,0,145,227,99,0,145,226,131,0,145,18,0,0,148,15,0,0,20,40,0,128,82,40,224,0,57,41,1,128,82,63,52,0,185,229,3,1,170,40,192,
      0,57,4,0,128,210,41,44,0,185,3,0,128,82,1,0,128,210,2,0,128,210,0,0,128,210,54,233,255,151,0,0,128,18,253,123,195,168,192,3,95,214,243,83,191,169,253,123,190,169,253,3,0,145,243,3,2,170,244,3,3,170,180,11,0,249,32,0,64,185,53,244,255,151,104,
      2,64,249,0,1,64,185,9,124,64,147,107,6,64,249,42,253,70,147,72,0,0,240,8,129,9,145,41,21,64,146,12,9,128,210,8,121,106,248,41,33,12,155,42,225,64,57,170,0,0,54,225,3,11,170,120,255,255,151,243,3,0,42,6,0,0,20,40,0,128,82,104,193,0,57,41,1,128,
      82,105,45,0,185,19,0,128,18,128,2,64,185,41,244,255,151,224,3,19,42,253,123,194,168,243,83,193,168,192,3,95,214,8,0,128,18,31,124,0,169,8,24,0,185,8,80,0,145,31,16,0,185,31,192,1,248,31,20,0,249,31,253,159,136,191,59,3,213,192,3,95,214,40,0,
      128,82,31,0,0,249,73,5,128,82,40,192,0,57,41,44,0,185,0,0,128,146,192,3,95,214,204,204,204,204,63,0,0,249,192,3,95,214,253,123,191,169,128,255,255,208,0,160,18,145,253,123,193,168,192,3,95,214,97,0,0,212,253,123,191,169,253,3,1,170,160,67,64,
      57,177,204,255,151,31,32,3,213,253,123,193,168,192,3,95,214,97,0,0,212,253,123,191,169,253,3,1,170,160,67,64,57,169,204,255,151,31,32,3,213,253,123,193,168,192,3,95,214,97,0,0,212,253,123,191,169,253,3,1,170,74,205,255,151,31,32,3,213,253,123,
      193,168,192,3,95,214,97,0,0,212,253,123,191,169,253,3,1,170,8,0,64,249,4,1,64,185,229,3,0,170,136,255,255,208,3,193,41,145,162,15,64,249,161,23,64,185,160,19,64,249,243,204,255,151,31,32,3,213,253,123,193,168,192,3,95,214,97,0,0,212,253,123,
      191,169,253,3,1,170,8,0,64,249,9,1,64,185,202,0,0,24,63,1,10,107,224,23,159,26,253,123,193,168,192,3,95,214,97,0,0,212,5,0,0,192,243,15,31,248,253,123,191,169,253,3,1,170,160,47,0,249,177,99,0,145,32,254,159,200,119,218,255,151,168,19,64,249,
      8,56,0,249,177,99,0,145,40,254,223,200,9,1,64,249,51,29,64,249,112,218,255,151,19,52,0,249,177,99,0,145,40,254,223,200,39,0,128,82,6,0,128,210,5,0,128,82,164,23,64,249,163,27,64,249,162,31,64,249,161,35,64,249,0,1,64,249,220,220,255,151,99,218,
      255,151,31,56,0,249,40,0,128,82,177,67,0,145,40,254,159,136,32,0,128,82,253,123,193,168,243,7,65,248,192,3,95,214,97,0,0,212,243,15,31,248,253,123,191,169,253,3,1,170,160,23,0,249,177,131,0,145,32,254,159,200,168,67,64,57,200,5,0,52,177,131,
      0,145,40,254,223,200,9,1,64,249,177,99,0,145,41,254,159,200,177,99,0,145,42,254,223,200,233,5,0,24,72,1,64,185,31,1,9,107,97,4,0,84,177,99,0,145,40,254,223,200,9,25,64,185,63,17,0,113,193,3,0,84,177,99,0,145,40,254,223,200,169,4,0,24,8,33,64,
      185,31,1,9,107,160,1,0,84,177,99,0,145,40,254,223,200,9,4,0,24,8,33,64,185,31,1,9,107,224,0,0,84,177,99,0,145,40,254,223,200,105,3,0,24,8,33,64,185,31,1,9,107,129,1,0,84,47,218,255,151,177,99,0,145,40,254,223,200,8,16,0,249,177,131,0,145,41,
      254,223,200,51,5,64,249,40,218,255,151,19,20,0,249,34,215,255,151,31,32,3,213,177,83,0,145,63,254,159,136,177,83,0,145,32,254,223,136,253,123,193,168,243,7,65,248,192,3,95,214,97,0,0,212,31,32,3,213,99,115,109,224,32,5,147,25,33,5,147,25,34,
      5,147,25,253,123,191,169,253,3,1,170,0,0,128,82,106,226,255,151,31,32,3,213,253,123,193,168,192,3,95,214,97,0,0,212,253,123,191,169,253,3,1,170,168,11,64,249,0,1,64,185,97,226,255,151,31,32,3,213,253,123,193,168,192,3,95,214,97,0,0,212,253,123,
      191,169,253,3,1,170,160,19,0,249,8,0,64,249,9,1,64,185,169,23,0,185,72,1,0,24,63,1,8,107,232,23,159,26,177,67,0,145,40,254,159,136,177,67,0,145,32,254,223,136,253,123,193,168,192,3,95,214,97,0,0,212,99,115,109,224,253,123,191,169,253,3,1,170,
      8,0,64,249,9,1,64,185,170,0,128,82,10,0,184,114,63,1,10,107,224,23,159,26,253,123,193,168,192,3,95,214,97,0,0,212,253,123,191,169,253,3,1,170,160,55,0,249,162,67,0,145,161,15,64,249,241,219,255,151,31,32,3,213,253,123,193,168,192,3,95,214,97,
      0,0,212,243,15,31,248,253,123,191,169,253,3,1,170,160,27,64,249,18,207,255,151,168,19,64,185,168,2,0,53,233,3,0,24,179,15,64,249,104,2,64,185,31,1,9,107,1,2,0,84,104,26,64,185,31,17,0,113,161,1,0,84,104,34,64,185,233,2,0,24,8,1,9,75,31,9,0,113,
      8,1,0,84,96,22,64,249,137,207,255,151,160,0,0,52,33,0,128,82,224,3,19,170,83,207,255,151,31,32,3,213,197,217,255,151,168,31,64,249,8,16,0,249,194,217,255,151,168,35,64,249,8,20,0,249,253,123,193,168,243,7,65,248,192,3,95,214,97,0,0,212,31,32,
      3,213,99,115,109,224,32,5,147,25,253,123,191,169,253,3,1,170,144,207,255,151,31,32,3,213,253,123,193,168,192,3,95,214,97,0,0,212,253,123,191,169,253,3,1,170,175,217,255,151,8,48,64,185,31,1,0,113,173,0,0,84,171,217,255,151,8,48,64,185,9,5,0,
      81,9,48,0,185,253,123,193,168,192,3,95,214,97,0,0,212,253,123,191,169,253,3,1,170,224,0,128,82,247,225,255,151,31,32,3,213,253,123,193,168,192,3,95,214,97,0,0,212,253,123,191,169,253,3,1,170,160,0,128,82,239,225,255,151,31,32,3,213,253,123,193,
      168,192,3,95,214,97,0,0,212,253,123,191,169,253,3,1,170,168,67,64,57,136,0,0,52,96,0,128,82,229,225,255,151,31,32,3,213,253,123,193,168,192,3,95,214,97,0,0,212,253,123,191,169,253,3,1,170,168,11,64,249,0,1,64,249,254,233,255,151,31,32,3,213,
      253,123,193,168,192,3,95,214,97,0,0,212,253,123,191,169,253,3,1,170,168,31,64,249,0,1,64,185,211,225,255,151,31,32,3,213,253,123,193,168,192,3,95,214,97,0,0,212,253,123,191,169,253,3,1,170,168,19,64,249,0,1,64,185,202,225,255,151,31,32,3,213,
      253,123,193,168,192,3,95,214,97,0,0,212,253,123,191,169,253,3,1,170,128,0,128,82,194,225,255,151,31,32,3,213,253,123,193,168,192,3,95,214,97,0,0,212,253,123,191,169,253,3,1,170,168,11,64,249,0,1,64,185,191,242,255,151,31,32,3,213,253,123,193,
      168,192,3,95,214,97,0,0,212,253,123,191,169,253,3,1,170,160,19,64,185,183,242,255,151,31,32,3,213,253,123,193,168,192,3,95,214,97,0,0,212,253,123,191,169,253,3,1,170,0,1,128,82,169,225,255,151,31,32,3,213,253,123,193,168,192,3,95,214,97,0,0,
      212,253,123,191,169,253,3,1,170,160,11,64,249,195,233,255,151,31,32,3,213,253,123,193,168,192,3,95,214,97,0,0,212,81,0,0,208,49,2,43,145,22,0,0,20,81,0,0,208,49,34,43,145,19,0,0,20,81,0,0,208,49,66,43,145,16,0,0,20,81,0,0,208,49,98,43,145,13,
      0,0,20,81,0,0,208,49,130,43,145,10,0,0,20,81,0,0,208,49,162,43,145,7,0,0,20,81,0,0,208,49,194,43,145,4,0,0,20,81,0,0,208,49,226,43,145,1,0,0,20,253,123,179,169,253,3,0,145,224,7,1,169,226,15,2,169,228,23,3,169,230,31,4,169,224,135,2,173,226,
      143,3,173,228,151,4,173,230,159,5,173,225,3,17,170,32,0,0,240,0,160,58,145,145,201,255,151,240,3,0,170,230,159,69,173,228,151,68,173,226,143,67,173,224,135,66,173,230,31,68,169,228,23,67,169,226,15,66,169,224,7,65,169,253,123,205,168,0,2,31,
      214,81,0,0,208,49,34,44,145,4,0,0,20,81,0,0,208,49,66,44,145,1,0,0,20,253,123,179,169,253,3,0,145,224,7,1,169,226,15,2,169,228,23,3,169,230,31,4,169,224,135,2,173,226,143,3,173,228,151,4,173,230,159,5,173,225,3,17,170,32,0,0,240,0,32,59,145,
      114,201,255,151,240,3,0,170,230,159,69,173,228,151,68,173,226,143,67,173,224,135,66,173,230,31,68,169,228,23,67,169,226,15,66,169,224,7,65,169,253,123,205,168,0,2,31,214,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,
      204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,
      204,204,204,204,204,204,204,204,204,34,5,147,25,2,0,0,0,12,172,1,0,1,0,0,0,28,172,1,0,1,0,0,0,72,172,1,0,240,255,255,255,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,224,153,1,128,1,0,0,0,224,70,0,128,1,0,0,0,192,70,0,128,1,0,0,0,85,110,107,110,111,119,110,
      32,101,120,99,101,112,116,105,111,110,0,0,0,0,0,0,0,88,154,1,128,1,0,0,0,224,70,0,128,1,0,0,0,192,70,0,128,1,0,0,0,98,97,100,32,97,108,108,111,99,97,116,105,111,110,0,0,216,154,1,128,1,0,0,0,224,70,0,128,1,0,0,0,192,70,0,128,1,0,0,0,98,97,100,
      32,97,114,114,97,121,32,110,101,119,32,108,101,110,103,116,104,0,0,0,0,96,155,1,128,1,0,0,0,96,72,0,128,1,0,0,0,0,0,0,0,0,0,0,0,75,0,69,0,82,0,78,0,69,0,76,0,51,0,50,0,46,0,68,0,76,0,76,0,0,0,0,0,0,0,0,0,65,99,113,117,105,114,101,83,82,87,76,
      111,99,107,69,120,99,108,117,115,105,118,101,0,82,101,108,101,97,115,101,83,82,87,76,111,99,107,69,120,99,108,117,115,105,118,101,0,56,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,192,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,240,1,128,1,0,0,0,0,0,0,0,0,0,0,0,103,157,1,128,1,0,0,0,50,0,0,0,0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,224,203,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,41,0,0,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,15,0,0,0,0,0,0,0,32,5,147,25,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,176,101,0,128,1,0,0,0,96,108,0,128,1,0,0,0,0,0,0,0,0,0,0,0,208,108,0,
      128,1,0,0,0,0,0,0,0,0,0,0,0,32,170,0,128,1,0,0,0,80,170,0,128,1,0,0,0,176,91,0,128,1,0,0,0,176,91,0,128,1,0,0,0,192,175,0,128,1,0,0,0,64,176,0,128,1,0,0,0,0,177,0,128,1,0,0,0,48,177,0,128,1,0,0,0,0,0,0,0,0,0,0,0,32,109,0,128,1,0,0,0,64,177,0,
      128,1,0,0,0,144,177,0,128,1,0,0,0,96,185,0,128,1,0,0,0,176,185,0,128,1,0,0,0,64,188,0,128,1,0,0,0,176,91,0,128,1,0,0,0,176,188,0,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,176,91,0,128,1,0,0,0,0,0,0,0,0,0,0,0,160,109,0,128,1,0,0,0,0,0,0,0,0,
      0,0,0,64,109,0,128,1,0,0,0,176,91,0,128,1,0,0,0,176,108,0,128,1,0,0,0,128,108,0,128,1,0,0,0,176,91,0,128,1,0,0,0,109,0,115,0,99,0,111,0,114,0,101,0,101,0,46,0,100,0,108,0,108,0,0,0,67,111,114,69,120,105,116,80,114,111,99,101,115,115,0,0,0,0,
      0,0,0,0,0,0,34,5,147,25,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,144,174,1,0,232,255,255,255,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,34,5,147,25,1,0,0,0,188,174,1,0,0,0,0,0,0,0,0,0,2,0,0,0,208,174,1,0,224,255,255,255,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,34,
      5,147,25,1,0,0,0,188,174,1,0,0,0,0,0,0,0,0,0,1,0,0,0,80,175,1,0,240,255,255,255,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,34,5,147,25,1,0,0,0,188,174,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,224,255,255,255,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,5,0,0,192,11,0,0,
      0,0,0,0,0,0,0,0,0,29,0,0,192,4,0,0,0,0,0,0,0,0,0,0,0,150,0,0,192,4,0,0,0,0,0,0,0,0,0,0,0,141,0,0,192,8,0,0,0,0,0,0,0,0,0,0,0,142,0,0,192,8,0,0,0,0,0,0,0,0,0,0,0,143,0,0,192,8,0,0,0,0,0,0,0,0,0,0,0,144,0,0,192,8,0,0,0,0,0,0,0,0,0,0,0,145,0,0,
      192,8,0,0,0,0,0,0,0,0,0,0,0,146,0,0,192,8,0,0,0,0,0,0,0,0,0,0,0,147,0,0,192,8,0,0,0,0,0,0,0,0,0,0,0,180,2,0,192,8,0,0,0,0,0,0,0,0,0,0,0,181,2,0,192,8,0,0,0,0,0,0,0,0,0,0,0,12,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,9,0,0,0,0,0,0,0,216,155,1,128,1,0,0,
      0,224,70,0,128,1,0,0,0,192,70,0,128,1,0,0,0,98,97,100,32,101,120,99,101,112,116,105,111,110,0,0,0,0,61,1,128,1,0,0,0,8,0,0,0,0,0,0,0,16,61,1,128,1,0,0,0,7,0,0,0,0,0,0,0,24,61,1,128,1,0,0,0,8,0,0,0,0,0,0,0,40,61,1,128,1,0,0,0,9,0,0,0,0,0,0,0,
      56,61,1,128,1,0,0,0,10,0,0,0,0,0,0,0,72,61,1,128,1,0,0,0,10,0,0,0,0,0,0,0,88,61,1,128,1,0,0,0,12,0,0,0,0,0,0,0,104,61,1,128,1,0,0,0,9,0,0,0,0,0,0,0,116,61,1,128,1,0,0,0,6,0,0,0,0,0,0,0,128,61,1,128,1,0,0,0,9,0,0,0,0,0,0,0,144,61,1,128,1,0,0,
      0,9,0,0,0,0,0,0,0,160,61,1,128,1,0,0,0,7,0,0,0,0,0,0,0,168,61,1,128,1,0,0,0,10,0,0,0,0,0,0,0,184,61,1,128,1,0,0,0,11,0,0,0,0,0,0,0,200,61,1,128,1,0,0,0,9,0,0,0,0,0,0,0,210,61,1,128,1,0,0,0,0,0,0,0,0,0,0,0,212,61,1,128,1,0,0,0,4,0,0,0,0,0,0,0,
      224,61,1,128,1,0,0,0,7,0,0,0,0,0,0,0,232,61,1,128,1,0,0,0,1,0,0,0,0,0,0,0,236,61,1,128,1,0,0,0,2,0,0,0,0,0,0,0,240,61,1,128,1,0,0,0,2,0,0,0,0,0,0,0,244,61,1,128,1,0,0,0,1,0,0,0,0,0,0,0,248,61,1,128,1,0,0,0,2,0,0,0,0,0,0,0,252,61,1,128,1,0,0,
      0,2,0,0,0,0,0,0,0,0,62,1,128,1,0,0,0,2,0,0,0,0,0,0,0,8,62,1,128,1,0,0,0,8,0,0,0,0,0,0,0,20,62,1,128,1,0,0,0,2,0,0,0,0,0,0,0,24,62,1,128,1,0,0,0,1,0,0,0,0,0,0,0,28,62,1,128,1,0,0,0,2,0,0,0,0,0,0,0,32,62,1,128,1,0,0,0,2,0,0,0,0,0,0,0,36,62,1,128,
      1,0,0,0,1,0,0,0,0,0,0,0,40,62,1,128,1,0,0,0,1,0,0,0,0,0,0,0,44,62,1,128,1,0,0,0,1,0,0,0,0,0,0,0,48,62,1,128,1,0,0,0,3,0,0,0,0,0,0,0,52,62,1,128,1,0,0,0,1,0,0,0,0,0,0,0,56,62,1,128,1,0,0,0,1,0,0,0,0,0,0,0,60,62,1,128,1,0,0,0,1,0,0,0,0,0,0,0,64,
      62,1,128,1,0,0,0,2,0,0,0,0,0,0,0,68,62,1,128,1,0,0,0,1,0,0,0,0,0,0,0,72,62,1,128,1,0,0,0,2,0,0,0,0,0,0,0,76,62,1,128,1,0,0,0,1,0,0,0,0,0,0,0,80,62,1,128,1,0,0,0,2,0,0,0,0,0,0,0,84,62,1,128,1,0,0,0,1,0,0,0,0,0,0,0,88,62,1,128,1,0,0,0,1,0,0,0,
      0,0,0,0,92,62,1,128,1,0,0,0,1,0,0,0,0,0,0,0,96,62,1,128,1,0,0,0,2,0,0,0,0,0,0,0,100,62,1,128,1,0,0,0,2,0,0,0,0,0,0,0,104,62,1,128,1,0,0,0,2,0,0,0,0,0,0,0,108,62,1,128,1,0,0,0,2,0,0,0,0,0,0,0,112,62,1,128,1,0,0,0,2,0,0,0,0,0,0,0,116,62,1,128,
      1,0,0,0,2,0,0,0,0,0,0,0,120,62,1,128,1,0,0,0,2,0,0,0,0,0,0,0,124,62,1,128,1,0,0,0,3,0,0,0,0,0,0,0,128,62,1,128,1,0,0,0,3,0,0,0,0,0,0,0,132,62,1,128,1,0,0,0,2,0,0,0,0,0,0,0,136,62,1,128,1,0,0,0,2,0,0,0,0,0,0,0,140,62,1,128,1,0,0,0,2,0,0,0,0,0,
      0,0,144,62,1,128,1,0,0,0,9,0,0,0,0,0,0,0,160,62,1,128,1,0,0,0,9,0,0,0,0,0,0,0,176,62,1,128,1,0,0,0,7,0,0,0,0,0,0,0,184,62,1,128,1,0,0,0,8,0,0,0,0,0,0,0,200,62,1,128,1,0,0,0,20,0,0,0,0,0,0,0,224,62,1,128,1,0,0,0,8,0,0,0,0,0,0,0,240,62,1,128,1,
      0,0,0,18,0,0,0,0,0,0,0,8,63,1,128,1,0,0,0,28,0,0,0,0,0,0,0,40,63,1,128,1,0,0,0,29,0,0,0,0,0,0,0,72,63,1,128,1,0,0,0,28,0,0,0,0,0,0,0,104,63,1,128,1,0,0,0,29,0,0,0,0,0,0,0,136,63,1,128,1,0,0,0,28,0,0,0,0,0,0,0,168,63,1,128,1,0,0,0,35,0,0,0,0,
      0,0,0,208,63,1,128,1,0,0,0,26,0,0,0,0,0,0,0,240,63,1,128,1,0,0,0,32,0,0,0,0,0,0,0,24,64,1,128,1,0,0,0,31,0,0,0,0,0,0,0,56,64,1,128,1,0,0,0,38,0,0,0,0,0,0,0,96,64,1,128,1,0,0,0,26,0,0,0,0,0,0,0,128,64,1,128,1,0,0,0,15,0,0,0,0,0,0,0,144,64,1,128,
      1,0,0,0,3,0,0,0,0,0,0,0,148,64,1,128,1,0,0,0,5,0,0,0,0,0,0,0,160,64,1,128,1,0,0,0,15,0,0,0,0,0,0,0,176,64,1,128,1,0,0,0,35,0,0,0,0,0,0,0,212,64,1,128,1,0,0,0,6,0,0,0,0,0,0,0,224,64,1,128,1,0,0,0,9,0,0,0,0,0,0,0,240,64,1,128,1,0,0,0,14,0,0,0,
      0,0,0,0,0,65,1,128,1,0,0,0,26,0,0,0,0,0,0,0,32,65,1,128,1,0,0,0,28,0,0,0,0,0,0,0,64,65,1,128,1,0,0,0,37,0,0,0,0,0,0,0,104,65,1,128,1,0,0,0,36,0,0,0,0,0,0,0,144,65,1,128,1,0,0,0,37,0,0,0,0,0,0,0,184,65,1,128,1,0,0,0,43,0,0,0,0,0,0,0,232,65,1,
      128,1,0,0,0,26,0,0,0,0,0,0,0,8,66,1,128,1,0,0,0,32,0,0,0,0,0,0,0,48,66,1,128,1,0,0,0,34,0,0,0,0,0,0,0,88,66,1,128,1,0,0,0,40,0,0,0,0,0,0,0,136,66,1,128,1,0,0,0,42,0,0,0,0,0,0,0,184,66,1,128,1,0,0,0,27,0,0,0,0,0,0,0,216,66,1,128,1,0,0,0,12,0,
      0,0,0,0,0,0,232,66,1,128,1,0,0,0,17,0,0,0,0,0,0,0,0,67,1,128,1,0,0,0,11,0,0,0,0,0,0,0,210,61,1,128,1,0,0,0,0,0,0,0,0,0,0,0,16,67,1,128,1,0,0,0,17,0,0,0,0,0,0,0,40,67,1,128,1,0,0,0,27,0,0,0,0,0,0,0,72,67,1,128,1,0,0,0,18,0,0,0,0,0,0,0,96,67,1,
      128,1,0,0,0,28,0,0,0,0,0,0,0,128,67,1,128,1,0,0,0,25,0,0,0,0,0,0,0,210,61,1,128,1,0,0,0,0,0,0,0,0,0,0,0,24,62,1,128,1,0,0,0,1,0,0,0,0,0,0,0,44,62,1,128,1,0,0,0,1,0,0,0,0,0,0,0,96,62,1,128,1,0,0,0,2,0,0,0,0,0,0,0,88,62,1,128,1,0,0,0,1,0,0,0,0,
      0,0,0,56,62,1,128,1,0,0,0,1,0,0,0,0,0,0,0,224,62,1,128,1,0,0,0,8,0,0,0,0,0,0,0,160,67,1,128,1,0,0,0,21,0,0,0,0,0,0,0,95,95,98,97,115,101,100,40,0,0,0,0,0,0,0,0,95,95,99,100,101,99,108,0,95,95,112,97,115,99,97,108,0,0,0,0,0,0,0,0,95,95,115,116,
      100,99,97,108,108,0,0,0,0,0,0,0,95,95,116,104,105,115,99,97,108,108,0,0,0,0,0,0,95,95,102,97,115,116,99,97,108,108,0,0,0,0,0,0,95,95,118,101,99,116,111,114,99,97,108,108,0,0,0,0,95,95,99,108,114,99,97,108,108,0,0,0,95,95,101,97,98,105,0,0,0,
      0,0,0,95,95,115,119,105,102,116,95,49,0,0,0,0,0,0,0,95,95,115,119,105,102,116,95,50,0,0,0,0,0,0,0,95,95,112,116,114,54,52,0,95,95,114,101,115,116,114,105,99,116,0,0,0,0,0,0,95,95,117,110,97,108,105,103,110,101,100,0,0,0,0,0,114,101,115,116,114,
      105,99,116,40,0,0,0,32,110,101,119,0,0,0,0,0,0,0,0,32,100,101,108,101,116,101,0,61,0,0,0,62,62,0,0,60,60,0,0,33,0,0,0,61,61,0,0,33,61,0,0,91,93,0,0,0,0,0,0,111,112,101,114,97,116,111,114,0,0,0,0,45,62,0,0,42,0,0,0,43,43,0,0,45,45,0,0,45,0,0,
      0,43,0,0,0,38,0,0,0,45,62,42,0,47,0,0,0,37,0,0,0,60,0,0,0,60,61,0,0,62,0,0,0,62,61,0,0,44,0,0,0,40,41,0,0,126,0,0,0,94,0,0,0,124,0,0,0,38,38,0,0,124,124,0,0,42,61,0,0,43,61,0,0,45,61,0,0,47,61,0,0,37,61,0,0,62,62,61,0,60,60,61,0,38,61,0,0,124,
      61,0,0,94,61,0,0,96,118,102,116,97,98,108,101,39,0,0,0,0,0,0,0,96,118,98,116,97,98,108,101,39,0,0,0,0,0,0,0,96,118,99,97,108,108,39,0,96,116,121,112,101,111,102,39,0,0,0,0,0,0,0,0,96,108,111,99,97,108,32,115,116,97,116,105,99,32,103,117,97,114,
      100,39,0,0,0,0,96,115,116,114,105,110,103,39,0,0,0,0,0,0,0,0,96,118,98,97,115,101,32,100,101,115,116,114,117,99,116,111,114,39,0,0,0,0,0,0,96,118,101,99,116,111,114,32,100,101,108,101,116,105,110,103,32,100,101,115,116,114,117,99,116,111,114,
      39,0,0,0,0,96,100,101,102,97,117,108,116,32,99,111,110,115,116,114,117,99,116,111,114,32,99,108,111,115,117,114,101,39,0,0,0,96,115,99,97,108,97,114,32,100,101,108,101,116,105,110,103,32,100,101,115,116,114,117,99,116,111,114,39,0,0,0,0,96,118,
      101,99,116,111,114,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,96,118,101,99,116,111,114,32,100,101,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,0,96,118,101,99,116,111,
      114,32,118,98,97,115,101,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,0,0,96,118,105,114,116,117,97,108,32,100,105,115,112,108,97,99,101,109,101,110,116,32,109,97,112,39,0,0,0,0,0,0,96,101,104,32,118,
      101,99,116,111,114,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,0,0,0,0,0,96,101,104,32,118,101,99,116,111,114,32,100,101,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,96,101,
      104,32,118,101,99,116,111,114,32,118,98,97,115,101,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,96,99,111,112,121,32,99,111,110,115,116,114,117,99,116,111,114,32,99,108,111,115,117,114,101,39,0,0,0,0,
      0,0,96,117,100,116,32,114,101,116,117,114,110,105,110,103,39,0,96,69,72,0,96,82,84,84,73,0,0,0,0,0,0,0,96,108,111,99,97,108,32,118,102,116,97,98,108,101,39,0,96,108,111,99,97,108,32,118,102,116,97,98,108,101,32,99,111,110,115,116,114,117,99,
      116,111,114,32,99,108,111,115,117,114,101,39,0,32,110,101,119,91,93,0,0,0,0,0,0,32,100,101,108,101,116,101,91,93,0,0,0,0,0,0,0,96,111,109,110,105,32,99,97,108,108,115,105,103,39,0,0,96,112,108,97,99,101,109,101,110,116,32,100,101,108,101,116,
      101,32,99,108,111,115,117,114,101,39,0,0,0,0,0,0,96,112,108,97,99,101,109,101,110,116,32,100,101,108,101,116,101,91,93,32,99,108,111,115,117,114,101,39,0,0,0,0,96,109,97,110,97,103,101,100,32,118,101,99,116,111,114,32,99,111,110,115,116,114,
      117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,96,109,97,110,97,103,101,100,32,118,101,99,116,111,114,32,100,101,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,0,96,101,104,32,118,101,99,116,111,114,
      32,99,111,112,121,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,96,101,104,32,118,101,99,116,111,114,32,118,98,97,115,101,32,99,111,112,121,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,
      114,97,116,111,114,39,0,0,0,0,0,96,100,121,110,97,109,105,99,32,105,110,105,116,105,97,108,105,122,101,114,32,102,111,114,32,39,0,0,0,0,0,0,96,100,121,110,97,109,105,99,32,97,116,101,120,105,116,32,100,101,115,116,114,117,99,116,111,114,32,102,
      111,114,32,39,0,0,0,0,0,0,0,0,96,118,101,99,116,111,114,32,99,111,112,121,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,0,0,0,96,118,101,99,116,111,114,32,118,98,97,115,101,32,99,111,112,121,32,99,111,
      110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,0,0,0,0,0,96,109,97,110,97,103,101,100,32,118,101,99,116,111,114,32,99,111,112,121,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,
      0,0,0,0,0,0,96,108,111,99,97,108,32,115,116,97,116,105,99,32,116,104,114,101,97,100,32,103,117,97,114,100,39,0,0,0,0,0,111,112,101,114,97,116,111,114,32,34,34,32,0,0,0,0,111,112,101,114,97,116,111,114,32,99,111,95,97,119,97,105,116,0,0,0,0,0,
      0,0,111,112,101,114,97,116,111,114,60,61,62,0,0,0,0,0,32,84,121,112,101,32,68,101,115,99,114,105,112,116,111,114,39,0,0,0,0,0,0,0,32,66,97,115,101,32,67,108,97,115,115,32,68,101,115,99,114,105,112,116,111,114,32,97,116,32,40,0,0,0,0,0,32,66,
      97,115,101,32,67,108,97,115,115,32,65,114,114,97,121,39,0,0,0,0,0,0,32,67,108,97,115,115,32,72,105,101,114,97,114,99,104,121,32,68,101,115,99,114,105,112,116,111,114,39,0,0,0,0,32,67,111,109,112,108,101,116,101,32,79,98,106,101,99,116,32,76,
      111,99,97,116,111,114,39,0,0,0,0,0,0,0,96,97,110,111,110,121,109,111,117,115,32,110,97,109,101,115,112,97,99,101,39,0,0,0,0,0,0,0,0,0,0,0,96,68,1,128,1,0,0,0,160,68,1,128,1,0,0,0,224,68,1,128,1,0,0,0,48,69,1,128,1,0,0,0,144,69,1,128,1,0,0,0,
      224,69,1,128,1,0,0,0,32,70,1,128,1,0,0,0,96,70,1,128,1,0,0,0,160,70,1,128,1,0,0,0,224,70,1,128,1,0,0,0,32,71,1,128,1,0,0,0,112,71,1,128,1,0,0,0,208,71,1,128,1,0,0,0,32,72,1,128,1,0,0,0,112,72,1,128,1,0,0,0,136,72,1,128,1,0,0,0,160,72,1,128,1,
      0,0,0,176,72,1,128,1,0,0,0,248,72,1,128,1,0,0,0,0,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,101,0,45,0,100,0,97,0,116,0,101,0,116,0,105,0,109,0,101,0,45,0,108,0,49,0,45,0,49,0,45,0,49,0,0,0,
      97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,101,0,45,0,102,0,105,0,108,0,101,0,45,0,108,0,49,0,45,0,50,0,45,0,50,0,0,0,0,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,
      0,114,0,101,0,45,0,108,0,111,0,99,0,97,0,108,0,105,0,122,0,97,0,116,0,105,0,111,0,110,0,45,0,108,0,49,0,45,0,50,0,45,0,49,0,0,0,0,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,101,0,45,0,108,0,111,
      0,99,0,97,0,108,0,105,0,122,0,97,0,116,0,105,0,111,0,110,0,45,0,111,0,98,0,115,0,111,0,108,0,101,0,116,0,101,0,45,0,108,0,49,0,45,0,50,0,45,0,48,0,0,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,
      101,0,45,0,112,0,114,0,111,0,99,0,101,0,115,0,115,0,116,0,104,0,114,0,101,0,97,0,100,0,115,0,45,0,108,0,49,0,45,0,49,0,45,0,50,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,101,0,45,0,115,0,116,
      0,114,0,105,0,110,0,103,0,45,0,108,0,49,0,45,0,49,0,45,0,48,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,101,0,45,0,115,0,121,0,110,0,99,0,104,0,45,0,108,0,49,0,45,0,50,0,45,0,48,0,0,0,0,0,0,0,
      0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,101,0,45,0,115,0,121,0,115,0,105,0,110,0,102,0,111,0,45,0,108,0,49,0,45,0,50,0,45,0,49,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,
      99,0,111,0,114,0,101,0,45,0,119,0,105,0,110,0,114,0,116,0,45,0,108,0,49,0,45,0,49,0,45,0,48,0,0,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,101,0,45,0,120,0,115,0,116,0,97,0,116,0,101,0,45,0,108,
      0,50,0,45,0,49,0,45,0,48,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,114,0,116,0,99,0,111,0,114,0,101,0,45,0,110,0,116,0,117,0,115,0,101,0,114,0,45,0,119,0,105,0,110,0,100,0,111,0,119,0,45,0,108,0,49,0,45,0,49,
      0,45,0,48,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,115,0,101,0,99,0,117,0,114,0,105,0,116,0,121,0,45,0,115,0,121,0,115,0,116,0,101,0,109,0,102,0,117,0,110,0,99,0,116,0,105,0,111,0,110,0,115,0,45,0,108,0,49,0,45,
      0,49,0,45,0,48,0,0,0,0,0,0,0,0,0,0,0,0,0,101,0,120,0,116,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,110,0,116,0,117,0,115,0,101,0,114,0,45,0,100,0,105,0,97,0,108,0,111,0,103,0,98,0,111,0,120,0,45,0,108,0,49,0,45,0,49,0,45,0,48,0,0,0,0,0,
      0,0,0,0,0,0,0,0,101,0,120,0,116,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,110,0,116,0,117,0,115,0,101,0,114,0,45,0,119,0,105,0,110,0,100,0,111,0,119,0,115,0,116,0,97,0,116,0,105,0,111,0,110,0,45,0,108,0,49,0,45,0,49,0,45,0,48,0,0,0,0,0,
      97,0,100,0,118,0,97,0,112,0,105,0,51,0,50,0,0,0,0,0,0,0,0,0,107,0,101,0,114,0,110,0,101,0,108,0,51,0,50,0,0,0,0,0,0,0,0,0,110,0,116,0,100,0,108,0,108,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,97,0,112,0,112,
      0,109,0,111,0,100,0,101,0,108,0,45,0,114,0,117,0,110,0,116,0,105,0,109,0,101,0,45,0,108,0,49,0,45,0,49,0,45,0,50,0,0,0,0,0,117,0,115,0,101,0,114,0,51,0,50,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,0,0,101,0,120,0,116,0,45,0,109,0,115,
      0,45,0,0,0,15,0,0,0,0,0,0,0,65,114,101,70,105,108,101,65,112,105,115,65,78,83,73,0,6,0,0,0,15,0,0,0,73,110,105,116,105,97,108,105,122,101,67,114,105,116,105,99,97,108,83,101,99,116,105,111,110,69,120,0,0,0,0,0,2,0,0,0,15,0,0,0,76,67,77,97,112,
      83,116,114,105,110,103,69,120,0,0,0,2,0,0,0,15,0,0,0,76,111,99,97,108,101,78,97,109,101,84,111,76,67,73,68,0,0,0,0,17,0,0,0,65,112,112,80,111,108,105,99,121,71,101,116,80,114,111,99,101,115,115,84,101,114,109,105,110,97,116,105,111,110,77,101,
      116,104,111,100,0,0,0,0,106,0,97,0,45,0,74,0,80,0,0,0,0,0,0,0,122,0,104,0,45,0,67,0,78,0,0,0,0,0,0,0,107,0,111,0,45,0,75,0,82,0,0,0,0,0,0,0,122,0,104,0,45,0,84,0,87,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,208,76,1,128,1,0,0,0,212,76,1,128,1,0,0,0,216,
      76,1,128,1,0,0,0,220,76,1,128,1,0,0,0,224,76,1,128,1,0,0,0,228,76,1,128,1,0,0,0,232,76,1,128,1,0,0,0,236,76,1,128,1,0,0,0,244,76,1,128,1,0,0,0,0,77,1,128,1,0,0,0,8,77,1,128,1,0,0,0,24,77,1,128,1,0,0,0,36,77,1,128,1,0,0,0,48,77,1,128,1,0,0,0,
      60,77,1,128,1,0,0,0,64,77,1,128,1,0,0,0,68,77,1,128,1,0,0,0,72,77,1,128,1,0,0,0,76,77,1,128,1,0,0,0,80,77,1,128,1,0,0,0,84,77,1,128,1,0,0,0,88,77,1,128,1,0,0,0,92,77,1,128,1,0,0,0,96,77,1,128,1,0,0,0,100,77,1,128,1,0,0,0,104,77,1,128,1,0,0,0,
      112,77,1,128,1,0,0,0,120,77,1,128,1,0,0,0,132,77,1,128,1,0,0,0,140,77,1,128,1,0,0,0,76,77,1,128,1,0,0,0,148,77,1,128,1,0,0,0,156,77,1,128,1,0,0,0,164,77,1,128,1,0,0,0,176,77,1,128,1,0,0,0,192,77,1,128,1,0,0,0,200,77,1,128,1,0,0,0,216,77,1,128,
      1,0,0,0,228,77,1,128,1,0,0,0,232,77,1,128,1,0,0,0,240,77,1,128,1,0,0,0,0,78,1,128,1,0,0,0,24,78,1,128,1,0,0,0,1,0,0,0,0,0,0,0,40,78,1,128,1,0,0,0,48,78,1,128,1,0,0,0,56,78,1,128,1,0,0,0,64,78,1,128,1,0,0,0,72,78,1,128,1,0,0,0,80,78,1,128,1,0,
      0,0,88,78,1,128,1,0,0,0,96,78,1,128,1,0,0,0,112,78,1,128,1,0,0,0,128,78,1,128,1,0,0,0,144,78,1,128,1,0,0,0,168,78,1,128,1,0,0,0,192,78,1,128,1,0,0,0,208,78,1,128,1,0,0,0,232,78,1,128,1,0,0,0,240,78,1,128,1,0,0,0,248,78,1,128,1,0,0,0,0,79,1,128,
      1,0,0,0,8,79,1,128,1,0,0,0,16,79,1,128,1,0,0,0,24,79,1,128,1,0,0,0,32,79,1,128,1,0,0,0,40,79,1,128,1,0,0,0,48,79,1,128,1,0,0,0,56,79,1,128,1,0,0,0,64,79,1,128,1,0,0,0,72,79,1,128,1,0,0,0,88,79,1,128,1,0,0,0,112,79,1,128,1,0,0,0,128,79,1,128,
      1,0,0,0,8,79,1,128,1,0,0,0,144,79,1,128,1,0,0,0,160,79,1,128,1,0,0,0,176,79,1,128,1,0,0,0,192,79,1,128,1,0,0,0,216,79,1,128,1,0,0,0,232,79,1,128,1,0,0,0,0,80,1,128,1,0,0,0,20,80,1,128,1,0,0,0,28,80,1,128,1,0,0,0,40,80,1,128,1,0,0,0,64,80,1,128,
      1,0,0,0,104,80,1,128,1,0,0,0,128,80,1,128,1,0,0,0,83,117,110,0,77,111,110,0,84,117,101,0,87,101,100,0,84,104,117,0,70,114,105,0,83,97,116,0,83,117,110,100,97,121,0,0,77,111,110,100,97,121,0,0,0,0,0,0,84,117,101,115,100,97,121,0,87,101,100,110,
      101,115,100,97,121,0,0,0,0,0,0,0,84,104,117,114,115,100,97,121,0,0,0,0,70,114,105,100,97,121,0,0,0,0,0,0,83,97,116,117,114,100,97,121,0,0,0,0,74,97,110,0,70,101,98,0,77,97,114,0,65,112,114,0,77,97,121,0,74,117,110,0,74,117,108,0,65,117,103,0,
      83,101,112,0,79,99,116,0,78,111,118,0,68,101,99,0,0,0,0,0,74,97,110,117,97,114,121,0,70,101,98,114,117,97,114,121,0,0,0,0,77,97,114,99,104,0,0,0,65,112,114,105,108,0,0,0,74,117,110,101,0,0,0,0,74,117,108,121,0,0,0,0,65,117,103,117,115,116,0,
      0,0,0,0,0,83,101,112,116,101,109,98,101,114,0,0,0,0,0,0,0,79,99,116,111,98,101,114,0,78,111,118,101,109,98,101,114,0,0,0,0,0,0,0,0,68,101,99,101,109,98,101,114,0,0,0,0,65,77,0,0,80,77,0,0,0,0,0,0,77,77,47,100,100,47,121,121,0,0,0,0,0,0,0,0,100,
      100,100,100,44,32,77,77,77,77,32,100,100,44,32,121,121,121,121,0,0,0,0,0,72,72,58,109,109,58,115,115,0,0,0,0,0,0,0,0,83,0,117,0,110,0,0,0,77,0,111,0,110,0,0,0,84,0,117,0,101,0,0,0,87,0,101,0,100,0,0,0,84,0,104,0,117,0,0,0,70,0,114,0,105,0,0,
      0,83,0,97,0,116,0,0,0,83,0,117,0,110,0,100,0,97,0,121,0,0,0,0,0,77,0,111,0,110,0,100,0,97,0,121,0,0,0,0,0,84,0,117,0,101,0,115,0,100,0,97,0,121,0,0,0,87,0,101,0,100,0,110,0,101,0,115,0,100,0,97,0,121,0,0,0,0,0,0,0,84,0,104,0,117,0,114,0,115,
      0,100,0,97,0,121,0,0,0,0,0,0,0,0,0,70,0,114,0,105,0,100,0,97,0,121,0,0,0,0,0,83,0,97,0,116,0,117,0,114,0,100,0,97,0,121,0,0,0,0,0,0,0,0,0,74,0,97,0,110,0,0,0,70,0,101,0,98,0,0,0,77,0,97,0,114,0,0,0,65,0,112,0,114,0,0,0,77,0,97,0,121,0,0,0,74,
      0,117,0,110,0,0,0,74,0,117,0,108,0,0,0,65,0,117,0,103,0,0,0,83,0,101,0,112,0,0,0,79,0,99,0,116,0,0,0,78,0,111,0,118,0,0,0,68,0,101,0,99,0,0,0,74,0,97,0,110,0,117,0,97,0,114,0,121,0,0,0,70,0,101,0,98,0,114,0,117,0,97,0,114,0,121,0,0,0,0,0,0,0,
      0,0,77,0,97,0,114,0,99,0,104,0,0,0,0,0,0,0,65,0,112,0,114,0,105,0,108,0,0,0,0,0,0,0,74,0,117,0,110,0,101,0,0,0,0,0,0,0,0,0,74,0,117,0,108,0,121,0,0,0,0,0,0,0,0,0,65,0,117,0,103,0,117,0,115,0,116,0,0,0,0,0,83,0,101,0,112,0,116,0,101,0,109,0,98,
      0,101,0,114,0,0,0,0,0,0,0,79,0,99,0,116,0,111,0,98,0,101,0,114,0,0,0,78,0,111,0,118,0,101,0,109,0,98,0,101,0,114,0,0,0,0,0,0,0,0,0,68,0,101,0,99,0,101,0,109,0,98,0,101,0,114,0,0,0,0,0,65,0,77,0,0,0,0,0,80,0,77,0,0,0,0,0,0,0,0,0,77,0,77,0,47,
      0,100,0,100,0,47,0,121,0,121,0,0,0,0,0,0,0,0,0,100,0,100,0,100,0,100,0,44,0,32,0,77,0,77,0,77,0,77,0,32,0,100,0,100,0,44,0,32,0,121,0,121,0,121,0,121,0,0,0,72,0,72,0,58,0,109,0,109,0,58,0,115,0,115,0,0,0,0,0,0,0,0,0,101,0,110,0,45,0,85,0,83,
      0,0,0,0,0,0,0,1,0,0,0,22,0,0,0,2,0,0,0,2,0,0,0,3,0,0,0,2,0,0,0,4,0,0,0,24,0,0,0,5,0,0,0,13,0,0,0,6,0,0,0,9,0,0,0,7,0,0,0,12,0,0,0,8,0,0,0,12,0,0,0,9,0,0,0,12,0,0,0,10,0,0,0,7,0,0,0,11,0,0,0,8,0,0,0,12,0,0,0,22,0,0,0,13,0,0,0,22,0,0,0,15,0,0,
      0,2,0,0,0,16,0,0,0,13,0,0,0,17,0,0,0,18,0,0,0,18,0,0,0,2,0,0,0,33,0,0,0,13,0,0,0,53,0,0,0,2,0,0,0,65,0,0,0,13,0,0,0,67,0,0,0,2,0,0,0,80,0,0,0,17,0,0,0,82,0,0,0,13,0,0,0,83,0,0,0,13,0,0,0,87,0,0,0,22,0,0,0,89,0,0,0,11,0,0,0,108,0,0,0,13,0,0,0,
      109,0,0,0,32,0,0,0,112,0,0,0,28,0,0,0,114,0,0,0,9,0,0,0,128,0,0,0,10,0,0,0,129,0,0,0,10,0,0,0,130,0,0,0,9,0,0,0,131,0,0,0,22,0,0,0,132,0,0,0,13,0,0,0,145,0,0,0,41,0,0,0,158,0,0,0,13,0,0,0,161,0,0,0,2,0,0,0,164,0,0,0,11,0,0,0,167,0,0,0,13,0,0,
      0,183,0,0,0,17,0,0,0,206,0,0,0,2,0,0,0,215,0,0,0,11,0,0,0,89,4,0,0,42,0,0,0,24,7,0,0,12,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,40,0,40,0,40,0,40,0,40,0,32,0,32,0,32,0,32,0,32,0,32,0,32,
      0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,72,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,132,0,132,0,132,0,132,0,132,0,132,0,132,0,132,0,132,0,132,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,129,0,129,
      0,129,0,129,0,129,0,129,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,16,0,16,0,16,0,16,0,16,0,16,0,130,0,130,0,130,0,130,0,130,0,130,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,
      0,2,0,2,0,16,0,16,0,16,0,16,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,
      173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,
      234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,
      55,56,57,58,59,60,61,62,63,64,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,
      121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,
      182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,
      243,244,245,246,247,248,249,250,251,252,253,254,255,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
      176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,
      237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,
      59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,123,124,125,126,127,128,129,130,131,132,133,134,135,
      136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,
      197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,0,0,32,
      0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,40,0,40,0,40,0,40,0,40,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,72,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,132,
      0,132,0,132,0,132,0,132,0,132,0,132,0,132,0,132,0,132,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,129,1,129,1,129,1,129,1,129,1,129,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,16,0,16,0,16,0,16,0,16,0,16,0,130,
      1,130,1,130,1,130,1,130,1,130,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,16,0,16,0,16,0,16,0,32,0,32,0,32,0,32,0,32,0,32,0,40,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,
      32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,8,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,18,1,16,0,16,0,48,0,16,0,16,0,16,0,16,0,20,0,20,0,16,0,18,1,16,0,16,0,16,0,20,0,18,1,16,0,16,0,16,0,16,0,16,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,16,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,16,0,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,1,1,0,0,0,0,
      0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,80,104,1,128,1,0,0,0,2,0,0,0,0,0,0,0,88,104,1,128,1,0,0,0,3,0,0,0,0,0,0,0,96,104,1,128,1,0,0,0,4,0,0,0,0,0,0,0,104,104,1,128,1,0,0,0,5,0,0,0,0,0,0,0,120,104,1,128,1,0,0,0,6,0,0,0,0,0,0,0,128,104,1,128,1,0,0,0,
      7,0,0,0,0,0,0,0,136,104,1,128,1,0,0,0,8,0,0,0,0,0,0,0,144,104,1,128,1,0,0,0,9,0,0,0,0,0,0,0,152,104,1,128,1,0,0,0,10,0,0,0,0,0,0,0,160,104,1,128,1,0,0,0,11,0,0,0,0,0,0,0,168,104,1,128,1,0,0,0,12,0,0,0,0,0,0,0,176,104,1,128,1,0,0,0,13,0,0,0,0,
      0,0,0,184,104,1,128,1,0,0,0,14,0,0,0,0,0,0,0,192,104,1,128,1,0,0,0,15,0,0,0,0,0,0,0,200,104,1,128,1,0,0,0,16,0,0,0,0,0,0,0,208,104,1,128,1,0,0,0,17,0,0,0,0,0,0,0,216,104,1,128,1,0,0,0,18,0,0,0,0,0,0,0,224,104,1,128,1,0,0,0,19,0,0,0,0,0,0,0,232,
      104,1,128,1,0,0,0,20,0,0,0,0,0,0,0,240,104,1,128,1,0,0,0,21,0,0,0,0,0,0,0,248,104,1,128,1,0,0,0,22,0,0,0,0,0,0,0,0,105,1,128,1,0,0,0,24,0,0,0,0,0,0,0,8,105,1,128,1,0,0,0,25,0,0,0,0,0,0,0,16,105,1,128,1,0,0,0,26,0,0,0,0,0,0,0,24,105,1,128,1,0,
      0,0,27,0,0,0,0,0,0,0,32,105,1,128,1,0,0,0,28,0,0,0,0,0,0,0,40,105,1,128,1,0,0,0,29,0,0,0,0,0,0,0,48,105,1,128,1,0,0,0,30,0,0,0,0,0,0,0,56,105,1,128,1,0,0,0,31,0,0,0,0,0,0,0,64,105,1,128,1,0,0,0,32,0,0,0,0,0,0,0,72,105,1,128,1,0,0,0,33,0,0,0,
      0,0,0,0,80,105,1,128,1,0,0,0,34,0,0,0,0,0,0,0,88,105,1,128,1,0,0,0,35,0,0,0,0,0,0,0,96,105,1,128,1,0,0,0,36,0,0,0,0,0,0,0,104,105,1,128,1,0,0,0,37,0,0,0,0,0,0,0,112,105,1,128,1,0,0,0,38,0,0,0,0,0,0,0,120,105,1,128,1,0,0,0,39,0,0,0,0,0,0,0,128,
      105,1,128,1,0,0,0,41,0,0,0,0,0,0,0,136,105,1,128,1,0,0,0,42,0,0,0,0,0,0,0,144,105,1,128,1,0,0,0,43,0,0,0,0,0,0,0,152,105,1,128,1,0,0,0,44,0,0,0,0,0,0,0,160,105,1,128,1,0,0,0,45,0,0,0,0,0,0,0,168,105,1,128,1,0,0,0,47,0,0,0,0,0,0,0,176,105,1,128,
      1,0,0,0,54,0,0,0,0,0,0,0,184,105,1,128,1,0,0,0,55,0,0,0,0,0,0,0,192,105,1,128,1,0,0,0,56,0,0,0,0,0,0,0,200,105,1,128,1,0,0,0,57,0,0,0,0,0,0,0,208,105,1,128,1,0,0,0,62,0,0,0,0,0,0,0,216,105,1,128,1,0,0,0,63,0,0,0,0,0,0,0,224,105,1,128,1,0,0,0,
      64,0,0,0,0,0,0,0,232,105,1,128,1,0,0,0,65,0,0,0,0,0,0,0,240,105,1,128,1,0,0,0,67,0,0,0,0,0,0,0,248,105,1,128,1,0,0,0,68,0,0,0,0,0,0,0,0,106,1,128,1,0,0,0,70,0,0,0,0,0,0,0,8,106,1,128,1,0,0,0,71,0,0,0,0,0,0,0,16,106,1,128,1,0,0,0,73,0,0,0,0,0,
      0,0,24,106,1,128,1,0,0,0,74,0,0,0,0,0,0,0,32,106,1,128,1,0,0,0,75,0,0,0,0,0,0,0,40,106,1,128,1,0,0,0,78,0,0,0,0,0,0,0,48,106,1,128,1,0,0,0,79,0,0,0,0,0,0,0,56,106,1,128,1,0,0,0,80,0,0,0,0,0,0,0,64,106,1,128,1,0,0,0,86,0,0,0,0,0,0,0,72,106,1,
      128,1,0,0,0,87,0,0,0,0,0,0,0,80,106,1,128,1,0,0,0,90,0,0,0,0,0,0,0,88,106,1,128,1,0,0,0,101,0,0,0,0,0,0,0,96,106,1,128,1,0,0,0,127,0,0,0,0,0,0,0,104,106,1,128,1,0,0,0,1,4,0,0,0,0,0,0,112,106,1,128,1,0,0,0,2,4,0,0,0,0,0,0,128,106,1,128,1,0,0,
      0,3,4,0,0,0,0,0,0,144,106,1,128,1,0,0,0,4,4,0,0,0,0,0,0,248,73,1,128,1,0,0,0,5,4,0,0,0,0,0,0,160,106,1,128,1,0,0,0,6,4,0,0,0,0,0,0,176,106,1,128,1,0,0,0,7,4,0,0,0,0,0,0,192,106,1,128,1,0,0,0,8,4,0,0,0,0,0,0,208,106,1,128,1,0,0,0,9,4,0,0,0,0,
      0,0,128,80,1,128,1,0,0,0,11,4,0,0,0,0,0,0,224,106,1,128,1,0,0,0,12,4,0,0,0,0,0,0,240,106,1,128,1,0,0,0,13,4,0,0,0,0,0,0,0,107,1,128,1,0,0,0,14,4,0,0,0,0,0,0,16,107,1,128,1,0,0,0,15,4,0,0,0,0,0,0,32,107,1,128,1,0,0,0,16,4,0,0,0,0,0,0,48,107,1,
      128,1,0,0,0,17,4,0,0,0,0,0,0,200,73,1,128,1,0,0,0,18,4,0,0,0,0,0,0,232,73,1,128,1,0,0,0,19,4,0,0,0,0,0,0,64,107,1,128,1,0,0,0,20,4,0,0,0,0,0,0,80,107,1,128,1,0,0,0,21,4,0,0,0,0,0,0,96,107,1,128,1,0,0,0,22,4,0,0,0,0,0,0,112,107,1,128,1,0,0,0,
      24,4,0,0,0,0,0,0,128,107,1,128,1,0,0,0,25,4,0,0,0,0,0,0,144,107,1,128,1,0,0,0,26,4,0,0,0,0,0,0,160,107,1,128,1,0,0,0,27,4,0,0,0,0,0,0,176,107,1,128,1,0,0,0,28,4,0,0,0,0,0,0,192,107,1,128,1,0,0,0,29,4,0,0,0,0,0,0,208,107,1,128,1,0,0,0,30,4,0,
      0,0,0,0,0,224,107,1,128,1,0,0,0,31,4,0,0,0,0,0,0,240,107,1,128,1,0,0,0,32,4,0,0,0,0,0,0,0,108,1,128,1,0,0,0,33,4,0,0,0,0,0,0,16,108,1,128,1,0,0,0,34,4,0,0,0,0,0,0,32,108,1,128,1,0,0,0,35,4,0,0,0,0,0,0,48,108,1,128,1,0,0,0,36,4,0,0,0,0,0,0,64,
      108,1,128,1,0,0,0,37,4,0,0,0,0,0,0,80,108,1,128,1,0,0,0,38,4,0,0,0,0,0,0,96,108,1,128,1,0,0,0,39,4,0,0,0,0,0,0,112,108,1,128,1,0,0,0,41,4,0,0,0,0,0,0,128,108,1,128,1,0,0,0,42,4,0,0,0,0,0,0,144,108,1,128,1,0,0,0,43,4,0,0,0,0,0,0,160,108,1,128,
      1,0,0,0,44,4,0,0,0,0,0,0,176,108,1,128,1,0,0,0,45,4,0,0,0,0,0,0,200,108,1,128,1,0,0,0,47,4,0,0,0,0,0,0,216,108,1,128,1,0,0,0,50,4,0,0,0,0,0,0,232,108,1,128,1,0,0,0,52,4,0,0,0,0,0,0,248,108,1,128,1,0,0,0,53,4,0,0,0,0,0,0,8,109,1,128,1,0,0,0,54,
      4,0,0,0,0,0,0,24,109,1,128,1,0,0,0,55,4,0,0,0,0,0,0,40,109,1,128,1,0,0,0,56,4,0,0,0,0,0,0,56,109,1,128,1,0,0,0,57,4,0,0,0,0,0,0,72,109,1,128,1,0,0,0,58,4,0,0,0,0,0,0,88,109,1,128,1,0,0,0,59,4,0,0,0,0,0,0,104,109,1,128,1,0,0,0,62,4,0,0,0,0,0,
      0,120,109,1,128,1,0,0,0,63,4,0,0,0,0,0,0,136,109,1,128,1,0,0,0,64,4,0,0,0,0,0,0,152,109,1,128,1,0,0,0,65,4,0,0,0,0,0,0,168,109,1,128,1,0,0,0,67,4,0,0,0,0,0,0,184,109,1,128,1,0,0,0,68,4,0,0,0,0,0,0,208,109,1,128,1,0,0,0,69,4,0,0,0,0,0,0,224,109,
      1,128,1,0,0,0,70,4,0,0,0,0,0,0,240,109,1,128,1,0,0,0,71,4,0,0,0,0,0,0,0,110,1,128,1,0,0,0,73,4,0,0,0,0,0,0,16,110,1,128,1,0,0,0,74,4,0,0,0,0,0,0,32,110,1,128,1,0,0,0,75,4,0,0,0,0,0,0,48,110,1,128,1,0,0,0,76,4,0,0,0,0,0,0,64,110,1,128,1,0,0,0,
      78,4,0,0,0,0,0,0,80,110,1,128,1,0,0,0,79,4,0,0,0,0,0,0,96,110,1,128,1,0,0,0,80,4,0,0,0,0,0,0,112,110,1,128,1,0,0,0,82,4,0,0,0,0,0,0,128,110,1,128,1,0,0,0,86,4,0,0,0,0,0,0,144,110,1,128,1,0,0,0,87,4,0,0,0,0,0,0,160,110,1,128,1,0,0,0,90,4,0,0,
      0,0,0,0,176,110,1,128,1,0,0,0,101,4,0,0,0,0,0,0,192,110,1,128,1,0,0,0,107,4,0,0,0,0,0,0,208,110,1,128,1,0,0,0,108,4,0,0,0,0,0,0,224,110,1,128,1,0,0,0,129,4,0,0,0,0,0,0,240,110,1,128,1,0,0,0,1,8,0,0,0,0,0,0,0,111,1,128,1,0,0,0,4,8,0,0,0,0,0,0,
      216,73,1,128,1,0,0,0,7,8,0,0,0,0,0,0,16,111,1,128,1,0,0,0,9,8,0,0,0,0,0,0,32,111,1,128,1,0,0,0,10,8,0,0,0,0,0,0,48,111,1,128,1,0,0,0,12,8,0,0,0,0,0,0,64,111,1,128,1,0,0,0,16,8,0,0,0,0,0,0,80,111,1,128,1,0,0,0,19,8,0,0,0,0,0,0,96,111,1,128,1,
      0,0,0,20,8,0,0,0,0,0,0,112,111,1,128,1,0,0,0,22,8,0,0,0,0,0,0,128,111,1,128,1,0,0,0,26,8,0,0,0,0,0,0,144,111,1,128,1,0,0,0,29,8,0,0,0,0,0,0,168,111,1,128,1,0,0,0,44,8,0,0,0,0,0,0,184,111,1,128,1,0,0,0,59,8,0,0,0,0,0,0,208,111,1,128,1,0,0,0,62,
      8,0,0,0,0,0,0,224,111,1,128,1,0,0,0,67,8,0,0,0,0,0,0,240,111,1,128,1,0,0,0,107,8,0,0,0,0,0,0,8,112,1,128,1,0,0,0,1,12,0,0,0,0,0,0,24,112,1,128,1,0,0,0,4,12,0,0,0,0,0,0,40,112,1,128,1,0,0,0,7,12,0,0,0,0,0,0,56,112,1,128,1,0,0,0,9,12,0,0,0,0,0,
      0,72,112,1,128,1,0,0,0,10,12,0,0,0,0,0,0,88,112,1,128,1,0,0,0,12,12,0,0,0,0,0,0,104,112,1,128,1,0,0,0,26,12,0,0,0,0,0,0,120,112,1,128,1,0,0,0,59,12,0,0,0,0,0,0,144,112,1,128,1,0,0,0,107,12,0,0,0,0,0,0,160,112,1,128,1,0,0,0,1,16,0,0,0,0,0,0,176,
      112,1,128,1,0,0,0,4,16,0,0,0,0,0,0,192,112,1,128,1,0,0,0,7,16,0,0,0,0,0,0,208,112,1,128,1,0,0,0,9,16,0,0,0,0,0,0,224,112,1,128,1,0,0,0,10,16,0,0,0,0,0,0,240,112,1,128,1,0,0,0,12,16,0,0,0,0,0,0,0,113,1,128,1,0,0,0,26,16,0,0,0,0,0,0,16,113,1,128,
      1,0,0,0,59,16,0,0,0,0,0,0,32,113,1,128,1,0,0,0,1,20,0,0,0,0,0,0,48,113,1,128,1,0,0,0,4,20,0,0,0,0,0,0,64,113,1,128,1,0,0,0,7,20,0,0,0,0,0,0,80,113,1,128,1,0,0,0,9,20,0,0,0,0,0,0,96,113,1,128,1,0,0,0,10,20,0,0,0,0,0,0,112,113,1,128,1,0,0,0,12,
      20,0,0,0,0,0,0,128,113,1,128,1,0,0,0,26,20,0,0,0,0,0,0,144,113,1,128,1,0,0,0,59,20,0,0,0,0,0,0,168,113,1,128,1,0,0,0,1,24,0,0,0,0,0,0,184,113,1,128,1,0,0,0,9,24,0,0,0,0,0,0,200,113,1,128,1,0,0,0,10,24,0,0,0,0,0,0,216,113,1,128,1,0,0,0,12,24,
      0,0,0,0,0,0,232,113,1,128,1,0,0,0,26,24,0,0,0,0,0,0,248,113,1,128,1,0,0,0,59,24,0,0,0,0,0,0,16,114,1,128,1,0,0,0,1,28,0,0,0,0,0,0,32,114,1,128,1,0,0,0,9,28,0,0,0,0,0,0,48,114,1,128,1,0,0,0,10,28,0,0,0,0,0,0,64,114,1,128,1,0,0,0,26,28,0,0,0,0,
      0,0,80,114,1,128,1,0,0,0,59,28,0,0,0,0,0,0,104,114,1,128,1,0,0,0,1,32,0,0,0,0,0,0,120,114,1,128,1,0,0,0,9,32,0,0,0,0,0,0,136,114,1,128,1,0,0,0,10,32,0,0,0,0,0,0,152,114,1,128,1,0,0,0,59,32,0,0,0,0,0,0,168,114,1,128,1,0,0,0,1,36,0,0,0,0,0,0,184,
      114,1,128,1,0,0,0,9,36,0,0,0,0,0,0,200,114,1,128,1,0,0,0,10,36,0,0,0,0,0,0,216,114,1,128,1,0,0,0,59,36,0,0,0,0,0,0,232,114,1,128,1,0,0,0,1,40,0,0,0,0,0,0,248,114,1,128,1,0,0,0,9,40,0,0,0,0,0,0,8,115,1,128,1,0,0,0,10,40,0,0,0,0,0,0,24,115,1,128,
      1,0,0,0,1,44,0,0,0,0,0,0,40,115,1,128,1,0,0,0,9,44,0,0,0,0,0,0,56,115,1,128,1,0,0,0,10,44,0,0,0,0,0,0,72,115,1,128,1,0,0,0,1,48,0,0,0,0,0,0,88,115,1,128,1,0,0,0,9,48,0,0,0,0,0,0,104,115,1,128,1,0,0,0,10,48,0,0,0,0,0,0,120,115,1,128,1,0,0,0,1,
      52,0,0,0,0,0,0,136,115,1,128,1,0,0,0,9,52,0,0,0,0,0,0,152,115,1,128,1,0,0,0,10,52,0,0,0,0,0,0,168,115,1,128,1,0,0,0,1,56,0,0,0,0,0,0,184,115,1,128,1,0,0,0,10,56,0,0,0,0,0,0,200,115,1,128,1,0,0,0,1,60,0,0,0,0,0,0,216,115,1,128,1,0,0,0,10,60,0,
      0,0,0,0,0,232,115,1,128,1,0,0,0,1,64,0,0,0,0,0,0,248,115,1,128,1,0,0,0,10,64,0,0,0,0,0,0,8,116,1,128,1,0,0,0,10,68,0,0,0,0,0,0,24,116,1,128,1,0,0,0,10,72,0,0,0,0,0,0,40,116,1,128,1,0,0,0,10,76,0,0,0,0,0,0,56,116,1,128,1,0,0,0,10,80,0,0,0,0,0,
      0,72,116,1,128,1,0,0,0,4,124,0,0,0,0,0,0,88,116,1,128,1,0,0,0,26,124,0,0,0,0,0,0,104,116,1,128,1,0,0,0,97,0,114,0,0,0,0,0,98,0,103,0,0,0,0,0,99,0,97,0,0,0,0,0,122,0,104,0,45,0,67,0,72,0,83,0,0,0,0,0,99,0,115,0,0,0,0,0,100,0,97,0,0,0,0,0,100,
      0,101,0,0,0,0,0,101,0,108,0,0,0,0,0,101,0,110,0,0,0,0,0,101,0,115,0,0,0,0,0,102,0,105,0,0,0,0,0,102,0,114,0,0,0,0,0,104,0,101,0,0,0,0,0,104,0,117,0,0,0,0,0,105,0,115,0,0,0,0,0,105,0,116,0,0,0,0,0,106,0,97,0,0,0,0,0,107,0,111,0,0,0,0,0,110,0,
      108,0,0,0,0,0,110,0,111,0,0,0,0,0,112,0,108,0,0,0,0,0,112,0,116,0,0,0,0,0,114,0,111,0,0,0,0,0,114,0,117,0,0,0,0,0,104,0,114,0,0,0,0,0,115,0,107,0,0,0,0,0,115,0,113,0,0,0,0,0,115,0,118,0,0,0,0,0,116,0,104,0,0,0,0,0,116,0,114,0,0,0,0,0,117,0,114,
      0,0,0,0,0,105,0,100,0,0,0,0,0,117,0,107,0,0,0,0,0,98,0,101,0,0,0,0,0,115,0,108,0,0,0,0,0,101,0,116,0,0,0,0,0,108,0,118,0,0,0,0,0,108,0,116,0,0,0,0,0,102,0,97,0,0,0,0,0,118,0,105,0,0,0,0,0,104,0,121,0,0,0,0,0,97,0,122,0,0,0,0,0,101,0,117,0,0,
      0,0,0,109,0,107,0,0,0,0,0,97,0,102,0,0,0,0,0,107,0,97,0,0,0,0,0,102,0,111,0,0,0,0,0,104,0,105,0,0,0,0,0,109,0,115,0,0,0,0,0,107,0,107,0,0,0,0,0,107,0,121,0,0,0,0,0,115,0,119,0,0,0,0,0,117,0,122,0,0,0,0,0,116,0,116,0,0,0,0,0,112,0,97,0,0,0,0,
      0,103,0,117,0,0,0,0,0,116,0,97,0,0,0,0,0,116,0,101,0,0,0,0,0,107,0,110,0,0,0,0,0,109,0,114,0,0,0,0,0,115,0,97,0,0,0,0,0,109,0,110,0,0,0,0,0,103,0,108,0,0,0,0,0,107,0,111,0,107,0,0,0,115,0,121,0,114,0,0,0,100,0,105,0,118,0,0,0,0,0,0,0,0,0,0,0,
      97,0,114,0,45,0,83,0,65,0,0,0,0,0,0,0,98,0,103,0,45,0,66,0,71,0,0,0,0,0,0,0,99,0,97,0,45,0,69,0,83,0,0,0,0,0,0,0,99,0,115,0,45,0,67,0,90,0,0,0,0,0,0,0,100,0,97,0,45,0,68,0,75,0,0,0,0,0,0,0,100,0,101,0,45,0,68,0,69,0,0,0,0,0,0,0,101,0,108,0,45,
      0,71,0,82,0,0,0,0,0,0,0,102,0,105,0,45,0,70,0,73,0,0,0,0,0,0,0,102,0,114,0,45,0,70,0,82,0,0,0,0,0,0,0,104,0,101,0,45,0,73,0,76,0,0,0,0,0,0,0,104,0,117,0,45,0,72,0,85,0,0,0,0,0,0,0,105,0,115,0,45,0,73,0,83,0,0,0,0,0,0,0,105,0,116,0,45,0,73,0,
      84,0,0,0,0,0,0,0,110,0,108,0,45,0,78,0,76,0,0,0,0,0,0,0,110,0,98,0,45,0,78,0,79,0,0,0,0,0,0,0,112,0,108,0,45,0,80,0,76,0,0,0,0,0,0,0,112,0,116,0,45,0,66,0,82,0,0,0,0,0,0,0,114,0,111,0,45,0,82,0,79,0,0,0,0,0,0,0,114,0,117,0,45,0,82,0,85,0,0,0,
      0,0,0,0,104,0,114,0,45,0,72,0,82,0,0,0,0,0,0,0,115,0,107,0,45,0,83,0,75,0,0,0,0,0,0,0,115,0,113,0,45,0,65,0,76,0,0,0,0,0,0,0,115,0,118,0,45,0,83,0,69,0,0,0,0,0,0,0,116,0,104,0,45,0,84,0,72,0,0,0,0,0,0,0,116,0,114,0,45,0,84,0,82,0,0,0,0,0,0,0,
      117,0,114,0,45,0,80,0,75,0,0,0,0,0,0,0,105,0,100,0,45,0,73,0,68,0,0,0,0,0,0,0,117,0,107,0,45,0,85,0,65,0,0,0,0,0,0,0,98,0,101,0,45,0,66,0,89,0,0,0,0,0,0,0,115,0,108,0,45,0,83,0,73,0,0,0,0,0,0,0,101,0,116,0,45,0,69,0,69,0,0,0,0,0,0,0,108,0,118,
      0,45,0,76,0,86,0,0,0,0,0,0,0,108,0,116,0,45,0,76,0,84,0,0,0,0,0,0,0,102,0,97,0,45,0,73,0,82,0,0,0,0,0,0,0,118,0,105,0,45,0,86,0,78,0,0,0,0,0,0,0,104,0,121,0,45,0,65,0,77,0,0,0,0,0,0,0,97,0,122,0,45,0,65,0,90,0,45,0,76,0,97,0,116,0,110,0,0,0,
      0,0,101,0,117,0,45,0,69,0,83,0,0,0,0,0,0,0,109,0,107,0,45,0,77,0,75,0,0,0,0,0,0,0,116,0,110,0,45,0,90,0,65,0,0,0,0,0,0,0,120,0,104,0,45,0,90,0,65,0,0,0,0,0,0,0,122,0,117,0,45,0,90,0,65,0,0,0,0,0,0,0,97,0,102,0,45,0,90,0,65,0,0,0,0,0,0,0,107,
      0,97,0,45,0,71,0,69,0,0,0,0,0,0,0,102,0,111,0,45,0,70,0,79,0,0,0,0,0,0,0,104,0,105,0,45,0,73,0,78,0,0,0,0,0,0,0,109,0,116,0,45,0,77,0,84,0,0,0,0,0,0,0,115,0,101,0,45,0,78,0,79,0,0,0,0,0,0,0,109,0,115,0,45,0,77,0,89,0,0,0,0,0,0,0,107,0,107,0,
      45,0,75,0,90,0,0,0,0,0,0,0,107,0,121,0,45,0,75,0,71,0,0,0,0,0,0,0,115,0,119,0,45,0,75,0,69,0,0,0,0,0,0,0,117,0,122,0,45,0,85,0,90,0,45,0,76,0,97,0,116,0,110,0,0,0,0,0,116,0,116,0,45,0,82,0,85,0,0,0,0,0,0,0,98,0,110,0,45,0,73,0,78,0,0,0,0,0,0,
      0,112,0,97,0,45,0,73,0,78,0,0,0,0,0,0,0,103,0,117,0,45,0,73,0,78,0,0,0,0,0,0,0,116,0,97,0,45,0,73,0,78,0,0,0,0,0,0,0,116,0,101,0,45,0,73,0,78,0,0,0,0,0,0,0,107,0,110,0,45,0,73,0,78,0,0,0,0,0,0,0,109,0,108,0,45,0,73,0,78,0,0,0,0,0,0,0,109,0,114,
      0,45,0,73,0,78,0,0,0,0,0,0,0,115,0,97,0,45,0,73,0,78,0,0,0,0,0,0,0,109,0,110,0,45,0,77,0,78,0,0,0,0,0,0,0,99,0,121,0,45,0,71,0,66,0,0,0,0,0,0,0,103,0,108,0,45,0,69,0,83,0,0,0,0,0,0,0,107,0,111,0,107,0,45,0,73,0,78,0,0,0,0,0,115,0,121,0,114,0,
      45,0,83,0,89,0,0,0,0,0,100,0,105,0,118,0,45,0,77,0,86,0,0,0,0,0,113,0,117,0,122,0,45,0,66,0,79,0,0,0,0,0,110,0,115,0,45,0,90,0,65,0,0,0,0,0,0,0,109,0,105,0,45,0,78,0,90,0,0,0,0,0,0,0,97,0,114,0,45,0,73,0,81,0,0,0,0,0,0,0,100,0,101,0,45,0,67,
      0,72,0,0,0,0,0,0,0,101,0,110,0,45,0,71,0,66,0,0,0,0,0,0,0,101,0,115,0,45,0,77,0,88,0,0,0,0,0,0,0,102,0,114,0,45,0,66,0,69,0,0,0,0,0,0,0,105,0,116,0,45,0,67,0,72,0,0,0,0,0,0,0,110,0,108,0,45,0,66,0,69,0,0,0,0,0,0,0,110,0,110,0,45,0,78,0,79,0,
      0,0,0,0,0,0,112,0,116,0,45,0,80,0,84,0,0,0,0,0,0,0,115,0,114,0,45,0,83,0,80,0,45,0,76,0,97,0,116,0,110,0,0,0,0,0,115,0,118,0,45,0,70,0,73,0,0,0,0,0,0,0,97,0,122,0,45,0,65,0,90,0,45,0,67,0,121,0,114,0,108,0,0,0,0,0,115,0,101,0,45,0,83,0,69,0,
      0,0,0,0,0,0,109,0,115,0,45,0,66,0,78,0,0,0,0,0,0,0,117,0,122,0,45,0,85,0,90,0,45,0,67,0,121,0,114,0,108,0,0,0,0,0,113,0,117,0,122,0,45,0,69,0,67,0,0,0,0,0,97,0,114,0,45,0,69,0,71,0,0,0,0,0,0,0,122,0,104,0,45,0,72,0,75,0,0,0,0,0,0,0,100,0,101,
      0,45,0,65,0,84,0,0,0,0,0,0,0,101,0,110,0,45,0,65,0,85,0,0,0,0,0,0,0,101,0,115,0,45,0,69,0,83,0,0,0,0,0,0,0,102,0,114,0,45,0,67,0,65,0,0,0,0,0,0,0,115,0,114,0,45,0,83,0,80,0,45,0,67,0,121,0,114,0,108,0,0,0,0,0,115,0,101,0,45,0,70,0,73,0,0,0,0,
      0,0,0,113,0,117,0,122,0,45,0,80,0,69,0,0,0,0,0,97,0,114,0,45,0,76,0,89,0,0,0,0,0,0,0,122,0,104,0,45,0,83,0,71,0,0,0,0,0,0,0,100,0,101,0,45,0,76,0,85,0,0,0,0,0,0,0,101,0,110,0,45,0,67,0,65,0,0,0,0,0,0,0,101,0,115,0,45,0,71,0,84,0,0,0,0,0,0,0,
      102,0,114,0,45,0,67,0,72,0,0,0,0,0,0,0,104,0,114,0,45,0,66,0,65,0,0,0,0,0,0,0,115,0,109,0,106,0,45,0,78,0,79,0,0,0,0,0,97,0,114,0,45,0,68,0,90,0,0,0,0,0,0,0,122,0,104,0,45,0,77,0,79,0,0,0,0,0,0,0,100,0,101,0,45,0,76,0,73,0,0,0,0,0,0,0,101,0,
      110,0,45,0,78,0,90,0,0,0,0,0,0,0,101,0,115,0,45,0,67,0,82,0,0,0,0,0,0,0,102,0,114,0,45,0,76,0,85,0,0,0,0,0,0,0,98,0,115,0,45,0,66,0,65,0,45,0,76,0,97,0,116,0,110,0,0,0,0,0,115,0,109,0,106,0,45,0,83,0,69,0,0,0,0,0,97,0,114,0,45,0,77,0,65,0,0,
      0,0,0,0,0,101,0,110,0,45,0,73,0,69,0,0,0,0,0,0,0,101,0,115,0,45,0,80,0,65,0,0,0,0,0,0,0,102,0,114,0,45,0,77,0,67,0,0,0,0,0,0,0,115,0,114,0,45,0,66,0,65,0,45,0,76,0,97,0,116,0,110,0,0,0,0,0,115,0,109,0,97,0,45,0,78,0,79,0,0,0,0,0,97,0,114,0,45,
      0,84,0,78,0,0,0,0,0,0,0,101,0,110,0,45,0,90,0,65,0,0,0,0,0,0,0,101,0,115,0,45,0,68,0,79,0,0,0,0,0,0,0,115,0,114,0,45,0,66,0,65,0,45,0,67,0,121,0,114,0,108,0,0,0,0,0,115,0,109,0,97,0,45,0,83,0,69,0,0,0,0,0,97,0,114,0,45,0,79,0,77,0,0,0,0,0,0,
      0,101,0,110,0,45,0,74,0,77,0,0,0,0,0,0,0,101,0,115,0,45,0,86,0,69,0,0,0,0,0,0,0,115,0,109,0,115,0,45,0,70,0,73,0,0,0,0,0,97,0,114,0,45,0,89,0,69,0,0,0,0,0,0,0,101,0,110,0,45,0,67,0,66,0,0,0,0,0,0,0,101,0,115,0,45,0,67,0,79,0,0,0,0,0,0,0,115,
      0,109,0,110,0,45,0,70,0,73,0,0,0,0,0,97,0,114,0,45,0,83,0,89,0,0,0,0,0,0,0,101,0,110,0,45,0,66,0,90,0,0,0,0,0,0,0,101,0,115,0,45,0,80,0,69,0,0,0,0,0,0,0,97,0,114,0,45,0,74,0,79,0,0,0,0,0,0,0,101,0,110,0,45,0,84,0,84,0,0,0,0,0,0,0,101,0,115,0,
      45,0,65,0,82,0,0,0,0,0,0,0,97,0,114,0,45,0,76,0,66,0,0,0,0,0,0,0,101,0,110,0,45,0,90,0,87,0,0,0,0,0,0,0,101,0,115,0,45,0,69,0,67,0,0,0,0,0,0,0,97,0,114,0,45,0,75,0,87,0,0,0,0,0,0,0,101,0,110,0,45,0,80,0,72,0,0,0,0,0,0,0,101,0,115,0,45,0,67,0,
      76,0,0,0,0,0,0,0,97,0,114,0,45,0,65,0,69,0,0,0,0,0,0,0,101,0,115,0,45,0,85,0,89,0,0,0,0,0,0,0,97,0,114,0,45,0,66,0,72,0,0,0,0,0,0,0,101,0,115,0,45,0,80,0,89,0,0,0,0,0,0,0,97,0,114,0,45,0,81,0,65,0,0,0,0,0,0,0,101,0,115,0,45,0,66,0,79,0,0,0,0,
      0,0,0,101,0,115,0,45,0,83,0,86,0,0,0,0,0,0,0,101,0,115,0,45,0,72,0,78,0,0,0,0,0,0,0,101,0,115,0,45,0,78,0,73,0,0,0,0,0,0,0,101,0,115,0,45,0,80,0,82,0,0,0,0,0,0,0,122,0,104,0,45,0,67,0,72,0,84,0,0,0,0,0,115,0,114,0,0,0,0,0,104,106,1,128,1,0,0,
      0,66,0,0,0,0,0,0,0,184,105,1,128,1,0,0,0,44,0,0,0,0,0,0,0,176,130,1,128,1,0,0,0,113,0,0,0,0,0,0,0,80,104,1,128,1,0,0,0,0,0,0,0,0,0,0,0,192,130,1,128,1,0,0,0,216,0,0,0,0,0,0,0,208,130,1,128,1,0,0,0,218,0,0,0,0,0,0,0,224,130,1,128,1,0,0,0,177,
      0,0,0,0,0,0,0,240,130,1,128,1,0,0,0,160,0,0,0,0,0,0,0,0,131,1,128,1,0,0,0,143,0,0,0,0,0,0,0,16,131,1,128,1,0,0,0,207,0,0,0,0,0,0,0,32,131,1,128,1,0,0,0,213,0,0,0,0,0,0,0,48,131,1,128,1,0,0,0,210,0,0,0,0,0,0,0,64,131,1,128,1,0,0,0,169,0,0,0,0,
      0,0,0,80,131,1,128,1,0,0,0,185,0,0,0,0,0,0,0,96,131,1,128,1,0,0,0,196,0,0,0,0,0,0,0,112,131,1,128,1,0,0,0,220,0,0,0,0,0,0,0,128,131,1,128,1,0,0,0,67,0,0,0,0,0,0,0,144,131,1,128,1,0,0,0,204,0,0,0,0,0,0,0,160,131,1,128,1,0,0,0,191,0,0,0,0,0,0,
      0,176,131,1,128,1,0,0,0,200,0,0,0,0,0,0,0,160,105,1,128,1,0,0,0,41,0,0,0,0,0,0,0,192,131,1,128,1,0,0,0,155,0,0,0,0,0,0,0,216,131,1,128,1,0,0,0,107,0,0,0,0,0,0,0,96,105,1,128,1,0,0,0,33,0,0,0,0,0,0,0,240,131,1,128,1,0,0,0,99,0,0,0,0,0,0,0,88,
      104,1,128,1,0,0,0,1,0,0,0,0,0,0,0,0,132,1,128,1,0,0,0,68,0,0,0,0,0,0,0,16,132,1,128,1,0,0,0,125,0,0,0,0,0,0,0,32,132,1,128,1,0,0,0,183,0,0,0,0,0,0,0,96,104,1,128,1,0,0,0,2,0,0,0,0,0,0,0,56,132,1,128,1,0,0,0,69,0,0,0,0,0,0,0,120,104,1,128,1,0,
      0,0,4,0,0,0,0,0,0,0,72,132,1,128,1,0,0,0,71,0,0,0,0,0,0,0,88,132,1,128,1,0,0,0,135,0,0,0,0,0,0,0,128,104,1,128,1,0,0,0,5,0,0,0,0,0,0,0,104,132,1,128,1,0,0,0,72,0,0,0,0,0,0,0,136,104,1,128,1,0,0,0,6,0,0,0,0,0,0,0,120,132,1,128,1,0,0,0,162,0,0,
      0,0,0,0,0,136,132,1,128,1,0,0,0,145,0,0,0,0,0,0,0,152,132,1,128,1,0,0,0,73,0,0,0,0,0,0,0,168,132,1,128,1,0,0,0,179,0,0,0,0,0,0,0,184,132,1,128,1,0,0,0,171,0,0,0,0,0,0,0,96,106,1,128,1,0,0,0,65,0,0,0,0,0,0,0,200,132,1,128,1,0,0,0,139,0,0,0,0,
      0,0,0,144,104,1,128,1,0,0,0,7,0,0,0,0,0,0,0,216,132,1,128,1,0,0,0,74,0,0,0,0,0,0,0,152,104,1,128,1,0,0,0,8,0,0,0,0,0,0,0,232,132,1,128,1,0,0,0,163,0,0,0,0,0,0,0,248,132,1,128,1,0,0,0,205,0,0,0,0,0,0,0,8,133,1,128,1,0,0,0,172,0,0,0,0,0,0,0,24,
      133,1,128,1,0,0,0,201,0,0,0,0,0,0,0,40,133,1,128,1,0,0,0,146,0,0,0,0,0,0,0,56,133,1,128,1,0,0,0,186,0,0,0,0,0,0,0,72,133,1,128,1,0,0,0,197,0,0,0,0,0,0,0,88,133,1,128,1,0,0,0,180,0,0,0,0,0,0,0,104,133,1,128,1,0,0,0,214,0,0,0,0,0,0,0,120,133,1,
      128,1,0,0,0,208,0,0,0,0,0,0,0,136,133,1,128,1,0,0,0,75,0,0,0,0,0,0,0,152,133,1,128,1,0,0,0,192,0,0,0,0,0,0,0,168,133,1,128,1,0,0,0,211,0,0,0,0,0,0,0,160,104,1,128,1,0,0,0,9,0,0,0,0,0,0,0,184,133,1,128,1,0,0,0,209,0,0,0,0,0,0,0,200,133,1,128,
      1,0,0,0,221,0,0,0,0,0,0,0,216,133,1,128,1,0,0,0,215,0,0,0,0,0,0,0,232,133,1,128,1,0,0,0,202,0,0,0,0,0,0,0,248,133,1,128,1,0,0,0,181,0,0,0,0,0,0,0,8,134,1,128,1,0,0,0,193,0,0,0,0,0,0,0,24,134,1,128,1,0,0,0,212,0,0,0,0,0,0,0,40,134,1,128,1,0,0,
      0,164,0,0,0,0,0,0,0,56,134,1,128,1,0,0,0,173,0,0,0,0,0,0,0,72,134,1,128,1,0,0,0,223,0,0,0,0,0,0,0,88,134,1,128,1,0,0,0,147,0,0,0,0,0,0,0,104,134,1,128,1,0,0,0,224,0,0,0,0,0,0,0,120,134,1,128,1,0,0,0,187,0,0,0,0,0,0,0,136,134,1,128,1,0,0,0,206,
      0,0,0,0,0,0,0,152,134,1,128,1,0,0,0,225,0,0,0,0,0,0,0,168,134,1,128,1,0,0,0,219,0,0,0,0,0,0,0,184,134,1,128,1,0,0,0,222,0,0,0,0,0,0,0,200,134,1,128,1,0,0,0,217,0,0,0,0,0,0,0,216,134,1,128,1,0,0,0,198,0,0,0,0,0,0,0,112,105,1,128,1,0,0,0,35,0,
      0,0,0,0,0,0,232,134,1,128,1,0,0,0,101,0,0,0,0,0,0,0,168,105,1,128,1,0,0,0,42,0,0,0,0,0,0,0,248,134,1,128,1,0,0,0,108,0,0,0,0,0,0,0,136,105,1,128,1,0,0,0,38,0,0,0,0,0,0,0,8,135,1,128,1,0,0,0,104,0,0,0,0,0,0,0,168,104,1,128,1,0,0,0,10,0,0,0,0,
      0,0,0,24,135,1,128,1,0,0,0,76,0,0,0,0,0,0,0,200,105,1,128,1,0,0,0,46,0,0,0,0,0,0,0,40,135,1,128,1,0,0,0,115,0,0,0,0,0,0,0,176,104,1,128,1,0,0,0,11,0,0,0,0,0,0,0,56,135,1,128,1,0,0,0,148,0,0,0,0,0,0,0,72,135,1,128,1,0,0,0,165,0,0,0,0,0,0,0,88,
      135,1,128,1,0,0,0,174,0,0,0,0,0,0,0,104,135,1,128,1,0,0,0,77,0,0,0,0,0,0,0,120,135,1,128,1,0,0,0,182,0,0,0,0,0,0,0,136,135,1,128,1,0,0,0,188,0,0,0,0,0,0,0,72,106,1,128,1,0,0,0,62,0,0,0,0,0,0,0,152,135,1,128,1,0,0,0,136,0,0,0,0,0,0,0,16,106,1,
      128,1,0,0,0,55,0,0,0,0,0,0,0,168,135,1,128,1,0,0,0,127,0,0,0,0,0,0,0,184,104,1,128,1,0,0,0,12,0,0,0,0,0,0,0,184,135,1,128,1,0,0,0,78,0,0,0,0,0,0,0,208,105,1,128,1,0,0,0,47,0,0,0,0,0,0,0,200,135,1,128,1,0,0,0,116,0,0,0,0,0,0,0,24,105,1,128,1,
      0,0,0,24,0,0,0,0,0,0,0,216,135,1,128,1,0,0,0,175,0,0,0,0,0,0,0,232,135,1,128,1,0,0,0,90,0,0,0,0,0,0,0,192,104,1,128,1,0,0,0,13,0,0,0,0,0,0,0,248,135,1,128,1,0,0,0,79,0,0,0,0,0,0,0,152,105,1,128,1,0,0,0,40,0,0,0,0,0,0,0,8,136,1,128,1,0,0,0,106,
      0,0,0,0,0,0,0,80,105,1,128,1,0,0,0,31,0,0,0,0,0,0,0,24,136,1,128,1,0,0,0,97,0,0,0,0,0,0,0,200,104,1,128,1,0,0,0,14,0,0,0,0,0,0,0,40,136,1,128,1,0,0,0,80,0,0,0,0,0,0,0,208,104,1,128,1,0,0,0,15,0,0,0,0,0,0,0,56,136,1,128,1,0,0,0,149,0,0,0,0,0,
      0,0,72,136,1,128,1,0,0,0,81,0,0,0,0,0,0,0,216,104,1,128,1,0,0,0,16,0,0,0,0,0,0,0,88,136,1,128,1,0,0,0,82,0,0,0,0,0,0,0,192,105,1,128,1,0,0,0,45,0,0,0,0,0,0,0,104,136,1,128,1,0,0,0,114,0,0,0,0,0,0,0,224,105,1,128,1,0,0,0,49,0,0,0,0,0,0,0,120,
      136,1,128,1,0,0,0,120,0,0,0,0,0,0,0,40,106,1,128,1,0,0,0,58,0,0,0,0,0,0,0,136,136,1,128,1,0,0,0,130,0,0,0,0,0,0,0,224,104,1,128,1,0,0,0,17,0,0,0,0,0,0,0,80,106,1,128,1,0,0,0,63,0,0,0,0,0,0,0,152,136,1,128,1,0,0,0,137,0,0,0,0,0,0,0,168,136,1,
      128,1,0,0,0,83,0,0,0,0,0,0,0,232,105,1,128,1,0,0,0,50,0,0,0,0,0,0,0,184,136,1,128,1,0,0,0,121,0,0,0,0,0,0,0,128,105,1,128,1,0,0,0,37,0,0,0,0,0,0,0,200,136,1,128,1,0,0,0,103,0,0,0,0,0,0,0,120,105,1,128,1,0,0,0,36,0,0,0,0,0,0,0,216,136,1,128,1,
      0,0,0,102,0,0,0,0,0,0,0,232,136,1,128,1,0,0,0,142,0,0,0,0,0,0,0,176,105,1,128,1,0,0,0,43,0,0,0,0,0,0,0,248,136,1,128,1,0,0,0,109,0,0,0,0,0,0,0,8,137,1,128,1,0,0,0,131,0,0,0,0,0,0,0,64,106,1,128,1,0,0,0,61,0,0,0,0,0,0,0,24,137,1,128,1,0,0,0,134,
      0,0,0,0,0,0,0,48,106,1,128,1,0,0,0,59,0,0,0,0,0,0,0,40,137,1,128,1,0,0,0,132,0,0,0,0,0,0,0,216,105,1,128,1,0,0,0,48,0,0,0,0,0,0,0,56,137,1,128,1,0,0,0,157,0,0,0,0,0,0,0,72,137,1,128,1,0,0,0,119,0,0,0,0,0,0,0,88,137,1,128,1,0,0,0,117,0,0,0,0,
      0,0,0,104,137,1,128,1,0,0,0,85,0,0,0,0,0,0,0,232,104,1,128,1,0,0,0,18,0,0,0,0,0,0,0,120,137,1,128,1,0,0,0,150,0,0,0,0,0,0,0,136,137,1,128,1,0,0,0,84,0,0,0,0,0,0,0,152,137,1,128,1,0,0,0,151,0,0,0,0,0,0,0,240,104,1,128,1,0,0,0,19,0,0,0,0,0,0,0,
      168,137,1,128,1,0,0,0,141,0,0,0,0,0,0,0,8,106,1,128,1,0,0,0,54,0,0,0,0,0,0,0,184,137,1,128,1,0,0,0,126,0,0,0,0,0,0,0,248,104,1,128,1,0,0,0,20,0,0,0,0,0,0,0,200,137,1,128,1,0,0,0,86,0,0,0,0,0,0,0,0,105,1,128,1,0,0,0,21,0,0,0,0,0,0,0,216,137,1,
      128,1,0,0,0,87,0,0,0,0,0,0,0,232,137,1,128,1,0,0,0,152,0,0,0,0,0,0,0,248,137,1,128,1,0,0,0,140,0,0,0,0,0,0,0,8,138,1,128,1,0,0,0,159,0,0,0,0,0,0,0,24,138,1,128,1,0,0,0,168,0,0,0,0,0,0,0,8,105,1,128,1,0,0,0,22,0,0,0,0,0,0,0,40,138,1,128,1,0,0,
      0,88,0,0,0,0,0,0,0,16,105,1,128,1,0,0,0,23,0,0,0,0,0,0,0,56,138,1,128,1,0,0,0,89,0,0,0,0,0,0,0,56,106,1,128,1,0,0,0,60,0,0,0,0,0,0,0,72,138,1,128,1,0,0,0,133,0,0,0,0,0,0,0,88,138,1,128,1,0,0,0,167,0,0,0,0,0,0,0,104,138,1,128,1,0,0,0,118,0,0,
      0,0,0,0,0,120,138,1,128,1,0,0,0,156,0,0,0,0,0,0,0,32,105,1,128,1,0,0,0,25,0,0,0,0,0,0,0,136,138,1,128,1,0,0,0,91,0,0,0,0,0,0,0,104,105,1,128,1,0,0,0,34,0,0,0,0,0,0,0,152,138,1,128,1,0,0,0,100,0,0,0,0,0,0,0,168,138,1,128,1,0,0,0,190,0,0,0,0,0,
      0,0,184,138,1,128,1,0,0,0,195,0,0,0,0,0,0,0,200,138,1,128,1,0,0,0,176,0,0,0,0,0,0,0,216,138,1,128,1,0,0,0,184,0,0,0,0,0,0,0,232,138,1,128,1,0,0,0,203,0,0,0,0,0,0,0,248,138,1,128,1,0,0,0,199,0,0,0,0,0,0,0,40,105,1,128,1,0,0,0,26,0,0,0,0,0,0,0,
      8,139,1,128,1,0,0,0,92,0,0,0,0,0,0,0,104,116,1,128,1,0,0,0,227,0,0,0,0,0,0,0,24,139,1,128,1,0,0,0,194,0,0,0,0,0,0,0,48,139,1,128,1,0,0,0,189,0,0,0,0,0,0,0,72,139,1,128,1,0,0,0,166,0,0,0,0,0,0,0,96,139,1,128,1,0,0,0,153,0,0,0,0,0,0,0,48,105,1,
      128,1,0,0,0,27,0,0,0,0,0,0,0,120,139,1,128,1,0,0,0,154,0,0,0,0,0,0,0,136,139,1,128,1,0,0,0,93,0,0,0,0,0,0,0,240,105,1,128,1,0,0,0,51,0,0,0,0,0,0,0,152,139,1,128,1,0,0,0,122,0,0,0,0,0,0,0,88,106,1,128,1,0,0,0,64,0,0,0,0,0,0,0,168,139,1,128,1,
      0,0,0,138,0,0,0,0,0,0,0,24,106,1,128,1,0,0,0,56,0,0,0,0,0,0,0,184,139,1,128,1,0,0,0,128,0,0,0,0,0,0,0,32,106,1,128,1,0,0,0,57,0,0,0,0,0,0,0,200,139,1,128,1,0,0,0,129,0,0,0,0,0,0,0,56,105,1,128,1,0,0,0,28,0,0,0,0,0,0,0,216,139,1,128,1,0,0,0,94,
      0,0,0,0,0,0,0,232,139,1,128,1,0,0,0,110,0,0,0,0,0,0,0,64,105,1,128,1,0,0,0,29,0,0,0,0,0,0,0,248,139,1,128,1,0,0,0,95,0,0,0,0,0,0,0,0,106,1,128,1,0,0,0,53,0,0,0,0,0,0,0,8,140,1,128,1,0,0,0,124,0,0,0,0,0,0,0,88,105,1,128,1,0,0,0,32,0,0,0,0,0,0,
      0,24,140,1,128,1,0,0,0,98,0,0,0,0,0,0,0,72,105,1,128,1,0,0,0,30,0,0,0,0,0,0,0,40,140,1,128,1,0,0,0,96,0,0,0,0,0,0,0,248,105,1,128,1,0,0,0,52,0,0,0,0,0,0,0,56,140,1,128,1,0,0,0,158,0,0,0,0,0,0,0,80,140,1,128,1,0,0,0,123,0,0,0,0,0,0,0,144,105,
      1,128,1,0,0,0,39,0,0,0,0,0,0,0,104,140,1,128,1,0,0,0,105,0,0,0,0,0,0,0,120,140,1,128,1,0,0,0,111,0,0,0,0,0,0,0,136,140,1,128,1,0,0,0,3,0,0,0,0,0,0,0,152,140,1,128,1,0,0,0,226,0,0,0,0,0,0,0,168,140,1,128,1,0,0,0,144,0,0,0,0,0,0,0,184,140,1,128,
      1,0,0,0,161,0,0,0,0,0,0,0,200,140,1,128,1,0,0,0,178,0,0,0,0,0,0,0,216,140,1,128,1,0,0,0,170,0,0,0,0,0,0,0,232,140,1,128,1,0,0,0,70,0,0,0,0,0,0,0,248,140,1,128,1,0,0,0,112,0,0,0,0,0,0,0,97,0,102,0,45,0,122,0,97,0,0,0,0,0,0,0,97,0,114,0,45,0,97,
      0,101,0,0,0,0,0,0,0,97,0,114,0,45,0,98,0,104,0,0,0,0,0,0,0,97,0,114,0,45,0,100,0,122,0,0,0,0,0,0,0,97,0,114,0,45,0,101,0,103,0,0,0,0,0,0,0,97,0,114,0,45,0,105,0,113,0,0,0,0,0,0,0,97,0,114,0,45,0,106,0,111,0,0,0,0,0,0,0,97,0,114,0,45,0,107,0,
      119,0,0,0,0,0,0,0,97,0,114,0,45,0,108,0,98,0,0,0,0,0,0,0,97,0,114,0,45,0,108,0,121,0,0,0,0,0,0,0,97,0,114,0,45,0,109,0,97,0,0,0,0,0,0,0,97,0,114,0,45,0,111,0,109,0,0,0,0,0,0,0,97,0,114,0,45,0,113,0,97,0,0,0,0,0,0,0,97,0,114,0,45,0,115,0,97,0,
      0,0,0,0,0,0,97,0,114,0,45,0,115,0,121,0,0,0,0,0,0,0,97,0,114,0,45,0,116,0,110,0,0,0,0,0,0,0,97,0,114,0,45,0,121,0,101,0,0,0,0,0,0,0,97,0,122,0,45,0,97,0,122,0,45,0,99,0,121,0,114,0,108,0,0,0,0,0,97,0,122,0,45,0,97,0,122,0,45,0,108,0,97,0,116,
      0,110,0,0,0,0,0,98,0,101,0,45,0,98,0,121,0,0,0,0,0,0,0,98,0,103,0,45,0,98,0,103,0,0,0,0,0,0,0,98,0,110,0,45,0,105,0,110,0,0,0,0,0,0,0,98,0,115,0,45,0,98,0,97,0,45,0,108,0,97,0,116,0,110,0,0,0,0,0,99,0,97,0,45,0,101,0,115,0,0,0,0,0,0,0,99,0,115,
      0,45,0,99,0,122,0,0,0,0,0,0,0,99,0,121,0,45,0,103,0,98,0,0,0,0,0,0,0,100,0,97,0,45,0,100,0,107,0,0,0,0,0,0,0,100,0,101,0,45,0,97,0,116,0,0,0,0,0,0,0,100,0,101,0,45,0,99,0,104,0,0,0,0,0,0,0,100,0,101,0,45,0,100,0,101,0,0,0,0,0,0,0,100,0,101,0,
      45,0,108,0,105,0,0,0,0,0,0,0,100,0,101,0,45,0,108,0,117,0,0,0,0,0,0,0,100,0,105,0,118,0,45,0,109,0,118,0,0,0,0,0,101,0,108,0,45,0,103,0,114,0,0,0,0,0,0,0,101,0,110,0,45,0,97,0,117,0,0,0,0,0,0,0,101,0,110,0,45,0,98,0,122,0,0,0,0,0,0,0,101,0,110,
      0,45,0,99,0,97,0,0,0,0,0,0,0,101,0,110,0,45,0,99,0,98,0,0,0,0,0,0,0,101,0,110,0,45,0,103,0,98,0,0,0,0,0,0,0,101,0,110,0,45,0,105,0,101,0,0,0,0,0,0,0,101,0,110,0,45,0,106,0,109,0,0,0,0,0,0,0,101,0,110,0,45,0,110,0,122,0,0,0,0,0,0,0,101,0,110,
      0,45,0,112,0,104,0,0,0,0,0,0,0,101,0,110,0,45,0,116,0,116,0,0,0,0,0,0,0,101,0,110,0,45,0,117,0,115,0,0,0,0,0,0,0,101,0,110,0,45,0,122,0,97,0,0,0,0,0,0,0,101,0,110,0,45,0,122,0,119,0,0,0,0,0,0,0,101,0,115,0,45,0,97,0,114,0,0,0,0,0,0,0,101,0,115,
      0,45,0,98,0,111,0,0,0,0,0,0,0,101,0,115,0,45,0,99,0,108,0,0,0,0,0,0,0,101,0,115,0,45,0,99,0,111,0,0,0,0,0,0,0,101,0,115,0,45,0,99,0,114,0,0,0,0,0,0,0,101,0,115,0,45,0,100,0,111,0,0,0,0,0,0,0,101,0,115,0,45,0,101,0,99,0,0,0,0,0,0,0,101,0,115,
      0,45,0,101,0,115,0,0,0,0,0,0,0,101,0,115,0,45,0,103,0,116,0,0,0,0,0,0,0,101,0,115,0,45,0,104,0,110,0,0,0,0,0,0,0,101,0,115,0,45,0,109,0,120,0,0,0,0,0,0,0,101,0,115,0,45,0,110,0,105,0,0,0,0,0,0,0,101,0,115,0,45,0,112,0,97,0,0,0,0,0,0,0,101,0,
      115,0,45,0,112,0,101,0,0,0,0,0,0,0,101,0,115,0,45,0,112,0,114,0,0,0,0,0,0,0,101,0,115,0,45,0,112,0,121,0,0,0,0,0,0,0,101,0,115,0,45,0,115,0,118,0,0,0,0,0,0,0,101,0,115,0,45,0,117,0,121,0,0,0,0,0,0,0,101,0,115,0,45,0,118,0,101,0,0,0,0,0,0,0,101,
      0,116,0,45,0,101,0,101,0,0,0,0,0,0,0,101,0,117,0,45,0,101,0,115,0,0,0,0,0,0,0,102,0,97,0,45,0,105,0,114,0,0,0,0,0,0,0,102,0,105,0,45,0,102,0,105,0,0,0,0,0,0,0,102,0,111,0,45,0,102,0,111,0,0,0,0,0,0,0,102,0,114,0,45,0,98,0,101,0,0,0,0,0,0,0,102,
      0,114,0,45,0,99,0,97,0,0,0,0,0,0,0,102,0,114,0,45,0,99,0,104,0,0,0,0,0,0,0,102,0,114,0,45,0,102,0,114,0,0,0,0,0,0,0,102,0,114,0,45,0,108,0,117,0,0,0,0,0,0,0,102,0,114,0,45,0,109,0,99,0,0,0,0,0,0,0,103,0,108,0,45,0,101,0,115,0,0,0,0,0,0,0,103,
      0,117,0,45,0,105,0,110,0,0,0,0,0,0,0,104,0,101,0,45,0,105,0,108,0,0,0,0,0,0,0,104,0,105,0,45,0,105,0,110,0,0,0,0,0,0,0,104,0,114,0,45,0,98,0,97,0,0,0,0,0,0,0,104,0,114,0,45,0,104,0,114,0,0,0,0,0,0,0,104,0,117,0,45,0,104,0,117,0,0,0,0,0,0,0,104,
      0,121,0,45,0,97,0,109,0,0,0,0,0,0,0,105,0,100,0,45,0,105,0,100,0,0,0,0,0,0,0,105,0,115,0,45,0,105,0,115,0,0,0,0,0,0,0,105,0,116,0,45,0,99,0,104,0,0,0,0,0,0,0,105,0,116,0,45,0,105,0,116,0,0,0,0,0,0,0,106,0,97,0,45,0,106,0,112,0,0,0,0,0,0,0,107,
      0,97,0,45,0,103,0,101,0,0,0,0,0,0,0,107,0,107,0,45,0,107,0,122,0,0,0,0,0,0,0,107,0,110,0,45,0,105,0,110,0,0,0,0,0,0,0,107,0,111,0,107,0,45,0,105,0,110,0,0,0,0,0,107,0,111,0,45,0,107,0,114,0,0,0,0,0,0,0,107,0,121,0,45,0,107,0,103,0,0,0,0,0,0,
      0,108,0,116,0,45,0,108,0,116,0,0,0,0,0,0,0,108,0,118,0,45,0,108,0,118,0,0,0,0,0,0,0,109,0,105,0,45,0,110,0,122,0,0,0,0,0,0,0,109,0,107,0,45,0,109,0,107,0,0,0,0,0,0,0,109,0,108,0,45,0,105,0,110,0,0,0,0,0,0,0,109,0,110,0,45,0,109,0,110,0,0,0,0,
      0,0,0,109,0,114,0,45,0,105,0,110,0,0,0,0,0,0,0,109,0,115,0,45,0,98,0,110,0,0,0,0,0,0,0,109,0,115,0,45,0,109,0,121,0,0,0,0,0,0,0,109,0,116,0,45,0,109,0,116,0,0,0,0,0,0,0,110,0,98,0,45,0,110,0,111,0,0,0,0,0,0,0,110,0,108,0,45,0,98,0,101,0,0,0,
      0,0,0,0,110,0,108,0,45,0,110,0,108,0,0,0,0,0,0,0,110,0,110,0,45,0,110,0,111,0,0,0,0,0,0,0,110,0,115,0,45,0,122,0,97,0,0,0,0,0,0,0,112,0,97,0,45,0,105,0,110,0,0,0,0,0,0,0,112,0,108,0,45,0,112,0,108,0,0,0,0,0,0,0,112,0,116,0,45,0,98,0,114,0,0,
      0,0,0,0,0,112,0,116,0,45,0,112,0,116,0,0,0,0,0,0,0,113,0,117,0,122,0,45,0,98,0,111,0,0,0,0,0,113,0,117,0,122,0,45,0,101,0,99,0,0,0,0,0,113,0,117,0,122,0,45,0,112,0,101,0,0,0,0,0,114,0,111,0,45,0,114,0,111,0,0,0,0,0,0,0,114,0,117,0,45,0,114,0,
      117,0,0,0,0,0,0,0,115,0,97,0,45,0,105,0,110,0,0,0,0,0,0,0,115,0,101,0,45,0,102,0,105,0,0,0,0,0,0,0,115,0,101,0,45,0,110,0,111,0,0,0,0,0,0,0,115,0,101,0,45,0,115,0,101,0,0,0,0,0,0,0,115,0,107,0,45,0,115,0,107,0,0,0,0,0,0,0,115,0,108,0,45,0,115,
      0,105,0,0,0,0,0,0,0,115,0,109,0,97,0,45,0,110,0,111,0,0,0,0,0,115,0,109,0,97,0,45,0,115,0,101,0,0,0,0,0,115,0,109,0,106,0,45,0,110,0,111,0,0,0,0,0,115,0,109,0,106,0,45,0,115,0,101,0,0,0,0,0,115,0,109,0,110,0,45,0,102,0,105,0,0,0,0,0,115,0,109,
      0,115,0,45,0,102,0,105,0,0,0,0,0,115,0,113,0,45,0,97,0,108,0,0,0,0,0,0,0,115,0,114,0,45,0,98,0,97,0,45,0,99,0,121,0,114,0,108,0,0,0,0,0,115,0,114,0,45,0,98,0,97,0,45,0,108,0,97,0,116,0,110,0,0,0,0,0,115,0,114,0,45,0,115,0,112,0,45,0,99,0,121,
      0,114,0,108,0,0,0,0,0,115,0,114,0,45,0,115,0,112,0,45,0,108,0,97,0,116,0,110,0,0,0,0,0,115,0,118,0,45,0,102,0,105,0,0,0,0,0,0,0,115,0,118,0,45,0,115,0,101,0,0,0,0,0,0,0,115,0,119,0,45,0,107,0,101,0,0,0,0,0,0,0,115,0,121,0,114,0,45,0,115,0,121,
      0,0,0,0,0,116,0,97,0,45,0,105,0,110,0,0,0,0,0,0,0,116,0,101,0,45,0,105,0,110,0,0,0,0,0,0,0,116,0,104,0,45,0,116,0,104,0,0,0,0,0,0,0,116,0,110,0,45,0,122,0,97,0,0,0,0,0,0,0,116,0,114,0,45,0,116,0,114,0,0,0,0,0,0,0,116,0,116,0,45,0,114,0,117,0,
      0,0,0,0,0,0,117,0,107,0,45,0,117,0,97,0,0,0,0,0,0,0,117,0,114,0,45,0,112,0,107,0,0,0,0,0,0,0,117,0,122,0,45,0,117,0,122,0,45,0,99,0,121,0,114,0,108,0,0,0,0,0,117,0,122,0,45,0,117,0,122,0,45,0,108,0,97,0,116,0,110,0,0,0,0,0,118,0,105,0,45,0,118,
      0,110,0,0,0,0,0,0,0,120,0,104,0,45,0,122,0,97,0,0,0,0,0,0,0,122,0,104,0,45,0,99,0,104,0,115,0,0,0,0,0,122,0,104,0,45,0,99,0,104,0,116,0,0,0,0,0,122,0,104,0,45,0,99,0,110,0,0,0,0,0,0,0,122,0,104,0,45,0,104,0,107,0,0,0,0,0,0,0,122,0,104,0,45,0,
      109,0,111,0,0,0,0,0,0,0,122,0,104,0,45,0,115,0,103,0,0,0,0,0,0,0,122,0,104,0,45,0,116,0,119,0,0,0,0,0,0,0,122,0,117,0,45,0,122,0,97,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,34,5,147,25,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,128,182,1,0,112,255,255,255,
      0,0,0,0,5,0,0,0,32,215,1,128,1,0,0,0,192,215,1,128,1,0,0,0,67,0,79,0,78,0,79,0,85,0,84,0,36,0,0,0,0,0,0,0,0,0,0,0,160,49,0,128,1,0,0,0,176,49,0,128,1,0,0,0,240,49,0,128,1,0,0,0,160,50,0,128,1,0,0,0,192,51,0,128,1,0,0,0,0,0,0,0,0,0,0,0,160,49,
      0,128,1,0,0,0,176,49,0,128,1,0,0,0,240,49,0,128,1,0,0,0,112,105,0,128,1,0,0,0,48,52,0,128,1,0,0,0,69,0,66,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,0,0,0,0,0,0,69,0,66,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,92,0,97,0,114,0,109,0,54,0,52,0,92,
      0,69,0,109,0,98,0,101,0,100,0,100,0,101,0,100,0,66,0,114,0,111,0,119,0,115,0,101,0,114,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,46,0,100,0,108,0,108,0,0,0,0,0,0,0,0,0,0,0,0,0,123,0,70,0,51,0,48,0,49,0,55,0,50,0,50,0,54,0,45,0,70,0,69,0,50,0,
      65,0,45,0,52,0,50,0,57,0,53,0,45,0,56,0,66,0,68,0,70,0,45,0,48,0,48,0,67,0,51,0,65,0,57,0,65,0,55,0,69,0,52,0,67,0,53,0,125,0,0,0,0,0,123,0,50,0,67,0,68,0,56,0,65,0,48,0,48,0,55,0,45,0,69,0,49,0,56,0,57,0,45,0,52,0,48,0,57,0,68,0,45,0,65,0,50,
      0,67,0,56,0,45,0,57,0,65,0,70,0,52,0,69,0,70,0,51,0,67,0,55,0,50,0,65,0,65,0,125,0,0,0,0,0,123,0,48,0,68,0,53,0,48,0,66,0,70,0,69,0,67,0,45,0,67,0,68,0,54,0,65,0,45,0,52,0,70,0,57,0,65,0,45,0,57,0,54,0,52,0,67,0,45,0,67,0,55,0,52,0,49,0,54,0,
      69,0,51,0,65,0,67,0,66,0,49,0,48,0,125,0,0,0,0,0,123,0,54,0,53,0,67,0,51,0,53,0,66,0,49,0,52,0,45,0,54,0,67,0,49,0,68,0,45,0,52,0,49,0,50,0,50,0,45,0,65,0,67,0,52,0,54,0,45,0,55,0,49,0,52,0,56,0,67,0,67,0,57,0,68,0,54,0,52,0,57,0,55,0,125,0,
      0,0,0,0,123,0,66,0,69,0,53,0,57,0,69,0,56,0,70,0,68,0,45,0,48,0,56,0,57,0,65,0,45,0,52,0,49,0,49,0,66,0,45,0,65,0,51,0,66,0,48,0,45,0,48,0,53,0,49,0,68,0,57,0,69,0,52,0,49,0,55,0,56,0,49,0,56,0,125,0,0,0,0,0,48,142,1,128,1,0,0,0,128,142,1,128,
      1,0,0,0,208,142,1,128,1,0,0,0,32,143,1,128,1,0,0,0,112,143,1,128,1,0,0,0,0,0,0,0,0,0,0,0,83,0,111,0,102,0,116,0,119,0,97,0,114,0,101,0,92,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,92,0,69,0,100,0,103,0,101,0,85,0,112,0,100,0,97,0,
      116,0,101,0,92,0,67,0,108,0,105,0,101,0,110,0,116,0,83,0,116,0,97,0,116,0,101,0,92,0,0,0,0,0,0,0,0,0,0,0,0,0,98,0,101,0,116,0,97,0,0,0,0,0,0,0,0,0,100,0,101,0,118,0,0,0,99,0,97,0,110,0,97,0,114,0,121,0,0,0,0,0,105,0,110,0,116,0,101,0,114,0,110,
      0,97,0,108,0,0,0,0,0,0,0,0,0,72,144,1,128,1,0,0,0,80,144,1,128,1,0,0,0,96,144,1,128,1,0,0,0,104,144,1,128,1,0,0,0,120,144,1,128,1,0,0,0,92,0,0,0,0,0,0,0,87,101,98,86,105,101,119,50,58,32,70,97,105,108,101,100,32,116,111,32,102,105,110,100,32,
      116,104,101,32,97,112,112,32,101,120,101,32,112,97,116,104,46,10,0,0,0,0,0,87,101,98,86,105,101,119,50,58,32,70,97,105,108,101,100,32,116,111,32,102,105,110,100,32,116,104,101,32,87,101,98,86,105,101,119,50,32,99,108,105,101,110,116,32,100,108,
      108,32,97,116,58,32,0,0,0,10,0,0,0,0,0,0,0,71,101,116,70,105,108,101,86,101,114,115,105,111,110,73,110,102,111,83,105,122,101,87,0,71,101,116,70,105,108,101,86,101,114,115,105,111,110,73,110,102,111,87,0,0,0,0,0,86,101,114,81,117,101,114,121,
      86,97,108,117,101,87,0,0,92,0,83,0,116,0,114,0,105,0,110,0,103,0,70,0,105,0,108,0,101,0,73,0,110,0,102,0,111,0,92,0,48,0,52,0,48,0,57,0,48,0,52,0,66,0,48,0,92,0,80,0,114,0,111,0,100,0,117,0,99,0,116,0,86,0,101,0,114,0,115,0,105,0,111,0,110,0,
      0,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,101,0,45,0,118,0,101,0,114,0,115,0,105,0,111,0,110,0,45,0,108,0,49,0,45,0,49,0,45,0,48,0,46,0,100,0,108,0,108,0,0,0,0,0,118,0,
      101,0,114,0,115,0,105,0,111,0,110,0,46,0,100,0,108,0,108,0,0,0,65,0,68,0,86,0,65,0,80,0,73,0,51,0,50,0,46,0,100,0,108,0,108,0,0,0,0,0,0,0,0,0,69,118,101,110,116,82,101,103,105,115,116,101,114,0,0,0,87,101,98,86,105,101,119,50,58,32,70,97,105,
      108,101,100,32,116,111,32,102,105,110,100,32,97,110,32,105,110,115,116,97,108,108,101,100,32,87,101,98,86,105,101,119,50,32,114,117,110,116,105,109,101,32,111,114,32,110,111,110,45,115,116,97,98,108,101,32,77,105,99,114,111,115,111,102,116,32,
      69,100,103,101,32,105,110,115,116,97,108,108,97,116,105,111,110,46,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,46,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,82,0,117,0,110,0,116,0,105,0,109,0,101,0,46,
      0,83,0,116,0,97,0,98,0,108,0,101,0,95,0,56,0,119,0,101,0,107,0,121,0,98,0,51,0,100,0,56,0,98,0,98,0,119,0,101,0,0,0,0,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,46,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,82,0,117,0,110,0,116,
      0,105,0,109,0,101,0,46,0,66,0,101,0,116,0,97,0,95,0,56,0,119,0,101,0,107,0,121,0,98,0,51,0,100,0,56,0,98,0,98,0,119,0,101,0,0,0,0,0,0,0,0,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,46,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,
      82,0,117,0,110,0,116,0,105,0,109,0,101,0,46,0,68,0,101,0,118,0,95,0,56,0,119,0,101,0,107,0,121,0,98,0,51,0,100,0,56,0,98,0,98,0,119,0,101,0,0,0,0,0,0,0,0,0,0,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,46,0,87,0,101,0,98,0,86,0,105,
      0,101,0,119,0,50,0,82,0,117,0,110,0,116,0,105,0,109,0,101,0,46,0,67,0,97,0,110,0,97,0,114,0,121,0,95,0,56,0,119,0,101,0,107,0,121,0,98,0,51,0,100,0,56,0,98,0,98,0,119,0,101,0,0,0,0,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,46,0,87,
      0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,82,0,117,0,110,0,116,0,105,0,109,0,101,0,46,0,73,0,110,0,116,0,101,0,114,0,110,0,97,0,108,0,95,0,56,0,119,0,101,0,107,0,121,0,98,0,51,0,100,0,56,0,98,0,98,0,119,0,101,0,0,0,0,0,0,0,0,0,208,146,1,128,1,
      0,0,0,48,147,1,128,1,0,0,0,144,147,1,128,1,0,0,0,240,147,1,128,1,0,0,0,80,148,1,128,1,0,0,0,86,0,0,0,0,0,0,0,104,2,0,0,0,0,0,0,76,0,111,0,97,0,100,0,101,0,114,0,32,0,115,0,107,0,105,0,112,0,112,0,101,0,100,0,32,0,97,0,110,0,32,0,105,0,110,0,
      99,0,111,0,109,0,112,0,97,0,116,0,105,0,98,0,108,0,101,0,32,0,118,0,101,0,114,0,115,0,105,0,111,0,110,0,58,0,32,0,0,0,0,0,10,0,0,0,46,0,0,0,0,0,0,0,71,101,116,67,117,114,114,101,110,116,80,97,99,107,97,103,101,73,110,102,111,0,0,0,107,0,101,
      0,114,0,110,0,101,0,108,0,98,0,97,0,115,0,101,0,46,0,100,0,108,0,108,0,0,0,0,0,94,117,109,185,25,3,146,78,162,150,35,67,111,70,161,252,71,101,116,67,117,114,114,101,110,116,65,112,112,108,105,99,97,116,105,111,110,85,115,101,114,77,111,100,101,
      108,73,100,0,0,0,0,0,0,0,0,75,0,101,0,114,0,110,0,101,0,108,0,51,0,50,0,46,0,100,0,108,0,108,0,0,0,0,0,0,0,0,0,66,0,114,0,111,0,119,0,115,0,101,0,114,0,69,0,120,0,101,0,99,0,117,0,116,0,97,0,98,0,108,0,101,0,70,0,111,0,108,0,100,0,101,0,114,
      0,0,0,87,0,69,0,66,0,86,0,73,0,69,0,87,0,50,0,95,0,66,0,82,0,79,0,87,0,83,0,69,0,82,0,95,0,69,0,88,0,69,0,67,0,85,0,84,0,65,0,66,0,76,0,69,0,95,0,70,0,79,0,76,0,68,0,69,0,82,0,0,0,0,0,85,0,115,0,101,0,114,0,68,0,97,0,116,0,97,0,70,0,111,0,108,
      0,100,0,101,0,114,0,0,0,0,0,87,0,69,0,66,0,86,0,73,0,69,0,87,0,50,0,95,0,85,0,83,0,69,0,82,0,95,0,68,0,65,0,84,0,65,0,95,0,70,0,79,0,76,0,68,0,69,0,82,0,0,0,0,0,0,0,82,0,101,0,108,0,101,0,97,0,115,0,101,0,67,0,104,0,97,0,110,0,110,0,101,0,108,
      0,80,0,114,0,101,0,102,0,101,0,114,0,101,0,110,0,99,0,101,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,87,0,69,0,66,0,86,0,73,0,69,0,87,0,50,0,95,0,82,0,69,0,76,0,69,0,65,0,83,0,69,0,95,0,67,0,72,0,65,0,78,0,78,0,69,0,76,0,95,0,80,0,82,0,69,0,70,0,69,0,
      82,0,69,0,78,0,67,0,69,0,0,0,115,0,104,0,101,0,108,0,108,0,51,0,50,0,46,0,100,0,108,0,108,0,0,0,71,101,116,67,117,114,114,101,110,116,80,114,111,99,101,115,115,69,120,112,108,105,99,105,116,65,112,112,85,115,101,114,77,111,100,101,108,73,68,
      0,0,0,0,0,0,0,0,0,83,0,111,0,102,0,116,0,119,0,97,0,114,0,101,0,92,0,80,0,111,0,108,0,105,0,99,0,105,0,101,0,115,0,92,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,92,0,69,0,100,0,103,0,101,0,92,0,87,0,101,0,98,0,86,0,105,0,101,0,119,
      0,50,0,92,0,0,0,0,0,42,0,0,0,0,0,0,0,48,49,50,51,52,53,54,55,56,57,65,66,67,68,69,70,0,0,0,0,0,0,0,0,67,114,101,97,116,101,87,101,98,86,105,101,119,69,110,118,105,114,111,110,109,101,110,116,87,105,116,104,79,112,116,105,111,110,115,73,110,116,
      101,114,110,97,108,0,0,0,0,0,0,0,0,0,0,0,0,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,58,0,32,0,67,0,111,0,114,0,101,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,69,0,110,0,118,0,105,0,114,0,111,0,110,0,109,0,101,0,110,0,116,0,32,0,102,0,
      97,0,105,0,108,0,101,0,100,0,32,0,119,0,104,0,101,0,110,0,32,0,116,0,114,0,121,0,105,0,110,0,103,0,32,0,116,0,111,0,32,0,99,0,97,0,108,0,108,0,32,0,105,0,110,0,116,0,111,0,32,0,69,0,109,0,98,0,101,0,100,0,100,0,101,0,100,0,66,0,114,0,111,0,119,
      0,115,0,101,0,114,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,46,0,100,0,108,0,108,0,46,0,32,0,104,0,114,0,61,0,48,0,120,0,0,0,68,108,108,67,97,110,85,110,108,111,97,100,78,111,119,0,0,0,0,0,0,0,0,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,58,
      0,32,0,67,0,111,0,114,0,101,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,69,0,110,0,118,0,105,0,114,0,111,0,110,0,109,0,101,0,110,0,116,0,32,0,102,0,97,0,105,0,108,0,101,0,100,0,32,0,119,0,104,0,101,0,110,0,32,0,116,0,114,0,121,0,105,0,110,
      0,103,0,32,0,116,0,111,0,32,0,76,0,111,0,97,0,100,0,76,0,105,0,98,0,114,0,97,0,114,0,121,0,58,0,32,0,104,0,114,0,61,0,48,0,120,0,0,0,0,0,32,0,112,0,97,0,116,0,104,0,61,0,0,0,0,0,0,0,2,128,1,0,0,0,8,0,2,128,1,0,0,0,48,203,1,128,1,0,0,0,104,158,
      1,128,1,0,0,0,0,0,0,0,0,0,48,0,1,0,0,0,0,0,0,0,0,0,0,0,16,202,1,0,8,154,1,0,224,153,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,32,154,1,0,0,0,0,0,0,0,0,0,48,154,1,0,0,0,0,0,0,0,0,0,0,0,0,0,16,202,1,0,0,0,0,0,0,0,0,0,255,255,
      255,255,0,0,0,0,64,0,0,0,8,154,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,232,201,1,0,128,154,1,0,88,154,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,152,154,1,0,0,0,0,0,0,0,0,0,176,154,1,0,48,154,1,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,232,201,1,0,1,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,64,0,0,0,128,154,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,56,202,1,0,0,155,1,0,216,154,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,24,155,1,0,0,0,
      0,0,0,0,0,0,56,155,1,0,176,154,1,0,48,154,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,56,202,1,0,2,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,64,0,0,0,0,155,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,144,202,1,0,136,155,1,0,96,155,1,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,160,155,1,0,0,0,0,0,0,0,0,0,176,155,1,0,0,0,0,0,0,0,0,0,0,0,0,0,144,202,1,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,64,0,0,0,136,155,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,104,
      202,1,0,0,156,1,0,216,155,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,24,156,1,0,0,0,0,0,0,0,0,0,48,156,1,0,48,154,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,104,202,1,0,1,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,64,0,0,0,0,156,1,0,0,0,
      0,0,0,0,0,0,0,0,0,0,69,84,87,48,16,0,0,1,134,14,4,136,43,5,138,187,6,11,2,0,0,0,0,0,0,64,0,0,95,0,0,67,114,101,97,116,101,87,101,98,86,105,101,119,69,110,118,105,114,111,110,109,101,110,116,69,114,114,111,114,0,72,82,69,83,85,76,84,0,135,15,
      67,108,105,101,110,116,68,108,108,70,111,117,110,100,0,132,3,73,110,115,116,97,108,108,101,100,82,117,110,116,105,109,101,0,132,3,80,97,114,116,65,95,80,114,105,118,84,97,103,115,0,10,4,126,221,29,149,255,242,63,91,113,55,226,241,160,221,152,
      4,52,0,77,105,99,114,111,115,111,102,116,46,77,83,69,100,103,101,87,101,98,86,105,101,119,46,76,111,97,100,101,114,0,19,0,1,26,115,80,79,207,137,130,71,179,224,220,232,201,4,118,186,1,0,0,0,0,0,0,0,211,27,187,98,0,0,0,0,2,0,0,0,47,0,0,0,56,157,
      1,0,56,141,1,0,82,83,68,83,217,73,8,38,88,238,61,76,76,76,68,32,80,68,66,46,1,0,0,0,87,101,98,86,105,101,119,50,76,111,97,100,101,114,46,100,108,108,46,112,100,98,0,0,16,0,0,0,17,0,0,16,29,0,0,112,35,0,0,48,40,0,0,144,45,0,0,64,47,0,0,160,49,
      0,0,176,49,0,0,240,49,0,0,160,50,0,0,192,51,0,0,48,52,0,0,192,70,0,0,224,70,0,0,96,72,0,0,112,74,0,0,112,76,0,0,160,91,0,0,176,91,0,0,176,101,0,0,112,105,0,0,96,108,0,0,128,108,0,0,176,108,0,0,208,108,0,0,32,109,0,0,64,109,0,0,160,109,0,0,48,
      145,0,0,16,149,0,0,32,170,0,0,80,170,0,0,192,175,0,0,64,176,0,0,0,177,0,0,48,177,0,0,64,177,0,0,144,177,0,0,112,181,0,0,96,185,0,0,176,185,0,0,64,188,0,0,176,188,0,0,128,209,0,0,160,210,0,0,96,224,0,0,224,232,0,0,160,250,0,0,224,33,1,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,160,250,0,128,1,0,0,0,128,209,0,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,224,232,0,128,1,0,0,0,224,33,1,128,1,0,0,0,160,210,0,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,26,160,1,0,176,202,1,0,192,202,1,0,8,159,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,39,160,1,0,184,202,1,0,8,203,1,0,80,159,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      104,159,1,0,0,0,0,0,120,159,1,0,0,0,0,0,142,159,1,0,0,0,0,0,160,159,1,0,0,0,0,0,182,159,1,0,0,0,0,0,196,159,1,0,0,0,0,0,212,159,1,0,0,0,0,0,228,159,1,0,0,0,0,0,0,0,0,0,0,0,0,0,248,159,1,0,0,0,0,0,10,160,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,69,118,
      101,110,116,82,101,103,105,115,116,101,114,0,0,0,69,118,101,110,116,83,101,116,73,110,102,111,114,109,97,116,105,111,110,0,0,0,69,118,101,110,116,85,110,114,101,103,105,115,116,101,114,0,0,0,69,118,101,110,116,87,114,105,116,101,84,114,97,110,
      115,102,101,114,0,0,0,0,82,101,103,67,108,111,115,101,75,101,121,0,0,0,82,101,103,71,101,116,86,97,108,117,101,87,0,0,0,0,82,101,103,79,112,101,110,75,101,121,69,120,87,0,0,0,82,101,103,81,117,101,114,121,86,97,108,117,101,69,120,87,0,0,0,0,
      67,111,84,97,115,107,77,101,109,65,108,108,111,99,0,0,0,0,67,111,84,97,115,107,77,101,109,70,114,101,101,0,65,68,86,65,80,73,51,50,46,100,108,108,0,111,108,101,51,50,46,100,108,108,0,0,0,0,0,0,0,0,0,0,0,0,0,89,160,1,0,0,0,0,0,5,0,0,0,4,0,0,0,
      108,160,1,0,128,160,1,0,144,160,1,0,87,101,98,86,105,101,119,50,76,111,97,100,101,114,46,100,108,108,0,0,0,0,0,144,45,0,0,64,47,0,0,112,35,0,0,48,40,0,0,152,160,1,0,175,160,1,0,205,160,1,0,246,160,1,0,1,0,2,0,3,0,4,0,67,111,109,112,97,114,101,
      66,114,111,119,115,101,114,86,101,114,115,105,111,110,115,0,67,114,101,97,116,101,67,111,114,101,87,101,98,86,105,101,119,50,69,110,118,105,114,111,110,109,101,110,116,0,67,114,101,97,116,101,67,111,114,101,87,101,98,86,105,101,119,50,69,110,
      118,105,114,111,110,109,101,110,116,87,105,116,104,79,112,116,105,111,110,115,0,71,101,116,65,118,97,105,108,97,98,108,101,67,111,114,101,87,101,98,86,105,101,119,50,66,114,111,119,115,101,114,86,101,114,115,105,111,110,83,116,114,105,110,103,
      0,80,161,1,0,0,0,0,0,0,0,0,0,198,171,1,0,184,163,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,166,1,0,0,0,0,0,58,166,1,0,0,0,0,0,72,166,1,0,0,0,0,0,86,166,1,0,0,0,0,0,110,166,1,0,0,0,0,0,126,166,1,0,0,0,0,0,150,166,1,0,0,0,0,0,164,
      166,1,0,0,0,0,0,176,166,1,0,0,0,0,0,196,166,1,0,0,0,0,0,212,166,1,0,0,0,0,0,224,166,1,0,0,0,0,0,234,166,1,0,0,0,0,0,248,166,1,0,0,0,0,0,6,167,1,0,0,0,0,0,26,167,1,0,0,0,0,0,52,167,1,0,0,0,0,0,66,167,1,0,0,0,0,0,76,167,1,0,0,0,0,0,88,167,1,0,
      0,0,0,0,106,167,1,0,0,0,0,0,124,167,1,0,0,0,0,0,142,167,1,0,0,0,0,0,164,167,1,0,0,0,0,0,184,167,1,0,0,0,0,0,206,167,1,0,0,0,0,0,228,167,1,0,0,0,0,0,254,167,1,0,0,0,0,0,24,168,1,0,0,0,0,0,46,168,1,0,0,0,0,0,60,168,1,0,0,0,0,0,76,168,1,0,0,0,0,
      0,98,168,1,0,0,0,0,0,120,168,1,0,0,0,0,0,140,168,1,0,0,0,0,0,152,168,1,0,0,0,0,0,170,168,1,0,0,0,0,0,188,168,1,0,0,0,0,0,206,168,1,0,0,0,0,0,222,168,1,0,0,0,0,0,240,168,1,0,0,0,0,0,0,169,1,0,0,0,0,0,26,169,1,0,0,0,0,0,38,169,1,0,0,0,0,0,50,169,
      1,0,0,0,0,0,64,169,1,0,0,0,0,0,76,169,1,0,0,0,0,0,116,169,1,0,0,0,0,0,146,169,1,0,0,0,0,0,168,169,1,0,0,0,0,0,192,169,1,0,0,0,0,0,210,169,1,0,0,0,0,0,226,169,1,0,0,0,0,0,250,169,1,0,0,0,0,0,12,170,1,0,0,0,0,0,30,170,1,0,0,0,0,0,46,170,1,0,0,
      0,0,0,68,170,1,0,0,0,0,0,90,170,1,0,0,0,0,0,112,170,1,0,0,0,0,0,138,170,1,0,0,0,0,0,156,170,1,0,0,0,0,0,182,170,1,0,0,0,0,0,208,170,1,0,0,0,0,0,228,170,1,0,0,0,0,0,242,170,1,0,0,0,0,0,6,171,1,0,0,0,0,0,22,171,1,0,0,0,0,0,38,171,1,0,0,0,0,0,66,
      171,1,0,0,0,0,0,86,171,1,0,0,0,0,0,104,171,1,0,0,0,0,0,120,171,1,0,0,0,0,0,148,171,1,0,0,0,0,0,170,171,1,0,0,0,0,0,186,171,1,0,0,0,0,0,0,0,0,0,0,0,0,0,32,166,1,0,0,0,0,0,58,166,1,0,0,0,0,0,72,166,1,0,0,0,0,0,86,166,1,0,0,0,0,0,110,166,1,0,0,
      0,0,0,126,166,1,0,0,0,0,0,150,166,1,0,0,0,0,0,164,166,1,0,0,0,0,0,176,166,1,0,0,0,0,0,196,166,1,0,0,0,0,0,212,166,1,0,0,0,0,0,224,166,1,0,0,0,0,0,234,166,1,0,0,0,0,0,248,166,1,0,0,0,0,0,6,167,1,0,0,0,0,0,26,167,1,0,0,0,0,0,52,167,1,0,0,0,0,0,
      66,167,1,0,0,0,0,0,76,167,1,0,0,0,0,0,88,167,1,0,0,0,0,0,106,167,1,0,0,0,0,0,124,167,1,0,0,0,0,0,142,167,1,0,0,0,0,0,164,167,1,0,0,0,0,0,184,167,1,0,0,0,0,0,206,167,1,0,0,0,0,0,228,167,1,0,0,0,0,0,254,167,1,0,0,0,0,0,24,168,1,0,0,0,0,0,46,168,
      1,0,0,0,0,0,60,168,1,0,0,0,0,0,76,168,1,0,0,0,0,0,98,168,1,0,0,0,0,0,120,168,1,0,0,0,0,0,140,168,1,0,0,0,0,0,152,168,1,0,0,0,0,0,170,168,1,0,0,0,0,0,188,168,1,0,0,0,0,0,206,168,1,0,0,0,0,0,222,168,1,0,0,0,0,0,240,168,1,0,0,0,0,0,0,169,1,0,0,
      0,0,0,26,169,1,0,0,0,0,0,38,169,1,0,0,0,0,0,50,169,1,0,0,0,0,0,64,169,1,0,0,0,0,0,76,169,1,0,0,0,0,0,116,169,1,0,0,0,0,0,146,169,1,0,0,0,0,0,168,169,1,0,0,0,0,0,192,169,1,0,0,0,0,0,210,169,1,0,0,0,0,0,226,169,1,0,0,0,0,0,250,169,1,0,0,0,0,0,
      12,170,1,0,0,0,0,0,30,170,1,0,0,0,0,0,46,170,1,0,0,0,0,0,68,170,1,0,0,0,0,0,90,170,1,0,0,0,0,0,112,170,1,0,0,0,0,0,138,170,1,0,0,0,0,0,156,170,1,0,0,0,0,0,182,170,1,0,0,0,0,0,208,170,1,0,0,0,0,0,228,170,1,0,0,0,0,0,242,170,1,0,0,0,0,0,6,171,
      1,0,0,0,0,0,22,171,1,0,0,0,0,0,38,171,1,0,0,0,0,0,66,171,1,0,0,0,0,0,86,171,1,0,0,0,0,0,104,171,1,0,0,0,0,0,120,171,1,0,0,0,0,0,148,171,1,0,0,0,0,0,170,171,1,0,0,0,0,0,186,171,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,65,99,113,117,105,114,101,83,82,87,
      76,111,99,107,69,120,99,108,117,115,105,118,101,0,137,0,67,108,111,115,101,72,97,110,100,108,101,0,206,0,67,114,101,97,116,101,70,105,108,101,87,0,18,1,68,101,108,101,116,101,67,114,105,116,105,99,97,108,83,101,99,116,105,111,110,0,47,1,69,110,
      99,111,100,101,80,111,105,110,116,101,114,0,51,1,69,110,116,101,114,67,114,105,116,105,99,97,108,83,101,99,116,105,111,110,0,0,96,1,69,120,105,116,80,114,111,99,101,115,115,0,119,1,70,105,110,100,67,108,111,115,101,0,125,1,70,105,110,100,70,
      105,114,115,116,70,105,108,101,69,120,87,0,0,142,1,70,105,110,100,78,101,120,116,70,105,108,101,87,0,156,1,70,108,115,65,108,108,111,99,0,0,157,1,70,108,115,70,114,101,101,0,158,1,70,108,115,71,101,116,86,97,108,117,101,0,159,1,70,108,115,83,
      101,116,86,97,108,117,101,0,161,1,70,108,117,115,104,70,105,108,101,66,117,102,102,101,114,115,0,0,172,1,70,114,101,101,69,110,118,105,114,111,110,109,101,110,116,83,116,114,105,110,103,115,87,0,173,1,70,114,101,101,76,105,98,114,97,114,121,
      0,180,1,71,101,116,65,67,80,0,0,195,1,71,101,116,67,80,73,110,102,111,0,216,1,71,101,116,67,111,109,109,97,110,100,76,105,110,101,65,0,217,1,71,101,116,67,111,109,109,97,110,100,76,105,110,101,87,0,254,1,71,101,116,67,111,110,115,111,108,101,
      77,111,100,101,0,0,2,2,71,101,116,67,111,110,115,111,108,101,79,117,116,112,117,116,67,80,0,0,25,2,71,101,116,67,117,114,114,101,110,116,80,114,111,99,101,115,115,0,26,2,71,101,116,67,117,114,114,101,110,116,80,114,111,99,101,115,115,73,100,
      0,30,2,71,101,116,67,117,114,114,101,110,116,84,104,114,101,97,100,73,100,0,0,56,2,71,101,116,69,110,118,105,114,111,110,109,101,110,116,83,116,114,105,110,103,115,87,0,0,58,2,71,101,116,69,110,118,105,114,111,110,109,101,110,116,86,97,114,105,
      97,98,108,101,87,0,70,2,71,101,116,70,105,108,101,65,116,116,114,105,98,117,116,101,115,87,0,0,79,2,71,101,116,70,105,108,101,84,121,112,101,0,97,2,71,101,116,76,97,115,116,69,114,114,111,114,0,0,116,2,71,101,116,77,111,100,117,108,101,70,105,
      108,101,78,97,109,101,87,0,0,119,2,71,101,116,77,111,100,117,108,101,72,97,110,100,108,101,69,120,87,0,0,120,2,71,101,116,77,111,100,117,108,101,72,97,110,100,108,101,87,0,0,152,2,71,101,116,79,69,77,67,80,0,0,177,2,71,101,116,80,114,111,99,
      65,100,100,114,101,115,115,0,0,184,2,71,101,116,80,114,111,99,101,115,115,72,101,97,112,0,0,212,2,71,101,116,83,116,97,114,116,117,112,73,110,102,111,87,0,214,2,71,101,116,83,116,100,72,97,110,100,108,101,0,0,219,2,71,101,116,83,116,114,105,
      110,103,84,121,112,101,87,0,0,231,2,71,101,116,83,121,115,116,101,109,73,110,102,111,0,237,2,71,101,116,83,121,115,116,101,109,84,105,109,101,65,115,70,105,108,101,84,105,109,101,0,75,3,72,101,97,112,65,108,108,111,99,0,79,3,72,101,97,112,70,
      114,101,101,0,0,82,3,72,101,97,112,82,101,65,108,108,111,99,0,84,3,72,101,97,112,83,105,122,101,0,0,101,3,73,110,105,116,105,97,108,105,122,101,67,114,105,116,105,99,97,108,83,101,99,116,105,111,110,65,110,100,83,112,105,110,67,111,117,110,116,
      0,102,3,73,110,105,116,105,97,108,105,122,101,67,114,105,116,105,99,97,108,83,101,99,116,105,111,110,69,120,0,105,3,73,110,105,116,105,97,108,105,122,101,83,76,105,115,116,72,101,97,100,0,109,3,73,110,116,101,114,108,111,99,107,101,100,70,108,
      117,115,104,83,76,105,115,116,0,140,3,73,115,86,97,108,105,100,67,111,100,101,80,97,103,101,0,178,3,76,67,77,97,112,83,116,114,105,110,103,87,0,0,190,3,76,101,97,118,101,67,114,105,116,105,99,97,108,83,101,99,116,105,111,110,0,0,195,3,76,111,
      97,100,76,105,98,114,97,114,121,69,120,65,0,0,196,3,76,111,97,100,76,105,98,114,97,114,121,69,120,87,0,0,197,3,76,111,97,100,76,105,98,114,97,114,121,87,0,0,239,3,77,117,108,116,105,66,121,116,101,84,111,87,105,100,101,67,104,97,114,0,22,4,79,
      117,116,112,117,116,68,101,98,117,103,83,116,114,105,110,103,65,0,0,23,4,79,117,116,112,117,116,68,101,98,117,103,83,116,114,105,110,103,87,0,0,75,4,81,117,101,114,121,80,101,114,102,111,114,109,97,110,99,101,67,111,117,110,116,101,114,0,97,
      4,82,97,105,115,101,69,120,99,101,112,116,105,111,110,0,0,177,4,82,101,108,101,97,115,101,83,82,87,76,111,99,107,69,120,99,108,117,115,105,118,101,0,213,4,82,116,108,76,111,111,107,117,112,70,117,110,99,116,105,111,110,69,110,116,114,121,0,0,
      215,4,82,116,108,80,99,84,111,70,105,108,101,72,101,97,100,101,114,0,218,4,82,116,108,85,110,119,105,110,100,69,120,0,44,5,83,101,116,70,105,108,101,80,111,105,110,116,101,114,69,120,0,0,58,5,83,101,116,76,97,115,116,69,114,114,111,114,0,0,85,
      5,83,101,116,83,116,100,72,97,110,100,108,101,0,0,138,5,83,108,101,101,112,67,111,110,100,105,116,105,111,110,86,97,114,105,97,98,108,101,83,82,87,0,151,5,84,101,114,109,105,110,97,116,101,80,114,111,99,101,115,115,0,0,215,5,86,105,114,116,117,
      97,108,80,114,111,116,101,99,116,0,0,217,5,86,105,114,116,117,97,108,81,117,101,114,121,0,0,234,5,87,97,107,101,65,108,108,67,111,110,100,105,116,105,111,110,86,97,114,105,97,98,108,101,0,0,9,6,87,105,100,101,67,104,97,114,84,111,77,117,108,
      116,105,66,121,116,101,0,28,6,87,114,105,116,101,67,111,110,115,111,108,101,87,0,29,6,87,114,105,116,101,70,105,108,101,0,75,69,82,78,69,76,51,50,46,100,108,108,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,10,0,
      80,8,7,0,64,0,225,131,228,227,184,100,0,0,0,48,1,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,48,172,1,0,64,0,0,0,0,0,0,0,0,0,0,0,176,36,1,0,0,0,0,0,0,0,0,0,160,68,0,0,0,0,0,0,6,0,80,8,3,0,0,0,129,228,227,
      227,184,100,0,0,0,48,1,0,53,0,64,16,44,0,64,0,225,129,208,130,36,228,227,227,18,0,64,16,11,0,64,0,225,129,212,1,228,227,227,227,8,0,0,8,225,133,228,227,76,0,80,16,69,0,64,0,225,131,209,4,200,130,38,228,168,92,0,0,2,0,0,0,212,72,0,0,68,73,0,0,
      200,36,1,0,0,0,0,0,192,73,0,0,200,73,0,0,200,36,1,0,0,0,0,0,8,0,64,8,5,0,0,0,129,228,227,227,40,0,80,16,10,0,64,0,225,131,208,130,36,228,227,227,168,92,0,0,4,0,0,0,20,74,0,0,52,74,0,0,232,36,1,0,0,0,0,0,8,74,0,0,84,74,0,0,8,37,1,0,0,0,0,0,96,
      74,0,0,104,74,0,0,232,36,1,0,0,0,0,0,96,74,0,0,104,74,0,0,8,37,1,0,0,0,0,0,7,0,64,8,4,0,0,0,129,228,227,227,78,0,112,16,225,133,200,130,36,228,227,227,168,92,0,0,1,0,0,0,116,75,0,0,76,76,0,0,36,37,1,0,76,76,0,0,15,0,64,8,12,0,0,0,129,228,227,
      227,52,0,112,8,225,131,228,227,168,92,0,0,1,0,0,0,96,86,0,0,28,87,0,0,96,37,1,0,28,87,0,0,11,0,64,8,7,0,0,0,129,228,227,227,27,0,64,8,23,0,64,0,225,129,228,227,39,0,64,8,34,0,64,0,225,129,34,228,44,0,80,8,38,0,0,0,225,129,228,227,88,168,0,0,
      232,255,255,255,88,0,64,16,82,0,64,0,225,133,201,4,200,130,38,228,23,0,64,8,19,0,64,0,225,129,34,228,28,0,112,8,225,139,34,228,168,92,0,0,1,0,0,0,104,101,0,0,152,101,0,0,140,37,1,0,152,101,0,0,36,0,64,8,32,0,0,0,129,212,1,228,18,0,64,16,13,0,
      64,0,225,129,212,1,228,227,227,227,37,0,64,8,25,0,64,0,225,129,34,228,46,0,80,8,40,0,64,0,225,133,34,228,168,92,0,0,1,0,0,0,4,102,0,0,24,102,0,0,28,38,1,0,24,102,0,0,66,0,64,8,57,0,0,0,129,212,1,228,12,0,0,16,225,129,212,1,228,227,227,227,18,
      0,112,16,225,129,212,1,228,227,227,227,168,92,0,0,1,0,0,0,60,111,0,0,88,111,0,0,36,39,1,0,0,0,0,0,39,0,112,8,225,131,34,228,184,100,0,0,64,52,1,0,228,111,0,0,255,255,255,255,23,0,0,16,225,129,212,1,228,227,227,227,56,0,80,16,51,0,64,0,225,137,
      212,1,228,227,227,227,184,100,0,0,112,52,1,0,255,255,255,255,104,102,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,113,0,0,255,255,255,255,172,113,0,0,0,0,0,0,68,0,80,16,63,0,64,0,225,133,208,130,36,228,227,227,168,92,0,0,1,0,0,0,4,114,0,0,120,114,0,0,104,
      39,1,0,220,114,0,0,17,0,64,8,13,0,0,0,129,228,227,227,15,0,112,8,225,131,34,228,168,92,0,0,1,0,0,0,0,115,0,0,8,115,0,0,68,39,1,0,0,0,0,0,9,0,64,8,6,0,0,0,129,228,227,227,17,0,112,8,225,135,228,227,184,100,0,0,160,52,1,0,220,119,0,0,0,0,0,0,17,
      0,112,8,225,131,34,228,168,92,0,0,1,0,0,0,88,121,0,0,100,121,0,0,68,39,1,0,0,0,0,0,40,0,112,8,225,131,34,228,184,100,0,0,208,52,1,0,79,0,64,16,68,0,64,0,225,129,209,4,200,130,38,228,137,0,64,16,21,0,64,0,225,129,209,4,200,130,38,228,5,0,32,8,
      1,228,227,227,13,0,16,16,225,129,212,1,228,227,227,227,56,234,0,0,1,0,0,0,136,131,0,0,160,131,0,0,1,0,0,0,160,131,0,0,49,0,0,8,228,0,0,0,71,0,0,8,228,0,0,0,34,0,64,8,30,0,64,0,225,131,34,228,51,0,64,8,47,0,64,0,225,131,34,228,25,0,112,8,225,
      129,34,228,168,92,0,0,1,0,0,0,56,142,0,0,124,142,0,0,172,39,1,0,124,142,0,0,11,0,64,8,8,0,0,0,129,228,227,227,7,0,64,8,5,0,128,0,2,228,2,228,1,0,0,8,228,0,0,0,21,0,64,48,14,0,64,3,226,10,74,202,8,201,134,201,4,200,130,44,228,74,202,8,201,134,
      201,4,200,130,44,228,21,0,64,48,14,0,64,3,226,10,74,202,8,201,134,201,4,200,130,44,228,74,202,8,201,134,201,4,200,130,44,228,12,0,0,8,225,129,228,227,7,0,64,8,4,0,64,0,225,129,228,227,120,0,80,24,111,0,64,0,225,137,210,7,209,134,201,4,200,130,
      40,228,168,92,0,0,3,0,0,0,48,147,0,0,228,147,0,0,160,40,1,0,228,147,0,0,248,146,0,0,8,148,0,0,188,40,1,0,0,0,0,0,96,148,0,0,100,148,0,0,188,40,1,0,0,0,0,0,13,0,64,8,10,0,0,0,129,228,227,227,132,0,80,24,122,0,64,0,225,143,210,7,209,134,201,4,
      200,130,40,228,168,92,0,0,2,0,0,0,216,149,0,0,248,149,0,0,216,39,1,0,248,149,0,0,216,149,0,0,128,150,0,0,0,40,1,0,0,0,0,0,10,0,64,8,7,0,0,0,129,228,227,227,40,0,64,8,33,0,0,0,129,212,1,228,60,0,64,24,52,0,64,0,225,131,210,8,201,134,201,4,200,
      130,42,228,161,0,64,24,145,0,64,0,225,129,210,8,201,134,201,4,200,130,42,228,122,0,80,16,111,0,64,0,225,131,201,4,200,130,38,228,168,92,0,0,2,0,0,0,148,157,0,0,184,158,0,0,1,0,0,0,236,158,0,0,216,158,0,0,236,158,0,0,1,0,0,0,236,158,0,0,55,0,
      80,16,50,0,64,0,225,129,208,130,36,228,227,227,168,92,0,0,1,0,0,0,36,159,0,0,184,159,0,0,1,0,0,0,200,159,0,0,79,1,80,32,51,1,0,0,225,210,10,201,136,201,6,200,132,200,2,139,228,227,227,227,88,168,0,0,152,255,255,255,150,0,64,24,139,0,0,0,225,
      141,210,8,201,134,201,4,200,130,42,228,6,0,64,8,5,0,128,0,1,228,228,0,11,0,64,16,6,0,64,0,228,1,236,228,228,0,0,0,5,0,64,16,3,0,0,1,226,2,66,228,66,228,0,0,31,0,64,8,12,0,64,0,225,129,228,227,128,0,64,24,92,0,64,0,225,133,210,8,201,134,201,4,
      200,130,42,228,59,0,64,16,54,0,64,0,225,129,208,130,36,228,227,227,45,0,64,8,41,0,64,0,225,129,34,228,49,0,64,16,15,0,64,0,225,129,208,130,36,228,227,227,21,0,112,8,225,131,34,228,168,92,0,0,1,0,0,0,232,183,0,0,8,184,0,0,68,39,1,0,0,0,0,0,19,
      0,112,8,225,131,34,228,168,92,0,0,1,0,0,0,64,184,0,0,88,184,0,0,68,39,1,0,0,0,0,0,28,0,112,8,225,131,34,228,168,92,0,0,1,0,0,0,144,184,0,0,204,184,0,0,68,39,1,0,0,0,0,0,31,0,112,8,225,131,34,228,168,92,0,0,1,0,0,0,0,185,0,0,72,185,0,0,68,39,
      1,0,0,0,0,0,18,0,112,16,225,129,212,1,228,227,227,227,168,92,0,0,1,0,0,0,120,185,0,0,144,185,0,0,240,40,1,0,0,0,0,0,163,0,48,16,225,200,132,200,2,133,228,227,88,168,0,0,200,255,255,255,50,0,80,16,45,0,64,0,225,129,208,130,36,228,227,227,56,234,
      0,0,1,0,0,0,28,193,0,0,124,193,0,0,16,41,1,0,0,0,0,0,102,0,96,16,225,64,192,39,208,130,36,228,131,0,48,16,225,208,2,131,228,227,227,227,88,168,0,0,216,255,255,255,86,0,112,16,225,131,200,130,36,228,227,227,168,92,0,0,1,0,0,0,188,197,0,0,216,
      198,0,0,68,39,1,0,0,0,0,0,52,0,64,24,29,0,64,0,225,129,209,134,201,4,200,130,40,228,227,227,154,0,80,24,37,0,64,0,225,133,210,7,209,134,201,4,200,130,40,228,168,92,0,0,2,0,0,0,140,203,0,0,60,204,0,0,48,41,1,0,0,0,0,0,204,204,0,0,224,204,0,0,
      48,41,1,0,0,0,0,0,20,0,112,8,225,131,34,228,168,92,0,0,1,0,0,0,100,205,0,0,128,205,0,0,68,39,1,0,0,0,0,0,49,0,112,8,225,131,34,228,168,92,0,0,1,0,0,0,176,207,0,0,64,208,0,0,88,41,1,0,0,0,0,0,59,0,112,16,225,139,200,130,36,228,227,227,168,92,
      0,0,1,0,0,0,124,208,0,0,44,209,0,0,124,41,1,0,0,0,0,0,154,0,64,24,141,0,64,0,225,135,210,8,201,134,201,4,200,130,42,228,100,0,96,16,225,129,209,3,208,130,36,228,102,0,96,16,225,129,200,194,36,228,227,227,100,0,64,24,21,0,64,0,225,129,209,134,
      201,4,200,130,40,228,227,227,198,0,80,24,189,0,0,0,225,209,136,201,6,200,132,200,2,137,228,227,88,168,0,0,168,255,255,255,67,0,48,16,225,200,2,131,228,227,227,227,88,168,0,0,216,255,255,255,74,0,96,16,225,129,208,194,36,228,227,227,33,0,112,
      16,225,133,200,130,36,228,227,227,168,92,0,0,1,0,0,0,48,233,0,0,124,233,0,0,160,41,1,0,0,0,0,0,25,0,64,8,15,0,64,0,225,129,34,228,41,0,64,56,33,0,64,3,202,12,201,138,201,8,200,134,200,4,66,7,228,202,12,201,138,201,8,200,134,200,4,66,7,228,0,
      0,18,0,0,8,228,0,0,0,57,0,0,8,228,0,0,0,19,0,0,8,228,0,0,0,72,0,0,8,228,0,0,0,6,0,64,8,5,0,64,0,228,228,0,0,11,0,0,8,225,131,228,0,44,0,64,16,28,0,64,0,225,129,201,4,200,130,38,228,27,0,80,8,23,0,64,0,225,129,34,228,168,92,0,0,1,0,0,0,160,240,
      0,0,180,240,0,0,196,41,1,0,0,0,0,0,46,0,80,16,13,0,64,0,225,131,201,4,200,130,38,228,168,92,0,0,2,0,0,0,92,244,0,0,152,244,0,0,240,40,1,0,0,0,0,0,168,244,0,0,192,244,0,0,240,40,1,0,0,0,0,0,113,0,48,32,225,210,10,201,136,201,6,200,132,200,2,139,
      228,227,227,227,88,168,0,0,152,255,255,255,196,0,48,32,225,210,10,201,136,201,6,200,132,200,2,139,228,227,227,227,88,168,0,0,152,255,255,255,99,0,0,8,228,0,0,0,45,0,112,8,225,131,34,228,168,92,0,0,1,0,0,0,128,251,0,0,252,251,0,0,228,41,1,0,0,
      0,0,0,222,0,48,32,225,210,10,201,136,201,6,200,132,200,2,139,228,227,227,227,88,168,0,0,152,255,255,255,82,0,112,24,225,131,210,7,209,134,201,4,200,130,40,228,168,92,0,0,1,0,0,0,100,0,1,0,172,0,1,0,8,42,1,0,0,0,0,0,53,1,80,32,11,1,0,0,225,210,
      10,201,136,201,6,200,132,200,2,139,228,227,227,227,80,28,1,0,16,141,1,0,155,255,255,255,152,1,1,0,255,255,255,255,78,0,48,24,225,209,6,200,132,200,2,135,228,227,227,227,88,168,0,0,184,255,255,255,76,0,48,16,225,200,132,200,2,133,228,227,88,168,
      0,0,200,255,255,255,95,0,48,32,225,210,10,201,136,201,6,200,132,200,2,139,228,227,227,227,88,168,0,0,152,255,255,255,53,0,112,16,225,131,200,130,36,228,227,227,168,92,0,0,1,0,0,0,240,9,1,0,136,10,1,0,40,42,1,0,0,0,0,0,5,1,80,32,19,0,0,0,225,
      210,10,201,136,201,6,200,132,200,2,139,228,227,227,227,88,168,0,0,152,255,255,255,60,0,64,8,19,0,64,0,225,129,228,227,43,0,80,8,39,0,0,0,225,129,228,227,88,168,0,0,232,255,255,255,244,0,0,8,228,0,0,0,12,0,0,8,228,0,0,0,110,0,64,24,73,0,64,0,
      225,131,210,8,201,134,201,4,200,130,42,228,38,0,80,8,19,0,64,0,225,131,34,228,168,92,0,0,1,0,0,0,144,30,1,0,160,30,1,0,72,42,1,0,0,0,0,0,113,0,48,8,225,129,228,227,88,168,0,0,232,255,255,255,36,0,112,8,225,131,34,228,168,92,0,0,1,0,0,0,240,35,
      1,0,72,36,1,0,228,41,1,0,0,0,0,0,24,0,128,16,20,0,192,0,22,0,192,0,225,129,228,129,228,227,227,227,26,0,160,16,226,3,67,208,130,38,228,227,21,0,160,16,226,1,65,212,3,228,227,227,64,0,160,16,226,5,69,209,4,230,40,228,120,0,64,32,115,0,0,2,2,226,
      3,67,208,130,38,228,2,67,208,130,38,228,227,227,108,0,64,32,101,0,64,2,4,226,8,72,230,230,230,42,228,4,72,230,230,230,42,228,42,0,128,24,19,0,128,1,22,0,128,1,226,1,65,212,3,228,65,212,3,228,227,227,106,0,64,32,100,0,0,2,12,226,6,70,230,230,
      40,228,12,70,230,230,40,228,227,227,196,0,64,32,168,0,0,2,12,226,6,70,230,230,40,228,12,70,230,230,40,228,227,227,113,0,64,24,108,0,192,1,12,226,4,68,230,38,228,12,68,230,38,228,229,0,64,40,221,0,128,2,6,226,10,74,230,230,230,230,44,228,6,74,
      230,230,230,230,44,228,227,227,54,0,64,32,43,0,0,2,3,226,3,67,208,130,38,228,3,67,208,130,38,228,227,227,53,0,64,24,48,0,192,1,1,226,4,68,230,38,228,1,68,230,38,228,20,0,128,16,10,0,192,0,18,0,192,0,225,129,228,129,228,227,227,227,90,0,64,40,
      39,0,128,2,192,35,226,5,69,210,68,230,40,228,192,35,69,210,68,230,40,228,227,227,34,0,128,16,16,0,64,1,31,0,64,1,226,2,66,36,228,66,36,228,44,0,64,24,39,0,128,1,226,1,65,212,3,228,65,212,3,228,227,227,71,0,64,24,67,0,128,1,4,226,2,66,36,228,
      4,66,36,228,227,227,27,0,160,16,226,2,66,36,228,227,227,227,131,0,64,24,89,0,192,1,1,226,4,68,230,38,228,1,68,230,38,228,18,0,128,16,14,0,192,0,16,0,192,0,225,129,228,129,228,227,227,227,49,0,64,24,45,0,128,1,3,226,2,66,36,228,3,66,36,228,227,
      227,69,0,128,24,57,0,192,1,64,0,192,1,226,6,70,230,230,40,228,70,230,230,40,228,31,0,64,24,27,0,192,1,1,226,1,65,212,3,228,1,65,212,3,228,130,0,64,32,111,0,0,2,6,226,6,70,230,230,40,228,6,70,230,230,40,228,227,227,71,0,128,32,47,0,64,2,63,0,
      64,2,1,226,5,69,209,4,230,40,228,1,69,209,4,230,40,228,48,0,64,40,42,0,128,2,192,34,226,5,69,210,68,230,40,228,192,34,69,210,68,230,40,228,227,227,15,0,160,16,226,1,65,212,3,228,227,227,11,0,160,16,226,1,65,212,3,228,227,227,43,0,64,24,35,0,
      128,1,226,4,68,230,38,228,68,230,38,228,227,227,16,0,160,16,226,2,66,36,228,227,227,227,37,0,160,16,226,3,67,208,130,38,228,227,38,0,160,16,226,4,68,230,38,228,227,227,29,0,160,16,226,3,67,208,130,38,228,227,35,0,128,24,26,0,128,1,31,0,128,1,
      226,4,68,230,38,228,68,230,38,228,227,227,50,0,160,24,226,9,73,210,8,230,230,230,44,228,227,227,128,0,64,40,121,0,128,2,3,226,7,71,209,134,230,230,42,228,3,71,209,134,230,230,42,228,227,227,0,0,0,0,72,71,0,0,0,0,0,0,168,186,1,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,2,0,0,0,192,186,1,0,232,186,1,0,0,0,0,0,0,0,0,0,0,0,0,0,16,0,0,0,232,201,1,0,0,0,0,0,255,255,255,255,0,0,0,0,24,0,0,0,88,71,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,16,202,1,0,0,0,0,0,255,255,255,255,0,0,0,0,24,0,0,0,128,70,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,72,71,0,0,0,0,0,0,48,187,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,80,187,1,0,192,186,1,0,232,186,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,56,202,1,0,0,0,0,0,255,255,255,255,0,0,0,0,24,0,0,0,200,71,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,72,71,0,0,0,0,0,0,152,187,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,176,187,1,0,232,186,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,104,202,1,0,0,0,0,0,255,255,255,255,0,0,0,0,24,0,0,0,96,151,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,255,255,255,255,50,162,223,45,153,43,0,0,205,93,32,210,102,212,255,255,2,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
      16,0,0,0,0,0,0,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,0,0,0,0,0,0,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,0,0,0,
      0,0,0,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,0,0,0,0,0,0,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      1,2,4,8,0,0,0,0,0,0,0,0,0,0,0,0,164,3,0,0,96,130,121,130,33,0,0,0,0,0,0,0,166,223,0,0,0,0,0,0,161,165,0,0,0,0,0,0,129,159,224,252,0,0,0,0,64,126,128,252,0,0,0,0,168,3,0,0,193,163,218,163,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,129,254,
      0,0,0,0,0,0,64,254,0,0,0,0,0,0,181,3,0,0,193,163,218,163,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,129,254,0,0,0,0,0,0,65,254,0,0,0,0,0,0,182,3,0,0,207,162,228,162,26,0,229,162,232,162,91,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,129,254,0,0,
      0,0,0,0,64,126,161,254,0,0,0,0,81,5,0,0,81,218,94,218,32,0,95,218,106,218,50,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,129,211,216,222,224,249,0,0,49,126,129,254,0,0,0,0,0,83,1,128,1,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,216,198,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,216,198,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,216,198,1,128,1,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,216,198,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,216,198,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,200,1,128,1,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,128,85,1,128,1,0,0,0,0,87,1,128,1,0,0,0,16,74,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,112,197,1,128,1,0,0,0,48,192,1,128,1,0,0,0,67,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,32,0,0,1,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,32,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,12,0,0,0,8,0,0,0,2,88,1,128,1,0,0,0,117,152,0,0,254,255,255,255,152,200,1,128,1,0,0,0,20,215,1,128,1,0,0,0,20,215,1,128,1,0,0,0,20,215,1,128,1,0,0,0,20,215,1,128,1,0,0,0,20,215,1,128,1,0,0,0,20,215,1,128,
      1,0,0,0,20,215,1,128,1,0,0,0,20,215,1,128,1,0,0,0,20,215,1,128,1,0,0,0,127,127,127,127,127,127,127,127,156,200,1,128,1,0,0,0,24,215,1,128,1,0,0,0,24,215,1,128,1,0,0,0,24,215,1,128,1,0,0,0,24,215,1,128,1,0,0,0,24,215,1,128,1,0,0,0,24,215,1,128,
      1,0,0,0,24,215,1,128,1,0,0,0,46,0,0,0,46,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
      1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,0,0,0,0,0,0,0,0,254,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,228,156,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
      192,48,1,128,1,0,0,0,0,0,0,0,0,0,0,0,46,63,65,86,98,97,100,95,97,108,108,111,99,64,115,116,100,64,64,0,0,0,0,0,192,48,1,128,1,0,0,0,0,0,0,0,0,0,0,0,46,63,65,86,101,120,99,101,112,116,105,111,110,64,115,116,100,64,64,0,0,0,0,0,192,48,1,128,1,
      0,0,0,0,0,0,0,0,0,0,0,46,63,65,86,98,97,100,95,97,114,114,97,121,95,110,101,119,95,108,101,110,103,116,104,64,115,116,100,64,64,0,0,192,48,1,128,1,0,0,0,0,0,0,0,0,0,0,0,46,63,65,86,98,97,100,95,101,120,99,101,112,116,105,111,110,64,115,116,100,
      64,64,0,192,48,1,128,1,0,0,0,0,0,0,0,0,0,0,0,46,63,65,86,116,121,112,101,95,105,110,102,111,64,64,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,104,42,1,128,1,0,0,0,116,42,1,128,1,0,0,0,128,42,1,128,1,0,0,0,140,42,1,128,1,0,0,0,152,42,1,128,1,0,0,0,164,
      42,1,128,1,0,0,0,176,42,1,128,1,0,0,0,188,42,1,128,1,0,0,0,0,0,0,0,0,0,0,0,44,43,1,128,1,0,0,0,56,43,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,16,0,0,212,175,1,0,0,17,0,0,220,175,1,0,32,18,0,0,40,176,1,0,60,18,0,0,52,176,1,0,64,18,0,0,60,176,1,0,148,18,0,0,92,176,1,0,240,18,0,0,248,177,
      1,0,16,19,0,0,4,178,1,0,64,19,0,0,20,178,1,0,88,19,0,0,244,180,1,0,0,20,0,0,24,181,1,0,96,20,0,0,32,181,1,0,96,21,0,0,40,181,1,0,192,21,0,0,48,181,1,0,224,22,0,0,56,181,1,0,248,22,0,0,68,181,1,0,64,23,0,0,240,181,1,0,224,24,0,0,52,183,1,0,208,
      28,0,0,60,183,1,0,16,29,0,0,172,183,1,0,116,29,0,0,192,183,1,0,220,29,0,0,204,183,1,0,48,30,0,0,216,183,1,0,48,31,0,0,228,183,1,0,16,33,0,0,252,183,1,0,192,34,0,0,20,184,1,0,112,35,0,0,44,184,1,0,24,37,0,0,68,184,1,0,48,40,0,0,92,184,1,0,244,
      41,0,0,112,184,1,0,144,45,0,0,140,184,1,0,104,46,0,0,164,184,1,0,84,47,0,0,184,184,1,0,164,47,0,0,204,184,1,0,12,49,0,0,232,184,1,0,240,49,0,0,252,184,1,0,160,50,0,0,16,185,1,0,192,51,0,0,36,185,1,0,56,52,0,0,169,0,224,0,224,52,0,0,48,185,1,
      0,236,54,0,0,68,185,1,0,52,55,0,0,88,185,1,0,248,55,0,0,108,185,1,0,12,57,0,0,132,185,1,0,136,57,0,0,152,185,1,0,144,59,0,0,176,185,1,0,172,60,0,0,204,185,1,0,120,61,0,0,232,185,1,0,180,61,0,0,244,185,1,0,224,61,0,0,0,186,1,0,140,62,0,0,20,186,
      1,0,204,62,0,0,32,186,1,0,156,63,0,0,20,186,1,0,220,63,0,0,44,186,1,0,200,64,0,0,56,186,1,0,60,65,0,0,68,186,1,0,200,65,0,0,92,186,1,0,144,66,0,0,108,186,1,0,144,68,0,0,248,171,1,0,208,68,0,0,100,172,1,0,168,69,0,0,145,0,98,1,56,70,0,0,116,172,
      1,0,128,70,0,0,65,0,97,1,224,70,0,0,73,0,98,1,88,71,0,0,77,0,97,1,200,71,0,0,77,0,97,1,24,72,0,0,132,172,1,0,56,72,0,0,132,172,1,0,96,72,0,0,57,0,97,1,152,72,0,0,140,172,1,0,200,73,0,0,208,172,1,0,112,74,0,0,97,0,224,0,208,74,0,0,97,0,228,1,
      48,75,0,0,52,173,1,0,112,76,0,0,73,0,227,1,184,76,0,0,213,0,98,1,144,77,0,0,141,0,98,1,184,78,0,0,201,0,98,4,128,79,0,0,161,0,100,2,32,80,0,0,145,0,98,1,176,80,0,0,141,0,226,1,64,81,0,0,149,3,233,6,232,84,0,0,221,0,98,2,0,86,0,0,53,0,224,0,88,
      86,0,0,100,173,1,0,40,87,0,0,144,173,1,0,152,87,0,0,57,0,97,1,208,87,0,0,93,0,224,0,48,88,0,0,69,0,97,1,120,88,0,0,156,173,1,0,24,89,0,0,125,0,102,2,152,89,0,0,37,0,224,0,192,89,0,0,61,0,224,0,0,90,0,0,53,0,224,0,56,90,0,0,29,0,224,0,88,90,0,
      0,61,0,224,0,152,90,0,0,29,0,224,0,208,90,0,0,101,0,228,1,56,91,0,0,101,0,228,1,208,91,0,0,193,0,229,3,168,92,0,0,161,2,105,4,80,95,0,0,109,0,228,1,192,95,0,0,61,0,97,1,0,96,0,0,109,0,99,2,112,96,0,0,5,1,104,3,120,97,0,0,168,173,1,0,40,98,0,
      0,188,173,1,0,136,99,0,0,25,0,224,0,160,99,0,0,37,0,97,1,200,99,0,0,25,0,224,0,224,99,0,0,37,0,97,1,8,100,0,0,81,0,97,1,88,100,0,0,204,173,1,0,184,100,0,0,125,0,100,2,56,101,0,0,216,173,1,0,176,101,0,0,32,174,1,0,120,102,0,0,4,174,1,0,232,102,
      0,0,20,174,1,0,128,103,0,0,153,0,228,1,24,104,0,0,53,0,97,1,80,104,0,0,61,0,97,1,176,104,0,0,61,0,224,0,240,104,0,0,37,0,224,0,24,105,0,0,25,0,224,0,48,105,0,0,29,0,224,0,80,105,0,0,29,0,224,0,112,105,0,0,80,174,1,0,128,108,0,0,45,0,224,0,176,
      108,0,0,25,0,224,0,208,108,0,0,77,0,97,1,32,109,0,0,29,0,224,0,64,109,0,0,89,0,98,1,160,109,0,0,93,0,97,1,0,110,0,0,37,0,224,0,40,110,0,0,69,0,224,0,112,110,0,0,29,0,224,0,144,110,0,0,29,0,224,0,176,110,0,0,25,0,224,0,200,110,0,0,81,0,98,1,40,
      111,0,0,92,174,1,0,176,111,0,0,128,174,1,0,80,112,0,0,61,0,224,0,144,112,0,0,152,174,1,0,240,112,0,0,164,174,1,0,208,113,0,0,224,174,1,0,224,114,0,0,20,175,1,0,32,115,0,0,153,1,228,2,184,116,0,0,113,0,97,1,40,117,0,0,73,2,231,2,168,119,0,0,64,
      175,1,0,240,119,0,0,73,1,105,3,56,121,0,0,88,175,1,0,136,121,0,0,120,175,1,0,40,122,0,0,133,0,98,1,176,122,0,0,85,0,98,1,8,123,0,0,85,0,98,1,96,123,0,0,136,175,1,0,160,124,0,0,85,0,228,1,248,124,0,0,93,0,228,1,128,125,0,0,152,175,1,0,176,127,
      0,0,133,0,97,1,56,128,0,0,197,0,224,0,0,129,0,0,77,0,224,0,80,129,0,0,29,1,228,2,176,130,0,0,157,0,97,1,80,131,0,0,168,175,1,0,112,131,0,0,176,175,1,0,168,131,0,0,197,0,226,3,112,132,0,0,105,0,98,1,216,132,0,0,228,175,1,0,96,133,0,0,240,175,
      1,0,48,134,0,0,121,0,226,1,168,134,0,0,173,6,232,2,40,142,0,0,252,175,1,0,144,142,0,0,124,176,1,0,192,142,0,0,121,0,97,1,56,143,0,0,69,0,97,1,128,143,0,0,132,176,1,0,160,143,0,0,21,1,228,1,184,144,0,0,121,0,98,1,8,146,0,0,73,0,226,1,80,146,0,
      0,57,0,225,1,136,146,0,0,144,176,1,0,104,148,0,0,165,0,100,2,16,149,0,0,232,176,1,0,96,151,0,0,77,0,97,1,176,151,0,0,181,0,98,1,112,152,0,0,60,177,1,0,96,153,0,0,80,177,1,0,232,155,0,0,33,1,228,1,8,157,0,0,100,177,1,0,240,158,0,0,156,177,1,0,
      208,159,0,0,196,177,1,0,16,165,0,0,209,0,233,3,224,165,0,0,228,177,1,0,88,168,0,0,57,0,224,0,208,168,0,0,36,178,1,0,80,169,0,0,85,0,224,0,168,169,0,0,109,0,101,2,80,170,0,0,101,0,227,1,184,170,0,0,89,0,97,1,80,171,0,0,141,0,228,1,224,171,0,0,
      245,0,105,3,216,172,0,0,117,0,227,1,80,173,0,0,101,0,98,1,192,173,0,0,48,178,1,0,192,175,0,0,121,0,228,1,64,176,0,0,109,0,101,2,0,177,0,0,49,0,224,0,64,177,0,0,77,0,224,0,144,177,0,0,57,0,97,1,200,177,0,0,68,178,1,0,184,178,0,0,84,178,1,0,112,
      179,0,0,229,0,227,1,88,180,0,0,81,0,98,1,168,180,0,0,96,178,1,0,112,181,0,0,45,0,97,1,160,181,0,0,109,0,98,1,16,182,0,0,201,0,224,2,216,182,0,0,237,0,96,2,200,183,0,0,112,178,1,0,32,184,0,0,144,178,1,0,112,184,0,0,176,178,1,0,224,184,0,0,208,
      178,1,0,96,185,0,0,240,178,1,0,176,185,0,0,69,0,98,1,248,185,0,0,253,0,103,6,248,186,0,0,65,1,105,3,64,188,0,0,69,0,224,0,136,188,0,0,33,0,224,0,176,188,0,0,113,0,98,1,32,189,0,0,20,179,1,0,176,191,0,0,169,0,97,2,88,192,0,0,125,0,97,1,216,192,
      0,0,40,179,1,0,160,193,0,0,80,179,1,0,136,195,0,0,92,179,1,0,152,197,0,0,112,179,1,0,240,198,0,0,97,0,97,1,80,199,0,0,193,0,101,2,16,200,0,0,97,0,228,1,152,200,0,0,148,179,1,0,104,201,0,0,101,0,98,1,208,201,0,0,93,0,224,2,72,202,0,0,49,0,96,
      1,120,202,0,0,168,179,1,0,72,205,0,0,228,179,1,0,176,205,0,0,181,0,97,3,104,206,0,0,197,0,228,1,48,207,0,0,97,0,96,2,144,207,0,0,4,180,1,0,88,208,0,0,36,180,1,0,128,209,0,0,33,1,232,2,160,210,0,0,117,0,228,1,24,211,0,0,69,0,96,1,96,211,0,0,41,
      0,224,0,136,211,0,0,41,0,224,0,176,211,0,0,97,0,97,1,120,212,0,0,53,0,98,1,176,212,0,0,177,0,227,1,104,213,0,0,193,0,226,2,40,214,0,0,72,180,1,0,144,216,0,0,92,180,1,0,32,218,0,0,104,180,1,0,184,219,0,0,116,180,1,0,72,221,0,0,136,180,1,0,128,
      224,0,0,164,180,1,0,144,225,0,0,184,180,1,0,208,226,0,0,141,0,227,2,96,227,0,0,161,0,228,1,64,229,0,0,57,1,228,1,120,230,0,0,9,1,96,2,128,231,0,0,77,0,97,1,208,231,0,0,77,0,97,1,32,232,0,0,81,0,97,1,112,232,0,0,81,0,97,1,224,232,0,0,49,0,96,
      1,16,233,0,0,196,180,1,0,152,233,0,0,161,0,227,1,56,234,0,0,232,180,1,0,160,234,0,0,121,0,225,1,168,235,0,0,76,181,1,0,80,238,0,0,81,1,101,2,32,240,0,0,73,0,97,1,104,240,0,0,92,181,1,0,216,240,0,0,121,0,98,1,80,241,0,0,145,0,224,0,224,241,0,
      0,237,0,101,2,208,242,0,0,205,0,102,2,160,243,0,0,101,0,228,1,8,244,0,0,128,181,1,0,32,245,0,0,184,181,1,0,232,246,0,0,165,0,231,3,144,247,0,0,212,181,1,0,160,250,0,0,33,0,224,0,192,250,0,0,161,0,224,1,96,251,0,0,248,181,1,0,24,252,0,0,53,0,
      224,0,80,252,0,0,24,182,1,0,200,255,0,0,52,182,1,0,16,1,1,0,92,182,1,0,232,5,1,0,136,182,1,0,32,7,1,0,160,182,1,0,80,8,1,0,180,182,1,0,208,9,1,0,208,182,1,0,168,10,1,0,105,0,97,1,16,11,1,0,125,0,224,0,160,11,1,0,244,182,1,0,184,15,1,0,20,183,
      1,0,168,16,1,0,32,183,1,0,96,17,1,0,177,0,98,2,16,18,1,0,77,0,224,0,96,18,1,0,197,0,228,1,40,19,1,0,145,0,102,2,184,19,1,0,45,1,98,1,232,20,1,0,141,0,98,1,120,21,1,0,121,1,227,1,240,22,1,0,25,0,128,0,8,23,1,0,25,0,224,0,32,23,1,0,25,0,128,0,
      72,23,1,0,101,1,228,1,64,25,1,0,205,0,100,2,16,26,1,0,73,0,225,1,88,26,1,0,68,183,1,0,16,28,1,0,61,0,96,1,80,28,1,0,141,0,101,2,224,28,1,0,109,0,97,3,80,29,1,0,197,0,227,1,24,30,1,0,88,183,1,0,176,30,1,0,124,183,1,0,120,32,1,0,101,0,97,1,224,
      32,1,0,245,0,101,2,8,34,1,0,245,0,228,1,0,35,1,0,209,0,224,1,208,35,1,0,140,183,1,0,176,36,1,0,80,172,1,0,200,36,1,0,196,172,1,0,232,36,1,0,196,172,1,0,8,37,1,0,40,173,1,0,36,37,1,0,88,173,1,0,96,37,1,0,132,173,1,0,140,37,1,0,248,173,1,0,28,
      38,1,0,68,174,1,0,36,39,1,0,196,172,1,0,68,39,1,0,52,175,1,0,104,39,1,0,8,175,1,0,172,39,1,0,28,176,1,0,216,39,1,0,36,177,1,0,0,40,1,0,48,177,1,0,160,40,1,0,40,173,1,0,188,40,1,0,220,176,1,0,240,40,1,0,196,172,1,0,16,41,1,0,196,172,1,0,48,41,
      1,0,36,177,1,0,88,41,1,0,52,175,1,0,124,41,1,0,52,175,1,0,160,41,1,0,52,175,1,0,196,41,1,0,196,172,1,0,228,41,1,0,52,175,1,0,8,42,1,0,196,172,1,0,40,42,1,0,196,172,1,0,72,42,1,0,196,172,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,160,91,0,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,16,0,0,0,24,0,0,128,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,1,0,1,0,0,0,48,0,0,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,9,4,0,0,72,0,0,0,96,16,2,0,40,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,40,5,52,0,0,0,86,0,83,0,95,0,86,0,69,0,82,0,83,0,73,0,79,0,78,0,95,0,73,0,78,0,70,0,79,0,0,0,0,0,189,4,
      239,254,0,0,1,0,0,0,1,0,42,0,240,4,0,0,1,0,42,0,240,4,63,0,0,0,0,0,0,0,4,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,4,0,0,1,0,83,0,116,0,114,0,105,0,110,0,103,0,70,0,105,0,108,0,101,0,73,0,110,0,102,0,111,0,0,0,100,4,0,0,1,0,48,0,52,0,48,0,57,
      0,48,0,52,0,98,0,48,0,0,0,76,0,22,0,1,0,67,0,111,0,109,0,112,0,97,0,110,0,121,0,78,0,97,0,109,0,101,0,0,0,0,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,32,0,67,0,111,0,114,0,112,0,111,0,114,0,97,0,116,0,105,0,111,0,110,0,0,0,134,0,
      47,0,1,0,70,0,105,0,108,0,101,0,68,0,101,0,115,0,99,0,114,0,105,0,112,0,116,0,105,0,111,0,110,0,0,0,0,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,32,0,69,0,100,0,103,0,101,0,32,0,69,0,109,0,98,0,101,0,100,0,100,0,101,0,100,0,32,0,66,
      0,114,0,111,0,119,0,115,0,101,0,114,0,32,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,32,0,76,0,111,0,97,0,100,0,101,0,114,0,0,0,0,0,56,0,12,0,1,0,70,0,105,0,108,0,101,0,86,0,101,0,114,0,115,0,105,0,111,0,110,0,0,0,0,0,49,0,46,0,48,0,46,0,49,0,50,
      0,54,0,52,0,46,0,52,0,50,0,0,0,70,0,19,0,1,0,73,0,110,0,116,0,101,0,114,0,110,0,97,0,108,0,78,0,97,0,109,0,101,0,0,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,76,0,111,0,97,0,100,0,101,0,114,0,46,0,100,0,108,0,108,0,0,0,0,0,144,0,54,0,1,0,
      76,0,101,0,103,0,97,0,108,0,67,0,111,0,112,0,121,0,114,0,105,0,103,0,104,0,116,0,0,0,67,0,111,0,112,0,121,0,114,0,105,0,103,0,104,0,116,0,32,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,32,0,67,0,111,0,114,0,112,0,111,0,114,0,97,0,116,
      0,105,0,111,0,110,0,46,0,32,0,65,0,108,0,108,0,32,0,114,0,105,0,103,0,104,0,116,0,115,0,32,0,114,0,101,0,115,0,101,0,114,0,118,0,101,0,100,0,46,0,0,0,78,0,19,0,1,0,79,0,114,0,105,0,103,0,105,0,110,0,97,0,108,0,70,0,105,0,108,0,101,0,110,0,97,
      0,109,0,101,0,0,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,76,0,111,0,97,0,100,0,101,0,114,0,46,0,100,0,108,0,108,0,0,0,0,0,126,0,47,0,1,0,80,0,114,0,111,0,100,0,117,0,99,0,116,0,78,0,97,0,109,0,101,0,0,0,0,0,77,0,105,0,99,0,114,0,111,0,115,
      0,111,0,102,0,116,0,32,0,69,0,100,0,103,0,101,0,32,0,69,0,109,0,98,0,101,0,100,0,100,0,101,0,100,0,32,0,66,0,114,0,111,0,119,0,115,0,101,0,114,0,32,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,32,0,76,0,111,0,97,0,100,0,101,0,114,0,0,0,0,0,60,0,
      12,0,1,0,80,0,114,0,111,0,100,0,117,0,99,0,116,0,86,0,101,0,114,0,115,0,105,0,111,0,110,0,0,0,49,0,46,0,48,0,46,0,49,0,50,0,54,0,52,0,46,0,52,0,50,0,0,0,60,0,10,0,1,0,67,0,111,0,109,0,112,0,97,0,110,0,121,0,83,0,104,0,111,0,114,0,116,0,78,0,
      97,0,109,0,101,0,0,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,0,0,134,0,47,0,1,0,80,0,114,0,111,0,100,0,117,0,99,0,116,0,83,0,104,0,111,0,114,0,116,0,78,0,97,0,109,0,101,0,0,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,32,
      0,69,0,100,0,103,0,101,0,32,0,69,0,109,0,98,0,101,0,100,0,100,0,101,0,100,0,32,0,66,0,114,0,111,0,119,0,115,0,101,0,114,0,32,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,32,0,76,0,111,0,97,0,100,0,101,0,114,0,0,0,0,0,110,0,41,0,1,0,76,0,97,0,115,
      0,116,0,67,0,104,0,97,0,110,0,103,0,101,0,0,0,99,0,99,0,54,0,54,0,97,0,100,0,52,0,102,0,57,0,50,0,99,0,54,0,51,0,97,0,54,0,49,0,57,0,102,0,48,0,54,0,57,0,52,0,48,0,101,0,49,0,102,0,98,0,52,0,54,0,51,0,53,0,99,0,102,0,57,0,53,0,101,0,102,0,102,
      0,50,0,99,0,0,0,0,0,40,0,2,0,1,0,79,0,102,0,102,0,105,0,99,0,105,0,97,0,108,0,32,0,66,0,117,0,105,0,108,0,100,0,0,0,49,0,0,0,68,0,0,0,1,0,86,0,97,0,114,0,70,0,105,0,108,0,101,0,73,0,110,0,102,0,111,0,0,0,0,0,36,0,4,0,0,0,84,0,114,0,97,0,110,
      0,115,0,108,0,97,0,116,0,105,0,111,0,110,0,0,0,0,0,9,4,176,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,48,1,0,64,1,0,0,48,160,56,160,64,160,96,160,104,160,112,160,136,160,144,160,152,160,184,160,192,160,120,161,144,161,160,161,80,162,8,163,16,163,32,163,48,163,56,163,64,163,72,163,
      80,163,88,163,96,163,104,163,120,163,128,163,136,163,144,163,152,163,160,163,168,163,176,163,200,163,216,163,232,163,240,163,248,163,0,164,8,164,216,165,224,165,232,165,0,166,16,166,32,166,48,166,64,166,80,166,96,166,112,166,128,166,144,166,
      160,166,176,166,192,166,208,166,224,166,240,166,0,167,16,167,32,167,48,167,64,167,80,167,96,167,112,167,128,167,144,167,160,167,176,167,192,167,208,167,224,167,240,167,0,168,16,168,32,168,48,168,64,168,80,168,96,168,112,168,128,168,144,168,160,
      168,176,168,192,168,208,168,224,168,240,168,0,169,16,169,32,169,48,169,64,169,80,169,96,169,112,169,128,169,144,169,160,169,176,169,192,169,208,169,224,169,240,169,0,170,16,170,32,170,48,170,64,170,80,170,96,170,112,170,128,170,144,170,160,170,
      176,170,192,170,208,170,224,170,240,170,0,171,16,171,32,171,48,171,64,171,80,171,96,171,112,171,128,171,144,171,160,171,176,171,192,171,208,171,224,171,240,171,0,172,16,172,32,172,48,172,64,172,80,172,96,172,112,172,128,172,144,172,160,172,176,
      172,192,172,208,172,224,172,240,172,0,64,1,0,220,0,0,0,192,163,200,163,208,163,216,163,224,163,232,163,240,163,248,163,0,164,8,164,16,164,24,164,32,164,40,164,48,164,56,164,64,164,72,164,80,164,16,170,24,170,32,170,40,170,48,170,56,170,64,170,
      72,170,80,170,88,170,96,170,104,170,112,170,120,170,128,170,136,170,144,170,152,170,160,170,168,170,176,170,184,170,192,170,200,170,208,170,216,170,224,170,232,170,240,170,248,170,0,171,8,171,16,171,24,171,32,171,40,171,48,171,56,171,64,171,
      72,171,80,171,88,171,96,171,112,171,120,171,128,171,136,171,144,171,152,171,160,171,168,171,176,171,184,171,192,171,200,171,208,171,216,171,224,171,232,171,240,171,248,171,0,172,8,172,16,172,24,172,32,172,40,172,48,172,56,172,64,172,72,172,80,
      172,88,172,96,172,104,172,112,172,120,172,128,172,136,172,144,172,152,172,160,172,168,172,176,172,184,172,192,172,200,172,0,80,1,0,200,0,0,0,24,170,40,170,56,170,72,170,88,170,104,170,120,170,136,170,152,170,168,170,184,170,200,170,216,170,232,
      170,248,170,8,171,24,171,40,171,56,171,72,171,88,171,104,171,120,171,136,171,152,171,168,171,184,171,200,171,216,171,232,171,248,171,8,172,24,172,40,172,56,172,72,172,88,172,104,172,120,172,136,172,152,172,168,172,184,172,200,172,216,172,232,
      172,248,172,8,173,24,173,40,173,56,173,72,173,88,173,104,173,120,173,136,173,152,173,168,173,184,173,200,173,216,173,232,173,248,173,8,174,24,174,40,174,56,174,72,174,88,174,104,174,120,174,136,174,152,174,168,174,184,174,200,174,216,174,232,
      174,248,174,8,175,24,175,40,175,56,175,72,175,88,175,104,175,120,175,136,175,152,175,168,175,184,175,200,175,216,175,232,175,248,175,0,0,0,96,1,0,20,1,0,0,8,160,24,160,40,160,56,160,72,160,88,160,104,160,120,160,136,160,152,160,168,160,184,160,
      200,160,216,160,232,160,248,160,8,161,24,161,40,161,56,161,72,161,88,161,104,161,120,161,136,161,152,161,168,161,184,161,200,161,216,161,232,161,248,161,8,162,24,162,40,162,56,162,72,162,88,162,104,162,120,162,136,162,152,162,168,162,184,162,
      200,162,216,162,232,162,248,162,8,163,24,163,40,163,56,163,72,163,88,163,104,163,120,163,136,163,152,163,168,163,184,163,200,163,216,163,232,163,248,163,8,164,24,164,40,164,56,164,72,164,88,164,104,164,120,164,136,164,152,164,168,164,184,164,
      200,164,216,164,232,164,248,164,8,165,24,165,40,165,56,165,72,165,88,165,104,165,120,165,136,165,152,165,168,165,184,165,200,165,216,165,232,165,248,165,8,166,24,166,40,166,56,166,72,166,88,166,104,166,120,166,136,166,152,166,168,166,184,166,
      200,166,216,166,232,166,248,166,8,167,24,167,40,167,56,167,72,167,88,167,104,167,120,167,136,167,152,167,168,167,184,167,200,167,216,167,232,167,248,167,8,168,24,168,40,168,56,168,72,168,0,0,0,112,1,0,124,1,0,0,112,164,128,164,144,164,160,164,
      176,164,192,164,208,164,224,164,240,164,0,165,16,165,32,165,48,165,64,165,80,165,96,165,112,165,128,165,144,165,160,165,176,165,192,165,208,165,224,165,240,165,0,166,16,166,32,166,48,166,64,166,80,166,96,166,112,166,128,166,144,166,160,166,176,
      166,192,166,208,166,224,166,240,166,0,167,16,167,32,167,48,167,64,167,80,167,96,167,112,167,128,167,144,167,160,167,176,167,192,167,208,167,224,167,240,167,0,168,16,168,32,168,48,168,64,168,80,168,96,168,112,168,128,168,144,168,160,168,176,168,
      192,168,208,168,224,168,240,168,0,169,16,169,32,169,48,169,64,169,80,169,96,169,112,169,128,169,144,169,160,169,176,169,192,169,208,169,224,169,240,169,0,170,16,170,32,170,48,170,64,170,80,170,96,170,112,170,128,170,144,170,160,170,176,170,192,
      170,208,170,224,170,240,170,0,171,16,171,32,171,48,171,64,171,80,171,96,171,112,171,128,171,144,171,160,171,176,171,192,171,208,171,224,171,240,171,0,172,16,172,32,172,48,172,64,172,80,172,96,172,112,172,128,172,144,172,160,172,176,172,192,172,
      208,172,224,172,240,172,0,173,16,173,32,173,48,173,64,173,80,173,96,173,112,173,128,173,144,173,160,173,176,173,192,173,208,173,224,173,240,173,0,174,16,174,32,174,48,174,64,174,80,174,96,174,112,174,128,174,144,174,160,174,176,174,192,174,208,
      174,224,174,240,174,0,175,16,175,32,175,48,175,64,175,80,175,96,175,112,175,128,175,144,175,160,175,176,175,192,175,208,175,224,175,240,175,0,0,0,128,1,0,128,0,0,0,0,160,16,160,32,160,48,160,64,160,80,160,96,160,112,160,128,160,144,160,160,160,
      176,160,192,160,208,160,224,160,240,160,0,161,16,161,32,161,48,161,64,161,80,161,96,161,112,161,128,161,144,161,160,161,176,161,192,161,208,161,224,161,240,161,0,162,16,162,32,162,48,162,64,162,80,162,96,162,112,162,128,162,144,162,160,162,56,
      173,64,173,96,173,104,173,112,173,120,173,128,173,144,173,152,173,160,173,168,173,176,173,192,175,200,175,208,175,216,175,224,175,0,144,1,0,48,0,0,0,144,160,152,160,160,160,168,160,176,160,184,164,192,164,200,164,208,164,216,164,184,169,192,
      169,200,169,208,169,72,174,80,174,120,174,128,174,136,174,0,0,0,192,1,0,104,0,0,0,112,165,184,165,216,165,248,165,24,166,56,166,104,166,128,166,136,166,144,166,200,166,208,166,240,167,0,168,8,168,16,168,24,168,32,168,40,168,48,168,56,168,64,
      168,72,168,88,168,96,168,104,168,112,168,120,168,128,168,136,168,144,168,176,169,232,169,16,170,56,170,104,170,144,170,192,170,200,170,208,170,216,170,224,170,232,170,240,170,248,170,8,171,16,171,0,0,0,240,1,0,12,0,0,0,0,160,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,184,39,0,0,0,2,2,
      0,48,130,39,171,6,9,42,134,72,134,247,13,1,7,2,160,130,39,156,48,130,39,152,2,1,1,49,15,48,13,6,9,96,134,72,1,101,3,4,2,1,5,0,48,92,6,10,43,6,1,4,1,130,55,2,1,4,160,78,48,76,48,23,6,10,43,6,1,4,1,130,55,2,1,15,48,9,3,1,0,160,4,162,2,128,0,48,
      49,48,13,6,9,96,134,72,1,101,3,4,2,1,5,0,4,32,117,23,235,185,206,246,144,184,0,165,90,48,7,79,204,58,74,236,253,25,148,223,31,54,184,241,164,140,106,114,95,100,160,130,13,118,48,130,5,244,48,130,3,220,160,3,2,1,2,2,19,51,0,0,2,84,202,43,243,
      203,157,218,166,117,0,0,0,0,2,84,48,13,6,9,42,134,72,134,247,13,1,1,11,5,0,48,126,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,
      48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,40,48,38,6,3,85,4,3,19,31,77,105,99,114,111,115,111,102,116,32,67,111,100,101,32,83,105,103,110,105,110,103,32,80,67,65,32,50,48,49,49,
      48,30,23,13,50,49,48,57,48,50,49,56,51,51,48,49,90,23,13,50,50,48,57,48,49,49,56,51,51,48,49,90,48,116,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,
      109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,30,48,28,6,3,85,4,3,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,48,130,1,
      34,48,13,6,9,42,134,72,134,247,13,1,1,1,5,0,3,130,1,15,0,48,130,1,10,2,130,1,1,0,151,7,140,40,63,36,195,85,171,46,104,228,72,188,116,24,83,247,93,244,26,235,122,210,75,107,154,24,76,220,90,153,27,8,202,74,33,196,192,166,250,139,159,55,222,80,
      164,227,19,9,31,210,53,166,84,180,241,44,143,19,49,217,186,179,16,28,106,168,42,180,143,177,216,148,170,61,146,82,161,24,219,125,165,92,103,195,21,110,151,21,210,37,143,142,198,239,107,27,68,123,131,72,214,139,223,253,135,59,175,159,120,211,
      10,198,172,120,236,128,250,66,96,173,232,140,58,48,230,209,6,107,146,155,68,192,164,167,178,163,123,133,228,200,141,224,175,35,102,100,106,26,2,83,70,123,146,240,116,127,247,34,190,95,70,100,246,96,141,100,55,223,169,132,96,4,205,241,11,6,222,
      86,58,123,155,108,1,21,32,100,9,66,3,172,117,34,199,63,144,32,143,242,204,98,17,203,8,244,228,45,148,20,197,247,181,150,126,24,173,86,137,174,144,101,184,38,19,210,241,208,40,146,87,66,211,19,230,79,253,94,83,51,51,130,251,175,192,186,31,248,
      31,65,42,197,221,66,180,91,2,3,1,0,1,163,130,1,115,48,130,1,111,48,31,6,3,85,29,37,4,24,48,22,6,10,43,6,1,4,1,130,55,10,3,21,6,8,43,6,1,5,5,7,3,3,48,29,6,3,85,29,14,4,22,4,20,55,169,25,48,180,12,215,59,160,140,236,164,168,1,158,116,234,6,41,
      205,48,69,6,3,85,29,17,4,62,48,60,164,58,48,56,49,30,48,28,6,3,85,4,11,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,22,48,20,6,3,85,4,5,19,13,50,51,48,50,49,55,43,52,54,55,51,49,54,48,31,6,3,85,29,35,
      4,24,48,22,128,20,72,110,100,229,80,5,211,130,170,23,55,55,34,181,109,168,202,117,2,149,48,84,6,3,85,29,31,4,77,48,75,48,73,160,71,160,69,134,67,104,116,116,112,58,47,47,119,119,119,46,109,105,99,114,111,115,111,102,116,46,99,111,109,47,112,
      107,105,111,112,115,47,99,114,108,47,77,105,99,67,111,100,83,105,103,80,67,65,50,48,49,49,95,50,48,49,49,45,48,55,45,48,56,46,99,114,108,48,97,6,8,43,6,1,5,5,7,1,1,4,85,48,83,48,81,6,8,43,6,1,5,5,7,48,2,134,69,104,116,116,112,58,47,47,119,119,
      119,46,109,105,99,114,111,115,111,102,116,46,99,111,109,47,112,107,105,111,112,115,47,99,101,114,116,115,47,77,105,99,67,111,100,83,105,103,80,67,65,50,48,49,49,95,50,48,49,49,45,48,55,45,48,56,46,99,114,116,48,12,6,3,85,29,19,1,1,255,4,2,48,
      0,48,13,6,9,42,134,72,134,247,13,1,1,11,5,0,3,130,2,1,0,160,84,177,238,79,104,199,79,206,37,136,239,156,178,227,251,98,44,125,252,193,87,89,169,17,201,78,2,3,59,210,84,124,174,82,205,112,188,43,202,208,241,25,69,181,111,186,4,184,253,81,146,
      185,147,253,28,215,105,153,102,89,224,49,213,90,206,179,60,202,247,104,45,219,89,79,91,22,14,169,137,88,148,213,145,232,4,195,59,77,26,162,39,51,179,205,72,64,169,18,3,243,213,91,105,38,205,206,147,218,53,239,199,48,134,108,31,2,223,161,35,51,
      191,123,247,2,101,140,91,128,63,117,99,136,243,27,34,156,207,123,210,34,181,110,148,241,247,195,155,90,229,249,80,226,96,94,154,74,227,44,72,210,69,251,31,13,110,244,64,105,62,34,233,186,129,33,228,166,88,218,126,172,189,34,171,220,130,116,108,
      121,202,175,119,3,14,34,221,118,86,104,62,237,55,146,208,212,90,77,76,131,201,131,213,20,166,107,21,236,183,184,36,244,190,145,36,47,65,144,152,76,142,137,235,63,224,205,232,85,62,47,208,76,204,185,116,108,20,90,19,194,74,221,24,12,132,78,103,
      237,194,134,47,64,251,254,124,211,13,73,39,247,236,217,235,174,175,28,231,141,43,239,95,25,1,33,156,55,80,103,44,182,163,21,29,209,150,101,209,143,28,166,146,9,65,71,131,37,165,21,143,57,90,39,235,16,0,190,206,145,114,128,119,176,105,86,219,
      159,235,124,104,45,4,86,187,232,246,52,31,31,106,50,23,62,70,141,160,225,78,199,127,46,161,52,164,101,40,82,169,135,247,52,135,54,7,16,60,62,88,165,166,15,130,171,43,240,44,185,47,71,134,17,104,70,4,175,150,240,188,51,17,208,104,160,181,86,197,
      141,40,103,3,213,165,61,246,78,172,215,68,178,178,216,137,21,29,55,128,53,153,185,210,224,142,209,57,38,120,225,228,13,218,246,15,112,252,113,188,108,142,138,105,56,246,213,174,60,10,43,11,101,143,230,6,135,51,32,88,94,125,103,82,61,71,178,150,
      251,208,39,0,43,185,255,89,8,87,153,51,122,43,77,222,115,74,114,15,149,162,80,46,113,148,175,68,131,243,62,172,99,208,175,73,162,181,141,44,251,226,52,102,134,108,151,145,4,216,48,130,7,122,48,130,5,98,160,3,2,1,2,2,10,97,14,144,210,0,0,0,0,
      0,3,48,13,6,9,42,134,72,134,247,13,1,1,11,5,0,48,129,136,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,
      77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,50,48,48,6,3,85,4,3,19,41,77,105,99,114,111,115,111,102,116,32,82,111,111,116,32,67,101,114,116,105,102,105,99,97,116,101,32,65,117,116,104,111,114,105,116,121,
      32,50,48,49,49,48,30,23,13,49,49,48,55,48,56,50,48,53,57,48,57,90,23,13,50,54,48,55,48,56,50,49,48,57,48,57,90,48,126,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,
      19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,40,48,38,6,3,85,4,3,19,31,77,105,99,114,111,115,111,102,116,32,67,111,100,101,32,83,105,103,110,105,
      110,103,32,80,67,65,32,50,48,49,49,48,130,2,34,48,13,6,9,42,134,72,134,247,13,1,1,1,5,0,3,130,2,15,0,48,130,2,10,2,130,2,1,0,171,240,250,114,16,28,46,173,216,110,170,130,16,77,52,186,242,182,88,33,159,66,27,42,107,233,90,80,170,184,6,56,26,4,
      73,186,127,195,12,30,221,55,107,198,18,216,11,240,56,194,153,6,176,200,57,213,1,20,49,66,211,137,13,121,100,135,126,148,96,36,108,175,158,73,156,233,104,94,210,223,155,83,178,10,44,195,175,217,169,43,174,122,9,175,215,150,89,202,96,26,5,233,
      102,118,232,50,82,38,18,47,231,171,8,80,207,179,68,183,93,216,196,46,3,117,171,104,243,203,109,243,58,92,161,22,244,70,186,224,56,100,172,110,100,53,120,166,160,99,15,45,211,64,147,248,227,222,7,13,213,92,121,165,73,41,231,13,190,160,19,119,
      190,148,61,239,251,227,43,90,16,31,77,86,40,162,122,114,224,18,58,183,73,94,216,237,237,67,145,131,217,123,178,123,134,27,217,62,177,140,93,232,137,79,132,26,242,161,47,89,228,144,59,45,174,51,88,197,183,62,254,50,211,179,3,61,177,178,175,146,
      56,126,210,157,128,44,245,78,86,145,33,53,37,195,57,110,100,127,83,186,156,15,173,25,35,132,203,244,186,3,134,141,247,95,240,208,82,191,140,148,135,188,192,33,116,37,95,24,40,182,204,39,40,56,37,152,57,74,54,207,124,177,146,174,28,35,167,169,
      102,236,97,31,106,225,40,73,157,95,136,226,37,93,211,33,75,62,82,196,181,87,63,36,3,240,209,122,91,47,213,35,227,112,93,15,81,70,119,179,248,0,225,188,172,2,130,95,219,192,21,179,189,27,212,85,75,231,57,161,15,233,35,73,188,24,184,68,124,69,
      228,193,195,114,122,224,114,231,36,223,191,70,153,197,239,194,28,87,219,131,141,236,77,73,48,167,171,142,223,236,91,159,175,252,221,176,102,226,193,151,129,123,237,214,237,75,231,73,41,167,19,40,166,167,125,103,128,230,138,98,120,95,178,47,132,
      215,87,156,92,191,119,40,40,241,237,109,195,40,143,44,143,64,55,79,193,225,133,68,137,196,9,76,197,212,165,67,47,116,149,247,110,248,120,32,88,44,19,93,96,149,154,62,79,51,132,218,176,136,23,222,158,78,244,150,176,188,70,160,108,152,210,224,
      214,136,140,11,2,3,1,0,1,163,130,1,237,48,130,1,233,48,16,6,9,43,6,1,4,1,130,55,21,1,4,3,2,1,0,48,29,6,3,85,29,14,4,22,4,20,72,110,100,229,80,5,211,130,170,23,55,55,34,181,109,168,202,117,2,149,48,25,6,9,43,6,1,4,1,130,55,20,2,4,12,30,10,0,83,
      0,117,0,98,0,67,0,65,48,11,6,3,85,29,15,4,4,3,2,1,134,48,15,6,3,85,29,19,1,1,255,4,5,48,3,1,1,255,48,31,6,3,85,29,35,4,24,48,22,128,20,114,45,58,2,49,144,67,185,20,5,78,225,234,167,199,49,209,35,137,52,48,90,6,3,85,29,31,4,83,48,81,48,79,160,
      77,160,75,134,73,104,116,116,112,58,47,47,99,114,108,46,109,105,99,114,111,115,111,102,116,46,99,111,109,47,112,107,105,47,99,114,108,47,112,114,111,100,117,99,116,115,47,77,105,99,82,111,111,67,101,114,65,117,116,50,48,49,49,95,50,48,49,49,
      95,48,51,95,50,50,46,99,114,108,48,94,6,8,43,6,1,5,5,7,1,1,4,82,48,80,48,78,6,8,43,6,1,5,5,7,48,2,134,66,104,116,116,112,58,47,47,119,119,119,46,109,105,99,114,111,115,111,102,116,46,99,111,109,47,112,107,105,47,99,101,114,116,115,47,77,105,
      99,82,111,111,67,101,114,65,117,116,50,48,49,49,95,50,48,49,49,95,48,51,95,50,50,46,99,114,116,48,129,159,6,3,85,29,32,4,129,151,48,129,148,48,129,145,6,9,43,6,1,4,1,130,55,46,3,48,129,131,48,63,6,8,43,6,1,5,5,7,2,1,22,51,104,116,116,112,58,
      47,47,119,119,119,46,109,105,99,114,111,115,111,102,116,46,99,111,109,47,112,107,105,111,112,115,47,100,111,99,115,47,112,114,105,109,97,114,121,99,112,115,46,104,116,109,48,64,6,8,43,6,1,5,5,7,2,2,48,52,30,50,32,29,0,76,0,101,0,103,0,97,0,108,
      0,95,0,112,0,111,0,108,0,105,0,99,0,121,0,95,0,115,0,116,0,97,0,116,0,101,0,109,0,101,0,110,0,116,0,46,32,29,48,13,6,9,42,134,72,134,247,13,1,1,11,5,0,3,130,2,1,0,103,242,134,165,152,224,84,121,26,46,211,216,116,103,34,155,11,150,17,225,99,146,
      153,66,150,125,210,121,12,144,193,101,95,46,44,62,248,195,114,209,109,131,254,190,63,232,10,202,59,191,71,169,163,243,105,219,99,191,34,53,165,151,93,101,132,144,125,139,70,80,85,216,12,146,124,210,26,75,28,243,60,66,139,82,208,176,253,107,227,
      62,7,46,41,155,230,61,27,165,212,181,29,119,148,57,226,233,100,201,68,61,120,122,35,243,19,125,166,144,116,131,141,244,203,38,2,70,42,194,138,16,187,164,169,5,12,155,237,104,250,104,46,149,160,42,63,42,107,88,73,99,31,9,105,110,90,152,150,228,
      131,244,192,143,243,70,43,222,252,59,208,189,53,239,110,37,174,229,175,39,237,208,221,243,14,175,153,40,151,152,77,14,61,11,242,8,137,214,31,195,50,24,226,240,197,45,206,91,158,180,73,57,10,198,10,194,198,173,174,229,178,217,219,21,136,81,69,
      88,56,50,113,39,26,127,177,244,39,248,222,44,58,32,105,152,178,89,137,104,110,111,167,183,116,195,64,5,6,166,1,42,40,62,130,63,19,77,102,11,192,179,77,245,225,143,127,28,111,21,125,69,167,118,229,64,42,101,163,195,93,82,98,134,195,29,99,54,151,
      134,223,218,243,248,242,22,161,154,39,225,205,165,151,208,238,93,99,65,227,91,7,156,135,62,6,119,6,209,6,177,117,31,20,190,97,97,181,240,220,198,27,4,190,223,65,199,14,40,238,222,101,47,236,151,246,161,92,150,216,0,214,161,70,189,89,243,151,
      165,9,75,72,16,153,128,31,208,0,41,197,177,155,165,63,69,119,30,53,198,210,162,162,159,122,122,34,250,72,149,31,171,251,71,35,128,245,158,248,191,107,183,75,151,226,235,117,120,26,236,234,55,153,121,24,75,255,214,179,35,104,117,230,175,250,252,
      139,235,11,128,234,105,59,175,252,48,237,4,76,142,223,223,117,109,99,145,61,209,157,86,78,79,191,128,87,34,161,120,17,50,33,122,239,65,10,177,63,251,168,204,164,93,193,161,136,155,87,113,86,78,72,69,192,66,201,155,118,91,10,128,72,107,253,121,
      159,193,189,109,109,106,201,82,115,19,13,122,80,205,49,130,25,168,48,130,25,164,2,1,1,48,129,149,48,126,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,
      109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,40,48,38,6,3,85,4,3,19,31,77,105,99,114,111,115,111,102,116,32,67,111,100,101,32,83,105,103,110,105,110,103,32,80,
      67,65,32,50,48,49,49,2,19,51,0,0,2,84,202,43,243,203,157,218,166,117,0,0,0,0,2,84,48,13,6,9,96,134,72,1,101,3,4,2,1,5,0,160,129,212,48,25,6,9,42,134,72,134,247,13,1,9,3,49,12,6,10,43,6,1,4,1,130,55,2,1,4,48,28,6,10,43,6,1,4,1,130,55,2,1,11,49,
      14,48,12,6,10,43,6,1,4,1,130,55,2,1,21,48,47,6,9,42,134,72,134,247,13,1,9,4,49,34,4,32,37,201,138,52,190,210,220,191,173,6,52,74,110,49,38,31,243,50,40,93,45,252,172,71,5,140,230,106,83,14,67,99,48,104,6,10,43,6,1,4,1,130,55,2,1,12,49,90,48,
      88,160,56,128,54,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,32,0,69,0,100,0,103,0,101,0,32,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,32,0,83,0,68,0,75,161,28,128,26,104,116,116,112,115,58,47,47,119,119,119,46,109,105,99,114,111,
      115,111,102,116,46,99,111,109,32,48,13,6,9,42,134,72,134,247,13,1,1,1,5,0,4,130,1,0,41,50,73,205,177,181,66,3,196,209,153,232,187,44,112,117,61,199,92,216,253,175,6,152,51,62,48,75,113,198,27,103,33,66,235,36,218,3,220,0,60,41,62,172,223,112,
      20,151,89,121,65,157,34,38,231,203,109,15,245,1,151,208,65,204,250,182,66,119,112,189,153,35,193,222,117,106,31,148,193,221,249,15,231,84,168,83,97,16,22,178,154,207,50,125,138,132,78,78,152,62,185,209,38,43,130,225,233,137,190,71,208,150,156,
      11,17,134,103,165,180,198,74,240,199,197,94,214,248,183,82,37,75,124,241,202,248,0,242,62,15,68,148,143,141,239,97,75,205,88,134,207,225,150,172,181,173,35,126,201,74,220,199,16,64,155,38,47,63,23,200,144,80,166,200,203,192,66,148,42,55,221,
      55,10,37,227,254,145,197,167,43,249,189,87,20,151,33,66,180,237,233,146,61,70,95,165,30,129,118,230,20,195,24,185,117,217,53,64,210,9,135,114,236,29,26,97,87,26,18,191,53,67,92,81,231,249,47,30,137,93,186,112,204,52,111,28,174,142,219,182,166,
      236,251,48,126,227,233,83,161,130,23,12,48,130,23,8,6,10,43,6,1,4,1,130,55,3,3,1,49,130,22,248,48,130,22,244,6,9,42,134,72,134,247,13,1,7,2,160,130,22,229,48,130,22,225,2,1,3,49,15,48,13,6,9,96,134,72,1,101,3,4,2,1,5,0,48,130,1,85,6,11,42,134,
      72,134,247,13,1,9,16,1,4,160,130,1,68,4,130,1,64,48,130,1,60,2,1,1,6,10,43,6,1,4,1,132,89,10,3,1,48,49,48,13,6,9,96,134,72,1,101,3,4,2,1,5,0,4,32,84,30,122,8,218,182,185,44,230,2,89,77,145,71,240,59,245,99,110,203,98,46,108,59,7,122,43,249,252,
      168,228,208,2,6,98,177,216,154,116,248,24,19,50,48,50,50,48,54,50,56,50,50,48,49,48,57,46,49,56,52,90,48,4,128,2,1,244,160,129,212,164,129,209,48,129,206,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,
      116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,41,48,39,6,3,85,4,11,19,32,77,105,99,114,111,115,111,102,116,32,
      79,112,101,114,97,116,105,111,110,115,32,80,117,101,114,116,111,32,82,105,99,111,49,38,48,36,6,3,85,4,11,19,29,84,104,97,108,101,115,32,84,83,83,32,69,83,78,58,55,56,56,48,45,69,51,57,48,45,56,48,49,52,49,37,48,35,6,3,85,4,3,19,28,77,105,99,
      114,111,115,111,102,116,32,84,105,109,101,45,83,116,97,109,112,32,83,101,114,118,105,99,101,160,130,17,95,48,130,7,16,48,130,4,248,160,3,2,1,2,2,19,51,0,0,1,168,85,240,97,169,204,48,24,92,0,1,0,0,1,168,48,13,6,9,42,134,72,134,247,13,1,1,11,5,
      0,48,124,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,
      112,111,114,97,116,105,111,110,49,38,48,36,6,3,85,4,3,19,29,77,105,99,114,111,115,111,102,116,32,84,105,109,101,45,83,116,97,109,112,32,80,67,65,32,50,48,49,48,48,30,23,13,50,50,48,51,48,50,49,56,53,49,50,51,90,23,13,50,51,48,53,49,49,49,56,
      53,49,50,51,90,48,129,206,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,
      116,32,67,111,114,112,111,114,97,116,105,111,110,49,41,48,39,6,3,85,4,11,19,32,77,105,99,114,111,115,111,102,116,32,79,112,101,114,97,116,105,111,110,115,32,80,117,101,114,116,111,32,82,105,99,111,49,38,48,36,6,3,85,4,11,19,29,84,104,97,108,
      101,115,32,84,83,83,32,69,83,78,58,55,56,56,48,45,69,51,57,48,45,56,48,49,52,49,37,48,35,6,3,85,4,3,19,28,77,105,99,114,111,115,111,102,116,32,84,105,109,101,45,83,116,97,109,112,32,83,101,114,118,105,99,101,48,130,2,34,48,13,6,9,42,134,72,134,
      247,13,1,1,1,5,0,3,130,2,15,0,48,130,2,10,2,130,2,1,0,163,218,109,202,192,46,37,215,242,152,242,94,151,12,55,207,74,78,247,38,148,8,131,195,138,84,248,119,163,102,24,155,81,234,63,29,76,190,63,213,113,90,16,151,238,136,30,120,163,201,235,190,
      254,45,229,93,61,209,185,133,59,198,14,4,174,163,124,62,1,201,179,113,118,118,64,33,148,129,68,94,45,66,155,154,186,244,233,176,116,198,27,237,103,10,30,150,28,109,62,72,201,22,198,211,26,212,173,249,94,196,225,80,163,61,214,42,65,159,226,55,
      64,43,93,121,160,108,71,213,200,44,18,250,29,174,162,128,97,51,149,28,108,250,32,182,190,85,166,7,115,109,67,56,254,181,174,166,97,85,140,240,117,106,201,247,251,11,251,104,16,138,242,63,80,80,154,232,99,140,181,166,102,82,127,68,174,24,197,
      152,237,201,182,114,250,219,118,110,215,149,217,99,169,23,71,91,224,144,0,30,4,170,35,170,130,233,159,71,230,146,166,142,83,239,146,134,211,233,236,6,72,229,25,43,20,212,24,181,230,158,43,63,207,41,102,116,168,167,189,198,122,111,70,88,237,134,
      68,188,15,215,150,4,156,171,28,207,24,70,99,230,124,52,70,180,67,3,12,137,89,100,177,211,212,236,158,106,43,216,49,1,8,206,39,58,136,72,156,225,35,4,22,180,66,234,128,154,7,194,74,147,197,233,183,130,182,59,7,176,206,205,4,231,66,146,150,160,
      189,7,89,31,74,168,139,218,11,184,189,94,83,239,250,231,188,246,63,94,237,249,129,226,120,117,134,233,115,16,156,215,233,19,20,245,49,201,50,86,206,232,234,207,220,211,68,40,149,188,142,42,66,159,47,7,213,251,175,74,225,95,184,30,237,251,226,
      138,246,13,156,216,210,174,228,76,210,179,89,202,54,171,101,224,163,183,83,60,151,45,107,211,68,51,184,15,147,213,64,231,90,210,218,163,75,240,22,161,74,241,40,46,89,62,89,11,19,230,15,177,46,70,248,79,213,10,0,154,130,19,242,5,155,7,52,174,
      240,30,84,159,255,110,174,132,185,202,167,165,209,0,138,232,136,70,37,255,156,150,210,100,161,142,249,192,68,250,244,24,137,82,155,66,171,169,44,2,141,30,92,145,100,245,176,166,132,111,204,55,2,3,1,0,1,163,130,1,54,48,130,1,50,48,29,6,3,85,29,
      14,4,22,4,20,54,199,219,227,226,248,82,235,101,54,31,204,198,49,164,143,73,11,34,213,48,31,6,3,85,29,35,4,24,48,22,128,20,159,167,21,93,0,94,98,93,131,244,229,210,101,167,27,83,53,25,233,114,48,95,6,3,85,29,31,4,88,48,86,48,84,160,82,160,80,
      134,78,104,116,116,112,58,47,47,119,119,119,46,109,105,99,114,111,115,111,102,116,46,99,111,109,47,112,107,105,111,112,115,47,99,114,108,47,77,105,99,114,111,115,111,102,116,37,50,48,84,105,109,101,45,83,116,97,109,112,37,50,48,80,67,65,37,50,
      48,50,48,49,48,40,49,41,46,99,114,108,48,108,6,8,43,6,1,5,5,7,1,1,4,96,48,94,48,92,6,8,43,6,1,5,5,7,48,2,134,80,104,116,116,112,58,47,47,119,119,119,46,109,105,99,114,111,115,111,102,116,46,99,111,109,47,112,107,105,111,112,115,47,99,101,114,
      116,115,47,77,105,99,114,111,115,111,102,116,37,50,48,84,105,109,101,45,83,116,97,109,112,37,50,48,80,67,65,37,50,48,50,48,49,48,40,49,41,46,99,114,116,48,12,6,3,85,29,19,1,1,255,4,2,48,0,48,19,6,3,85,29,37,4,12,48,10,6,8,43,6,1,5,5,7,3,8,48,
      13,6,9,42,134,72,134,247,13,1,1,11,5,0,3,130,2,1,0,113,59,130,75,100,106,170,103,246,152,250,250,57,76,157,134,101,49,250,110,175,164,67,236,205,104,168,37,118,231,177,22,231,203,173,167,23,214,23,76,171,146,53,97,245,64,87,242,115,253,141,221,
      132,196,167,136,68,241,139,97,96,168,3,51,241,212,133,68,68,95,117,147,192,107,30,98,235,131,121,44,86,68,149,130,162,176,7,23,89,48,67,50,10,130,181,28,220,111,174,87,0,38,27,241,8,212,66,209,19,16,56,126,220,116,18,26,223,231,214,137,155,179,
      167,130,125,37,77,36,162,41,71,174,24,15,140,203,170,2,139,245,124,17,60,115,249,230,229,189,0,13,106,91,73,128,103,233,166,229,151,97,62,35,138,196,10,17,59,232,38,48,111,231,31,129,3,74,4,22,238,224,25,167,140,14,70,99,156,238,111,117,254,
      135,120,169,222,32,187,61,102,252,8,90,72,24,66,45,153,122,140,219,198,9,70,229,140,4,198,136,177,122,76,107,69,48,217,192,202,36,33,140,87,153,199,44,113,13,49,94,104,126,35,123,232,238,21,125,100,194,147,107,211,218,207,153,222,225,199,60,
      57,220,53,79,238,182,226,96,116,89,165,189,28,1,53,134,136,193,232,38,223,186,119,109,87,248,240,198,103,74,135,94,105,148,149,231,55,92,140,236,226,253,219,129,234,2,34,98,176,118,167,164,225,9,225,73,189,111,120,69,81,113,54,184,66,75,245,
      166,46,34,9,44,85,130,42,212,253,189,173,30,23,238,92,247,184,66,71,232,35,164,160,78,190,81,198,174,55,162,88,22,8,77,244,143,9,68,21,128,132,117,248,25,1,229,158,11,29,126,213,139,46,32,217,51,41,229,129,16,153,206,70,8,219,131,125,115,31,
      131,223,174,216,193,201,106,53,142,130,102,224,32,207,12,181,218,228,88,21,6,172,96,118,10,214,26,115,201,59,110,35,240,54,7,199,6,248,115,58,95,88,223,217,223,110,1,28,7,235,232,75,144,251,109,223,254,77,6,82,215,164,233,121,137,156,145,148,
      24,187,136,53,228,88,214,183,142,225,159,44,2,119,33,32,242,54,30,242,187,225,248,201,6,50,70,207,204,86,176,164,89,66,108,244,72,175,44,171,13,39,96,127,34,51,107,74,214,48,130,7,113,48,130,5,89,160,3,2,1,2,2,19,51,0,0,0,21,197,231,107,158,
      2,155,73,153,0,0,0,0,0,21,48,13,6,9,42,134,72,134,247,13,1,1,11,5,0,48,129,136,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,
      28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,50,48,48,6,3,85,4,3,19,41,77,105,99,114,111,115,111,102,116,32,82,111,111,116,32,67,101,114,116,105,102,105,99,97,116,101,32,65,117,116,104,
      111,114,105,116,121,32,50,48,49,48,48,30,23,13,50,49,48,57,51,48,49,56,50,50,50,53,90,23,13,51,48,48,57,51,48,49,56,51,50,50,53,90,48,124,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,
      16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,38,48,36,6,3,85,4,3,19,29,77,105,99,114,111,115,111,102,116,32,84,105,109,101,
      45,83,116,97,109,112,32,80,67,65,32,50,48,49,48,48,130,2,34,48,13,6,9,42,134,72,134,247,13,1,1,1,5,0,3,130,2,15,0,48,130,2,10,2,130,2,1,0,228,225,166,76,231,180,114,33,11,121,162,203,215,36,121,189,14,213,130,211,253,238,156,7,7,210,169,108,
      78,117,200,202,53,87,246,1,127,108,74,224,226,189,185,62,23,96,51,255,92,79,199,102,247,149,83,113,90,226,126,74,90,254,184,54,103,133,70,35,12,181,141,19,207,119,50,192,16,24,232,96,125,106,82,131,68,183,166,142,70,107,7,20,243,197,118,245,
      134,80,220,193,68,200,113,92,81,49,55,160,10,56,110,141,237,215,15,216,38,83,124,57,97,2,122,196,170,253,114,105,175,29,171,172,246,54,190,53,38,100,218,152,59,186,26,123,51,173,128,91,126,140,16,28,157,82,254,182,232,98,37,220,106,15,207,93,
      244,254,142,83,207,214,236,133,86,77,239,221,188,141,164,227,145,143,178,57,44,81,156,233,112,105,13,202,54,45,112,142,49,200,53,40,189,227,180,135,36,195,224,201,143,126,181,84,143,220,250,5,85,152,109,104,59,154,70,189,237,164,174,122,41,55,
      172,203,235,131,69,231,70,110,202,50,213,192,134,48,92,79,44,226,98,178,205,185,226,141,136,228,150,172,1,74,187,190,113,169,23,91,103,96,222,248,146,145,30,29,61,253,32,207,115,125,65,154,70,117,205,196,95,52,221,18,137,214,253,165,32,125,126,
      252,217,158,69,223,182,114,47,219,125,95,128,186,219,170,126,54,236,54,76,246,43,110,168,18,81,232,191,5,3,163,209,115,166,77,55,116,148,28,52,130,15,240,16,242,183,71,24,237,167,232,153,124,63,76,219,175,94,194,243,213,216,115,61,67,78,193,
      51,57,76,142,2,188,66,104,46,16,234,132,81,70,226,209,189,106,24,90,97,1,115,202,103,162,94,215,40,118,2,226,51,24,114,215,167,32,240,194,250,18,10,215,99,111,12,201,54,100,139,91,160,166,131,33,93,95,48,116,145,148,148,216,185,80,249,11,137,
      97,243,54,6,53,24,132,71,219,220,27,209,253,178,212,28,197,107,246,92,82,81,93,18,219,37,186,175,80,5,122,108,197,17,29,114,239,141,249,82,196,133,23,147,192,60,21,219,26,55,199,8,21,24,63,120,171,69,182,245,30,135,94,218,143,158,22,114,105,
      198,174,123,183,183,62,106,226,46,173,2,3,1,0,1,163,130,1,221,48,130,1,217,48,18,6,9,43,6,1,4,1,130,55,21,1,4,5,2,3,1,0,1,48,35,6,9,43,6,1,4,1,130,55,21,2,4,22,4,20,42,167,82,254,100,196,154,190,130,145,60,70,53,41,207,16,255,47,4,238,48,29,
      6,3,85,29,14,4,22,4,20,159,167,21,93,0,94,98,93,131,244,229,210,101,167,27,83,53,25,233,114,48,92,6,3,85,29,32,4,85,48,83,48,81,6,12,43,6,1,4,1,130,55,76,131,125,1,1,48,65,48,63,6,8,43,6,1,5,5,7,2,1,22,51,104,116,116,112,58,47,47,119,119,119,
      46,109,105,99,114,111,115,111,102,116,46,99,111,109,47,112,107,105,111,112,115,47,68,111,99,115,47,82,101,112,111,115,105,116,111,114,121,46,104,116,109,48,19,6,3,85,29,37,4,12,48,10,6,8,43,6,1,5,5,7,3,8,48,25,6,9,43,6,1,4,1,130,55,20,2,4,12,
      30,10,0,83,0,117,0,98,0,67,0,65,48,11,6,3,85,29,15,4,4,3,2,1,134,48,15,6,3,85,29,19,1,1,255,4,5,48,3,1,1,255,48,31,6,3,85,29,35,4,24,48,22,128,20,213,246,86,203,143,232,162,92,98,104,209,61,148,144,91,215,206,154,24,196,48,86,6,3,85,29,31,4,
      79,48,77,48,75,160,73,160,71,134,69,104,116,116,112,58,47,47,99,114,108,46,109,105,99,114,111,115,111,102,116,46,99,111,109,47,112,107,105,47,99,114,108,47,112,114,111,100,117,99,116,115,47,77,105,99,82,111,111,67,101,114,65,117,116,95,50,48,
      49,48,45,48,54,45,50,51,46,99,114,108,48,90,6,8,43,6,1,5,5,7,1,1,4,78,48,76,48,74,6,8,43,6,1,5,5,7,48,2,134,62,104,116,116,112,58,47,47,119,119,119,46,109,105,99,114,111,115,111,102,116,46,99,111,109,47,112,107,105,47,99,101,114,116,115,47,77,
      105,99,82,111,111,67,101,114,65,117,116,95,50,48,49,48,45,48,54,45,50,51,46,99,114,116,48,13,6,9,42,134,72,134,247,13,1,1,11,5,0,3,130,2,1,0,157,85,125,252,42,173,225,44,31,103,1,49,36,91,225,158,114,75,252,169,111,234,92,20,182,62,78,71,100,
      120,177,6,147,151,61,49,51,181,57,215,194,113,54,63,218,100,108,124,208,117,57,109,187,15,49,228,194,143,251,108,209,161,148,24,34,238,233,102,103,58,83,77,221,152,186,182,30,120,216,54,46,156,169,130,86,0,3,176,5,190,137,232,105,224,186,9,238,
      123,223,106,111,190,41,203,110,216,63,72,117,1,217,24,222,109,130,12,245,109,35,84,228,120,83,117,36,87,185,221,159,243,142,61,198,243,104,223,101,246,164,86,170,247,149,182,40,85,39,208,36,189,64,160,191,25,182,18,18,17,93,61,39,224,64,150,
      56,172,247,249,41,137,195,188,23,176,84,133,66,179,252,12,158,139,25,137,231,240,11,106,129,194,129,25,66,25,82,117,138,54,194,29,195,97,115,46,44,107,123,110,63,44,9,120,20,233,145,178,169,91,223,73,163,116,12,188,236,145,128,210,61,230,74,
      62,102,59,79,187,134,250,50,26,217,150,244,143,246,145,1,246,206,198,116,253,246,76,114,111,16,171,117,48,197,52,176,122,216,80,254,10,88,221,64,60,199,84,109,157,99,116,72,44,177,78,71,45,193,20,4,113,191,100,249,36,190,115,109,202,142,9,189,
      179,1,87,73,84,100,217,115,215,127,30,91,68,1,142,90,25,145,107,13,159,164,40,220,103,25,40,36,186,56,75,154,110,251,33,84,107,106,69,17,71,169,241,183,174,200,232,137,94,79,157,210,208,76,118,181,87,84,9,177,105,1,68,126,124,161,97,108,115,
      254,10,187,236,65,102,61,105,253,203,193,65,73,126,126,147,190,203,248,59,228,183,21,191,180,206,62,165,49,81,132,188,191,2,193,130,162,123,23,29,21,137,141,112,254,231,181,208,40,26,137,11,143,54,218,186,76,249,155,255,10,233,52,248,36,53,103,
      43,224,13,184,230,140,153,214,225,34,234,240,39,66,61,37,148,230,116,116,91,106,209,158,62,237,126,160,49,51,125,188,203,233,123,191,56,112,68,209,144,241,200,171,58,138,58,8,98,127,217,112,99,83,77,141,238,130,109,165,5,16,193,113,6,106,16,
      180,29,85,51,88,179,161,112,102,242,161,130,2,210,48,130,2,59,2,1,1,48,129,252,161,129,212,164,129,209,48,129,206,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,
      82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,41,48,39,6,3,85,4,11,19,32,77,105,99,114,111,115,111,102,116,32,79,112,101,114,97,116,105,111,110,115,
      32,80,117,101,114,116,111,32,82,105,99,111,49,38,48,36,6,3,85,4,11,19,29,84,104,97,108,101,115,32,84,83,83,32,69,83,78,58,55,56,56,48,45,69,51,57,48,45,56,48,49,52,49,37,48,35,6,3,85,4,3,19,28,77,105,99,114,111,115,111,102,116,32,84,105,109,
      101,45,83,116,97,109,112,32,83,101,114,118,105,99,101,162,35,10,1,1,48,7,6,5,43,14,3,2,26,3,21,0,108,186,252,196,159,65,7,138,203,225,136,57,241,125,75,103,152,144,119,44,160,129,131,48,129,128,164,126,48,124,49,11,48,9,6,3,85,4,6,19,2,85,83,
      49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,38,48,36,
      6,3,85,4,3,19,29,77,105,99,114,111,115,111,102,116,32,84,105,109,101,45,83,116,97,109,112,32,80,67,65,32,50,48,49,48,48,13,6,9,42,134,72,134,247,13,1,1,5,5,0,2,5,0,230,101,145,101,48,34,24,15,50,48,50,50,48,54,50,56,49,56,52,48,51,55,90,24,15,
      50,48,50,50,48,54,50,57,49,56,52,48,51,55,90,48,119,48,61,6,10,43,6,1,4,1,132,89,10,4,1,49,47,48,45,48,10,2,5,0,230,101,145,101,2,1,0,48,10,2,1,0,2,2,4,120,2,1,255,48,7,2,1,0,2,2,19,161,48,10,2,5,0,230,102,226,229,2,1,0,48,54,6,10,43,6,1,4,1,
      132,89,10,4,2,49,40,48,38,48,12,6,10,43,6,1,4,1,132,89,10,3,2,160,10,48,8,2,1,0,2,3,7,161,32,161,10,48,8,2,1,0,2,3,1,134,160,48,13,6,9,42,134,72,134,247,13,1,1,5,5,0,3,129,129,0,134,119,92,194,21,139,46,114,99,8,157,62,92,178,230,175,124,156,
      65,7,115,208,191,239,113,68,27,208,236,82,181,115,106,230,149,126,109,204,238,78,237,237,171,48,197,134,1,165,204,254,75,78,197,216,82,206,255,75,147,103,149,53,20,142,124,16,148,73,251,162,37,174,105,130,215,147,195,227,127,71,235,230,39,132,
      45,209,243,161,49,102,113,154,86,83,188,174,160,121,110,115,121,226,72,112,116,129,255,103,151,6,227,65,66,111,210,77,14,83,136,201,100,152,214,185,66,178,112,15,49,130,4,13,48,130,4,9,2,1,1,48,129,147,48,124,49,11,48,9,6,3,85,4,6,19,2,85,83,
      49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,38,48,36,
      6,3,85,4,3,19,29,77,105,99,114,111,115,111,102,116,32,84,105,109,101,45,83,116,97,109,112,32,80,67,65,32,50,48,49,48,2,19,51,0,0,1,168,85,240,97,169,204,48,24,92,0,1,0,0,1,168,48,13,6,9,96,134,72,1,101,3,4,2,1,5,0,160,130,1,74,48,26,6,9,42,134,
      72,134,247,13,1,9,3,49,13,6,11,42,134,72,134,247,13,1,9,16,1,4,48,47,6,9,42,134,72,134,247,13,1,9,4,49,34,4,32,232,0,244,180,104,205,75,56,65,234,215,47,156,78,240,48,226,46,176,252,201,82,20,225,104,156,201,59,114,177,41,233,48,129,250,6,11,
      42,134,72,134,247,13,1,9,16,2,47,49,129,234,48,129,231,48,129,228,48,129,189,4,32,116,254,203,29,0,203,7,194,115,112,133,241,65,92,249,69,157,27,214,132,122,146,95,214,11,83,16,67,151,92,101,166,48,129,152,48,129,128,164,126,48,124,49,11,48,
      9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,
      105,111,110,49,38,48,36,6,3,85,4,3,19,29,77,105,99,114,111,115,111,102,116,32,84,105,109,101,45,83,116,97,109,112,32,80,67,65,32,50,48,49,48,2,19,51,0,0,1,168,85,240,97,169,204,48,24,92,0,1,0,0,1,168,48,34,4,32,163,3,157,98,78,15,245,111,7,225,
      69,98,100,163,161,234,107,174,93,245,37,230,231,24,2,243,49,238,56,248,168,134,48,13,6,9,42,134,72,134,247,13,1,1,11,5,0,4,130,2,0,62,109,249,137,14,120,151,222,75,227,110,168,239,222,120,31,159,130,33,43,2,89,71,57,225,1,195,83,11,139,70,35,
      169,5,181,140,140,245,121,50,66,94,249,178,55,165,156,71,27,238,135,194,79,196,8,71,81,39,192,165,197,108,163,126,219,118,22,93,78,210,28,58,143,174,232,134,132,182,92,184,152,21,66,181,91,81,177,109,12,81,252,192,109,132,199,36,221,218,86,100,
      122,204,17,43,243,109,195,242,146,155,218,45,15,238,95,165,215,143,194,12,214,237,50,179,63,214,167,177,112,181,209,97,30,108,213,217,251,219,206,121,181,1,34,222,212,65,227,184,176,227,13,111,115,196,150,9,192,249,229,250,134,5,10,22,96,243,
      76,80,214,95,36,243,148,214,153,56,52,94,74,80,90,102,54,244,105,142,70,124,133,22,71,98,31,110,159,153,158,171,104,120,104,245,253,88,17,61,241,184,42,0,224,27,223,61,141,178,146,30,204,0,49,184,236,38,210,109,230,62,240,126,58,254,186,190,
      245,111,187,186,88,112,60,232,59,63,119,109,158,215,166,114,162,180,177,31,95,128,187,218,209,42,193,22,227,13,31,78,208,200,3,99,238,87,128,116,95,15,235,207,81,199,171,60,72,213,143,100,113,228,84,92,14,215,91,182,118,163,50,243,137,61,214,
      182,41,79,7,107,127,22,103,139,105,58,164,5,69,88,169,26,231,143,152,132,99,155,245,223,224,88,70,134,247,89,87,247,213,179,3,91,88,117,25,150,92,50,137,233,40,155,180,54,24,147,219,49,149,211,191,219,37,52,215,95,73,190,183,56,106,86,112,1,
      164,5,21,245,198,129,62,212,57,182,42,62,122,184,217,121,12,188,74,160,157,150,197,243,175,251,52,198,202,240,181,195,49,35,8,93,217,99,76,222,139,178,137,21,30,31,115,254,211,70,141,73,79,128,150,166,85,87,197,14,232,12,106,152,25,206,216,27,
      210,156,178,50,158,44,110,193,67,196,199,219,165,166,46,53,55,52,130,112,162,253,60,101,125,48,12,97,216,93,73,206,125,87,146,83,130,28,183,0,155,56,176,128,9,101,0,9,192,171,230,252,131,243,9,94,147,71,116,12,185,48,69,20,122,247,214,21,73,
      66,191,185,96,110,0
    };
   #else
    static constexpr unsigned char dllData[135056] = {
      77,90,120,0,1,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,120,0,0,0,14,31,186,14,0,180,9,205,33,184,1,76,205,33,84,104,105,115,32,112,114,111,103,114,97,109,32,99,97,110,110,
      111,116,32,98,101,32,114,117,110,32,105,110,32,68,79,83,32,109,111,100,101,46,36,0,0,80,69,0,0,100,134,9,0,183,180,15,95,0,0,0,0,0,0,0,0,240,0,34,32,11,2,14,0,0,16,1,0,0,218,0,0,0,0,0,0,220,54,0,0,0,16,0,0,0,0,0,128,1,0,0,0,0,16,0,0,0,2,0,0,
      5,0,2,0,0,0,0,0,5,0,2,0,0,0,0,0,0,96,2,0,0,4,0,0,47,164,2,0,3,0,96,1,0,0,16,0,0,0,0,0,0,16,0,0,0,0,0,0,0,0,16,0,0,0,0,0,0,16,0,0,0,0,0,0,0,0,0,0,16,0,0,0,71,166,1,0,242,0,0,0,57,167,1,0,40,0,0,0,0,64,2,0,128,5,0,0,0,240,1,0,192,18,0,0,0,238,
      1,0,144,33,0,0,0,80,2,0,156,6,0,0,100,163,1,0,28,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,200,160,1,0,40,0,0,0,192,33,1,0,48,1,0,0,0,0,0,0,0,0,0,0,240,169,1,0,136,2,0,0,56,164,1,0,160,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,116,101,120,116,0,0,
      0,122,15,1,0,0,16,0,0,0,16,1,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,0,96,46,114,100,97,116,97,0,0,124,164,0,0,0,32,1,0,0,166,0,0,0,20,1,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,0,64,46,100,97,116,97,0,0,0,148,29,0,0,0,208,1,0,0,12,0,0,0,186,1,0,0,0,0,
      0,0,0,0,0,0,0,0,0,64,0,0,192,46,112,100,97,116,97,0,0,192,18,0,0,0,240,1,0,0,20,0,0,0,198,1,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,0,64,46,48,48,99,102,103,0,0,40,0,0,0,0,16,2,0,0,2,0,0,0,218,1,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,0,64,46,116,108,115,0,0,
      0,0,9,0,0,0,0,32,2,0,0,2,0,0,0,220,1,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,0,192,95,82,68,65,84,65,0,0,148,0,0,0,0,48,2,0,0,2,0,0,0,222,1,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,0,64,46,114,115,114,99,0,0,0,128,5,0,0,0,64,2,0,0,6,0,0,0,224,1,0,0,0,0,0,0,0,0,
      0,0,0,0,0,64,0,0,64,46,114,101,108,111,99,0,0,156,6,0,0,0,80,2,0,0,8,0,0,0,230,1,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,0,66,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,184,1,0,0,0,195,204,204,86,72,131,236,32,72,137,
      206,72,141,21,131,139,1,0,65,184,1,0,0,0,232,240,22,0,0,72,141,21,7,137,1,0,65,184,40,0,0,0,72,137,241,232,219,22,0,0,72,139,78,16,255,21,111,154,1,0,131,248,255,15,149,192,72,131,196,32,94,195,86,87,83,72,131,236,32,72,137,214,72,137,207,186,
      4,1,0,0,72,137,241,232,41,21,0,0,68,139,70,8,72,139,86,16,72,137,249,255,21,80,154,1,0,137,195,137,192,72,57,70,8,117,43,255,21,56,154,1,0,131,248,122,117,32,186,0,16,0,0,72,137,241,232,246,20,0,0,68,139,70,8,72,139,86,16,72,137,249,255,21,29,
      154,1,0,137,195,133,219,116,20,137,218,72,57,86,8,118,12,72,137,241,232,1,22,0,0,49,192,235,21,255,21,245,153,1,0,137,193,15,183,193,13,0,0,7,128,133,201,15,78,193,72,131,196,32,91,95,94,195,86,87,83,72,131,236,64,72,137,214,72,137,207,72,139,
      5,50,191,1,0,72,49,224,72,137,68,36,56,186,4,1,0,0,72,137,241,232,133,20,0,0,72,137,241,72,137,250,232,6,21,0,0,72,131,62,3,114,53,72,139,70,16,15,183,72,2,102,131,249,58,117,27,102,131,120,4,92,117,32,15,183,0,131,224,223,131,192,191,102,131,
      248,26,115,17,233,128,0,0,0,102,131,249,92,117,6,102,131,56,92,116,116,15,87,192,72,141,84,36,32,15,41,2,72,199,66,16,0,0,0,0,49,201,232,226,254,255,255,133,192,120,123,76,139,68,36,32,72,139,84,36,48,72,137,241,232,205,20,0,0,72,139,76,36,48,
      102,186,92,0,232,129,60,0,0,72,133,192,116,90,72,141,92,36,32,72,139,83,16,72,41,208,72,131,192,2,72,209,248,72,137,241,73,137,192,232,156,20,0,0,72,137,241,72,137,250,232,35,21,0,0,72,137,217,232,147,19,0,0,72,137,241,232,59,254,255,255,49,
      201,132,192,190,2,0,7,128,15,69,241,72,139,76,36,56,72,49,225,232,218,125,0,0,137,240,235,34,137,195,235,5,187,5,64,0,128,72,141,76,36,32,232,91,19,0,0,72,139,76,36,56,72,49,225,232,182,125,0,0,137,216,72,131,196,64,91,95,94,195,65,86,86,87,
      83,72,129,236,216,0,0,0,72,139,5,1,190,1,0,72,49,224,72,137,132,36,208,0,0,0,77,133,201,15,132,220,0,0,0,76,137,206,72,141,132,36,144,0,0,0,199,64,24,0,0,0,0,72,137,8,72,137,80,8,76,137,64,16,15,87,192,72,141,84,36,64,15,41,66,48,15,41,66,32,
      15,41,66,16,15,41,2,72,199,66,64,0,0,0,0,72,137,193,232,145,13,0,0,72,141,21,122,13,1,0,185,64,0,0,0,232,8,22,0,0,72,133,192,15,132,133,0,0,0,72,137,199,15,16,132,36,144,0,0,0,15,16,140,36,160,0,0,0,15,41,140,36,192,0,0,0,15,41,132,36,176,0,
      0,0,199,64,12,1,0,0,0,72,141,5,22,134,1,0,72,137,7,72,139,13,172,218,1,0,72,133,201,116,6,72,139,1,255,80,8,72,141,5,202,133,1,0,72,137,7,15,40,132,36,176,0,0,0,15,40,140,36,192,0,0,0,15,17,71,16,15,17,79,32,72,137,119,48,72,139,6,72,137,241,
      255,80,8,199,71,56,1,0,0,0,235,9,190,3,64,0,128,235,90,49,255,76,141,116,36,88,72,141,92,36,112,15,16,132,36,144,0,0,0,15,16,140,36,160,0,0,0,72,141,76,36,32,15,41,1,15,41,73,16,72,137,250,232,73,0,0,0,137,198,72,133,255,116,9,72,139,7,72,137,
      249,255,80,16,72,137,217,232,242,17,0,0,76,137,241,232,234,17,0,0,72,141,76,36,64,232,224,17,0,0,72,139,140,36,208,0,0,0,72,49,225,232,56,124,0,0,137,240,72,129,196,216,0,0,0,91,95,94,65,94,195,86,87,85,83,72,131,236,88,72,137,214,72,137,207,
      72,139,5,124,188,1,0,72,49,224,72,137,68,36,80,15,87,192,15,41,68,36,48,72,199,68,36,64,0,0,0,0,72,139,9,72,133,201,116,25,102,131,57,0,116,19,72,141,84,36,48,232,5,253,255,255,137,197,187,1,0,0,0,235,23,139,79,24,49,219,72,141,84,36,48,69,49,
      192,69,49,201,232,238,2,0,0,137,197,133,237,117,35,76,139,79,8,72,139,71,16,72,139,76,36,64,72,137,116,36,40,72,137,68,36,32,178,1,65,137,216,232,14,20,0,0,137,197,72,141,76,36,48,232,42,17,0,0,72,139,76,36,80,72,49,225,232,133,123,0,0,137,232,
      72,131,196,88,91,93,95,94,195,65,86,86,87,85,83,72,129,236,64,1,0,0,15,41,180,36,48,1,0,0,72,139,5,198,187,1,0,72,49,224,72,137,132,36,40,1,0,0,72,133,210,15,132,184,0,0,0,72,137,211,15,87,246,72,141,180,36,208,0,0,0,15,17,118,8,199,70,24,0,
      0,0,0,72,137,14,76,141,180,36,152,0,0,0,72,141,188,36,176,0,0,0,49,237,72,137,111,16,15,41,55,15,41,119,240,15,41,119,224,15,41,119,208,72,141,148,36,128,0,0,0,72,137,241,232,73,11,0,0,72,137,108,36,112,15,41,116,36,96,72,137,108,36,80,15,41,
      116,36,64,72,137,108,36,48,15,41,116,36,32,72,139,14,72,133,201,116,28,102,131,57,0,116,22,72,141,84,36,96,232,229,251,255,255,133,192,15,132,222,0,0,0,137,197,235,33,139,140,36,232,0,0,0,72,141,84,36,96,76,141,68,36,64,76,141,76,36,32,232,197,
      1,0,0,137,197,133,237,116,14,49,192,235,79,189,3,64,0,128,233,131,0,0,0,72,131,124,36,32,0,116,44,72,141,21,167,134,1,0,72,141,116,36,64,65,184,1,0,0,0,72,137,241,232,184,17,0,0,76,139,68,36,32,72,139,84,36,48,72,137,241,232,166,17,0,0,72,139,
      84,36,64,72,139,76,36,80,232,13,18,0,0,49,237,72,137,3,72,141,76,36,32,232,206,15,0,0,72,141,76,36,64,232,196,15,0,0,72,141,76,36,96,232,186,15,0,0,72,137,249,232,178,15,0,0,76,137,241,232,170,15,0,0,72,141,140,36,128,0,0,0,232,157,15,0,0,72,
      139,140,36,40,1,0,0,72,49,225,232,245,121,0,0,137,232,15,40,180,36,48,1,0,0,72,129,196,64,1,0,0,91,93,95,94,65,94,195,72,139,108,36,112,72,141,148,36,36,1,0,0,199,2,0,0,0,0,72,137,233,232,130,7,1,0,133,192,15,132,202,0,0,0,137,198,15,87,192,
      72,141,140,36,0,1,0,0,15,41,1,72,199,65,16,0,0,0,0,137,194,209,234,255,194,232,103,15,0,0,132,192,116,125,76,139,140,36,16,1,0,0,139,148,36,36,1,0,0,72,137,233,65,137,240,232,73,7,1,0,133,192,116,95,76,141,132,36,248,0,0,0,73,199,0,0,0,0,0,76,
      141,140,36,244,0,0,0,65,199,1,0,0,0,0,72,139,140,36,16,1,0,0,72,141,21,38,133,1,0,232,35,7,1,0,133,192,116,41,72,139,180,36,248,0,0,0,72,133,246,116,28,72,137,241,232,202,76,0,0,72,141,76,36,64,72,137,242,73,137,192,232,168,15,0,0,49,237,235,
      20,255,21,16,148,1,0,15,183,232,129,205,0,0,7,128,133,192,15,78,232,72,141,140,36,0,1,0,0,232,141,14,0,0,233,86,254,255,255,255,21,234,147,1,0,15,183,232,129,205,0,0,7,128,133,192,15,78,232,233,61,254,255,255,65,87,65,86,65,85,65,84,86,87,85,
      83,72,129,236,200,0,0,0,102,15,127,188,36,176,0,0,0,102,15,127,180,36,160,0,0,0,76,137,76,36,40,76,137,195,73,137,212,137,76,36,36,72,139,5,4,185,1,0,72,49,224,72,137,132,36,152,0,0,0,102,15,239,246,72,141,116,36,48,69,49,255,184,4,0,0,0,68,
      41,248,131,124,36,36,1,65,15,69,199,72,141,13,159,132,1,0,72,137,76,36,112,72,141,13,241,132,1,0,72,137,76,36,120,72,141,13,63,133,1,0,72,137,140,36,128,0,0,0,72,141,13,136,133,1,0,72,137,140,36,136,0,0,0,72,141,13,215,133,1,0,72,137,140,36,
      144,0,0,0,72,99,232,76,139,108,236,112,102,15,127,116,36,48,72,199,68,36,64,0,0,0,0,72,141,5,211,129,1,0,72,137,68,36,112,72,141,5,21,130,1,0,72,137,68,36,120,72,141,5,87,130,1,0,72,137,132,36,128,0,0,0,72,141,5,150,130,1,0,72,137,132,36,136,
      0,0,0,72,141,5,213,130,1,0,72,137,132,36,144,0,0,0,72,137,223,76,137,227,76,139,100,236,112,76,137,225,232,96,75,0,0,73,137,198,72,141,80,42,72,137,241,232,129,13,0,0,65,184,42,0,0,0,72,137,241,72,141,21,241,130,1,0,232,42,14,0,0,72,137,241,
      76,137,226,73,137,220,72,137,251,77,137,240,232,218,14,0,0,72,139,76,36,64,49,210,73,137,248,77,137,225,232,117,4,0,0,132,192,15,133,120,2,0,0,72,139,76,36,64,178,1,73,137,216,77,137,225,232,91,4,0,0,132,192,15,133,94,2,0,0,139,5,30,213,1,0,
      139,13,92,195,1,0,101,72,139,20,37,88,0,0,0,72,139,12,202,59,129,4,0,0,0,15,143,235,1,0,0,72,139,5,240,212,1,0,72,133,192,15,132,130,0,0,0,199,68,36,104,0,0,0,0,185,1,0,0,0,72,141,84,36,104,69,49,192,76,141,76,36,108,255,208,131,248,122,117,
      97,139,84,36,104,133,210,116,89,102,15,127,116,36,112,72,199,132,36,128,0,0,0,0,0,0,0,72,209,234,72,255,194,72,141,116,36,112,72,137,241,232,161,12,0,0,132,192,116,40,76,139,132,36,128,0,0,0,185,1,0,0,0,72,141,84,36,104,76,141,76,36,108,255,
      21,120,212,1,0,133,192,116,46,72,141,76,36,112,235,3,72,137,241,232,53,12,0,0,72,141,116,36,48,72,137,241,232,40,12,0,0,65,255,199,65,131,255,5,15,133,254,253,255,255,233,239,1,0,0,131,124,36,108,0,116,42,72,139,156,36,128,0,0,0,72,131,195,40,
      49,246,72,139,75,240,76,137,234,232,117,71,0,0,133,192,116,27,255,198,72,131,195,80,59,116,36,108,114,228,72,141,76,36,112,232,219,11,0,0,72,137,251,235,161,243,15,126,3,242,15,112,248,27,102,15,97,254,102,15,127,124,36,80,72,139,83,224,76,137,
      225,232,123,12,0,0,72,141,76,36,112,232,173,11,0,0,72,141,76,36,80,76,137,226,232,56,4,0,0,72,137,251,72,133,255,15,132,174,0,0,0,132,192,15,132,166,0,0,0,186,15,0,0,0,72,137,217,232,183,11,0,0,102,65,15,126,253,65,184,11,0,0,0,68,137,233,72,
      141,116,36,112,72,137,242,65,185,10,0,0,0,232,138,69,0,0,133,192,15,133,33,255,255,255,72,137,241,232,86,73,0,0,72,137,217,72,137,242,73,137,192,232,54,12,0,0,65,190,3,0,0,0,72,141,61,17,132,1,0,65,184,11,0,0,0,68,137,233,72,137,242,65,185,10,
      0,0,0,232,72,69,0,0,133,192,15,133,223,254,255,255,65,184,1,0,0,0,72,137,217,72,137,250,232,189,12,0,0,72,137,241,232,3,73,0,0,72,137,217,72,137,242,73,137,192,232,167,12,0,0,65,255,206,117,181,235,88,132,192,15,132,169,254,255,255,235,78,72,
      141,13,13,211,1,0,232,12,15,0,0,131,61,1,211,1,0,255,15,133,252,253,255,255,72,141,13,180,131,1,0,255,21,54,144,1,0,72,137,193,72,141,21,142,131,1,0,255,21,54,144,1,0,72,137,5,207,210,1,0,72,141,13,208,210,1,0,232,55,15,0,0,233,199,253,255,255,
      72,139,76,36,40,72,133,201,116,79,72,141,5,139,128,1,0,72,137,68,36,112,72,141,5,129,128,1,0,72,137,68,36,120,72,141,5,127,128,1,0,72,137,132,36,128,0,0,0,72,141,5,120,128,1,0,72,137,132,36,136,0,0,0,72,141,5,119,128,1,0,72,137,132,36,144,0,
      0,0,72,139,84,236,112,232,249,10,0,0,72,141,76,36,48,232,43,10,0,0,49,246,235,5,190,2,0,7,128,72,139,140,36,152,0,0,0,72,49,225,232,122,116,0,0,137,240,15,40,180,36,160,0,0,0,15,40,188,36,176,0,0,0,72,129,196,200,0,0,0,91,93,95,94,65,92,65,93,
      65,94,65,95,195,86,87,83,72,131,236,80,72,139,5,174,180,1,0,72,49,224,72,137,68,36,72,77,133,192,116,84,191,87,0,7,128,72,133,201,116,79,72,137,211,72,133,210,116,71,76,137,198,72,141,84,36,48,232,99,0,0,0,191,87,0,7,128,132,192,116,49,72,141,
      84,36,32,72,137,217,232,77,0,0,0,132,192,116,32,49,192,139,76,132,32,57,76,132,48,119,43,114,48,72,255,192,72,131,248,4,117,235,49,192,235,40,191,3,64,0,128,72,139,76,36,72,72,49,225,232,211,115,0,0,137,248,72,131,196,80,91,95,94,195,184,1,0,
      0,0,235,5,184,255,255,255,255,137,6,49,255,235,215,65,87,65,86,86,87,83,72,131,236,48,73,137,215,72,137,203,72,139,5,7,180,1,0,72,49,224,72,137,68,36,40,76,141,116,36,32,73,199,6,0,0,0,0,64,182,1,49,255,64,246,198,1,117,10,65,199,4,191,0,0,0,
      0,235,63,72,137,217,76,137,242,65,184,10,0,0,0,232,9,74,0,0,65,137,4,191,72,139,68,36,32,72,57,216,116,20,72,133,192,116,15,102,131,56,46,117,9,72,131,192,2,72,137,195,235,12,49,246,72,133,255,117,5,72,57,216,116,12,72,255,199,72,131,255,4,117,
      168,64,182,1,72,139,76,36,40,72,49,225,232,30,115,0,0,137,240,72,131,196,48,91,95,94,65,94,65,95,195,73,137,201,49,201,49,210,69,49,192,233,85,245,255,255,86,87,85,83,72,129,236,104,2,0,0,76,137,206,76,137,199,72,137,200,72,139,13,78,179,1,0,
      72,49,225,72,137,140,36,96,2,0,0,199,68,36,60,4,1,0,0,15,182,210,72,199,193,2,0,0,128,72,41,209,72,141,84,36,48,72,137,84,36,32,49,219,72,137,194,69,49,192,65,185,25,2,2,0,255,21,232,189,1,0,133,192,15,133,178,0,0,0,72,139,76,36,48,72,141,68,
      36,60,72,137,68,36,40,72,141,68,36,80,72,137,68,36,32,72,141,21,208,123,1,0,49,219,69,49,192,69,49,201,255,21,186,189,1,0,137,197,72,139,76,36,48,255,21,149,189,1,0,133,237,117,115,72,141,92,36,80,72,137,217,232,244,69,0,0,72,137,241,72,137,
      218,73,137,192,232,212,8,0,0,72,139,78,16,102,186,92,0,232,137,48,0,0,72,133,192,116,68,72,137,195,72,131,195,2,72,141,84,36,64,72,137,217,232,105,254,255,255,132,192,116,44,72,133,255,116,22,72,137,217,232,175,69,0,0,72,137,249,72,137,218,73,
      137,192,232,143,8,0,0,72,141,76,36,64,72,137,242,232,36,0,0,0,137,195,235,2,49,219,72,139,140,36,96,2,0,0,72,49,225,232,222,113,0,0,137,216,72,129,196,104,2,0,0,91,93,95,94,195,86,87,72,131,236,40,72,137,215,49,192,72,141,21,206,127,1,0,139,
      52,16,57,52,1,119,12,114,24,72,131,192,4,72,131,248,16,117,236,72,137,249,72,131,196,40,95,94,233,228,241,255,255,72,141,13,181,127,1,0,72,139,53,110,141,1,0,255,214,72,139,79,16,255,214,72,141,13,241,127,1,0,255,214,49,192,72,131,196,40,95,
      94,195,72,131,236,40,73,199,0,0,0,0,0,68,139,10,184,2,64,0,128,69,133,201,116,14,65,129,249,206,152,79,139,116,31,72,131,196,40,195,131,122,4,0,117,245,129,122,8,192,0,0,0,117,236,129,122,12,0,0,0,70,117,227,235,27,129,122,4,13,219,113,78,117,
      216,129,122,8,133,253,196,196,117,207,129,122,12,239,31,38,48,117,198,73,137,8,72,139,1,255,80,8,49,192,235,185,184,1,0,0,0,240,15,193,65,12,255,192,195,204,86,72,131,236,32,190,255,255,255,255,240,15,193,113,12,255,206,117,34,72,133,201,116,
      11,72,139,1,186,1,0,0,0,255,80,32,72,139,13,148,206,1,0,72,133,201,116,6,72,139,1,255,80,16,137,240,72,131,196,32,94,195,204,86,87,72,131,236,88,72,137,206,72,139,5,24,177,1,0,72,49,224,72,137,68,36,80,133,210,120,44,72,199,68,36,40,0,0,0,0,
      77,133,192,116,91,73,139,0,72,141,21,60,127,1,0,72,141,124,36,40,76,137,193,73,137,248,255,16,137,194,76,139,7,235,64,139,70,56,133,192,126,205,255,200,137,70,56,15,16,70,16,15,16,78,32,72,141,76,36,48,15,41,73,16,15,41,1,72,137,242,232,39,244,
      255,255,133,192,121,55,72,139,78,48,72,139,57,137,194,69,49,192,255,87,24,235,38,69,49,192,72,139,78,48,72,139,1,255,80,24,72,139,76,36,40,72,133,201,116,15,72,199,68,36,40,0,0,0,0,72,139,1,255,80,16,72,139,76,36,80,72,49,225,232,8,112,0,0,49,
      192,72,131,196,88,95,94,195,204,86,87,72,131,236,40,137,215,72,137,206,72,141,5,220,120,1,0,72,137,1,72,139,73,48,72,133,201,116,14,72,199,70,48,0,0,0,0,72,139,1,255,80,16,199,70,12,1,0,0,192,133,255,116,8,72,137,241,232,171,8,0,0,72,137,240,
      72,131,196,40,95,94,195,204,15,11,204,204,86,87,83,72,131,236,96,72,137,215,72,137,206,72,139,5,4,176,1,0,72,49,224,72,137,68,36,88,49,219,72,137,92,36,32,198,68,36,40,1,72,141,13,173,126,1,0,72,141,21,118,126,1,0,73,137,240,73,137,249,232,124,
      0,0,0,76,141,79,24,76,141,70,8,136,92,36,40,72,137,92,36,32,72,141,13,231,126,1,0,72,141,21,194,126,1,0,232,88,0,0,0,76,141,68,36,80,73,137,24,72,141,124,36,48,72,137,95,16,15,87,192,15,41,7,72,131,198,24,136,92,36,40,72,137,116,36,32,72,141,
      13,22,127,1,0,72,141,21,221,126,1,0,73,137,249,232,30,0,0,0,72,137,249,232,161,4,0,0,72,139,76,36,88,72,49,225,232,252,110,0,0,144,72,131,196,96,91,95,94,195,65,87,65,86,86,87,83,72,131,236,48,76,137,206,77,137,199,73,137,214,138,156,36,136,
      0,0,0,72,139,188,36,128,0,0,0,132,219,116,7,198,5,233,184,1,0,1,72,137,242,232,213,6,0,0,132,192,116,37,72,139,78,16,73,137,15,72,133,255,116,75,49,210,65,184,10,0,0,0,232,68,69,0,0,49,201,131,248,1,15,148,193,137,15,235,50,128,61,177,184,1,
      0,0,117,4,132,219,116,37,198,5,164,184,1,0,0,72,137,124,36,32,72,199,193,2,0,0,128,76,137,242,77,137,248,73,137,241,232,48,0,0,0,132,192,116,12,72,131,196,48,91,95,94,65,94,65,95,195,72,199,193,1,0,0,128,76,137,242,77,137,248,73,137,241,72,131,
      196,48,91,95,94,65,94,65,95,233,0,0,0,0,65,87,65,86,65,84,86,87,83,72,129,236,152,0,0,0,77,137,204,77,137,198,73,137,215,72,137,206,72,139,5,123,174,1,0,72,49,224,72,137,132,36,144,0,0,0,15,87,192,15,41,68,36,80,72,199,68,36,96,0,0,0,0,72,141,
      13,210,124,1,0,255,21,2,137,1,0,72,141,21,163,124,1,0,72,137,193,255,21,2,137,1,0,72,133,192,116,41,72,137,195,72,141,124,36,80,186,131,0,0,0,72,137,249,232,144,3,0,0,139,71,8,72,141,76,36,112,137,1,72,139,87,16,255,211,133,192,116,69,72,141,
      76,36,112,72,199,1,0,0,0,0,255,21,242,184,1,0,133,192,120,63,72,139,84,36,112,72,141,76,36,80,232,227,3,0,0,132,192,117,6,255,21,125,136,1,0,72,139,76,36,112,255,21,226,184,1,0,72,199,68,36,112,0,0,0,0,235,16,139,84,36,112,255,202,72,141,76,
      36,80,232,89,4,0,0,72,139,188,36,240,0,0,0,15,87,192,15,41,68,36,48,49,192,72,137,68,36,64,72,141,84,36,112,72,137,66,16,15,41,2,49,201,232,186,237,255,255,133,192,120,45,72,139,140,36,128,0,0,0,102,186,92,0,232,104,43,0,0,72,133,192,117,8,72,
      139,132,36,128,0,0,0,72,141,76,36,48,72,137,194,232,90,3,0,0,235,9,72,199,68,36,48,0,0,0,0,72,141,76,36,112,232,129,2,0,0,72,139,84,36,96,72,137,124,36,40,76,137,100,36,32,72,137,241,77,137,248,77,137,241,232,127,0,0,0,179,1,132,192,117,66,72,
      139,84,36,64,72,137,124,36,40,76,137,100,36,32,72,137,241,77,137,248,77,137,241,232,92,0,0,0,132,192,117,33,72,137,124,36,40,76,137,100,36,32,72,141,21,210,124,1,0,72,137,241,77,137,248,77,137,241,232,57,0,0,0,137,195,72,141,76,36,48,232,18,
      2,0,0,72,141,76,36,80,232,8,2,0,0,72,139,140,36,144,0,0,0,72,49,225,232,96,108,0,0,137,216,72,129,196,152,0,0,0,91,95,94,65,92,65,94,65,95,195,65,87,65,86,86,87,83,72,129,236,96,2,0,0,72,139,5,160,172,1,0,72,49,224,72,137,132,36,88,2,0,0,72,
      199,68,36,64,0,0,0,0,72,133,210,15,132,7,1,0,0,72,137,211,102,131,58,0,15,132,250,0,0,0,77,137,206,77,137,199,72,137,207,72,137,217,232,157,63,0,0,72,141,80,66,15,87,192,72,141,116,36,80,15,41,6,72,199,70,16,0,0,0,0,72,137,241,232,174,1,0,0,
      72,141,21,39,124,1,0,65,184,66,0,0,0,72,137,241,232,27,3,0,0,72,137,217,232,97,63,0,0,72,137,241,72,137,218,73,137,192,232,5,3,0,0,72,139,86,16,72,141,68,36,64,72,137,68,36,32,49,219,72,137,249,69,49,192,65,185,1,0,0,0,255,21,209,182,1,0,137,
      199,133,192,15,148,192,8,5,168,181,1,0,72,137,241,232,20,1,0,0,133,255,117,102,72,139,188,36,184,2,0,0,72,133,255,116,122,199,68,36,80,0,0,0,0,72,141,68,36,76,199,0,4,0,0,0,72,139,76,36,64,72,137,68,36,48,72,137,116,36,40,72,199,68,36,32,0,0,
      0,0,49,219,49,210,77,137,248,65,185,16,0,0,0,255,21,101,182,1,0,133,192,15,133,173,0,0,0,49,192,131,124,36,80,1,15,148,192,137,7,233,154,0,0,0,49,219,72,139,140,36,88,2,0,0,72,49,225,232,2,107,0,0,137,216,72,129,196,96,2,0,0,91,95,94,65,94,65,
      95,195,72,139,124,36,64,49,219,65,184,8,2,0,0,72,137,241,49,210,232,154,39,0,0,72,141,68,36,76,199,0,4,1,0,0,72,137,68,36,48,72,137,116,36,40,72,199,68,36,32,0,0,0,0,72,137,249,49,210,77,137,248,65,185,2,0,0,0,255,21,224,181,1,0,133,192,117,
      44,72,139,180,36,176,2,0,0,72,141,124,36,80,72,137,249,232,47,62,0,0,72,137,241,72,137,250,73,137,192,232,15,1,0,0,72,139,70,16,73,137,6,179,1,72,139,76,36,64,255,21,157,181,1,0,233,86,255,255,255,86,72,131,236,32,72,199,1,0,0,0,0,72,139,65,
      16,72,133,192,116,29,72,137,206,102,199,0,0,0,72,139,73,16,72,133,201,116,12,232,57,3,0,0,15,87,192,15,17,70,8,72,131,196,32,94,195,65,87,65,86,86,87,83,72,131,236,32,64,182,1,72,57,81,8,115,106,72,137,215,72,131,250,255,116,62,72,137,203,72,
      141,71,1,72,1,192,72,199,193,255,255,255,255,72,15,67,200,232,237,2,0,0,73,137,198,72,139,3,72,133,192,116,29,76,139,123,16,76,141,4,0,73,131,192,2,76,137,241,76,137,250,232,231,167,0,0,235,14,49,246,235,31,102,65,199,6,0,0,76,139,123,16,77,
      133,255,116,8,76,137,249,232,182,2,0,0,76,137,115,16,72,137,123,8,137,240,72,131,196,32,91,95,94,65,94,65,95,195,86,87,72,131,236,40,72,137,214,72,137,207,72,133,210,116,13,72,137,241,232,43,61,0,0,73,137,192,235,3,69,49,192,72,137,249,72,137,
      242,72,131,196,40,95,94,233,0,0,0,0,86,87,85,83,72,131,236,40,76,137,199,72,137,214,72,137,203,72,199,1,0,0,0,0,72,139,65,16,72,133,192,116,5,102,199,0,0,0,64,181,1,72,133,246,116,39,72,137,217,72,137,250,232,9,255,255,255,132,192,116,22,72,
      139,75,16,76,141,4,63,72,137,242,232,53,167,0,0,72,57,123,8,115,13,49,237,137,232,72,131,196,40,91,93,95,94,195,72,137,59,72,139,67,16,72,133,192,116,233,102,199,4,120,0,0,235,225,72,57,81,8,115,3,49,192,195,72,137,17,72,139,73,16,176,1,72,133,
      201,116,241,102,199,4,81,0,0,195,86,87,72,131,236,40,72,137,214,72,137,207,72,133,210,116,13,72,137,241,232,103,60,0,0,73,137,192,235,3,69,49,192,72,137,249,72,137,242,72,131,196,40,95,94,233,0,0,0,0,65,86,86,87,85,83,72,131,236,32,65,182,1,
      72,133,210,116,65,76,137,198,72,137,207,72,139,25,76,1,195,114,48,72,137,213,72,137,249,72,137,218,232,80,254,255,255,132,192,116,30,72,139,15,72,1,201,72,3,79,16,72,1,246,72,137,234,73,137,240,232,116,166,0,0,72,57,95,8,115,17,69,49,246,68,
      137,240,72,131,196,32,91,93,95,94,65,94,195,72,137,31,72,139,71,16,72,133,192,116,230,102,199,4,88,0,0,235,222,204,86,87,83,72,131,236,32,72,133,201,72,141,61,205,115,1,0,72,15,69,249,72,141,28,18,72,131,195,2,72,57,211,118,33,72,137,217,255,
      21,117,179,1,0,72,137,198,72,133,192,116,18,72,137,241,72,137,250,73,137,216,232,7,166,0,0,235,2,49,246,72,137,240,72,131,196,32,91,95,94,195,86,87,83,72,131,236,32,72,137,214,72,137,207,49,210,69,49,192,255,21,184,130,1,0,133,192,116,49,137,
      195,137,194,72,137,241,232,144,253,255,255,132,192,116,33,72,139,86,16,72,137,249,65,137,216,255,21,148,130,1,0,137,194,72,137,241,72,131,196,32,91,95,94,233,157,254,255,255,49,192,72,131,196,32,91,95,94,195,204,86,87,83,72,131,236,48,76,137,
      206,68,137,199,137,211,255,21,75,131,1,0,72,133,192,116,47,72,141,21,69,120,1,0,72,137,193,255,21,142,130,1,0,72,133,192,116,26,72,139,76,36,120,76,139,76,36,112,72,137,76,36,32,137,217,137,250,73,137,240,255,208,235,21,255,21,65,130,1,0,137,
      193,15,183,193,13,0,0,7,128,133,201,15,78,193,72,131,196,48,91,95,94,195,204,204,72,131,236,40,232,139,2,0,0,235,2,51,192,72,131,196,40,195,204,204,233,115,58,0,0,204,204,204,233,115,2,0,0,204,204,204,233,235,255,255,255,204,204,204,72,137,92,
      36,8,87,72,131,236,32,186,160,15,0,0,72,141,13,118,178,1,0,255,21,96,130,1,0,72,141,13,41,247,0,0,255,21,235,129,1,0,72,139,216,72,133,192,117,21,72,141,13,92,247,0,0,255,21,214,129,1,0,72,139,216,72,133,192,116,127,72,141,21,103,247,0,0,72,
      139,203,255,21,206,129,1,0,72,141,21,119,247,0,0,72,139,203,72,139,248,255,21,187,129,1,0,72,133,255,116,21,72,133,192,116,16,72,137,61,58,178,1,0,72,137,5,59,178,1,0,235,30,69,51,201,69,51,192,51,201,65,141,81,1,255,21,159,128,1,0,72,137,5,
      232,177,1,0,72,133,192,116,36,51,201,232,192,3,0,0,132,192,116,25,72,141,13,141,1,0,0,232,236,1,0,0,72,139,92,36,48,51,192,72,131,196,32,95,195,185,7,0,0,0,232,93,5,0,0,204,64,83,72,131,236,32,72,139,217,72,141,13,172,177,1,0,255,21,110,128,
      1,0,131,59,0,117,17,131,11,255,235,52,185,100,0,0,0,232,158,0,0,0,235,234,131,59,255,116,239,101,72,139,4,37,88,0,0,0,139,13,0,178,1,0,65,184,4,0,0,0,72,139,20,200,139,5,28,166,1,0,65,137,4,16,72,141,13,97,177,1,0,72,131,196,32,91,72,255,37,
      125,129,1,0,204,64,83,72,131,236,32,72,139,217,72,141,13,68,177,1,0,255,21,6,128,1,0,139,5,232,165,1,0,72,141,13,49,177,1,0,139,21,175,177,1,0,255,192,137,5,211,165,1,0,137,3,101,72,139,4,37,88,0,0,0,65,185,4,0,0,0,76,139,4,208,139,5,184,165,
      1,0,67,137,4,1,255,21,38,129,1,0,72,131,196,32,91,233,100,0,0,0,64,83,72,131,236,32,72,139,5,15,177,1,0,139,217,72,133,192,116,29,68,139,193,72,141,21,214,176,1,0,72,141,13,191,176,1,0,72,131,196,32,91,72,255,37,131,229,1,0,72,141,13,188,176,
      1,0,255,21,222,128,1,0,72,139,13,167,176,1,0,69,51,192,139,211,255,21,164,129,1,0,72,141,13,157,176,1,0,72,131,196,32,91,72,255,37,89,127,1,0,204,72,131,236,40,72,139,5,181,176,1,0,72,133,192,116,18,72,141,13,105,176,1,0,72,131,196,40,72,255,
      37,46,229,1,0,72,139,13,95,176,1,0,255,21,249,128,1,0,72,139,13,82,176,1,0,72,131,196,40,72,255,37,183,128,1,0,204,204,204,72,131,236,40,72,141,13,65,176,1,0,255,21,243,126,1,0,72,139,13,44,176,1,0,72,133,201,116,6,255,21,201,126,1,0,72,131,
      196,40,195,64,83,72,131,236,32,72,139,217,235,15,72,139,203,232,49,37,0,0,133,192,116,19,72,139,203,232,237,55,0,0,72,133,192,116,231,72,131,196,32,91,195,72,131,251,255,116,6,232,55,6,0,0,204,232,81,6,0,0,204,72,131,236,40,232,15,0,0,0,72,247,
      216,27,192,247,216,255,200,72,131,196,40,195,204,64,83,72,131,236,32,72,131,61,18,176,1,0,255,72,139,217,117,7,232,48,44,0,0,235,15,72,139,211,72,141,13,252,175,1,0,232,155,43,0,0,51,210,133,192,72,15,68,211,72,139,194,72,131,196,32,91,195,204,
      204,72,131,236,24,76,139,193,184,77,90,0,0,102,57,5,49,212,255,255,117,120,72,99,13,100,212,255,255,72,141,21,33,212,255,255,72,3,202,129,57,80,69,0,0,117,95,184,11,2,0,0,102,57,65,24,117,84,76,43,194,15,183,65,20,72,141,81,24,72,3,208,15,183,
      65,6,72,141,12,128,76,141,12,202,72,137,20,36,73,59,209,116,24,139,74,12,76,59,193,114,10,139,66,8,3,193,76,59,192,114,8,72,131,194,40,235,223,51,210,72,133,210,117,4,50,192,235,20,131,122,36,0,125,4,50,192,235,10,176,1,235,6,50,192,235,2,50,
      192,72,131,196,24,195,72,131,236,40,232,39,7,0,0,133,192,116,33,101,72,139,4,37,48,0,0,0,72,139,72,8,235,5,72,59,200,116,20,51,192,240,72,15,177,13,16,175,1,0,117,238,50,192,72,131,196,40,195,176,1,235,247,204,204,204,64,83,72,131,236,32,138,
      217,232,231,6,0,0,51,210,133,192,116,11,132,219,117,7,72,135,21,226,174,1,0,72,131,196,32,91,195,64,83,72,131,236,32,15,182,5,215,174,1,0,133,201,187,1,0,0,0,15,68,195,136,5,199,174,1,0,232,2,5,0,0,232,213,30,0,0,132,192,117,4,50,192,235,20,
      232,8,35,0,0,132,192,117,9,51,201,232,229,30,0,0,235,234,138,195,72,131,196,32,91,195,204,204,204,64,83,72,131,236,32,128,61,139,174,1,0,0,138,217,116,4,132,210,117,12,232,234,34,0,0,138,203,232,183,30,0,0,176,1,72,131,196,32,91,195,204,204,
      204,64,83,72,131,236,32,128,61,96,174,1,0,0,139,217,117,103,131,249,1,119,106,232,61,6,0,0,133,192,116,40,133,219,117,36,72,141,13,74,174,1,0,232,193,41,0,0,133,192,117,16,72,141,13,82,174,1,0,232,177,41,0,0,133,192,116,46,50,192,235,51,102,
      15,111,5,69,243,0,0,72,131,200,255,243,15,127,5,25,174,1,0,72,137,5,34,174,1,0,243,15,127,5,34,174,1,0,72,137,5,43,174,1,0,198,5,245,173,1,0,1,176,1,72,131,196,32,91,195,185,5,0,0,0,232,58,1,0,0,204,204,72,137,92,36,8,72,137,108,36,16,72,137,
      116,36,24,87,72,131,236,32,73,139,249,73,139,240,139,218,72,139,233,232,168,5,0,0,133,192,117,22,131,251,1,117,17,76,139,198,51,210,72,139,205,72,139,199,255,21,26,226,1,0,72,139,84,36,88,139,76,36,80,72,139,92,36,48,72,139,108,36,56,72,139,
      116,36,64,72,131,196,32,95,233,212,47,0,0,72,131,236,40,51,201,232,9,255,255,255,132,192,15,149,192,72,131,196,40,195,204,204,204,72,131,236,40,232,75,5,0,0,133,192,116,7,232,150,3,0,0,235,25,232,51,5,0,0,139,200,232,36,37,0,0,133,192,116,4,
      50,192,235,7,232,187,44,0,0,176,1,72,131,196,40,195,72,131,236,40,232,23,5,0,0,133,192,116,16,72,141,13,40,173,1,0,72,131,196,40,233,11,41,0,0,232,86,34,0,0,133,192,117,5,232,97,34,0,0,72,131,196,40,195,72,131,236,40,51,201,232,157,33,0,0,72,
      131,196,40,233,76,29,0,0,72,131,236,40,232,83,29,0,0,132,192,117,4,50,192,235,18,232,142,33,0,0,132,192,117,7,232,81,29,0,0,235,236,176,1,72,131,196,40,195,72,131,236,40,232,135,33,0,0,232,58,29,0,0,176,1,72,131,196,40,195,204,204,204,131,37,
      225,172,1,0,0,195,72,137,92,36,8,85,72,141,172,36,64,251,255,255,72,129,236,192,5,0,0,139,217,185,23,0,0,0,232,159,238,0,0,133,192,116,4,139,203,205,41,185,3,0,0,0,232,197,255,255,255,51,210,72,141,77,240,65,184,208,4,0,0,232,76,29,0,0,72,141,
      77,240,255,21,130,124,1,0,72,139,157,232,0,0,0,72,141,149,216,4,0,0,72,139,203,69,51,192,255,21,112,124,1,0,72,133,192,116,60,72,131,100,36,56,0,72,141,141,224,4,0,0,72,139,149,216,4,0,0,76,139,200,72,137,76,36,48,76,139,195,72,141,141,232,4,
      0,0,72,137,76,36,40,72,141,77,240,72,137,76,36,32,51,201,255,21,71,124,1,0,72,139,133,200,4,0,0,72,141,76,36,80,72,137,133,232,0,0,0,51,210,72,141,133,200,4,0,0,65,184,152,0,0,0,72,131,192,8,72,137,133,136,0,0,0,232,181,28,0,0,72,139,133,200,
      4,0,0,72,137,68,36,96,199,68,36,80,21,0,0,64,199,68,36,84,1,0,0,0,255,21,107,123,1,0,131,248,1,72,141,68,36,80,72,137,68,36,64,72,141,69,240,15,148,195,72,137,68,36,72,51,201,255,21,250,123,1,0,72,141,76,36,64,255,21,31,124,1,0,133,192,117,12,
      132,219,117,8,141,72,3,232,191,254,255,255,72,139,156,36,208,5,0,0,72,129,196,192,5,0,0,93,195,204,204,194,0,0,204,64,83,72,131,236,32,72,139,217,72,139,194,72,141,13,133,240,0,0,15,87,192,72,137,11,72,141,83,8,72,141,72,8,15,17,2,232,59,26,
      0,0,72,139,195,72,131,196,32,91,195,204,204,72,131,121,8,0,72,141,5,104,240,0,0,72,15,69,65,8,195,204,204,72,137,92,36,8,87,72,131,236,32,72,141,5,63,240,0,0,72,139,249,72,137,1,139,218,72,131,193,8,232,138,26,0,0,246,195,1,116,13,186,24,0,0,
      0,72,139,207,232,0,248,255,255,72,139,92,36,48,72,139,199,72,131,196,32,95,195,204,204,72,131,97,16,0,72,141,5,64,240,0,0,72,137,65,8,72,141,5,37,240,0,0,72,137,1,72,139,193,195,204,204,72,141,5,229,239,0,0,72,137,1,72,131,193,8,233,53,26,0,
      0,204,64,83,72,131,236,32,72,139,217,72,139,194,72,141,13,197,239,0,0,15,87,192,72,137,11,72,141,83,8,72,141,72,8,15,17,2,232,123,25,0,0,72,141,5,216,239,0,0,72,137,3,72,139,195,72,131,196,32,91,195,72,131,97,16,0,72,141,5,248,239,0,0,72,137,
      65,8,72,141,5,221,239,0,0,72,137,1,72,139,193,195,204,204,64,83,72,131,236,32,72,139,217,72,139,194,72,141,13,105,239,0,0,15,87,192,72,137,11,72,141,83,8,72,141,72,8,15,17,2,232,31,25,0,0,72,141,5,164,239,0,0,72,137,3,72,139,195,72,131,196,32,
      91,195,72,131,236,72,72,141,76,36,32,232,38,255,255,255,72,141,21,131,145,1,0,72,141,76,36,32,232,81,13,0,0,204,72,131,236,72,72,141,76,36,32,232,118,255,255,255,72,141,21,235,145,1,0,72,141,76,36,32,232,49,13,0,0,204,72,137,92,36,16,72,137,
      116,36,24,87,72,131,236,16,51,192,51,201,15,162,68,139,193,69,51,219,68,139,203,65,129,240,110,116,101,108,65,129,241,71,101,110,117,68,139,210,139,240,51,201,65,141,67,1,69,11,200,15,162,65,129,242,105,110,101,73,137,4,36,69,11,202,137,92,36,
      4,139,249,137,76,36,8,137,84,36,12,117,80,72,131,13,215,157,1,0,255,37,240,63,255,15,61,192,6,1,0,116,40,61,96,6,2,0,116,33,61,112,6,2,0,116,26,5,176,249,252,255,131,248,32,119,36,72,185,1,0,1,0,1,0,0,0,72,15,163,193,115,20,68,139,5,100,169,
      1,0,65,131,200,1,68,137,5,89,169,1,0,235,7,68,139,5,80,169,1,0,184,7,0,0,0,68,141,72,251,59,240,124,38,51,201,15,162,137,4,36,68,139,219,137,92,36,4,137,76,36,8,137,84,36,12,15,186,227,9,115,10,69,11,193,68,137,5,29,169,1,0,199,5,67,157,1,0,
      1,0,0,0,68,137,13,64,157,1,0,15,186,231,20,15,131,145,0,0,0,68,137,13,43,157,1,0,187,6,0,0,0,137,29,36,157,1,0,15,186,231,27,115,121,15,186,231,28,115,115,51,201,15,1,208,72,193,226,32,72,11,208,72,137,84,36,32,72,139,68,36,32,34,195,58,195,
      117,87,139,5,246,156,1,0,131,200,8,199,5,229,156,1,0,3,0,0,0,137,5,227,156,1,0,65,246,195,32,116,56,131,200,32,199,5,204,156,1,0,5,0,0,0,137,5,202,156,1,0,184,0,0,3,208,68,35,216,68,59,216,117,24,72,139,68,36,32,36,224,60,224,117,13,131,13,171,
      156,1,0,64,137,29,161,156,1,0,72,139,92,36,40,51,192,72,139,116,36,48,72,131,196,16,95,195,204,204,204,184,1,0,0,0,195,204,204,51,192,57,5,80,168,1,0,15,149,192,195,64,83,72,131,236,32,72,141,5,195,237,0,0,72,139,217,72,137,1,246,194,1,116,10,
      186,24,0,0,0,232,10,245,255,255,72,139,195,72,131,196,32,91,195,204,72,137,92,36,8,72,137,116,36,16,72,137,124,36,32,65,86,72,131,236,32,72,139,242,76,139,241,51,201,232,214,248,255,255,132,192,15,132,200,0,0,0,232,105,248,255,255,138,216,136,
      68,36,64,64,183,1,131,61,141,167,1,0,0,15,133,197,0,0,0,199,5,125,167,1,0,1,0,0,0,232,8,250,255,255,132,192,116,79,232,119,10,0,0,232,34,10,0,0,232,57,10,0,0,72,141,21,190,111,1,0,72,141,13,143,111,1,0,232,118,41,0,0,133,192,117,41,232,241,249,
      255,255,132,192,116,32,72,141,21,110,111,1,0,72,141,13,95,111,1,0,232,242,40,0,0,199,5,40,167,1,0,2,0,0,0,64,50,255,138,203,232,38,248,255,255,64,132,255,117,63,232,20,10,0,0,72,139,216,72,131,56,0,116,36,72,139,200,232,55,247,255,255,132,192,
      116,24,76,139,198,186,2,0,0,0,73,139,206,72,139,3,76,139,13,114,219,1,0,65,255,209,255,5,57,167,1,0,184,1,0,0,0,235,2,51,192,72,139,92,36,48,72,139,116,36,56,72,139,124,36,72,72,131,196,32,65,94,195,185,7,0,0,0,232,32,250,255,255,144,204,204,
      204,72,137,92,36,8,87,72,131,236,48,64,138,249,139,5,249,166,1,0,133,192,127,13,51,192,72,139,92,36,64,72,131,196,48,95,195,255,200,137,5,224,166,1,0,232,79,247,255,255,138,216,136,68,36,32,131,61,118,166,1,0,2,117,55,232,75,249,255,255,232,
      50,9,0,0,232,169,9,0,0,131,37,94,166,1,0,0,138,203,232,95,247,255,255,51,210,64,138,207,232,197,247,255,255,246,216,27,219,131,227,1,232,77,249,255,255,139,195,235,162,185,7,0,0,0,232,155,249,255,255,144,144,204,72,131,236,40,133,210,116,57,
      131,234,1,116,40,131,234,1,116,22,131,250,1,116,10,184,1,0,0,0,72,131,196,40,195,232,82,249,255,255,235,5,232,35,249,255,255,15,182,192,72,131,196,40,195,73,139,208,72,131,196,40,233,35,254,255,255,77,133,192,15,149,193,72,131,196,40,233,44,
      255,255,255,72,139,196,72,137,88,32,76,137,64,24,137,80,16,72,137,72,8,86,87,65,86,72,131,236,64,73,139,240,139,250,76,139,241,133,210,117,15,57,21,12,166,1,0,127,7,51,192,233,238,0,0,0,141,66,255,131,248,1,119,69,72,139,5,124,235,0,0,72,133,
      192,117,10,199,68,36,48,1,0,0,0,235,20,255,21,15,218,1,0,139,216,137,68,36,48,133,192,15,132,178,0,0,0,76,139,198,139,215,73,139,206,232,60,255,255,255,139,216,137,68,36,48,133,192,15,132,151,0,0,0,76,139,198,139,215,73,139,206,232,201,217,255,
      255,139,216,137,68,36,48,131,255,1,117,54,133,192,117,50,76,139,198,51,210,73,139,206,232,173,217,255,255,72,133,246,15,149,193,232,118,254,255,255,72,139,5,3,235,0,0,72,133,192,116,14,76,139,198,51,210,73,139,206,255,21,152,217,1,0,133,255,
      116,5,131,255,3,117,64,76,139,198,139,215,73,139,206,232,202,254,255,255,139,216,137,68,36,48,133,192,116,41,72,139,5,201,234,0,0,72,133,192,117,9,141,88,1,137,92,36,48,235,20,76,139,198,139,215,73,139,206,255,21,85,217,1,0,139,216,137,68,36,
      48,235,6,51,219,137,92,36,48,139,195,72,139,92,36,120,72,131,196,64,65,94,95,94,195,204,204,204,72,137,92,36,8,72,137,116,36,16,87,72,131,236,32,73,139,248,139,218,72,139,241,131,250,1,117,5,232,151,6,0,0,76,139,199,139,211,72,139,206,72,139,
      92,36,48,72,139,116,36,56,72,131,196,32,95,233,143,254,255,255,204,204,204,72,137,92,36,8,87,72,131,236,32,72,139,5,187,164,1,0,191,1,0,0,0,72,59,199,116,115,72,133,192,117,105,72,141,13,45,234,0,0,255,21,143,115,1,0,72,139,216,72,133,192,117,
      5,72,139,223,235,56,72,141,21,51,234,0,0,72,139,203,255,21,130,115,1,0,72,133,192,116,230,72,141,21,54,234,0,0,72,137,5,119,164,1,0,72,139,203,255,21,102,115,1,0,72,133,192,116,202,72,137,5,106,164,1,0,51,192,240,72,15,177,29,79,164,1,0,117,
      5,72,59,223,116,10,72,59,199,116,5,64,138,199,235,2,50,192,72,139,92,36,48,72,131,196,32,95,195,204,72,137,92,36,8,72,137,116,36,16,72,137,124,36,24,76,99,5,110,200,255,255,72,141,53,43,200,255,255,76,3,198,72,139,218,72,139,249,65,131,184,132,
      0,0,0,13,118,72,69,139,144,240,0,0,0,69,51,201,69,133,210,116,57,65,15,183,64,20,73,141,72,24,69,15,183,88,6,72,3,200,69,139,84,50,12,69,133,219,116,30,139,65,12,68,59,208,114,10,139,81,8,3,194,68,59,208,114,30,65,255,193,72,131,193,40,69,59,
      203,114,226,51,192,72,139,92,36,8,72,139,116,36,16,72,139,124,36,24,195,137,23,139,65,36,137,3,139,65,12,72,3,198,235,225,204,204,204,72,137,92,36,8,87,72,129,236,128,0,0,0,72,139,250,65,184,48,0,0,0,72,141,84,36,32,72,139,217,255,21,208,115,
      1,0,72,133,192,117,5,141,72,25,205,41,246,68,36,68,68,116,82,72,141,76,36,80,255,21,124,114,1,0,68,139,76,36,84,51,210,69,139,193,73,247,216,76,35,195,65,141,73,255,139,193,35,203,35,199,3,193,72,255,200,73,3,193,73,247,241,51,210,72,139,200,
      72,139,199,73,247,241,72,3,200,139,193,133,201,116,14,240,65,131,8,0,77,3,193,72,131,232,1,117,242,72,139,156,36,144,0,0,0,72,129,196,128,0,0,0,95,195,72,139,196,72,137,88,8,72,137,112,16,87,72,131,236,32,72,139,250,139,241,72,141,80,32,72,141,
      72,24,232,170,254,255,255,72,139,216,72,133,192,117,8,199,7,4,0,0,0,235,76,131,61,235,162,1,0,0,117,39,247,68,36,72,0,0,0,128,199,5,215,162,1,0,1,0,0,0,117,7,185,25,0,0,0,205,41,139,84,36,64,72,139,203,232,6,255,255,255,139,84,36,64,76,139,207,
      68,139,198,72,139,203,255,21,223,114,1,0,133,192,117,5,141,72,25,205,41,72,139,92,36,48,72,139,116,36,56,72,131,196,32,95,195,204,204,64,83,72,131,236,32,247,5,196,232,0,0,0,16,0,0,15,132,142,0,0,0,232,133,253,255,255,187,1,0,0,0,132,192,116,
      24,72,139,5,73,162,1,0,72,141,13,82,162,1,0,255,21,92,214,1,0,235,25,243,144,72,139,5,65,162,1,0,72,133,192,117,242,240,72,15,177,29,51,162,1,0,117,233,139,5,51,162,1,0,3,195,137,5,43,162,1,0,59,195,117,17,72,141,21,36,162,1,0,185,4,0,0,0,232,
      250,254,255,255,232,37,253,255,255,132,192,116,26,72,139,5,246,161,1,0,72,141,13,247,161,1,0,72,131,196,32,91,72,255,37,251,213,1,0,72,199,5,224,161,1,0,0,0,0,0,72,131,196,32,91,195,204,204,72,131,236,40,247,5,26,232,0,0,0,16,0,0,116,126,232,
      223,252,255,255,132,192,116,24,72,139,5,168,161,1,0,72,141,13,177,161,1,0,255,21,187,213,1,0,235,28,243,144,72,139,5,160,161,1,0,72,133,192,117,242,141,72,1,240,72,15,177,13,143,161,1,0,117,230,131,5,142,161,1,0,255,117,16,139,13,138,161,1,0,
      72,141,84,36,48,232,96,254,255,255,232,139,252,255,255,132,192,116,22,72,139,5,92,161,1,0,72,141,13,93,161,1,0,255,21,103,213,1,0,235,11,72,199,5,74,161,1,0,0,0,0,0,72,131,196,40,195,204,72,137,92,36,16,72,137,116,36,24,72,137,124,36,32,85,65,
      84,65,85,65,86,65,87,72,139,236,72,131,236,112,76,139,226,72,139,241,232,150,254,255,255,139,70,4,72,141,21,16,197,255,255,68,139,118,8,72,3,194,68,139,78,12,76,3,242,139,78,16,76,3,202,68,139,110,20,72,3,202,72,131,101,224,0,76,3,234,72,131,
      101,232,0,15,87,192,131,101,240,0,139,86,28,72,137,69,200,139,6,131,224,1,137,85,48,199,69,176,72,0,0,0,72,137,117,184,76,137,101,192,15,17,69,208,132,192,117,41,72,141,69,176,72,137,69,48,232,214,254,255,255,51,210,76,141,77,48,185,87,0,109,
      192,68,141,66,1,255,21,73,112,1,0,51,192,233,5,2,0,0,73,139,62,77,139,252,77,43,249,73,193,255,3,69,139,255,74,139,4,249,72,193,232,63,131,240,1,137,69,208,116,20,66,139,4,249,72,141,13,105,196,255,255,72,3,193,72,137,69,216,235,8,66,15,183,
      4,249,137,69,216,72,139,5,63,231,0,0,51,219,72,133,192,116,31,72,141,85,176,51,201,255,21,76,212,1,0,72,139,216,72,133,192,15,133,116,1,0,0,72,139,5,25,231,0,0,72,133,255,15,133,161,0,0,0,72,133,192,116,21,72,141,85,176,141,79,1,255,21,30,212,
      1,0,72,139,248,72,133,192,117,108,72,139,77,200,69,51,192,51,210,255,21,119,111,1,0,72,139,248,72,133,192,117,85,255,21,169,110,1,0,137,69,240,72,139,5,215,230,0,0,72,133,192,116,21,72,141,85,176,141,79,3,255,21,221,211,1,0,72,139,248,72,133,
      192,117,43,72,141,69,176,72,137,69,48,232,224,253,255,255,51,210,76,141,77,48,185,126,0,109,192,68,141,66,1,255,21,83,111,1,0,72,139,69,232,233,13,1,0,0,72,139,199,73,135,6,72,59,199,117,9,72,139,207,255,21,214,109,1,0,72,139,5,111,230,0,0,72,
      137,125,224,72,133,192,116,18,72,141,85,176,185,2,0,0,0,255,21,119,211,1,0,72,139,216,72,133,219,15,133,155,0,0,0,57,94,20,116,44,57,94,28,116,39,72,99,71,60,129,60,56,80,69,0,0,117,26,139,77,48,57,76,56,8,117,17,72,59,124,56,48,117,10,75,139,
      92,253,0,72,133,219,117,106,72,139,85,216,72,139,207,255,21,5,110,1,0,72,139,216,72,133,192,117,85,255,21,207,109,1,0,137,69,240,72,139,5,253,229,0,0,72,133,192,116,21,72,141,85,176,141,75,4,255,21,3,211,1,0,72,139,216,72,133,192,117,43,72,141,
      69,176,72,137,69,48,232,6,253,255,255,51,210,76,141,77,48,185,127,0,109,192,68,141,66,1,255,21,121,110,1,0,232,64,252,255,255,72,139,93,232,73,137,28,36,72,139,5,165,229,0,0,72,133,192,116,27,131,101,240,0,72,141,85,176,185,5,0,0,0,72,137,125,
      224,72,137,93,232,255,21,165,210,1,0,232,184,252,255,255,72,139,195,76,141,92,36,112,73,139,91,56,73,139,115,64,73,139,123,72,73,139,227,65,95,65,94,65,93,65,92,93,195,204,204,204,72,137,92,36,32,85,72,139,236,72,131,236,32,72,139,5,128,146,
      1,0,72,187,50,162,223,45,153,43,0,0,72,59,195,117,116,72,131,101,24,0,72,141,77,24,255,21,82,109,1,0,72,139,69,24,72,137,69,16,255,21,196,108,1,0,139,192,72,49,69,16,255,21,176,108,1,0,139,192,72,141,77,32,72,49,69,16,255,21,184,109,1,0,139,
      69,32,72,141,77,16,72,193,224,32,72,51,69,32,72,51,69,16,72,51,193,72,185,255,255,255,255,255,255,0,0,72,35,193,72,185,51,162,223,45,153,43,0,0,72,59,195,72,15,68,193,72,137,5,253,145,1,0,72,139,92,36,72,72,247,208,72,137,5,230,145,1,0,72,131,
      196,32,93,195,72,141,13,217,157,1,0,72,255,37,250,108,1,0,204,204,72,141,13,201,157,1,0,233,0,13,0,0,72,131,236,40,232,19,0,0,0,72,131,8,36,232,18,0,0,0,72,131,8,2,72,131,196,40,195,204,72,141,5,177,157,1,0,195,72,141,5,177,157,1,0,195,72,141,
      5,177,157,1,0,195,72,137,92,36,8,87,72,131,236,32,72,141,29,199,115,1,0,72,141,61,192,115,1,0,235,18,72,139,3,72,133,192,116,6,255,21,88,209,1,0,72,131,195,8,72,59,223,114,233,72,139,92,36,48,72,131,196,32,95,195,72,137,92,36,8,87,72,131,236,
      32,72,141,29,155,115,1,0,72,141,61,148,115,1,0,235,18,72,139,3,72,133,192,116,6,255,21,28,209,1,0,72,131,195,8,72,59,223,114,233,72,139,92,36,48,72,131,196,32,95,195,72,137,92,36,24,72,137,116,36,32,87,72,131,236,80,72,139,218,72,139,241,191,
      32,5,147,25,72,133,210,116,29,246,2,16,116,24,72,139,9,72,131,233,8,72,139,1,72,139,88,48,72,139,64,64,255,21,204,208,1,0,72,141,84,36,32,72,139,203,255,21,126,108,1,0,72,137,68,36,32,72,133,219,116,15,246,3,8,117,5,72,133,192,117,5,191,0,64,
      153,1,186,1,0,0,0,72,137,124,36,40,76,141,76,36,40,72,137,116,36,48,185,99,115,109,224,72,137,92,36,56,72,137,68,36,64,68,141,66,3,255,21,24,108,1,0,72,139,92,36,112,72,139,116,36,120,72,131,196,80,95,195,72,137,92,36,8,72,137,108,36,16,72,137,
      116,36,24,87,65,84,65,85,65,86,65,87,72,131,236,64,72,139,233,77,139,249,73,139,200,73,139,248,76,139,234,232,124,46,0,0,77,139,103,8,77,139,55,73,139,95,56,77,43,244,246,69,4,102,65,139,119,72,15,133,220,0,0,0,72,137,108,36,48,72,137,124,36,
      56,59,51,15,131,138,1,0,0,139,254,72,3,255,139,68,251,4,76,59,240,15,130,170,0,0,0,139,68,251,8,76,59,240,15,131,157,0,0,0,131,124,251,16,0,15,132,146,0,0,0,131,124,251,12,1,116,23,139,68,251,12,72,141,76,36,48,73,3,196,73,139,213,255,208,133,
      192,120,125,126,116,129,125,0,99,115,109,224,117,40,72,131,61,89,227,0,0,0,116,30,72,141,13,80,227,0,0,232,83,45,0,0,133,192,116,14,186,1,0,0,0,72,139,205,255,21,57,227,0,0,139,76,251,16,65,184,1,0,0,0,73,3,204,73,139,213,232,140,45,0,0,73,139,
      71,64,76,139,197,139,84,251,16,73,139,205,68,139,77,0,73,3,212,72,137,68,36,40,73,139,71,40,72,137,68,36,32,255,21,27,107,1,0,232,142,45,0,0,255,198,233,53,255,255,255,51,192,233,197,0,0,0,73,139,127,32,68,139,11,73,43,252,65,59,241,15,131,173,
      0,0,0,69,139,193,139,214,65,139,200,72,3,210,139,68,211,4,76,59,240,15,130,136,0,0,0,139,68,211,8,76,59,240,115,127,68,139,93,4,65,131,227,32,116,68,69,51,210,69,133,192,116,52,65,139,202,72,3,201,139,68,203,4,72,59,248,114,29,139,68,203,8,72,
      59,248,115,20,139,68,211,16,57,68,203,16,117,10,139,68,211,12,57,68,203,12,116,8,65,255,194,69,59,208,114,204,65,139,201,69,59,209,117,62,139,68,211,16,133,192,116,12,72,59,248,117,36,69,133,219,117,44,235,29,141,70,1,177,1,65,137,71,72,68,139,
      68,211,12,73,139,213,77,3,196,65,255,208,68,139,11,65,139,201,255,198,68,139,193,59,241,15,130,86,255,255,255,184,1,0,0,0,76,141,92,36,64,73,139,91,48,73,139,107,56,73,139,115,64,73,139,227,65,95,65,94,65,93,65,92,95,195,204,72,139,196,72,137,
      88,8,72,137,104,16,72,137,112,24,72,137,120,32,65,86,138,25,76,141,81,1,136,26,65,139,241,76,141,53,37,190,255,255,73,139,232,76,139,218,72,139,249,246,195,4,116,36,65,15,182,10,131,225,15,74,15,190,132,49,0,35,1,0,66,138,140,49,16,35,1,0,76,
      43,208,65,139,66,252,211,232,137,66,4,246,195,8,116,10,65,139,2,73,131,194,4,137,66,8,246,195,16,116,10,65,139,2,73,131,194,4,137,66,12,73,99,2,77,141,66,4,69,51,201,68,56,76,36,48,117,80,246,195,2,116,75,72,141,20,40,15,182,10,131,225,15,74,
      15,190,132,49,0,35,1,0,66,138,140,49,16,35,1,0,72,43,208,68,139,82,252,65,211,234,69,137,75,16,69,133,210,116,32,139,2,139,74,4,72,141,82,8,59,198,116,10,65,255,193,69,59,202,114,235,235,9,65,137,75,16,235,3,137,66,16,246,195,1,116,37,65,15,
      182,8,131,225,15,74,15,190,148,49,0,35,1,0,66,138,140,49,16,35,1,0,76,43,194,65,139,80,252,211,234,65,137,83,20,72,139,92,36,16,76,43,199,72,139,108,36,24,73,139,192,72,139,116,36,32,72,139,124,36,40,65,94,195,204,204,76,139,65,16,76,141,29,
      29,189,255,255,76,137,65,8,76,139,201,76,139,210,65,15,182,8,131,225,15,74,15,190,132,25,0,35,1,0,66,138,140,25,16,35,1,0,76,43,192,65,139,64,252,77,137,65,8,211,232,65,137,65,24,65,15,182,8,131,225,15,74,15,190,132,25,0,35,1,0,66,138,140,25,
      16,35,1,0,76,43,192,65,139,64,252,77,137,65,8,211,232,65,137,65,28,65,15,182,8,131,225,15,74,15,190,132,25,0,35,1,0,66,138,140,25,16,35,1,0,76,43,192,65,139,64,252,77,137,65,8,211,232,65,137,65,32,73,141,64,4,65,139,8,69,51,192,73,137,65,8,65,
      137,73,36,68,57,66,8,15,132,23,1,0,0,73,139,81,8,65,255,192,15,182,10,131,225,15,74,15,190,132,25,0,35,1,0,66,138,140,25,16,35,1,0,72,43,208,139,66,252,73,137,81,8,211,232,65,137,65,24,15,182,10,131,225,15,74,15,190,132,25,0,35,1,0,66,138,140,
      25,16,35,1,0,72,43,208,139,66,252,73,137,81,8,211,232,65,137,65,28,15,182,10,131,225,15,74,15,190,132,25,0,35,1,0,66,138,140,25,16,35,1,0,72,43,208,139,66,252,73,137,81,8,211,232,65,137,65,32,139,2,72,131,194,4,65,137,65,36,73,137,81,8,15,182,
      10,131,225,15,74,15,190,132,25,0,35,1,0,66,138,140,25,16,35,1,0,72,43,208,139,66,252,211,232,73,137,81,8,65,137,65,24,15,182,10,131,225,15,74,15,190,132,25,0,35,1,0,66,138,140,25,16,35,1,0,72,43,208,139,66,252,211,232,73,137,81,8,65,137,65,28,
      15,182,10,131,225,15,74,15,190,132,25,0,35,1,0,66,138,140,25,16,35,1,0,72,43,208,139,66,252,211,232,65,137,65,32,72,141,66,4,73,137,81,8,139,10,73,137,65,8,65,137,73,36,69,59,66,8,15,133,233,254,255,255,195,138,2,36,1,195,204,204,204,72,131,
      236,40,65,246,0,1,72,139,9,72,137,76,36,48,116,13,65,139,64,20,72,139,12,8,72,137,76,36,48,65,131,201,255,72,141,76,36,48,232,207,49,0,0,72,131,196,40,195,204,204,64,85,72,141,108,36,225,72,129,236,224,0,0,0,72,139,5,59,139,1,0,72,51,196,72,
      137,69,15,76,139,85,119,72,141,5,33,222,0,0,15,16,0,76,139,217,72,141,76,36,48,15,16,72,16,15,17,1,15,16,64,32,15,17,73,16,15,16,72,48,15,17,65,32,15,16,64,64,15,17,73,48,15,16,72,80,15,17,65,64,15,16,64,96,15,17,73,80,15,16,136,128,0,0,0,15,
      17,65,96,15,16,64,112,72,139,128,144,0,0,0,15,17,65,112,15,17,137,128,0,0,0,72,137,129,144,0,0,0,72,141,5,32,52,0,0,72,137,69,143,72,139,69,79,72,137,69,159,72,99,69,95,76,137,69,175,76,139,69,111,72,137,69,167,15,182,69,127,72,137,69,199,73,
      139,72,24,77,139,64,32,73,3,74,8,77,3,66,8,72,99,69,103,72,137,69,231,73,139,66,64,72,137,68,36,40,73,139,66,40,76,137,77,151,69,51,201,72,137,77,183,73,139,11,72,137,85,191,73,139,18,76,137,69,215,76,141,68,36,48,72,137,68,36,32,72,199,69,207,
      32,5,147,25,255,21,246,101,1,0,72,139,77,15,72,51,204,232,210,73,0,0,72,129,196,224,0,0,0,93,195,204,72,139,1,72,139,209,73,137,1,65,246,0,1,116,14,65,139,72,20,72,139,2,72,139,12,1,73,137,9,73,139,193,195,204,204,204,72,139,196,72,137,88,8,
      72,137,112,16,72,137,120,24,85,65,86,65,87,72,139,236,72,131,236,96,72,139,218,72,137,85,192,15,41,112,216,76,139,241,72,137,85,208,51,255,137,125,200,72,141,85,224,15,40,117,192,72,139,203,102,15,127,117,224,69,139,248,51,246,232,112,252,255,
      255,68,139,11,69,51,192,69,133,201,15,132,184,0,0,0,76,141,21,130,185,255,255,72,139,67,24,68,59,248,124,21,72,193,232,32,68,59,248,127,12,133,255,65,139,240,65,15,69,248,137,125,200,72,139,83,8,65,255,192,15,182,10,131,225,15,74,15,190,132,
      17,0,35,1,0,66,138,140,17,16,35,1,0,72,43,208,139,66,252,211,232,72,137,83,8,137,67,24,15,182,10,131,225,15,74,15,190,132,17,0,35,1,0,66,138,140,17,16,35,1,0,72,43,208,139,66,252,211,232,72,137,83,8,137,67,28,15,182,10,131,225,15,74,15,190,132,
      17,0,35,1,0,66,138,140,17,16,35,1,0,72,43,208,139,66,252,211,232,137,67,32,72,141,66,4,72,137,83,8,139,10,72,137,67,8,137,75,36,69,59,193,15,133,83,255,255,255,15,40,117,192,255,198,102,15,127,117,224,72,141,85,224,137,117,216,72,139,203,232,
      147,251,255,255,15,16,69,208,76,141,92,36,96,73,139,198,73,139,91,32,73,139,115,40,73,139,123,48,243,65,15,127,54,15,40,116,36,80,243,65,15,127,70,16,73,139,227,65,95,65,94,93,195,204,204,72,131,236,40,232,207,39,0,0,72,139,64,96,72,131,196,
      40,195,204,204,64,83,72,131,236,32,72,139,217,232,182,39,0,0,72,137,88,96,72,131,196,32,91,195,72,131,236,40,232,163,39,0,0,72,139,64,104,72,131,196,40,195,204,204,64,83,72,131,236,32,72,139,217,232,138,39,0,0,72,137,88,104,72,131,196,32,91,
      195,64,83,72,131,236,32,72,139,217,72,137,17,232,111,39,0,0,72,59,88,88,115,11,232,100,39,0,0,72,139,72,88,235,2,51,201,72,137,75,8,232,83,39,0,0,72,137,88,88,72,139,195,72,131,196,32,91,195,204,204,72,137,92,36,8,87,72,131,236,32,72,139,249,
      232,50,39,0,0,72,59,120,88,117,53,232,39,39,0,0,72,139,80,88,72,133,210,116,39,72,139,90,8,72,59,250,116,10,72,139,211,72,133,219,116,22,235,237,232,6,39,0,0,72,137,88,88,72,139,92,36,48,72,131,196,32,95,195,232,46,38,0,0,204,204,72,139,196,
      72,137,88,16,72,137,104,24,72,137,112,32,87,72,131,236,96,131,96,220,0,73,139,249,131,96,224,0,73,139,240,131,96,228,0,72,139,233,131,96,232,0,131,96,236,0,73,139,89,8,198,64,216,0,72,137,80,8,232,174,38,0,0,72,137,88,96,72,139,93,56,232,161,
      38,0,0,72,137,88,104,232,152,38,0,0,72,139,79,56,72,141,84,36,64,76,139,71,8,198,68,36,32,0,139,9,72,3,72,96,72,139,71,16,68,139,8,232,216,248,255,255,198,68,36,56,0,72,141,68,36,64,72,131,100,36,48,0,72,141,84,36,112,131,100,36,40,0,76,139,
      207,76,139,198,72,137,68,36,32,72,139,205,232,115,53,0,0,76,141,92,36,96,73,139,91,24,73,139,107,32,73,139,115,40,73,139,227,95,195,204,72,139,196,76,137,72,32,76,137,64,24,72,137,80,16,72,137,72,8,83,72,131,236,112,72,139,217,131,96,200,0,72,
      137,72,224,76,137,64,232,232,4,38,0,0,72,141,84,36,88,139,11,72,139,64,16,255,21,179,198,1,0,199,68,36,64,0,0,0,0,235,0,139,68,36,64,72,131,196,112,91,195,204,204,204,72,133,201,116,103,136,84,36,16,72,131,236,72,129,57,99,115,109,224,117,83,
      131,121,24,4,117,77,139,65,32,45,32,5,147,25,131,248,2,119,64,72,139,65,48,72,133,192,116,55,72,99,80,4,133,210,116,17,72,3,81,56,72,139,73,40,232,42,0,0,0,235,32,235,30,246,0,16,116,25,72,139,65,40,72,139,8,72,133,201,116,13,72,139,1,72,139,
      64,16,255,21,52,198,1,0,72,131,196,72,195,204,204,204,72,255,226,204,64,83,72,131,236,32,72,139,217,232,90,37,0,0,72,139,80,88,235,9,72,57,26,116,18,72,139,82,8,72,133,210,117,242,141,66,1,72,131,196,32,91,195,51,192,235,246,204,72,99,2,72,3,
      193,131,122,4,0,124,22,76,99,74,4,72,99,82,8,73,139,12,9,76,99,4,10,77,3,193,73,3,192,195,204,72,137,92,36,8,87,72,131,236,32,72,139,57,72,139,217,129,63,82,67,67,224,116,18,129,63,77,79,67,224,116,10,129,63,99,115,109,224,116,34,235,19,232,
      229,36,0,0,131,120,48,0,126,8,232,218,36,0,0,255,72,48,72,139,92,36,48,51,192,72,131,196,32,95,195,232,197,36,0,0,72,137,120,32,72,139,91,8,232,184,36,0,0,72,137,88,40,232,159,24,0,0,204,204,204,72,137,92,36,8,72,137,116,36,16,72,137,124,36,
      24,65,86,72,131,236,32,128,121,8,0,76,139,242,72,139,241,116,76,72,139,1,72,133,192,116,68,72,131,207,255,72,255,199,128,60,56,0,117,247,72,141,79,1,232,85,24,0,0,72,139,216,72,133,192,116,28,76,139,6,72,141,87,1,72,139,200,232,234,68,0,0,72,
      139,195,65,198,70,8,1,73,137,6,51,219,72,139,203,232,21,24,0,0,235,10,72,139,1,72,137,2,198,66,8,0,72,139,92,36,48,72,139,116,36,56,72,139,124,36,64,72,131,196,32,65,94,195,204,204,204,64,83,72,131,236,32,128,121,8,0,72,139,217,116,8,72,139,
      9,232,217,23,0,0,72,131,35,0,198,67,8,0,72,131,196,32,91,195,204,204,204,64,83,72,131,236,32,255,21,232,95,1,0,72,133,192,116,19,72,139,24,72,139,200,232,172,23,0,0,72,139,195,72,133,219,117,237,72,131,196,32,91,195,204,204,72,59,202,116,25,
      72,131,194,9,72,141,65,9,72,43,208,138,8,58,12,16,117,10,72,255,192,132,201,117,242,51,192,195,27,192,131,200,1,195,204,72,131,236,40,232,139,68,0,0,132,192,117,4,50,192,235,18,232,30,35,0,0,132,192,117,7,232,169,68,0,0,235,236,176,1,72,131,
      196,40,195,72,131,236,40,132,201,117,10,232,71,35,0,0,232,142,68,0,0,176,1,72,131,196,40,195,204,204,204,72,131,236,40,232,47,35,0,0,176,1,72,131,196,40,195,72,131,236,40,232,95,35,0,0,72,133,192,15,149,192,72,131,196,40,195,72,131,236,40,51,
      201,232,9,36,0,0,176,1,72,131,196,40,195,204,204,72,131,236,40,232,19,0,0,0,72,133,192,116,6,255,21,208,195,1,0,232,71,34,0,0,204,204,204,72,139,5,153,146,1,0,195,204,204,204,204,204,204,102,102,15,31,132,0,0,0,0,0,87,139,194,72,139,249,73,139,
      200,243,170,73,139,195,95,195,204,204,204,204,204,204,102,102,15,31,132,0,0,0,0,0,76,139,217,15,182,210,73,185,1,1,1,1,1,1,1,1,76,15,175,202,73,131,248,16,15,134,242,0,0,0,102,73,15,110,193,102,15,96,192,73,129,248,128,0,0,0,119,16,233,107,0,
      0,0,102,102,102,15,31,132,0,0,0,0,0,246,5,17,143,1,0,2,117,151,15,17,1,76,3,193,72,131,193,16,72,131,225,240,76,43,193,77,139,200,73,193,233,7,116,61,76,59,13,46,131,1,0,15,135,96,0,0,0,15,41,1,15,41,65,16,72,129,193,128,0,0,0,15,41,65,160,15,
      41,65,176,73,255,201,15,41,65,192,15,41,65,208,15,41,65,224,102,15,41,65,240,117,212,73,131,224,127,77,139,200,73,193,233,4,116,19,15,31,128,0,0,0,0,15,17,1,72,131,193,16,73,255,201,117,244,73,131,224,15,116,6,66,15,17,68,1,240,73,139,195,195,
      15,31,64,0,15,43,1,15,43,65,16,72,129,193,128,0,0,0,15,43,65,160,15,43,65,176,73,255,201,15,43,65,192,15,43,65,208,15,43,65,224,15,43,65,240,117,213,15,174,248,73,131,224,127,235,156,102,102,102,102,15,31,132,0,0,0,0,0,73,139,209,76,141,13,102,
      178,255,255,67,139,132,129,0,48,2,0,76,3,200,73,3,200,73,139,195,65,255,225,102,144,72,137,81,241,137,81,249,102,137,81,253,136,81,255,195,144,72,137,81,244,137,81,252,195,72,137,81,247,136,81,255,195,72,137,81,243,137,81,251,136,81,255,195,
      15,31,68,0,0,72,137,81,242,137,81,250,102,137,81,254,195,72,137,16,195,72,137,16,102,137,80,8,136,80,10,195,15,31,68,0,0,72,137,16,102,137,80,8,195,72,137,16,72,137,80,8,195,131,61,241,129,1,0,2,76,139,193,68,15,183,202,125,46,72,139,209,65,
      15,183,0,73,131,192,2,102,133,192,117,243,73,131,232,2,76,59,194,116,8,102,69,57,8,117,241,235,6,102,69,57,8,117,4,73,139,192,195,51,192,195,51,201,139,209,235,18,102,69,57,8,73,15,68,208,102,65,57,8,116,87,73,131,192,2,65,141,64,1,168,14,117,
      230,102,65,59,201,117,36,184,1,0,255,255,102,15,110,200,235,4,73,131,192,16,243,65,15,111,0,102,15,58,99,200,21,117,239,72,99,193,73,141,4,64,195,102,65,15,110,201,243,65,15,111,0,102,15,58,99,200,65,115,7,72,99,193,73,141,20,64,116,6,73,131,
      192,16,235,228,72,139,194,195,204,204,204,72,141,5,201,134,1,0,72,137,5,154,152,1,0,176,1,195,204,204,204,72,131,236,40,72,141,13,153,142,1,0,232,56,8,0,0,72,141,13,165,142,1,0,232,44,8,0,0,176,1,72,131,196,40,195,204,176,1,195,204,72,131,236,
      40,232,27,12,0,0,176,1,72,131,196,40,195,64,83,72,131,236,32,72,139,29,15,129,1,0,72,139,203,232,43,94,0,0,72,139,203,232,119,1,0,0,72,139,203,232,211,94,0,0,72,139,203,232,207,97,0,0,72,139,203,232,191,1,0,0,176,1,72,131,196,32,91,195,204,204,
      204,51,201,233,133,252,255,255,204,64,83,72,131,236,32,72,139,13,199,151,1,0,131,200,255,240,15,193,1,131,248,1,117,31,72,139,13,180,151,1,0,72,141,29,213,128,1,0,72,59,203,116,12,232,7,91,0,0,72,137,29,156,151,1,0,176,1,72,131,196,32,91,195,
      72,131,236,40,72,139,13,225,151,1,0,232,232,90,0,0,72,139,13,221,151,1,0,72,131,37,205,151,1,0,0,232,212,90,0,0,72,139,13,57,151,1,0,72,131,37,193,151,1,0,0,232,192,90,0,0,72,139,13,45,151,1,0,72,131,37,29,151,1,0,0,232,172,90,0,0,72,131,37,
      24,151,1,0,0,176,1,72,131,196,40,195,204,72,141,21,201,212,0,0,72,141,13,194,211,0,0,233,201,90,0,0,204,72,131,236,40,132,201,116,22,72,131,61,100,151,1,0,0,116,5,232,69,97,0,0,176,1,72,131,196,40,195,72,141,21,151,212,0,0,72,141,13,144,211,
      0,0,72,131,196,40,233,19,91,0,0,204,204,204,72,131,236,40,232,31,71,0,0,176,1,72,131,196,40,195,72,131,236,40,232,175,72,0,0,72,133,192,15,149,192,72,131,196,40,195,72,131,236,40,232,103,73,0,0,176,1,72,131,196,40,195,64,83,72,131,236,32,72,
      139,217,232,42,0,0,0,72,133,192,116,20,72,139,203,255,21,132,191,1,0,133,192,116,7,184,1,0,0,0,235,2,51,192,72,131,196,32,91,195,204,72,137,13,169,139,1,0,195,64,83,72,131,236,32,51,201,232,19,70,0,0,144,72,139,29,107,127,1,0,139,203,131,225,
      63,72,51,29,135,139,1,0,72,211,203,51,201,232,17,70,0,0,72,139,195,72,131,196,32,91,195,139,5,126,139,1,0,195,204,69,51,192,65,141,80,2,233,204,0,0,0,51,210,51,201,68,141,66,1,233,191,0,0,0,204,204,204,72,137,13,81,139,1,0,195,64,83,72,131,236,
      32,72,131,100,36,56,0,76,141,68,36,56,139,217,72,141,21,158,211,0,0,51,201,255,21,166,89,1,0,133,192,116,31,72,139,76,36,56,72,141,21,158,211,0,0,255,21,168,89,1,0,72,133,192,116,8,139,203,255,21,195,190,1,0,72,139,76,36,56,72,133,201,116,6,
      255,21,243,88,1,0,72,131,196,32,91,195,204,64,83,72,131,236,32,139,217,232,47,100,0,0,131,248,1,116,40,101,72,139,4,37,96,0,0,0,139,144,188,0,0,0,193,234,8,246,194,1,117,17,255,21,245,88,1,0,72,139,200,139,211,255,21,114,90,1,0,139,203,232,99,
      255,255,255,139,203,255,21,115,88,1,0,204,204,204,51,192,129,249,99,115,109,224,15,148,192,195,72,137,92,36,8,68,137,68,36,24,137,84,36,16,85,72,139,236,72,131,236,80,139,217,69,133,192,117,74,51,201,255,21,247,88,1,0,72,133,192,116,61,185,77,
      90,0,0,102,57,8,117,51,72,99,72,60,72,3,200,129,57,80,69,0,0,117,36,184,11,2,0,0,102,57,65,24,117,25,131,185,132,0,0,0,14,118,16,131,185,248,0,0,0,0,116,7,139,203,232,229,254,255,255,72,141,69,24,198,69,40,0,72,137,69,224,76,141,77,212,72,141,
      69,32,72,137,69,232,76,141,69,224,72,141,69,40,72,137,69,240,72,141,85,216,184,2,0,0,0,72,141,77,208,137,69,212,137,69,216,232,221,0,0,0,131,125,32,0,116,11,72,139,92,36,96,72,131,196,80,93,195,139,203,232,237,254,255,255,204,64,83,72,131,236,
      32,72,139,217,128,61,220,137,1,0,0,15,133,159,0,0,0,184,1,0,0,0,135,5,199,137,1,0,72,139,1,139,8,133,201,117,52,72,139,5,127,125,1,0,139,200,131,225,63,72,139,21,163,137,1,0,72,59,208,116,19,72,51,194,72,211,200,69,51,192,51,210,51,201,255,21,
      67,189,1,0,72,141,13,164,138,1,0,235,12,131,249,1,117,13,72,141,13,174,138,1,0,232,165,4,0,0,144,72,139,3,131,56,0,117,19,72,141,21,41,81,1,0,72,141,13,2,81,1,0,232,69,10,0,0,72,141,21,38,81,1,0,72,141,13,23,81,1,0,232,50,10,0,0,72,139,67,8,
      131,56,0,117,14,198,5,62,137,1,0,1,72,139,67,16,198,0,1,72,131,196,32,91,195,232,8,16,0,0,144,204,204,204,72,137,92,36,8,76,137,76,36,32,87,72,131,236,32,73,139,217,73,139,248,139,10,232,112,67,0,0,144,72,139,207,232,23,255,255,255,144,139,11,
      232,123,67,0,0,72,139,92,36,48,72,131,196,32,95,195,72,137,92,36,8,85,86,87,65,86,65,87,72,139,236,72,131,236,48,51,255,68,139,241,133,201,15,132,83,1,0,0,141,65,255,131,248,1,118,22,232,59,98,0,0,141,95,22,137,24,232,221,87,0,0,139,251,233,
      53,1,0,0,232,145,76,0,0,72,141,29,178,136,1,0,65,184,4,1,0,0,72,139,211,51,201,232,34,109,0,0,72,139,53,51,147,1,0,72,137,29,12,147,1,0,72,133,246,116,5,64,56,62,117,3,72,139,243,72,141,69,72,72,137,125,64,76,141,77,64,72,137,68,36,32,69,51,
      192,72,137,125,72,51,210,72,139,206,232,81,1,0,0,76,139,125,64,65,184,1,0,0,0,72,139,85,72,73,139,207,232,219,0,0,0,72,139,216,72,133,192,117,24,232,174,97,0,0,187,12,0,0,0,51,201,137,24,232,80,86,0,0,233,106,255,255,255,78,141,4,248,72,139,
      211,72,141,69,72,72,139,206,76,141,77,64,72,137,68,36,32,232,255,0,0,0,65,131,254,1,117,22,139,69,64,255,200,72,137,29,137,146,1,0,137,5,123,146,1,0,51,201,235,105,72,141,85,56,72,137,125,56,72,139,203,232,163,98,0,0,139,240,133,192,116,25,72,
      139,77,56,232,244,85,0,0,72,139,203,72,137,125,56,232,232,85,0,0,139,254,235,63,72,139,85,56,72,139,207,72,139,194,72,57,58,116,12,72,141,64,8,72,255,193,72,57,56,117,244,137,13,39,146,1,0,51,201,72,137,125,56,72,137,21,34,146,1,0,232,177,85,
      0,0,72,139,203,72,137,125,56,232,165,85,0,0,72,139,92,36,96,139,199,72,131,196,48,65,95,65,94,95,94,93,195,204,204,64,83,72,131,236,32,72,184,255,255,255,255,255,255,255,31,76,139,202,72,59,200,115,61,51,210,72,131,200,255,73,247,240,76,59,200,
      115,47,72,193,225,3,77,15,175,200,72,139,193,72,247,208,73,59,193,118,28,73,3,201,186,1,0,0,0,232,114,97,0,0,51,201,72,139,216,232,64,85,0,0,72,139,195,235,2,51,192,72,131,196,32,91,195,204,204,204,72,139,196,72,137,88,8,72,137,104,16,72,137,
      112,24,72,137,120,32,65,84,65,86,65,87,72,131,236,32,76,139,124,36,96,77,139,225,73,139,216,76,139,242,72,139,249,73,131,39,0,73,199,1,1,0,0,0,72,133,210,116,7,72,137,26,73,131,198,8,64,50,237,128,63,34,117,15,64,132,237,64,182,34,64,15,148,
      197,72,255,199,235,55,73,255,7,72,133,219,116,7,138,7,136,3,72,255,195,15,190,55,72,255,199,139,206,232,80,109,0,0,133,192,116,18,73,255,7,72,133,219,116,7,138,7,136,3,72,255,195,72,255,199,64,132,246,116,28,64,132,237,117,176,64,128,254,32,
      116,6,64,128,254,9,117,164,72,133,219,116,9,198,67,255,0,235,3,72,255,207,64,50,246,138,7,132,192,15,132,212,0,0,0,60,32,116,4,60,9,117,7,72,255,199,138,7,235,241,132,192,15,132,189,0,0,0,77,133,246,116,7,73,137,30,73,131,198,8,73,255,4,36,186,
      1,0,0,0,51,192,235,5,72,255,199,255,192,138,15,128,249,92,116,244,128,249,34,117,48,132,194,117,24,64,132,246,116,10,56,79,1,117,5,72,255,199,235,9,51,210,64,132,246,64,15,148,198,209,232,235,16,255,200,72,133,219,116,6,198,3,92,72,255,195,73,
      255,7,133,192,117,236,138,7,132,192,116,70,64,132,246,117,8,60,32,116,61,60,9,116,57,133,210,116,45,72,133,219,116,7,136,3,72,255,195,138,7,15,190,200,232,105,108,0,0,133,192,116,18,73,255,7,72,255,199,72,133,219,116,7,138,7,136,3,72,255,195,
      73,255,7,72,255,199,233,102,255,255,255,72,133,219,116,6,198,3,0,72,255,195,73,255,7,233,34,255,255,255,77,133,246,116,4,73,131,38,0,73,255,4,36,72,139,92,36,64,72,139,108,36,72,72,139,116,36,80,72,139,124,36,88,72,131,196,32,65,95,65,94,65,
      92,195,204,72,133,201,117,4,131,200,255,195,72,139,65,16,72,57,1,117,18,72,139,5,243,120,1,0,72,137,1,72,137,65,8,72,137,65,16,51,192,195,204,72,137,84,36,16,72,137,76,36,8,85,72,139,236,72,131,236,64,72,141,69,16,72,137,69,232,76,141,77,40,
      72,141,69,24,72,137,69,240,76,141,69,232,184,2,0,0,0,72,141,85,224,72,141,77,32,137,69,40,137,69,224,232,26,3,0,0,72,131,196,64,93,195,76,139,220,73,137,75,8,72,131,236,56,73,141,67,8,73,137,67,232,77,141,75,24,184,2,0,0,0,77,141,67,232,73,141,
      83,32,137,68,36,80,73,141,75,16,137,68,36,88,232,27,3,0,0,72,131,196,56,195,204,204,72,139,209,72,141,13,166,133,1,0,233,109,255,255,255,204,72,137,92,36,16,72,137,108,36,24,72,137,116,36,32,87,65,86,65,87,72,131,236,32,72,139,1,51,237,76,139,
      249,72,139,24,72,133,219,15,132,104,1,0,0,76,139,21,29,120,1,0,76,139,75,8,73,139,242,72,51,51,77,51,202,72,139,91,16,65,139,202,131,225,63,73,51,218,72,211,203,72,211,206,73,211,201,76,59,203,15,133,167,0,0,0,72,43,222,184,0,2,0,0,72,193,251,
      3,72,59,216,72,139,251,72,15,71,248,141,69,32,72,3,251,72,15,68,248,72,59,251,114,30,68,141,69,8,72,139,215,72,139,206,232,69,107,0,0,51,201,76,139,240,232,23,82,0,0,77,133,246,117,40,72,141,123,4,65,184,8,0,0,0,72,139,215,72,139,206,232,33,
      107,0,0,51,201,76,139,240,232,243,81,0,0,77,133,246,15,132,202,0,0,0,76,139,21,127,119,1,0,77,141,12,222,73,141,28,254,73,139,246,72,139,203,73,43,201,72,131,193,7,72,193,233,3,76,59,203,72,15,71,205,72,133,201,116,16,73,139,194,73,139,249,243,
      72,171,76,139,21,74,119,1,0,65,184,64,0,0,0,73,141,121,8,65,139,200,65,139,194,131,224,63,43,200,73,139,71,8,72,139,16,65,139,192,72,211,202,73,51,210,73,137,17,72,139,21,27,119,1,0,139,202,131,225,63,43,193,138,200,73,139,7,72,211,206,72,51,
      242,72,139,8,72,137,49,65,139,200,72,139,21,249,118,1,0,139,194,131,224,63,43,200,73,139,7,72,211,207,72,51,250,72,139,16,72,137,122,8,72,139,21,219,118,1,0,139,194,131,224,63,68,43,192,73,139,7,65,138,200,72,211,203,72,51,218,72,139,8,51,192,
      72,137,89,16,235,3,131,200,255,72,139,92,36,72,72,139,108,36,80,72,139,116,36,88,72,131,196,32,65,95,65,94,95,195,72,137,92,36,8,72,137,108,36,16,72,137,116,36,24,87,65,86,65,87,72,131,236,32,72,139,1,72,139,241,72,139,24,72,133,219,117,8,131,
      200,255,233,207,0,0,0,76,139,5,107,118,1,0,65,139,200,73,139,248,72,51,59,131,225,63,72,139,91,8,72,211,207,73,51,216,72,211,203,72,141,71,255,72,131,248,253,15,135,159,0,0,0,65,139,200,77,139,240,131,225,63,76,139,255,72,139,235,72,131,235,
      8,72,59,223,114,85,72,139,3,73,59,198,116,239,73,51,192,76,137,51,72,211,200,255,21,253,181,1,0,76,139,5,14,118,1,0,72,139,6,65,139,200,131,225,63,72,139,16,76,139,10,72,139,66,8,77,51,200,73,51,192,73,211,201,72,211,200,77,59,207,117,5,72,59,
      197,116,176,77,139,249,73,139,249,72,139,232,72,139,216,235,162,72,131,255,255,116,15,72,139,207,232,45,80,0,0,76,139,5,194,117,1,0,72,139,6,72,139,8,76,137,1,72,139,6,72,139,8,76,137,65,8,72,139,6,72,139,8,76,137,65,16,51,192,72,139,92,36,64,
      72,139,108,36,72,72,139,116,36,80,72,131,196,32,65,95,65,94,95,195,204,204,72,137,92,36,8,76,137,76,36,32,87,72,131,236,32,73,139,217,73,139,248,139,10,232,12,60,0,0,144,72,139,207,232,19,253,255,255,139,248,139,11,232,22,60,0,0,139,199,72,139,
      92,36,48,72,131,196,32,95,195,204,72,137,92,36,8,76,137,76,36,32,87,72,131,236,32,73,139,217,73,139,248,139,10,232,208,59,0,0,144,72,139,207,232,135,254,255,255,139,248,139,11,232,218,59,0,0,139,199,72,139,92,36,48,72,131,196,32,95,195,204,233,
      59,0,0,0,204,204,204,72,131,236,40,72,141,13,125,130,1,0,232,148,0,0,0,72,141,13,121,130,1,0,232,164,0,0,0,72,139,13,125,130,1,0,232,180,0,0,0,72,139,13,105,130,1,0,72,131,196,40,233,164,0,0,0,72,137,92,36,8,87,72,131,236,32,51,255,72,57,61,
      61,130,1,0,116,4,51,192,235,72,232,210,68,0,0,232,217,105,0,0,72,139,216,72,133,192,117,5,131,207,255,235,39,72,139,203,232,176,0,0,0,72,133,192,117,5,131,207,255,235,14,72,137,5,31,130,1,0,72,137,5,0,130,1,0,51,201,232,221,78,0,0,72,139,203,
      232,213,78,0,0,139,199,72,139,92,36,48,72,131,196,32,95,195,72,131,236,40,72,139,9,72,59,13,238,129,1,0,116,5,232,35,0,0,0,72,131,196,40,195,204,204,72,131,236,40,72,139,9,72,59,13,202,129,1,0,116,5,232,7,0,0,0,72,131,196,40,195,204,204,72,133,
      201,116,59,72,137,92,36,8,87,72,131,236,32,72,139,1,72,139,217,72,139,249,235,15,72,139,200,232,110,78,0,0,72,141,127,8,72,139,7,72,133,192,117,236,72,139,203,232,90,78,0,0,72,139,92,36,48,72,131,196,32,95,195,204,204,204,72,137,92,36,8,72,137,
      108,36,16,72,137,116,36,24,87,65,86,65,87,72,131,236,48,76,139,241,51,246,139,206,77,139,198,65,138,22,235,36,128,250,61,72,141,65,1,72,15,68,193,72,139,200,72,131,200,255,72,255,192,65,56,52,0,117,247,73,255,192,76,3,192,65,138,16,132,210,117,
      216,72,255,193,186,8,0,0,0,232,24,90,0,0,72,139,216,72,133,192,116,108,76,139,248,65,138,6,132,192,116,95,72,131,205,255,72,255,197,65,56,52,46,117,247,72,255,197,60,61,116,53,186,1,0,0,0,72,139,205,232,229,89,0,0,72,139,248,72,133,192,116,37,
      77,139,198,72,139,213,72,139,200,232,255,50,0,0,51,201,133,192,117,72,73,137,63,73,131,199,8,232,149,77,0,0,76,3,245,235,171,72,139,203,232,248,254,255,255,51,201,232,129,77,0,0,235,3,72,139,243,51,201,232,117,77,0,0,72,139,92,36,80,72,139,198,
      72,139,116,36,96,72,139,108,36,88,72,131,196,48,65,95,65,94,95,195,69,51,201,72,137,116,36,32,69,51,192,51,210,232,99,78,0,0,204,204,204,72,137,92,36,8,72,137,108,36,16,72,137,116,36,24,87,72,131,236,32,51,237,72,139,250,72,43,249,72,139,217,
      72,131,199,7,139,245,72,193,239,3,72,59,202,72,15,71,253,72,133,255,116,26,72,139,3,72,133,192,116,6,255,21,133,178,1,0,72,131,195,8,72,255,198,72,59,247,117,230,72,139,92,36,48,72,139,108,36,56,72,139,116,36,64,72,131,196,32,95,195,72,137,92,
      36,8,87,72,131,236,32,72,139,250,72,139,217,72,59,202,116,27,72,139,3,72,133,192,116,10,255,21,65,178,1,0,133,192,117,11,72,131,195,8,72,59,223,235,227,51,192,72,139,92,36,48,72,131,196,32,95,195,204,204,204,184,99,115,109,224,59,200,116,3,51,
      192,195,139,200,233,1,0,0,0,204,72,137,92,36,8,72,137,108,36,16,72,137,116,36,24,87,72,131,236,32,72,139,242,139,249,232,230,58,0,0,69,51,201,72,139,216,72,133,192,15,132,62,1,0,0,72,139,8,72,139,193,76,141,129,192,0,0,0,73,59,200,116,13,57,
      56,116,12,72,131,192,16,73,59,192,117,243,73,139,193,72,133,192,15,132,19,1,0,0,76,139,64,8,77,133,192,15,132,6,1,0,0,73,131,248,5,117,13,76,137,72,8,65,141,64,252,233,245,0,0,0,73,131,248,1,117,8,131,200,255,233,231,0,0,0,72,139,107,8,72,137,
      115,8,131,120,4,8,15,133,186,0,0,0,72,131,193,48,72,141,145,144,0,0,0,235,8,76,137,73,8,72,131,193,16,72,59,202,117,243,129,56,141,0,0,192,139,123,16,116,122,129,56,142,0,0,192,116,107,129,56,143,0,0,192,116,92,129,56,144,0,0,192,116,77,129,
      56,145,0,0,192,116,62,129,56,146,0,0,192,116,47,129,56,147,0,0,192,116,32,129,56,180,2,0,192,116,17,129,56,181,2,0,192,139,215,117,64,186,141,0,0,0,235,54,186,142,0,0,0,235,47,186,133,0,0,0,235,40,186,138,0,0,0,235,33,186,132,0,0,0,235,26,186,
      129,0,0,0,235,19,186,134,0,0,0,235,12,186,131,0,0,0,235,5,186,130,0,0,0,137,83,16,185,8,0,0,0,73,139,192,255,21,195,176,1,0,137,123,16,235,16,139,72,4,76,137,72,8,73,139,192,255,21,174,176,1,0,72,137,107,8,233,19,255,255,255,51,192,72,139,92,
      36,48,72,139,108,36,56,72,139,116,36,64,72,131,196,32,95,195,204,204,72,131,236,56,198,68,36,32,0,232,6,0,0,0,72,131,196,56,195,204,64,83,72,131,236,48,51,192,68,139,209,72,133,210,117,25,232,47,86,0,0,187,22,0,0,0,137,24,232,207,75,0,0,139,
      195,72,131,196,48,91,195,77,133,192,116,226,15,182,76,36,96,102,137,2,72,141,65,1,76,59,192,119,12,232,0,86,0,0,187,34,0,0,0,235,207,65,141,65,254,187,34,0,0,0,59,195,119,184,136,76,36,96,65,139,202,72,131,196,48,91,233,3,0,0,0,204,204,204,72,
      137,92,36,8,72,137,108,36,16,72,137,116,36,24,87,65,86,65,87,72,131,236,32,69,51,255,65,139,233,73,139,248,76,139,218,76,139,210,65,139,223,68,56,124,36,96,116,16,65,141,71,45,247,217,102,137,2,141,88,212,76,141,82,2,77,139,194,51,210,73,141,
      114,2,139,193,77,139,202,247,245,139,200,131,250,9,184,87,0,0,0,68,141,112,217,102,65,15,70,198,72,255,195,102,3,194,102,65,137,2,133,201,116,8,76,139,214,72,59,223,114,202,72,59,223,114,25,102,69,137,59,232,80,85,0,0,187,34,0,0,0,137,24,232,
      240,74,0,0,139,195,235,35,102,68,137,62,65,15,183,0,65,15,183,9,102,65,137,1,73,131,233,2,102,65,137,8,73,131,192,2,77,59,193,114,227,51,192,72,139,92,36,64,72,139,108,36,72,72,139,116,36,80,72,131,196,32,65,95,65,94,95,195,72,131,236,40,131,
      61,173,134,1,0,0,117,45,72,133,201,117,26,232,237,84,0,0,199,0,22,0,0,0,232,142,74,0,0,184,255,255,255,127,72,131,196,40,195,72,133,210,116,225,72,131,196,40,233,54,1,0,0,69,51,192,72,131,196,40,233,2,0,0,0,204,204,72,137,92,36,8,72,137,108,
      36,16,72,137,116,36,24,87,65,86,65,87,72,131,236,64,72,139,242,72,139,217,72,133,201,117,26,232,148,84,0,0,199,0,22,0,0,0,232,53,74,0,0,184,255,255,255,127,233,209,0,0,0,72,133,246,116,225,73,139,208,72,141,76,36,32,232,25,1,0,0,72,139,84,36,
      40,72,131,186,56,1,0,0,0,117,18,72,139,214,72,139,203,232,191,0,0,0,139,248,233,137,0,0,0,65,190,0,1,0,0,76,141,61,3,231,0,0,102,68,57,51,115,26,15,182,11,65,246,68,79,2,1,116,10,72,139,130,16,1,0,0,138,12,1,15,182,193,235,18,15,183,11,72,141,
      84,36,40,232,142,100,0,0,72,139,84,36,40,72,131,195,2,15,183,232,139,253,102,68,57,54,115,26,15,182,14,65,246,68,79,2,1,116,10,72,139,130,16,1,0,0,138,12,1,15,182,193,235,18,15,183,14,72,141,84,36,40,232,83,100,0,0,72,139,84,36,40,72,131,198,
      2,15,183,192,43,248,117,4,133,237,117,132,128,124,36,56,0,116,12,72,139,68,36,32,131,160,168,3,0,0,253,139,199,72,139,92,36,96,72,139,108,36,104,72,139,116,36,112,72,131,196,64,65,95,65,94,95,195,204,76,139,218,76,139,209,69,15,183,2,77,141,
      82,2,65,15,183,19,77,141,91,2,65,141,64,191,131,248,25,69,141,72,32,141,66,191,69,15,71,200,141,74,32,131,248,25,65,139,193,15,71,202,43,193,117,5,69,133,201,117,201,195,204,204,72,137,92,36,8,72,137,116,36,16,87,72,131,236,32,198,65,24,0,72,
      139,249,72,141,113,8,72,133,210,116,5,15,16,2,235,16,131,61,221,132,1,0,0,117,13,15,16,5,44,116,1,0,243,15,127,6,235,78,232,193,52,0,0,72,137,7,72,139,214,72,139,136,144,0,0,0,72,137,14,72,139,136,136,0,0,0,72,137,79,16,72,139,200,232,42,100,
      0,0,72,139,15,72,141,87,16,232,82,100,0,0,72,139,15,139,129,168,3,0,0,168,2,117,13,131,200,2,137,129,168,3,0,0,198,71,24,1,72,139,92,36,48,72,139,199,72,139,116,36,56,72,131,196,32,95,195,204,199,68,36,16,0,0,0,0,139,68,36,16,233,87,71,0,0,204,
      204,204,233,27,101,0,0,204,204,204,72,131,236,40,232,63,52,0,0,72,139,64,24,72,133,192,116,8,255,21,184,172,1,0,235,0,232,45,11,0,0,144,139,5,162,108,1,0,76,139,193,72,139,209,131,248,5,15,140,130,0,0,0,65,246,192,1,116,17,51,201,102,57,10,15,
      132,249,0,0,0,72,131,194,2,235,241,131,225,31,184,32,0,0,0,72,43,193,72,247,217,77,27,201,51,201,76,35,200,73,209,233,75,141,4,72,76,59,192,116,14,102,57,10,116,9,72,131,194,2,72,59,208,117,242,73,43,208,72,209,250,73,59,209,15,133,186,0,0,0,
      73,141,20,80,197,241,239,201,197,245,117,10,197,253,215,193,133,192,197,248,119,117,6,72,131,194,32,235,231,102,57,10,15,132,142,0,0,0,72,131,194,2,235,241,131,248,1,124,118,65,246,192,1,116,13,51,201,102,57,10,116,118,72,131,194,2,235,245,131,
      225,15,184,16,0,0,0,72,43,193,72,247,217,77,27,201,51,201,76,35,200,73,209,233,75,141,4,72,76,59,192,116,14,102,57,10,116,9,72,131,194,2,72,59,208,117,242,73,43,208,72,209,250,73,59,209,117,59,73,141,20,80,15,87,201,243,15,111,2,102,15,117,193,
      102,15,215,192,133,192,117,6,72,131,194,16,235,234,102,57,10,116,19,72,131,194,2,235,245,51,201,102,57,10,116,6,72,131,194,2,235,245,73,43,208,72,209,250,72,139,194,195,204,204,204,139,5,118,107,1,0,76,139,210,76,139,193,131,248,5,15,140,204,
      0,0,0,65,246,192,1,116,41,72,141,4,81,72,139,209,72,59,200,15,132,161,1,0,0,51,201,102,57,10,15,132,150,1,0,0,72,131,194,2,72,59,208,117,238,233,136,1,0,0,131,225,31,184,32,0,0,0,72,43,193,73,139,208,72,247,217,77,27,219,76,35,216,73,209,235,
      77,59,211,77,15,66,218,51,201,75,141,4,88,76,59,192,116,14,102,57,10,116,9,72,131,194,2,72,59,208,117,242,73,43,208,72,209,250,73,59,211,15,133,69,1,0,0,77,141,12,80,73,139,194,73,43,195,72,131,224,224,72,3,194,73,141,20,64,76,59,202,116,29,
      197,241,239,201,196,193,117,117,9,197,253,215,193,133,192,197,248,119,117,9,73,131,193,32,76,59,202,117,227,75,141,4,80,235,10,102,65,57,9,116,9,73,131,193,2,76,59,200,117,241,73,139,209,233,235,0,0,0,131,248,1,15,140,198,0,0,0,65,246,192,1,
      116,41,72,141,4,81,73,139,208,76,59,192,15,132,204,0,0,0,51,201,102,57,10,15,132,193,0,0,0,72,131,194,2,72,59,208,117,238,233,179,0,0,0,131,225,15,184,16,0,0,0,72,43,193,73,139,208,72,247,217,77,27,219,76,35,216,73,209,235,77,59,211,77,15,66,
      218,51,201,75,141,4,88,76,59,192,116,14,102,57,10,116,9,72,131,194,2,72,59,208,117,242,73,43,208,72,209,250,73,59,211,117,116,73,139,194,77,141,12,80,73,43,195,15,87,201,72,131,224,240,72,3,194,73,141,20,64,235,21,102,15,111,193,102,65,15,117,
      1,102,15,215,192,133,192,117,9,73,131,193,16,76,59,202,117,230,75,141,4,80,235,14,102,65,57,9,15,132,55,255,255,255,73,131,193,2,76,59,200,117,237,233,41,255,255,255,72,141,4,81,73,139,208,76,59,192,116,16,51,201,102,57,10,116,9,72,131,194,2,
      72,59,208,117,242,73,43,208,72,209,250,72,139,194,195,204,204,72,131,236,56,72,137,76,36,32,72,137,84,36,40,72,133,210,116,3,72,137,10,65,177,1,72,141,84,36,32,51,201,232,7,0,0,0,72,131,196,56,195,204,204,72,137,92,36,8,72,137,108,36,24,86,87,
      65,84,65,86,65,87,72,131,236,64,69,51,228,65,15,182,241,69,139,240,72,139,250,76,57,34,117,21,232,31,79,0,0,199,0,22,0,0,0,232,192,68,0,0,233,121,5,0,0,69,133,246,116,9,65,141,64,254,131,248,34,119,221,72,139,209,72,141,76,36,32,232,160,251,
      255,255,76,139,63,65,139,236,76,137,124,36,120,65,15,183,31,73,141,71,2,235,10,72,139,7,15,183,24,72,131,192,2,186,8,0,0,0,72,137,7,15,183,203,232,161,97,0,0,133,192,117,226,139,198,185,253,255,0,0,131,206,2,102,131,251,45,15,69,240,141,67,213,
      102,133,193,117,13,72,139,7,15,183,24,72,131,192,2,72,137,7,184,230,9,0,0,65,131,202,255,185,16,255,0,0,186,96,6,0,0,65,187,48,0,0,0,65,184,240,6,0,0,68,141,72,128,65,247,198,239,255,255,255,15,133,97,2,0,0,102,65,59,219,15,130,183,1,0,0,102,
      131,251,58,115,11,15,183,195,65,43,195,233,161,1,0,0,102,59,217,15,131,135,1,0,0,102,59,218,15,130,148,1,0,0,185,106,6,0,0,102,59,217,115,10,15,183,195,43,194,233,123,1,0,0,102,65,59,216,15,130,118,1,0,0,185,250,6,0,0,102,59,217,115,11,15,183,
      195,65,43,192,233,92,1,0,0,102,65,59,217,15,130,87,1,0,0,185,112,9,0,0,102,59,217,115,11,15,183,195,65,43,193,233,61,1,0,0,102,59,216,15,130,57,1,0,0,184,240,9,0,0,102,59,216,115,13,15,183,195,45,230,9,0,0,233,29,1,0,0,185,102,10,0,0,102,59,
      217,15,130,20,1,0,0,141,65,10,102,59,216,115,10,15,183,195,43,193,233,253,0,0,0,185,230,10,0,0,102,59,217,15,130,244,0,0,0,141,65,10,102,59,216,114,224,141,72,118,102,59,217,15,130,224,0,0,0,141,65,10,102,59,216,114,204,185,102,12,0,0,102,59,
      217,15,130,202,0,0,0,141,65,10,102,59,216,114,182,141,72,118,102,59,217,15,130,182,0,0,0,141,65,10,102,59,216,114,162,141,72,118,102,59,217,15,130,162,0,0,0,141,65,10,102,59,216,114,142,185,80,14,0,0,102,59,217,15,130,140,0,0,0,141,65,10,102,
      59,216,15,130,116,255,255,255,141,72,118,102,59,217,114,120,141,65,10,102,59,216,15,130,96,255,255,255,141,72,70,102,59,217,114,100,141,65,10,102,59,216,15,130,76,255,255,255,185,64,16,0,0,102,59,217,114,78,141,65,10,102,59,216,15,130,54,255,
      255,255,185,224,23,0,0,102,59,217,114,56,141,65,10,102,59,216,15,130,32,255,255,255,15,183,195,185,16,24,0,0,102,43,193,102,131,248,9,119,27,233,10,255,255,255,184,26,255,0,0,102,59,216,15,130,252,254,255,255,131,200,255,131,248,255,117,36,15,
      183,203,141,65,191,141,81,159,131,248,25,118,10,131,250,25,118,5,65,139,194,235,12,131,250,25,141,65,224,15,71,193,131,192,201,133,192,116,7,184,10,0,0,0,235,103,72,139,7,65,184,223,255,0,0,15,183,16,72,141,72,2,72,137,15,141,66,168,102,65,133,
      192,116,60,69,133,246,184,8,0,0,0,65,15,69,198,72,131,193,254,72,137,15,68,139,240,102,133,210,116,58,102,57,17,116,53,232,58,76,0,0,199,0,22,0,0,0,232,219,65,0,0,65,131,202,255,65,187,48,0,0,0,235,25,15,183,25,72,141,65,2,72,137,7,184,16,0,
      0,0,69,133,246,65,15,69,198,68,139,240,51,210,65,139,194,65,247,246,65,188,16,255,0,0,65,191,96,6,0,0,68,139,202,68,139,192,102,65,59,219,15,130,168,1,0,0,102,131,251,58,115,11,15,183,203,65,43,203,233,146,1,0,0,102,65,59,220,15,131,115,1,0,
      0,102,65,59,223,15,130,131,1,0,0,184,106,6,0,0,102,59,216,115,11,15,183,203,65,43,207,233,105,1,0,0,184,240,6,0,0,102,59,216,15,130,96,1,0,0,141,72,10,102,59,217,115,10,15,183,203,43,200,233,73,1,0,0,184,102,9,0,0,102,59,216,15,130,64,1,0,0,
      141,72,10,102,59,217,114,224,141,65,118,102,59,216,15,130,44,1,0,0,141,72,10,102,59,217,114,204,141,65,118,102,59,216,15,130,24,1,0,0,141,72,10,102,59,217,114,184,141,65,118,102,59,216,15,130,4,1,0,0,141,72,10,102,59,217,114,164,141,65,118,102,
      59,216,15,130,240,0,0,0,141,72,10,102,59,217,114,144,184,102,12,0,0,102,59,216,15,130,218,0,0,0,141,72,10,102,59,217,15,130,118,255,255,255,141,65,118,102,59,216,15,130,194,0,0,0,141,72,10,102,59,217,15,130,94,255,255,255,141,65,118,102,59,216,
      15,130,170,0,0,0,141,72,10,102,59,217,15,130,70,255,255,255,184,80,14,0,0,102,59,216,15,130,144,0,0,0,141,72,10,102,59,217,15,130,44,255,255,255,141,65,118,102,59,216,114,124,141,72,10,102,59,217,15,130,24,255,255,255,141,65,70,102,59,216,114,
      104,141,72,10,102,59,217,15,130,4,255,255,255,184,64,16,0,0,102,59,216,114,82,141,72,10,102,59,217,15,130,238,254,255,255,184,224,23,0,0,102,59,216,114,60,141,72,10,102,59,217,15,130,216,254,255,255,15,183,195,141,81,38,102,43,194,102,131,248,
      9,119,33,15,183,203,43,202,235,21,184,26,255,0,0,102,59,216,115,8,15,183,203,65,43,204,235,3,131,201,255,131,249,255,117,36,15,183,211,141,66,191,131,248,25,141,66,159,118,10,131,248,25,118,5,65,139,202,235,12,131,248,25,141,74,224,15,71,202,
      131,233,55,65,59,202,116,55,65,59,206,115,50,65,59,232,114,14,117,5,65,59,201,118,7,185,12,0,0,0,235,11,65,15,175,238,3,233,185,8,0,0,0,72,139,7,15,183,24,72,131,192,2,72,137,7,11,241,233,238,253,255,255,72,139,7,69,51,228,76,139,124,36,120,
      72,131,192,254,72,137,7,102,133,219,116,21,102,57,24,116,16,232,189,73,0,0,199,0,22,0,0,0,232,94,63,0,0,64,246,198,8,117,44,76,137,63,68,56,100,36,56,116,12,72,139,68,36,32,131,160,168,3,0,0,253,72,139,79,8,72,133,201,116,6,72,139,7,72,137,1,
      51,192,233,192,0,0,0,139,222,65,190,255,255,255,127,131,227,1,65,191,0,0,0,128,64,246,198,4,117,15,133,219,116,75,64,246,198,2,116,64,65,59,239,118,64,131,230,2,232,82,73,0,0,199,0,34,0,0,0,133,219,117,56,131,205,255,68,56,100,36,56,116,12,72,
      139,76,36,32,131,161,168,3,0,0,253,72,139,87,8,72,133,210,116,6,72,139,15,72,137,10,139,197,235,95,65,59,238,119,192,64,246,198,2,116,207,247,221,235,203,133,246,116,39,68,56,100,36,56,116,12,72,139,76,36,32,131,161,168,3,0,0,253,72,139,87,8,
      72,133,210,116,6,72,139,15,72,137,10,65,139,199,235,37,68,56,100,36,56,116,12,72,139,76,36,32,131,161,168,3,0,0,253,72,139,87,8,72,133,210,116,6,72,139,15,72,137,10,65,139,198,76,141,92,36,64,73,139,91,48,73,139,107,64,73,139,227,65,95,65,94,
      65,92,95,94,195,204,204,204,204,204,204,204,204,204,204,204,184,77,90,0,0,102,57,1,117,32,72,99,65,60,72,3,193,129,56,80,69,0,0,117,17,185,11,2,0,0,102,57,72,24,117,6,184,1,0,0,0,195,51,192,195,204,204,204,76,99,65,60,69,51,201,76,3,193,76,139,
      210,65,15,183,64,20,69,15,183,88,6,72,131,192,24,73,3,192,69,133,219,116,30,139,80,12,76,59,210,114,10,139,72,8,3,202,76,59,209,114,14,65,255,193,72,131,192,40,69,59,203,114,226,51,192,195,204,204,204,204,204,204,204,204,204,204,204,204,72,137,
      92,36,8,87,72,131,236,32,72,139,217,72,141,61,44,146,255,255,72,139,207,232,100,255,255,255,133,192,116,34,72,43,223,72,139,211,72,139,207,232,130,255,255,255,72,133,192,116,15,139,64,36,193,232,31,247,208,131,224,1,235,2,51,192,72,139,92,36,
      48,72,131,196,32,95,195,204,204,204,204,204,204,204,204,204,102,102,15,31,132,0,0,0,0,0,72,137,76,36,8,72,137,84,36,24,68,137,68,36,16,73,199,193,32,5,147,25,235,8,204,204,204,204,204,204,102,144,195,204,204,204,204,204,204,102,15,31,132,0,0,
      0,0,0,195,204,204,204,72,139,5,165,161,1,0,72,141,21,218,193,255,255,72,59,194,116,35,101,72,139,4,37,48,0,0,0,72,139,137,152,0,0,0,72,59,72,16,114,6,72,59,72,8,118,7,185,13,0,0,0,205,41,195,204,72,131,236,40,232,143,63,0,0,72,133,192,116,10,
      185,22,0,0,0,232,176,63,0,0,246,5,133,97,1,0,2,116,42,185,23,0,0,0,255,21,168,60,1,0,133,192,116,7,185,7,0,0,0,205,41,65,184,1,0,0,0,186,21,0,0,64,65,141,72,2,232,25,61,0,0,185,3,0,0,0,232,3,226,255,255,204,204,204,72,131,236,40,72,141,13,141,
      1,0,0,232,56,90,0,0,137,5,58,97,1,0,131,248,255,116,37,72,141,21,202,110,1,0,139,200,232,247,90,0,0,133,192,116,14,199,5,45,111,1,0,254,255,255,255,176,1,235,7,232,8,0,0,0,50,192,72,131,196,40,195,204,72,131,236,40,139,13,254,96,1,0,131,249,
      255,116,12,232,52,90,0,0,131,13,237,96,1,0,255,176,1,72,131,196,40,195,204,204,72,131,236,40,232,19,0,0,0,72,133,192,116,5,72,131,196,40,195,232,36,255,255,255,204,204,204,204,72,137,92,36,8,72,137,116,36,16,87,72,131,236,32,131,61,178,96,1,
      0,255,117,7,51,192,233,144,0,0,0,255,21,47,59,1,0,139,13,157,96,1,0,139,248,232,30,90,0,0,72,131,202,255,51,246,72,59,194,116,103,72,133,192,116,5,72,139,240,235,93,139,13,123,96,1,0,232,70,90,0,0,133,192,116,78,186,128,0,0,0,141,74,129,232,
      85,89,0,0,139,13,95,96,1,0,72,139,216,72,133,192,116,36,72,139,208,232,31,90,0,0,133,192,116,18,72,139,195,199,67,120,254,255,255,255,72,139,222,72,139,240,235,13,139,13,51,96,1,0,51,210,232,252,89,0,0,72,139,203,232,20,243,255,255,139,207,255,
      21,224,59,1,0,72,139,198,72,139,92,36,48,72,139,116,36,56,72,131,196,32,95,195,204,64,83,72,131,236,32,72,139,217,139,13,249,95,1,0,131,249,255,116,51,72,133,219,117,14,232,114,89,0,0,139,13,228,95,1,0,72,139,216,51,210,232,170,89,0,0,72,133,
      219,116,20,72,141,5,106,109,1,0,72,59,216,116,8,72,139,203,232,177,242,255,255,72,131,196,32,91,195,204,204,204,72,131,236,40,72,133,201,116,17,72,141,5,68,109,1,0,72,59,200,116,5,232,142,242,255,255,72,131,196,40,195,204,76,139,2,233,0,0,0,
      0,72,139,196,72,137,88,8,72,137,112,16,72,137,120,24,76,137,112,32,131,121,16,0,73,139,216,76,139,210,15,132,172,0,0,0,76,99,73,16,76,141,53,46,143,255,255,72,139,122,8,51,246,76,3,207,131,202,255,69,51,192,65,15,182,9,131,225,15,74,15,190,132,
      49,0,35,1,0,66,138,140,49,16,35,1,0,76,43,200,69,139,89,252,65,211,235,69,133,219,116,107,73,139,66,16,68,139,16,65,15,182,9,131,225,15,74,15,190,132,49,0,35,1,0,66,138,140,49,16,35,1,0,76,43,200,65,139,65,252,211,232,3,240,139,198,73,3,194,
      72,3,199,72,59,216,114,43,65,15,182,9,65,255,192,131,225,15,74,15,190,132,49,0,35,1,0,66,138,140,49,16,35,1,0,76,43,200,65,139,81,252,211,234,255,202,69,59,195,114,165,69,133,192,116,4,139,194,235,3,131,200,255,72,139,92,36,8,72,139,116,36,16,
      72,139,124,36,24,76,139,116,36,32,195,204,76,139,220,73,137,91,24,77,137,75,32,137,84,36,16,85,86,87,65,84,65,85,65,86,65,87,72,131,236,32,72,139,65,8,64,50,237,69,50,246,73,137,67,8,51,255,77,139,225,69,139,232,72,139,217,72,141,112,255,76,
      139,254,57,57,126,67,69,139,99,16,65,59,252,117,6,72,139,240,64,181,1,65,59,253,117,6,76,139,248,65,182,1,64,132,237,116,5,69,132,246,117,26,72,141,84,36,96,72,139,203,232,37,1,0,0,255,199,59,59,125,7,72,139,68,36,96,235,198,76,139,100,36,120,
      73,139,4,36,73,137,116,36,8,15,16,3,15,17,0,15,16,75,16,15,17,72,16,72,139,132,36,128,0,0,0,72,139,8,76,137,120,8,15,16,3,15,17,1,15,16,75,16,72,139,92,36,112,15,17,73,16,72,131,196,32,65,95,65,94,65,93,65,92,95,94,93,195,204,204,72,137,92,36,
      8,72,137,116,36,16,87,72,131,236,48,72,139,124,36,96,73,139,240,139,218,76,139,87,8,77,59,80,8,15,135,141,0,0,0,76,57,81,8,15,135,131,0,0,0,73,139,64,8,73,139,210,72,43,81,8,73,43,194,72,59,208,125,53,15,16,1,15,17,68,36,32,102,15,115,216,8,
      102,72,15,126,192,76,59,208,118,85,72,139,76,36,32,72,141,84,36,40,232,94,0,0,0,72,139,68,36,40,255,195,72,57,71,8,119,228,235,55,15,16,7,65,139,217,15,17,68,36,32,102,15,115,216,8,102,72,15,126,192,73,57,64,8,118,28,72,139,76,36,32,72,141,84,
      36,40,232,37,0,0,0,72,139,76,36,40,255,203,72,57,78,8,119,228,139,195,235,3,131,200,255,72,139,92,36,64,72,139,116,36,72,72,131,196,48,95,195,204,76,139,2,76,141,29,210,140,255,255,76,139,209,76,139,202,65,15,182,8,131,225,15,74,15,190,132,25,
      0,35,1,0,66,138,140,25,16,35,1,0,76,43,192,65,139,64,252,211,232,139,200,76,137,2,131,225,3,193,232,2,65,137,74,20,65,137,66,16,131,249,1,116,27,131,249,2,116,22,131,249,3,117,74,72,139,2,139,8,72,131,192,4,72,137,2,65,137,74,24,195,72,139,2,
      139,8,72,131,192,4,72,137,2,65,137,74,24,72,139,18,15,182,10,131,225,15,74,15,190,132,25,0,35,1,0,66,138,140,25,16,35,1,0,72,43,208,139,66,252,211,232,73,137,17,65,137,66,28,195,204,204,131,122,12,0,76,139,201,15,132,193,0,0,0,72,99,82,12,73,
      3,208,76,141,5,33,140,255,255,72,137,81,8,15,182,10,131,225,15,74,15,190,132,1,0,35,1,0,66,138,140,1,16,35,1,0,72,43,208,139,66,252,211,232,73,137,81,8,65,137,1,73,137,81,16,15,182,10,131,225,15,74,15,190,132,1,0,35,1,0,66,138,140,1,16,35,1,
      0,72,43,208,139,66,252,211,232,73,137,81,8,65,137,65,24,15,182,10,131,225,15,74,15,190,132,1,0,35,1,0,66,138,140,1,16,35,1,0,72,43,208,139,66,252,211,232,73,137,81,8,65,137,65,28,15,182,10,131,225,15,74,15,190,132,1,0,35,1,0,66,138,140,1,16,
      35,1,0,72,43,208,139,66,252,211,232,65,137,65,32,72,141,66,4,73,137,81,8,139,10,73,137,65,8,65,137,73,36,235,3,131,33,0,73,139,193,195,204,204,204,64,83,72,131,236,32,51,192,15,87,192,136,65,24,72,139,217,72,137,65,28,72,137,65,36,15,17,65,48,
      76,137,65,64,68,137,73,72,57,66,12,116,69,72,99,82,12,73,3,208,76,141,5,44,139,255,255,72,137,81,8,15,182,10,131,225,15,74,15,190,132,1,0,35,1,0,66,138,140,1,16,35,1,0,72,43,208,139,66,252,211,232,72,139,203,72,137,83,8,137,3,72,137,83,16,232,
      15,0,0,0,235,2,137,1,72,139,195,72,131,196,32,91,195,204,204,51,192,76,141,29,223,138,255,255,136,65,24,15,87,192,72,137,65,28,76,139,193,72,137,65,36,15,17,65,48,72,139,65,8,68,138,16,72,141,80,1,68,136,81,24,72,137,81,8,65,246,194,1,116,39,
      15,182,10,131,225,15,74,15,190,132,25,0,35,1,0,66,138,140,25,16,35,1,0,72,43,208,139,66,252,211,232,65,137,64,28,73,137,80,8,65,246,194,2,116,14,139,2,72,131,194,4,73,137,80,8,65,137,64,32,65,246,194,4,116,39,15,182,10,131,225,15,74,15,190,132,
      25,0,35,1,0,66,138,140,25,16,35,1,0,72,43,208,139,66,252,211,232,65,137,64,36,73,137,80,8,139,2,76,141,74,4,65,137,64,40,65,138,194,36,48,77,137,72,8,65,246,194,8,116,59,60,16,117,16,73,99,9,73,141,65,4,73,137,64,8,73,137,72,48,195,60,32,15,
      133,179,0,0,0,73,99,1,73,141,81,4,73,137,80,8,73,137,64,48,72,141,66,4,72,99,10,73,137,64,8,233,144,0,0,0,60,16,117,48,65,15,182,9,131,225,15,74,15,190,132,25,0,35,1,0,66,138,140,25,16,35,1,0,76,43,200,65,139,64,72,65,139,81,252,211,234,3,194,
      77,137,72,8,73,137,64,48,195,60,32,117,92,65,15,182,9,65,139,80,72,131,225,15,74,15,190,132,25,0,35,1,0,66,138,140,25,16,35,1,0,76,43,200,65,139,65,252,211,232,77,137,72,8,141,12,2,73,137,72,48,65,15,182,9,131,225,15,74,15,190,132,25,0,35,1,
      0,66,138,140,25,16,35,1,0,76,43,200,65,139,65,252,211,232,77,137,72,8,141,12,2,73,137,72,56,195,72,139,196,83,86,87,65,84,65,85,65,86,65,87,72,129,236,240,0,0,0,15,41,112,184,72,139,5,104,89,1,0,72,51,196,72,137,132,36,208,0,0,0,69,139,225,73,
      139,216,72,139,250,76,139,249,72,137,76,36,112,72,137,76,36,96,72,137,84,36,120,68,137,76,36,72,232,136,208,255,255,76,139,232,72,137,68,36,104,72,139,215,72,139,203,232,153,249,255,255,139,240,131,127,72,0,116,23,232,64,248,255,255,131,120,
      120,254,15,133,102,2,0,0,139,119,72,131,238,2,235,31,232,41,248,255,255,131,120,120,254,116,20,232,30,248,255,255,139,112,120,232,22,248,255,255,199,64,120,254,255,255,255,232,10,248,255,255,255,64,48,131,123,8,0,116,64,72,99,83,8,72,3,87,8,
      15,182,10,131,225,15,76,141,5,156,136,255,255,74,15,190,132,1,0,35,1,0,66,15,182,140,1,16,35,1,0,72,43,208,139,66,252,211,232,137,132,36,176,0,0,0,72,137,148,36,184,0,0,0,235,16,131,164,36,176,0,0,0,0,72,139,148,36,184,0,0,0,72,141,132,36,176,
      0,0,0,72,137,68,36,48,72,137,84,36,56,72,141,132,36,176,0,0,0,72,137,68,36,80,72,137,84,36,88,72,141,68,36,80,72,137,68,36,32,76,141,76,36,48,69,139,196,139,214,72,141,140,36,176,0,0,0,232,168,249,255,255,144,72,141,132,36,176,0,0,0,72,137,132,
      36,144,0,0,0,72,139,132,36,184,0,0,0,72,137,132,36,152,0,0,0,76,139,116,36,56,76,59,240,15,130,47,1,0,0,76,59,116,36,88,15,134,36,1,0,0,72,141,84,36,56,72,139,76,36,48,232,247,250,255,255,76,137,116,36,56,72,139,92,36,48,15,16,115,16,15,17,180,
      36,128,0,0,0,15,40,68,36,48,102,15,127,132,36,160,0,0,0,72,141,84,36,56,72,139,203,232,198,250,255,255,139,67,16,76,43,240,76,137,116,36,56,72,141,68,36,48,72,137,68,36,32,68,139,206,76,141,132,36,160,0,0,0,65,139,212,72,141,76,36,80,232,209,
      249,255,255,139,240,137,68,36,68,131,100,36,64,0,69,51,201,102,15,111,198,102,15,115,216,8,102,15,126,192,102,15,115,222,4,102,15,126,241,133,201,68,15,69,200,68,137,76,36,64,69,133,201,116,126,141,70,2,137,71,72,141,65,255,131,248,1,118,23,
      73,99,201,72,3,79,8,65,184,3,1,0,0,73,139,215,232,62,83,0,0,235,55,72,139,68,36,96,72,139,16,131,249,2,117,13,139,132,36,140,0,0,0,76,139,4,16,235,11,68,139,132,36,140,0,0,0,76,3,194,73,99,201,72,3,79,8,65,185,3,1,0,0,232,181,83,0,0,73,139,205,
      232,105,206,255,255,235,24,76,139,108,36,104,139,116,36,68,76,139,124,36,112,72,139,124,36,120,68,139,100,36,72,233,163,254,255,255,232,9,246,255,255,131,120,48,0,126,8,232,254,245,255,255,255,72,48,72,139,140,36,208,0,0,0,72,51,204,232,91,22,
      0,0,15,40,180,36,224,0,0,0,72,129,196,240,0,0,0,65,95,65,94,65,93,65,92,95,94,91,195,232,7,245,255,255,144,204,204,72,139,196,83,86,87,65,84,65,85,65,87,72,129,236,168,0,0,0,72,139,249,69,51,228,68,137,100,36,32,68,33,164,36,240,0,0,0,76,33,
      100,36,40,76,33,100,36,64,68,136,96,128,68,33,96,132,68,33,96,136,68,33,96,140,68,33,96,144,68,33,96,148,232,123,245,255,255,72,139,64,40,72,137,68,36,56,232,109,245,255,255,72,139,64,32,72,137,68,36,48,72,139,119,80,72,137,180,36,248,0,0,0,
      72,139,95,64,72,139,71,48,72,137,68,36,80,76,139,127,40,72,139,71,72,72,137,68,36,112,72,139,71,104,72,137,68,36,120,139,71,120,137,132,36,232,0,0,0,139,71,56,137,132,36,224,0,0,0,72,139,203,232,29,244,255,255,232,20,245,255,255,72,137,112,32,
      232,11,245,255,255,72,137,88,40,232,2,245,255,255,72,139,80,32,72,139,82,40,72,141,140,36,136,0,0,0,232,109,205,255,255,76,139,232,72,137,68,36,72,76,57,103,88,116,25,199,132,36,240,0,0,0,1,0,0,0,232,207,244,255,255,72,139,72,112,72,137,76,36,
      64,65,184,0,1,0,0,73,139,215,72,139,76,36,80,232,211,81,0,0,72,139,216,72,137,68,36,40,72,131,248,2,125,19,72,139,92,196,112,72,133,219,15,132,24,1,0,0,72,137,92,36,40,73,139,215,72,139,203,232,215,81,0,0,72,139,124,36,56,76,139,124,36,48,235,
      124,199,68,36,32,1,0,0,0,232,110,244,255,255,131,96,64,0,232,101,244,255,255,139,140,36,232,0,0,0,137,72,120,72,139,180,36,248,0,0,0,131,188,36,240,0,0,0,0,116,30,178,1,72,139,206,232,99,206,255,255,72,139,68,36,64,76,141,72,32,68,139,64,24,
      139,80,4,139,8,235,13,76,141,78,32,68,139,70,24,139,86,4,139,14,255,21,120,48,1,0,68,139,100,36,32,72,139,92,36,40,72,139,124,36,56,76,139,124,36,48,76,139,108,36,72,73,139,205,232,179,204,255,255,69,133,228,117,50,129,62,99,115,109,224,117,
      42,131,126,24,4,117,36,139,70,32,45,32,5,147,25,131,248,2,119,23,72,139,78,40,232,102,206,255,255,133,192,116,10,178,1,72,139,206,232,228,205,255,255,232,187,243,255,255,76,137,120,32,232,178,243,255,255,72,137,120,40,232,169,243,255,255,139,
      140,36,224,0,0,0,137,72,120,232,154,243,255,255,199,64,120,254,255,255,255,72,139,195,72,129,196,168,0,0,0,65,95,65,93,65,92,95,94,91,195,232,182,242,255,255,144,204,72,139,194,73,139,208,72,255,224,204,204,204,73,139,192,76,139,210,72,139,208,
      69,139,193,73,255,226,204,72,131,97,16,0,72,141,5,224,169,0,0,72,137,65,8,72,141,5,197,169,0,0,72,137,1,72,139,193,195,204,204,64,83,72,131,236,32,72,139,217,72,139,194,72,141,13,177,164,0,0,15,87,192,72,137,11,72,141,83,8,72,141,72,8,15,17,
      2,232,103,206,255,255,72,141,5,140,169,0,0,72,137,3,72,139,195,72,131,196,32,91,195,64,83,72,131,236,32,76,139,9,73,139,216,65,131,32,0,185,99,115,109,224,65,184,32,5,147,25,65,139,1,59,193,117,93,65,131,121,24,4,117,86,65,139,65,32,65,43,192,
      131,248,2,119,23,72,139,66,40,73,57,65,40,117,13,199,3,1,0,0,0,65,139,1,59,193,117,51,65,131,121,24,4,117,44,65,139,73,32,65,43,200,131,249,2,119,32,73,131,121,48,0,117,25,232,149,242,255,255,199,64,64,1,0,0,0,184,1,0,0,0,199,3,1,0,0,0,235,2,
      51,192,72,131,196,32,91,195,204,72,137,92,36,8,87,72,131,236,32,65,139,248,77,139,193,232,99,255,255,255,139,216,133,192,117,8,232,88,242,255,255,137,120,120,139,195,72,139,92,36,48,72,131,196,32,95,195,72,137,92,36,8,72,137,108,36,24,72,137,
      116,36,32,87,65,84,65,85,65,86,65,87,72,131,236,32,72,139,234,76,139,233,72,133,210,15,132,185,0,0,0,69,50,255,51,246,57,50,15,142,140,0,0,0,232,95,202,255,255,72,139,208,73,139,69,48,76,99,96,12,73,131,196,4,76,3,226,232,72,202,255,255,72,139,
      208,73,139,69,48,72,99,72,12,68,139,52,10,69,133,246,126,81,72,141,4,182,72,137,68,36,88,232,38,202,255,255,73,139,93,48,72,139,248,73,99,4,36,72,3,248,232,231,201,255,255,72,139,84,36,88,76,139,195,72,99,77,4,72,141,4,144,72,139,215,72,3,200,
      232,100,3,0,0,133,192,117,14,65,255,206,73,131,196,4,69,133,246,127,189,235,3,65,183,1,255,198,59,117,0,15,140,116,255,255,255,72,139,92,36,80,65,138,199,72,139,108,36,96,72,139,116,36,104,72,131,196,32,65,95,65,94,65,93,65,92,95,195,232,155,
      240,255,255,204,204,204,72,137,92,36,8,72,137,108,36,16,72,137,116,36,24,87,72,131,236,32,51,237,72,139,249,57,41,126,80,51,246,232,96,201,255,255,72,99,79,4,72,3,198,131,124,1,4,0,116,27,232,77,201,255,255,72,99,79,4,72,3,198,72,99,92,1,4,232,
      60,201,255,255,72,3,195,235,2,51,192,72,141,72,8,72,141,21,66,92,1,0,232,53,205,255,255,133,192,116,33,255,197,72,131,198,20,59,47,124,178,50,192,72,139,92,36,48,72,139,108,36,56,72,139,116,36,64,72,131,196,32,95,195,176,1,235,231,72,137,92,
      36,8,72,137,108,36,16,72,137,116,36,24,87,65,86,65,87,72,129,236,128,0,0,0,72,139,217,73,139,233,73,139,200,77,139,248,76,139,242,232,169,239,255,255,232,160,240,255,255,72,139,188,36,192,0,0,0,51,246,65,184,41,0,0,128,65,185,38,0,0,128,57,112,
      64,117,43,129,59,99,115,109,224,116,35,68,57,3,117,16,131,123,24,15,117,15,72,129,123,96,32,5,147,25,116,14,68,57,11,116,9,246,7,32,15,133,242,1,0,0,246,67,4,102,15,132,26,1,0,0,57,119,8,15,132,223,1,0,0,72,99,87,8,76,141,61,236,128,255,255,
      72,3,85,8,15,182,10,131,225,15,74,15,190,132,57,0,35,1,0,66,138,140,57,16,35,1,0,72,43,208,139,66,252,211,232,133,192,15,132,169,1,0,0,57,180,36,200,0,0,0,15,133,156,1,0,0,246,67,4,32,15,132,177,0,0,0,68,57,11,117,99,76,139,69,32,72,139,213,
      72,139,207,232,58,241,255,255,68,139,200,131,248,255,15,140,148,1,0,0,57,119,8,116,39,72,99,87,8,72,3,85,8,15,182,10,131,225,15,74,15,190,132,57,0,35,1,0,66,138,140,57,16,35,1,0,72,43,208,139,114,252,211,238,68,59,206,15,141,95,1,0,0,73,139,
      206,72,139,213,76,139,199,232,231,246,255,255,233,42,1,0,0,68,57,3,117,68,68,139,75,56,65,131,249,255,15,140,57,1,0,0,72,99,87,8,72,3,85,8,15,182,10,131,225,15,74,15,190,132,57,0,35,1,0,66,138,140,57,16,35,1,0,72,43,208,139,66,252,211,232,68,
      59,200,15,141,9,1,0,0,72,139,75,40,235,167,76,139,199,72,139,213,73,139,206,232,143,196,255,255,233,206,0,0,0,76,139,69,8,72,141,76,36,80,72,139,215,232,153,243,255,255,57,116,36,80,117,9,246,7,64,15,132,174,0,0,0,129,59,99,115,109,224,117,109,
      131,123,24,3,114,103,129,123,32,34,5,147,25,118,94,72,139,67,48,57,112,8,116,85,232,69,199,255,255,76,139,208,72,139,67,48,72,99,72,8,76,3,209,116,64,15,182,140,36,216,0,0,0,76,139,205,137,76,36,56,77,139,199,72,139,140,36,208,0,0,0,73,139,214,
      72,137,76,36,48,73,139,194,139,140,36,200,0,0,0,137,76,36,40,72,139,203,72,137,124,36,32,255,21,99,143,1,0,235,62,72,139,132,36,208,0,0,0,76,139,205,72,137,68,36,56,77,139,199,139,132,36,200,0,0,0,73,139,214,137,68,36,48,72,139,203,138,132,36,
      216,0,0,0,136,68,36,40,72,137,124,36,32,232,116,5,0,0,184,1,0,0,0,76,141,156,36,128,0,0,0,73,139,91,32,73,139,107,40,73,139,115,48,73,139,227,65,95,65,94,95,195,232,125,237,255,255,204,72,139,196,72,137,88,8,72,137,104,16,72,137,112,24,72,137,
      120,32,65,86,72,131,236,32,51,219,77,139,240,72,139,234,72,139,249,57,89,4,15,132,240,0,0,0,72,99,113,4,232,50,198,255,255,76,139,200,76,3,206,15,132,219,0,0,0,133,246,116,15,72,99,119,4,232,25,198,255,255,72,141,12,6,235,5,72,139,203,139,243,
      56,89,16,15,132,186,0,0,0,246,7,128,116,10,246,69,0,16,15,133,171,0,0,0,133,246,116,17,232,237,197,255,255,72,139,240,72,99,71,4,72,3,240,235,3,72,139,243,232,5,198,255,255,72,139,200,72,99,69,4,72,3,200,72,59,241,116,75,57,95,4,116,17,232,192,
      197,255,255,72,139,240,72,99,71,4,72,3,240,235,3,72,139,243,232,216,197,255,255,76,99,69,4,73,131,192,16,76,3,192,72,141,70,16,76,43,192,15,182,8,66,15,182,20,0,43,202,117,7,72,255,192,133,210,117,237,133,201,116,4,51,192,235,57,176,2,132,69,
      0,116,5,246,7,8,116,36,65,246,6,1,116,5,246,7,1,116,25,65,246,6,4,116,5,246,7,4,116,14,65,132,6,116,4,132,7,116,5,187,1,0,0,0,139,195,235,5,184,1,0,0,0,72,139,92,36,48,72,139,108,36,56,72,139,116,36,64,72,139,124,36,72,72,131,196,32,65,94,195,
      204,204,204,72,139,196,72,137,88,8,72,137,104,16,72,137,112,24,72,137,120,32,65,86,72,131,236,32,51,219,77,139,240,72,139,234,72,139,249,57,89,8,15,132,245,0,0,0,72,99,113,8,232,242,196,255,255,76,139,200,76,3,206,15,132,224,0,0,0,133,246,116,
      15,72,99,119,8,232,217,196,255,255,72,141,12,6,235,5,72,139,203,139,243,56,89,16,15,132,191,0,0,0,246,71,4,128,116,10,246,69,0,16,15,133,175,0,0,0,133,246,116,17,232,172,196,255,255,72,139,240,72,99,71,8,72,3,240,235,3,72,139,243,232,196,196,
      255,255,72,139,200,72,99,69,4,72,3,200,72,59,241,116,75,57,95,8,116,17,232,127,196,255,255,72,139,240,72,99,71,8,72,3,240,235,3,72,139,243,232,151,196,255,255,76,99,69,4,73,131,192,16,76,3,192,72,141,70,16,76,43,192,15,182,8,66,15,182,20,0,43,
      202,117,7,72,255,192,133,210,117,237,133,201,116,4,51,192,235,61,176,2,132,69,0,116,6,246,71,4,8,116,39,65,246,6,1,116,6,246,71,4,1,116,27,65,246,6,4,116,6,246,71,4,4,116,15,65,132,6,116,5,132,71,4,116,5,187,1,0,0,0,139,195,235,5,184,1,0,0,0,
      72,139,92,36,48,72,139,108,36,56,72,139,116,36,64,72,139,124,36,72,72,131,196,32,65,94,195,204,204,72,137,92,36,8,72,137,116,36,16,72,137,124,36,24,65,85,65,86,65,87,72,131,236,48,77,139,241,73,139,216,72,139,242,76,139,233,51,255,65,57,120,
      8,116,15,77,99,120,8,232,174,195,255,255,73,141,20,7,235,6,72,139,215,68,139,255,72,133,210,15,132,122,1,0,0,69,133,255,116,17,232,143,195,255,255,72,139,200,72,99,67,8,72,3,200,235,3,72,139,207,64,56,121,16,15,132,87,1,0,0,57,123,12,117,9,57,
      123,4,15,141,73,1,0,0,57,123,4,124,9,139,67,12,72,3,6,72,139,240,246,67,4,128,116,50,65,246,6,16,116,44,72,139,5,23,88,1,0,72,133,192,116,32,255,21,212,139,1,0,72,133,192,15,132,48,1,0,0,72,133,246,15,132,39,1,0,0,72,137,6,72,139,200,235,96,
      246,67,4,8,116,27,73,139,77,40,72,133,201,15,132,17,1,0,0,72,133,246,15,132,8,1,0,0,72,137,14,235,63,65,246,6,1,116,74,73,139,85,40,72,133,210,15,132,245,0,0,0,72,133,246,15,132,236,0,0,0,77,99,70,20,72,139,206,232,49,73,0,0,65,131,126,20,8,
      15,133,171,0,0,0,72,57,62,15,132,162,0,0,0,72,139,14,73,141,86,8,232,89,197,255,255,72,137,6,233,142,0,0,0,65,57,126,24,116,15,73,99,94,24,232,206,194,255,255,72,141,12,3,235,5,72,139,207,139,223,72,133,201,117,52,73,57,125,40,15,132,148,0,0,
      0,72,133,246,15,132,139,0,0,0,73,99,94,20,73,141,86,8,73,139,77,40,232,14,197,255,255,72,139,208,76,139,195,72,139,206,232,184,72,0,0,235,59,73,57,125,40,116,105,72,133,246,116,100,133,219,116,17,232,118,194,255,255,72,139,200,73,99,70,24,72,
      3,200,235,3,72,139,207,72,133,201,116,71,65,138,6,36,4,246,216,27,201,247,217,255,193,139,249,137,76,36,32,139,199,235,2,51,192,72,139,92,36,80,72,139,116,36,88,72,139,124,36,96,72,131,196,48,65,95,65,94,65,93,195,232,18,233,255,255,232,13,233,
      255,255,232,8,233,255,255,232,3,233,255,255,232,254,232,255,255,144,232,248,232,255,255,144,204,204,204,72,137,92,36,8,72,137,116,36,16,72,137,124,36,24,65,86,72,131,236,32,73,139,249,76,139,241,51,219,65,57,88,4,125,5,72,139,242,235,7,65,139,
      112,12,72,3,50,232,200,253,255,255,131,232,1,116,60,131,248,1,117,103,72,141,87,8,73,139,78,40,232,53,196,255,255,76,139,240,57,95,24,116,12,232,180,193,255,255,72,99,95,24,72,3,216,65,185,1,0,0,0,77,139,198,72,139,211,72,139,206,232,217,245,
      255,255,235,48,72,141,87,8,73,139,78,40,232,254,195,255,255,76,139,240,57,95,24,116,12,232,125,193,255,255,72,99,95,24,72,3,216,77,139,198,72,139,211,72,139,206,232,156,245,255,255,144,72,139,92,36,48,72,139,116,36,56,72,139,124,36,64,72,131,
      196,32,65,94,195,232,52,232,255,255,144,204,204,204,64,85,83,86,87,65,84,65,85,65,86,65,87,72,141,172,36,120,255,255,255,72,129,236,136,1,0,0,72,139,5,169,73,1,0,72,51,196,72,137,69,112,76,139,181,240,0,0,0,76,139,250,76,139,165,8,1,0,0,72,139,
      217,72,137,84,36,120,73,139,206,73,139,209,76,137,101,160,73,139,241,198,68,36,96,0,77,139,232,198,68,36,97,0,232,222,233,255,255,131,126,72,0,139,248,116,23,232,133,232,255,255,131,120,120,254,15,133,144,4,0,0,139,126,72,131,239,2,235,31,232,
      110,232,255,255,131,120,120,254,116,20,232,99,232,255,255,139,120,120,232,91,232,255,255,199,64,120,254,255,255,255,131,255,255,15,140,96,4,0,0,65,131,126,8,0,76,141,5,239,120,255,255,116,41,73,99,86,8,72,3,86,8,15,182,10,131,225,15,74,15,190,
      132,1,0,35,1,0,66,138,140,1,16,35,1,0,72,43,208,139,66,252,211,232,235,2,51,192,59,248,15,141,31,4,0,0,129,59,99,115,109,224,15,133,196,0,0,0,131,123,24,4,15,133,186,0,0,0,139,67,32,45,32,5,147,25,131,248,2,15,135,169,0,0,0,72,131,123,48,0,15,
      133,158,0,0,0,232,211,231,255,255,72,131,120,32,0,15,132,123,3,0,0,232,195,231,255,255,72,139,88,32,232,186,231,255,255,72,139,75,56,198,68,36,96,1,76,139,104,40,232,16,192,255,255,129,59,99,115,109,224,117,30,131,123,24,4,117,24,139,67,32,45,
      32,5,147,25,131,248,2,119,11,72,131,123,48,0,15,132,151,3,0,0,232,125,231,255,255,72,131,120,56,0,116,60,232,113,231,255,255,76,139,120,56,232,104,231,255,255,73,139,215,72,139,203,72,131,96,56,0,232,16,245,255,255,132,192,117,21,73,139,207,
      232,240,245,255,255,132,192,15,132,59,3,0,0,233,18,3,0,0,76,139,124,36,120,76,139,70,8,72,141,77,240,73,139,214,232,158,235,255,255,129,59,99,115,109,224,15,133,137,2,0,0,131,123,24,4,15,133,127,2,0,0,139,67,32,45,32,5,147,25,131,248,2,15,135,
      110,2,0,0,131,125,240,0,15,134,73,2,0,0,139,133,0,1,0,0,72,141,85,240,137,68,36,40,72,141,77,168,76,139,206,76,137,116,36,32,68,139,199,232,163,189,255,255,15,16,69,168,243,15,127,69,136,102,15,115,216,8,102,15,126,192,59,69,192,15,131,12,2,
      0,0,76,139,125,168,76,141,5,97,119,255,255,139,69,144,76,137,125,128,137,68,36,104,65,15,16,71,24,102,72,15,126,192,15,17,69,136,59,199,15,143,63,1,0,0,72,193,232,32,59,248,15,143,51,1,0,0,72,139,70,16,72,141,85,136,76,139,70,8,72,141,77,32,
      68,139,8,232,184,235,255,255,139,69,32,69,51,228,68,137,100,36,100,137,68,36,108,133,192,15,132,253,0,0,0,15,16,69,56,15,16,77,72,15,17,69,200,242,15,16,69,88,242,15,17,69,232,15,17,77,216,232,138,190,255,255,72,139,75,48,72,131,192,4,72,99,
      81,12,72,3,194,72,137,68,36,112,232,113,190,255,255,72,139,75,48,72,99,81,12,68,139,60,16,69,133,255,126,58,232,91,190,255,255,76,139,67,48,76,139,224,72,139,68,36,112,72,99,8,76,3,225,72,141,77,200,73,139,212,232,233,248,255,255,133,192,117,
      48,72,131,68,36,112,4,65,255,207,69,133,255,127,203,68,139,100,36,100,72,141,77,32,232,145,235,255,255,65,255,196,68,137,100,36,100,68,59,100,36,108,116,94,233,96,255,255,255,138,133,248,0,0,0,76,139,206,72,139,84,36,120,77,139,197,136,68,36,
      88,72,139,203,138,68,36,96,136,68,36,80,72,139,69,160,72,137,68,36,72,139,133,0,1,0,0,137,68,36,64,72,141,69,136,72,137,68,36,56,72,141,69,200,76,137,100,36,48,72,137,68,36,40,76,137,116,36,32,198,68,36,97,1,232,120,1,0,0,76,139,125,128,76,141,
      5,1,118,255,255,73,139,87,8,15,182,10,131,225,15,74,15,190,132,1,0,35,1,0,66,138,140,1,16,35,1,0,72,43,208,139,66,252,211,232,73,137,87,8,65,137,71,24,15,182,10,131,225,15,74,15,190,132,1,0,35,1,0,66,138,140,1,16,35,1,0,72,43,208,139,66,252,
      211,232,73,137,87,8,65,137,71,28,15,182,10,131,225,15,74,15,190,132,1,0,35,1,0,66,138,140,1,16,35,1,0,72,43,208,139,66,252,211,232,65,137,71,32,72,141,66,4,73,137,87,8,139,10,65,137,79,36,139,76,36,104,255,193,73,137,71,8,137,76,36,104,59,77,
      192,15,130,17,254,255,255,128,124,36,97,0,117,87,65,246,6,64,116,81,73,139,214,72,139,206,232,235,185,255,255,132,192,15,132,148,0,0,0,235,60,131,125,240,0,118,54,128,189,248,0,0,0,0,15,133,151,0,0,0,139,133,0,1,0,0,76,139,206,76,137,100,36,
      56,77,139,197,137,68,36,48,73,139,215,137,124,36,40,72,139,203,76,137,116,36,32,232,73,1,0,0,232,84,228,255,255,72,131,120,56,0,117,98,72,139,77,112,72,51,204,232,177,4,0,0,72,129,196,136,1,0,0,65,95,65,94,65,93,65,92,95,94,91,93,195,178,1,72,
      139,203,232,71,190,255,255,72,141,77,136,232,190,240,255,255,72,141,21,227,56,1,0,72,141,77,136,232,194,179,255,255,204,232,244,215,255,255,204,232,254,227,255,255,72,137,88,32,232,245,227,255,255,76,137,104,40,232,220,215,255,255,204,232,34,
      227,255,255,204,204,72,139,196,72,137,88,8,76,137,64,24,85,86,87,65,84,65,85,65,86,65,87,72,131,236,96,76,139,172,36,192,0,0,0,77,139,249,76,139,226,76,141,72,16,72,139,233,77,139,197,73,139,215,73,139,204,232,79,186,255,255,76,139,140,36,208,
      0,0,0,76,139,240,72,139,180,36,200,0,0,0,77,133,201,116,14,76,139,198,72,139,208,72,139,205,232,201,249,255,255,72,139,140,36,216,0,0,0,139,89,8,139,57,232,151,187,255,255,72,99,78,16,77,139,206,76,139,132,36,176,0,0,0,72,3,193,138,140,36,248,
      0,0,0,72,139,213,136,76,36,80,73,139,204,76,137,124,36,72,72,137,116,36,64,137,92,36,56,137,124,36,48,76,137,108,36,40,72,137,68,36,32,232,179,184,255,255,72,139,156,36,160,0,0,0,72,131,196,96,65,95,65,94,65,93,65,92,95,94,93,195,204,204,204,
      64,85,83,86,87,65,84,65,85,65,86,65,87,72,141,108,36,200,72,129,236,56,1,0,0,72,139,5,200,67,1,0,72,51,196,72,137,69,40,129,57,3,0,0,128,73,139,249,72,139,133,184,0,0,0,76,139,234,76,139,181,160,0,0,0,72,139,241,72,137,68,36,112,76,137,68,36,
      120,15,132,117,2,0,0,232,183,226,255,255,68,139,165,176,0,0,0,68,139,189,168,0,0,0,72,131,120,16,0,116,90,51,201,255,21,90,29,1,0,72,139,216,232,146,226,255,255,72,57,88,16,116,68,129,62,77,79,67,224,116,60,129,62,82,67,67,224,116,52,72,139,
      68,36,112,76,139,207,76,139,68,36,120,73,139,213,68,137,124,36,56,72,139,206,72,137,68,36,48,68,137,100,36,40,76,137,116,36,32,232,32,188,255,255,133,192,15,133,1,2,0,0,76,139,71,8,72,141,77,0,73,139,214,232,172,230,255,255,131,125,0,0,15,134,
      7,2,0,0,68,137,100,36,40,72,141,85,0,76,139,207,76,137,116,36,32,69,139,199,72,141,77,144,232,221,184,255,255,15,16,69,144,243,15,127,69,128,102,15,115,216,8,102,15,126,192,59,69,168,15,131,175,1,0,0,76,139,69,144,76,141,13,155,114,255,255,139,
      69,136,76,137,68,36,104,137,68,36,96,65,15,16,64,24,102,72,15,126,192,15,17,69,128,65,59,199,15,143,231,0,0,0,72,193,232,32,68,59,248,15,143,218,0,0,0,72,139,71,16,72,141,85,128,76,139,71,8,72,141,77,176,68,139,8,232,239,230,255,255,72,139,69,
      192,72,141,77,176,72,137,69,184,232,90,231,255,255,72,139,69,192,72,141,77,176,139,93,176,72,137,69,184,232,70,231,255,255,131,235,1,116,15,72,141,77,176,232,56,231,255,255,72,131,235,1,117,241,131,125,208,0,116,40,232,135,185,255,255,72,99,
      85,208,72,3,194,116,26,133,210,116,14,232,117,185,255,255,72,99,77,208,72,3,193,235,2,51,192,128,120,16,0,117,79,246,69,204,64,117,73,72,139,68,36,112,76,139,207,76,139,68,36,120,73,139,213,198,68,36,88,0,72,139,206,198,68,36,80,1,72,137,68,
      36,72,72,141,69,128,68,137,100,36,64,72,137,68,36,56,72,141,69,200,72,131,100,36,48,0,72,137,68,36,40,76,137,116,36,32,232,9,253,255,255,76,139,68,36,104,76,141,13,145,113,255,255,73,139,80,8,15,182,10,131,225,15,74,15,190,132,9,0,35,1,0,66,
      138,140,9,16,35,1,0,72,43,208,139,66,252,211,232,73,137,80,8,65,137,64,24,15,182,10,131,225,15,74,15,190,132,9,0,35,1,0,66,138,140,9,16,35,1,0,72,43,208,139,66,252,211,232,73,137,80,8,65,137,64,28,15,182,10,131,225,15,74,15,190,132,9,0,35,1,
      0,66,138,140,9,16,35,1,0,72,43,208,139,66,252,211,232,65,137,64,32,72,141,66,4,73,137,80,8,139,10,65,137,72,36,139,76,36,96,255,193,73,137,64,8,137,76,36,96,59,77,168,15,130,104,254,255,255,72,139,77,40,72,51,204,232,171,0,0,0,72,129,196,56,
      1,0,0,65,95,65,94,65,93,65,92,95,94,91,93,195,232,94,223,255,255,204,204,64,83,69,139,24,72,139,218,65,131,227,248,76,139,201,65,246,0,4,76,139,209,116,19,65,139,64,8,77,99,80,4,247,216,76,3,209,72,99,200,76,35,209,73,99,195,74,139,20,16,72,
      139,67,16,139,72,8,72,139,67,8,246,68,1,3,15,116,11,15,182,68,1,3,131,224,240,76,3,200,76,51,202,73,139,201,91,233,53,0,0,0,204,72,131,236,40,77,139,65,56,72,139,202,73,139,209,232,145,255,255,255,184,1,0,0,0,72,131,196,40,195,204,204,204,204,
      204,204,204,204,204,204,204,204,204,102,102,15,31,132,0,0,0,0,0,72,59,13,97,64,1,0,242,117,18,72,193,193,16,102,247,193,255,255,242,117,2,242,195,72,193,201,16,233,119,66,0,0,204,204,204,64,83,72,131,236,32,51,219,72,133,201,116,12,72,133,210,
      116,7,77,133,192,117,27,136,25,232,218,37,0,0,187,22,0,0,0,137,24,232,122,27,0,0,139,195,72,131,196,32,91,195,76,139,201,76,43,193,67,138,4,8,65,136,1,73,255,193,132,192,116,6,72,131,234,1,117,236,72,133,210,117,217,136,25,232,160,37,0,0,187,
      34,0,0,0,235,196,204,72,131,236,40,69,51,192,72,141,13,102,78,1,0,186,160,15,0,0,232,252,57,0,0,133,192,116,10,255,5,122,78,1,0,176,1,235,7,232,9,0,0,0,50,192,72,131,196,40,195,204,204,64,83,72,131,236,32,139,29,92,78,1,0,235,29,72,141,5,43,
      78,1,0,255,203,72,141,12,155,72,141,12,200,255,21,107,25,1,0,255,13,61,78,1,0,133,219,117,223,176,1,72,131,196,32,91,195,204,72,137,124,36,8,72,141,61,212,78,1,0,72,141,5,221,79,1,0,72,59,199,72,139,5,91,63,1,0,72,27,201,72,247,209,131,225,34,
      243,72,171,72,139,124,36,8,176,1,195,204,204,204,64,83,72,131,236,32,132,201,117,47,72,141,29,251,77,1,0,72,139,11,72,133,201,116,16,72,131,249,255,116,6,255,21,71,25,1,0,72,131,35,0,72,131,195,8,72,141,5,120,78,1,0,72,59,216,117,216,176,1,72,
      131,196,32,91,195,204,204,204,72,131,236,40,76,141,13,41,168,0,0,51,201,76,141,5,28,168,0,0,72,141,21,29,168,0,0,232,40,3,0,0,72,133,192,116,11,72,131,196,40,72,255,37,184,126,1,0,184,1,0,0,0,72,131,196,40,195,204,204,64,83,72,131,236,32,72,
      139,217,76,141,13,4,168,0,0,185,3,0,0,0,76,141,5,240,167,0,0,72,141,21,241,167,0,0,232,228,2,0,0,72,133,192,116,15,72,139,203,72,131,196,32,91,72,255,37,112,126,1,0,72,131,196,32,91,72,255,37,108,26,1,0,64,83,72,131,236,32,139,217,76,141,13,
      213,167,0,0,185,4,0,0,0,76,141,5,193,167,0,0,72,141,21,194,167,0,0,232,157,2,0,0,139,203,72,133,192,116,12,72,131,196,32,91,72,255,37,42,126,1,0,72,131,196,32,91,72,255,37,46,26,1,0,204,204,64,83,72,131,236,32,139,217,76,141,13,157,167,0,0,185,
      5,0,0,0,76,141,5,137,167,0,0,72,141,21,138,167,0,0,232,85,2,0,0,139,203,72,133,192,116,12,72,131,196,32,91,72,255,37,226,125,1,0,72,131,196,32,91,72,255,37,238,25,1,0,204,204,72,137,92,36,8,87,72,131,236,32,72,139,218,76,141,13,104,167,0,0,139,
      249,72,141,21,95,167,0,0,185,6,0,0,0,76,141,5,75,167,0,0,232,6,2,0,0,72,139,211,139,207,72,133,192,116,8,255,21,150,125,1,0,235,6,255,21,174,25,1,0,72,139,92,36,48,72,131,196,32,95,195,204,204,204,72,137,92,36,8,72,137,116,36,16,87,72,131,236,
      32,65,139,240,76,141,13,39,167,0,0,139,218,76,141,5,22,167,0,0,72,139,249,72,141,21,20,167,0,0,185,18,0,0,0,232,170,1,0,0,139,211,72,139,207,72,133,192,116,11,68,139,198,255,21,55,125,1,0,235,6,255,21,95,24,1,0,72,139,92,36,48,72,139,116,36,
      56,72,131,196,32,95,195,204,204,204,72,137,92,36,8,72,137,108,36,16,72,137,116,36,24,87,72,131,236,80,65,139,217,73,139,248,139,242,76,141,13,225,166,0,0,72,139,233,76,141,5,207,166,0,0,72,141,21,208,166,0,0,185,20,0,0,0,232,62,1,0,0,72,133,
      192,116,82,76,139,132,36,160,0,0,0,68,139,203,72,139,140,36,152,0,0,0,139,214,76,137,68,36,64,76,139,199,72,137,76,36,56,72,139,140,36,144,0,0,0,72,137,76,36,48,139,140,36,136,0,0,0,137,76,36,40,72,139,140,36,128,0,0,0,72,137,76,36,32,72,139,
      205,255,21,137,124,1,0,235,50,51,210,72,139,205,232,61,0,0,0,139,200,68,139,203,139,132,36,136,0,0,0,76,139,199,137,68,36,40,139,214,72,139,132,36,128,0,0,0,72,137,68,36,32,255,21,181,23,1,0,72,139,92,36,96,72,139,108,36,104,72,139,116,36,112,
      72,131,196,80,95,195,72,137,92,36,8,87,72,131,236,32,139,250,76,141,13,45,166,0,0,72,139,217,72,141,21,35,166,0,0,185,22,0,0,0,76,141,5,15,166,0,0,232,114,0,0,0,72,139,203,72,133,192,116,10,139,215,255,21,2,124,1,0,235,5,232,247,64,0,0,72,139,
      92,36,48,72,131,196,32,95,195,64,83,72,131,236,32,72,139,217,76,141,13,248,165,0,0,185,28,0,0,0,76,141,5,232,165,0,0,72,141,21,229,165,0,0,232,40,0,0,0,72,133,192,116,22,72,139,211,72,199,193,250,255,255,255,72,131,196,32,91,72,255,37,173,123,
      1,0,184,37,2,0,192,72,131,196,32,91,195,204,204,72,137,92,36,8,72,137,108,36,16,72,137,116,36,24,87,65,84,65,85,65,86,65,87,72,131,236,32,68,139,249,76,141,53,106,107,255,255,77,139,225,73,139,232,76,139,234,75,139,140,254,144,223,1,0,76,139,
      21,122,59,1,0,72,131,207,255,65,139,194,73,139,210,72,51,209,131,224,63,138,200,72,211,202,72,59,215,15,132,91,1,0,0,72,133,210,116,8,72,139,194,233,80,1,0,0,77,59,196,15,132,217,0,0,0,139,117,0,73,139,156,246,240,222,1,0,72,133,219,116,14,72,
      59,223,15,132,172,0,0,0,233,162,0,0,0,77,139,180,246,176,51,1,0,51,210,73,139,206,65,184,0,8,0,0,255,21,111,22,1,0,72,139,216,72,133,192,117,79,255,21,153,21,1,0,131,248,87,117,66,141,88,176,73,139,206,68,139,195,72,141,21,252,163,0,0,232,159,
      63,0,0,133,192,116,41,68,139,195,72,141,21,249,163,0,0,73,139,206,232,137,63,0,0,133,192,116,19,69,51,192,51,210,73,139,206,255,21,31,22,1,0,72,139,216,235,2,51,219,76,141,53,137,106,255,255,72,133,219,117,13,72,139,199,73,135,132,246,240,222,
      1,0,235,30,72,139,195,73,135,132,246,240,222,1,0,72,133,192,116,9,72,139,203,255,21,174,20,1,0,72,133,219,117,85,72,131,197,4,73,59,236,15,133,46,255,255,255,76,139,21,109,58,1,0,51,219,72,133,219,116,74,73,139,213,72,139,203,255,21,26,21,1,
      0,72,133,192,116,50,76,139,5,78,58,1,0,186,64,0,0,0,65,139,200,131,225,63,43,209,138,202,72,139,208,72,211,202,73,51,208,75,135,148,254,144,223,1,0,235,45,76,139,21,37,58,1,0,235,184,76,139,21,28,58,1,0,65,139,194,185,64,0,0,0,131,224,63,43,
      200,72,211,207,73,51,250,75,135,188,254,144,223,1,0,51,192,72,139,92,36,80,72,139,108,36,88,72,139,116,36,96,72,131,196,32,65,95,65,94,65,93,65,92,95,195,204,204,64,83,72,131,236,32,51,219,72,141,21,73,74,1,0,69,51,192,72,141,12,155,72,141,12,
      202,186,160,15,0,0,232,36,252,255,255,133,192,116,17,255,5,90,76,1,0,255,195,131,251,14,114,211,176,1,235,9,51,201,232,8,0,0,0,50,192,72,131,196,32,91,195,64,83,72,131,236,32,139,29,52,76,1,0,235,29,72,141,5,251,73,1,0,255,203,72,141,12,155,
      72,141,12,200,255,21,83,19,1,0,255,13,21,76,1,0,133,219,117,223,176,1,72,131,196,32,91,195,204,72,99,193,72,141,12,128,72,141,5,202,73,1,0,72,141,12,200,72,255,37,55,19,1,0,204,204,204,72,99,193,72,141,12,128,72,141,5,174,73,1,0,72,141,12,200,
      72,255,37,123,20,1,0,204,204,204,72,131,236,40,255,21,230,19,1,0,72,133,192,72,137,5,196,75,1,0,15,149,192,72,131,196,40,195,72,131,37,180,75,1,0,0,176,1,195,204,72,131,236,40,72,141,13,225,2,0,0,232,44,250,255,255,137,5,2,57,1,0,131,248,255,
      117,4,50,192,235,21,232,184,1,0,0,72,133,192,117,9,51,201,232,12,0,0,0,235,233,176,1,72,131,196,40,195,204,204,204,72,131,236,40,139,13,210,56,1,0,131,249,255,116,12,232,52,250,255,255,131,13,193,56,1,0,255,176,1,72,131,196,40,195,204,204,72,
      137,92,36,8,72,137,116,36,16,87,72,131,236,32,255,21,35,19,1,0,139,13,157,56,1,0,139,216,131,249,255,116,31,232,69,250,255,255,72,139,248,72,133,192,116,12,72,131,248,255,117,115,51,255,51,246,235,112,139,13,119,56,1,0,72,131,202,255,232,106,
      250,255,255,133,192,116,231,186,200,3,0,0,185,1,0,0,0,232,207,30,0,0,139,13,85,56,1,0,72,139,248,72,133,192,117,16,51,210,232,66,250,255,255,51,201,232,139,18,0,0,235,186,72,139,215,232,49,250,255,255,133,192,117,18,139,13,43,56,1,0,51,210,232,
      32,250,255,255,72,139,207,235,219,72,139,207,232,115,2,0,0,51,201,232,92,18,0,0,72,139,247,139,203,255,21,189,19,1,0,72,247,223,72,27,192,72,35,198,116,16,72,139,92,36,48,72,139,116,36,56,72,131,196,32,95,195,232,49,214,255,255,204,64,83,72,
      131,236,32,139,13,216,55,1,0,131,249,255,116,27,232,130,249,255,255,72,139,216,72,133,192,116,8,72,131,248,255,116,125,235,109,139,13,184,55,1,0,72,131,202,255,232,171,249,255,255,133,192,116,104,186,200,3,0,0,185,1,0,0,0,232,16,30,0,0,139,13,
      150,55,1,0,72,139,216,72,133,192,117,16,51,210,232,131,249,255,255,51,201,232,204,17,0,0,235,59,72,139,211,232,114,249,255,255,133,192,117,18,139,13,108,55,1,0,51,210,232,97,249,255,255,72,139,203,235,219,72,139,203,232,180,1,0,0,51,201,232,
      157,17,0,0,72,133,219,116,9,72,139,195,72,131,196,32,91,195,232,138,213,255,255,204,204,72,137,92,36,8,72,137,116,36,16,87,72,131,236,32,255,21,167,17,1,0,139,13,33,55,1,0,139,216,131,249,255,116,31,232,201,248,255,255,72,139,248,72,133,192,
      116,12,72,131,248,255,117,115,51,255,51,246,235,112,139,13,251,54,1,0,72,131,202,255,232,238,248,255,255,133,192,116,231,186,200,3,0,0,185,1,0,0,0,232,83,29,0,0,139,13,217,54,1,0,72,139,248,72,133,192,117,16,51,210,232,198,248,255,255,51,201,
      232,15,17,0,0,235,186,72,139,215,232,181,248,255,255,133,192,117,18,139,13,175,54,1,0,51,210,232,164,248,255,255,72,139,207,235,219,72,139,207,232,247,0,0,0,51,201,232,224,16,0,0,72,139,247,139,203,255,21,65,18,1,0,72,139,92,36,48,72,247,223,
      72,27,192,72,35,198,72,139,116,36,56,72,131,196,32,95,195,64,83,72,131,236,32,139,13,100,54,1,0,131,249,255,116,42,232,14,248,255,255,72,139,216,72,133,192,116,29,139,13,76,54,1,0,51,210,232,65,248,255,255,72,139,203,232,105,1,0,0,72,139,203,
      232,129,16,0,0,72,131,196,32,91,195,204,204,204,72,133,201,116,26,83,72,131,236,32,72,139,217,232,70,1,0,0,72,139,203,232,94,16,0,0,72,131,196,32,91,195,72,137,92,36,8,87,72,131,236,32,72,139,249,72,139,218,72,139,137,144,0,0,0,72,133,201,116,
      44,232,243,59,0,0,72,139,143,144,0,0,0,72,59,13,5,77,1,0,116,23,72,141,5,36,59,1,0,72,59,200,116,11,131,121,16,0,117,5,232,116,60,0,0,72,137,159,144,0,0,0,72,133,219,116,8,72,139,203,232,44,59,0,0,72,139,92,36,48,72,131,196,32,95,195,204,64,
      85,72,139,236,72,131,236,80,72,137,77,216,72,141,69,216,72,137,69,232,76,141,77,32,186,1,0,0,0,76,141,69,232,184,5,0,0,0,137,69,32,137,69,40,72,141,69,216,72,137,69,240,72,141,69,224,72,137,69,248,184,4,0,0,0,137,69,208,137,69,212,72,141,5,125,
      76,1,0,72,137,69,224,137,81,40,72,141,13,247,137,0,0,72,139,69,216,72,137,8,72,141,13,73,53,1,0,72,139,69,216,137,144,168,3,0,0,72,139,69,216,72,137,136,136,0,0,0,141,74,66,72,139,69,216,72,141,85,40,102,137,136,188,0,0,0,72,139,69,216,102,137,
      136,194,1,0,0,72,141,77,24,72,139,69,216,72,131,160,160,3,0,0,0,232,22,1,0,0,76,141,77,208,76,141,69,240,72,141,85,212,72,141,77,24,232,65,1,0,0,72,131,196,80,93,195,204,204,204,64,85,72,139,236,72,131,236,64,72,141,69,232,72,137,77,232,72,137,
      69,240,72,141,21,104,137,0,0,184,5,0,0,0,137,69,32,137,69,40,72,141,69,232,72,137,69,248,184,4,0,0,0,137,69,224,137,69,228,72,139,1,72,59,194,116,12,72,139,200,232,214,14,0,0,72,139,77,232,72,139,73,112,232,201,14,0,0,72,139,77,232,72,139,73,
      88,232,188,14,0,0,72,139,77,232,72,139,73,96,232,175,14,0,0,72,139,77,232,72,139,73,104,232,162,14,0,0,72,139,77,232,72,139,73,72,232,149,14,0,0,72,139,77,232,72,139,73,80,232,136,14,0,0,72,139,77,232,72,139,73,120,232,123,14,0,0,72,139,77,232,
      72,139,137,128,0,0,0,232,107,14,0,0,72,139,77,232,72,139,137,192,3,0,0,232,91,14,0,0,76,141,77,32,76,141,69,240,72,141,85,40,72,141,77,24,232,166,0,0,0,76,141,77,224,76,141,69,248,72,141,85,228,72,141,77,24,232,241,0,0,0,72,131,196,64,93,195,
      204,204,204,72,137,92,36,8,76,137,76,36,32,87,72,131,236,32,73,139,217,73,139,248,139,10,232,72,250,255,255,144,72,139,7,72,139,8,72,139,129,136,0,0,0,240,255,0,139,11,232,76,250,255,255,72,139,92,36,48,72,131,196,32,95,195,204,72,137,92,36,
      8,76,137,76,36,32,87,72,131,236,32,73,139,217,73,139,248,139,10,232,8,250,255,255,144,72,139,71,8,72,139,16,72,139,15,72,139,18,72,139,9,232,94,253,255,255,144,139,11,232,6,250,255,255,72,139,92,36,48,72,131,196,32,95,195,204,204,204,72,137,
      92,36,8,76,137,76,36,32,87,72,131,236,32,73,139,217,73,139,248,139,10,232,192,249,255,255,144,72,139,7,72,139,8,72,139,137,136,0,0,0,72,133,201,116,30,131,200,255,240,15,193,1,131,248,1,117,18,72,141,5,34,51,1,0,72,59,200,116,6,232,84,13,0,0,
      144,139,11,232,164,249,255,255,72,139,92,36,48,72,131,196,32,95,195,204,72,137,92,36,8,76,137,76,36,32,87,72,131,236,32,73,139,217,73,139,248,139,10,232,96,249,255,255,144,72,139,15,51,210,72,139,9,232,190,252,255,255,144,139,11,232,102,249,
      255,255,72,139,92,36,48,72,131,196,32,95,195,204,204,204,64,83,72,131,236,32,185,7,0,0,0,232,44,249,255,255,51,219,51,201,232,79,62,0,0,133,192,117,12,232,90,0,0,0,232,69,1,0,0,179,1,185,7,0,0,0,232,37,249,255,255,138,195,72,131,196,32,91,195,
      204,72,137,92,36,8,87,72,131,236,32,51,219,72,141,61,5,69,1,0,72,139,12,59,72,133,201,116,10,232,187,61,0,0,72,131,36,59,0,72,131,195,8,72,129,251,0,4,0,0,114,217,72,139,92,36,48,176,1,72,131,196,32,95,195,72,139,196,72,137,88,8,72,137,104,16,
      72,137,112,24,72,137,120,32,65,86,72,129,236,144,0,0,0,72,141,72,136,255,21,202,12,1,0,69,51,246,102,68,57,116,36,98,15,132,154,0,0,0,72,139,68,36,104,72,133,192,15,132,140,0,0,0,72,99,24,72,141,112,4,191,0,32,0,0,72,3,222,57,56,15,76,56,139,
      207,232,142,61,0,0,59,61,116,72,1,0,15,79,61,109,72,1,0,133,255,116,96,65,139,238,72,131,59,255,116,71,72,131,59,254,116,65,246,6,1,116,60,246,6,8,117,13,72,139,11,255,21,31,12,1,0,133,192,116,42,72,139,197,76,141,5,57,68,1,0,72,139,205,72,193,
      249,6,131,224,63,73,139,12,200,72,141,20,192,72,139,3,72,137,68,209,40,138,6,136,68,209,56,72,255,197,72,255,198,72,131,195,8,72,131,239,1,117,163,76,141,156,36,144,0,0,0,73,139,91,16,73,139,107,24,73,139,115,32,73,139,123,40,73,139,227,65,94,
      195,204,204,204,72,139,196,72,137,88,8,72,137,104,16,72,137,112,24,72,137,120,32,65,86,72,131,236,32,51,246,69,51,246,72,99,206,72,141,61,192,67,1,0,72,139,193,131,225,63,72,193,248,6,72,141,28,201,72,139,60,199,72,139,68,223,40,72,131,192,2,
      72,131,248,1,118,10,128,76,223,56,128,233,143,0,0,0,198,68,223,56,129,139,206,133,246,116,22,131,233,1,116,10,131,249,1,185,244,255,255,255,235,12,185,245,255,255,255,235,5,185,246,255,255,255,255,21,137,11,1,0,72,139,232,72,141,72,1,72,131,
      249,1,118,11,72,139,200,255,21,43,11,1,0,235,2,51,192,133,192,116,32,15,182,200,72,137,108,223,40,131,249,2,117,7,128,76,223,56,64,235,49,131,249,3,117,44,128,76,223,56,8,235,37,128,76,223,56,64,72,199,68,223,40,254,255,255,255,72,139,5,174,
      71,1,0,72,133,192,116,11,73,139,4,6,199,64,24,254,255,255,255,255,198,73,131,198,8,131,254,3,15,133,45,255,255,255,72,139,92,36,48,72,139,108,36,56,72,139,116,36,64,72,139,124,36,72,72,131,196,32,65,94,195,72,131,236,40,255,21,90,10,1,0,72,137,
      5,243,70,1,0,255,21,85,10,1,0,72,137,5,238,70,1,0,176,1,72,131,196,40,195,204,204,204,72,131,236,40,232,83,247,255,255,72,139,200,72,141,21,233,70,1,0,72,131,196,40,233,44,4,0,0,72,131,236,40,128,61,225,70,1,0,0,117,76,72,141,13,52,51,1,0,72,
      137,13,189,70,1,0,72,141,5,230,47,1,0,72,141,13,15,50,1,0,72,137,5,176,70,1,0,72,137,13,153,70,1,0,232,216,247,255,255,76,141,13,157,70,1,0,76,139,192,178,1,185,253,255,255,255,232,146,4,0,0,198,5,147,70,1,0,1,176,1,72,131,196,40,195,72,137,
      92,36,24,85,86,87,65,84,65,85,65,86,65,87,72,131,236,64,72,139,5,101,47,1,0,72,51,196,72,137,68,36,56,72,139,242,232,145,2,0,0,51,219,139,248,133,192,15,132,83,2,0,0,76,141,45,186,51,1,0,68,139,243,73,139,197,141,107,1,57,56,15,132,78,1,0,0,
      68,3,245,72,131,192,48,65,131,254,5,114,235,129,255,232,253,0,0,15,132,45,1,0,0,15,183,207,255,21,79,10,1,0,133,192,15,132,28,1,0,0,184,233,253,0,0,59,248,117,46,72,137,70,4,72,137,158,32,2,0,0,137,94,24,102,137,94,28,72,141,126,12,15,183,195,
      185,6,0,0,0,102,243,171,72,139,206,232,153,5,0,0,233,226,1,0,0,72,141,84,36,32,139,207,255,21,251,8,1,0,133,192,15,132,196,0,0,0,51,210,72,141,78,24,65,184,1,1,0,0,232,2,171,255,255,131,124,36,32,2,137,126,4,72,137,158,32,2,0,0,15,133,148,0,
      0,0,72,141,76,36,38,56,92,36,38,116,44,56,89,1,116,39,15,182,65,1,15,182,17,59,208,119,20,43,194,141,122,1,141,20,40,128,76,55,24,4,3,253,72,43,213,117,244,72,131,193,2,56,25,117,212,72,141,70,26,185,254,0,0,0,128,8,8,72,3,197,72,43,205,117,
      245,139,78,4,129,233,164,3,0,0,116,46,131,233,4,116,32,131,233,13,116,18,59,205,116,5,72,139,195,235,34,72,139,5,101,152,0,0,235,25,72,139,5,84,152,0,0,235,16,72,139,5,67,152,0,0,235,7,72,139,5,50,152,0,0,72,137,134,32,2,0,0,235,2,139,235,137,
      110,8,233,11,255,255,255,57,29,249,68,1,0,15,133,245,0,0,0,131,200,255,233,247,0,0,0,51,210,72,141,78,24,65,184,1,1,0,0,232,42,170,255,255,65,139,198,77,141,77,16,76,141,61,44,50,1,0,65,190,4,0,0,0,76,141,28,64,73,193,227,4,77,3,203,73,139,209,
      65,56,25,116,62,56,90,1,116,57,68,15,182,2,15,182,66,1,68,59,192,119,36,69,141,80,1,65,129,250,1,1,0,0,115,23,65,138,7,68,3,197,65,8,68,50,24,68,3,213,15,182,66,1,68,59,192,118,224,72,131,194,2,56,26,117,194,73,131,193,8,76,3,253,76,43,245,117,
      174,137,126,4,137,110,8,129,239,164,3,0,0,116,41,131,239,4,116,27,131,239,13,116,13,59,253,117,34,72,139,29,126,151,0,0,235,25,72,139,29,109,151,0,0,235,16,72,139,29,92,151,0,0,235,7,72,139,29,75,151,0,0,76,43,222,72,137,158,32,2,0,0,72,141,
      86,12,185,6,0,0,0,75,141,60,43,15,183,68,23,248,102,137,2,72,141,82,2,72,43,205,117,239,233,25,254,255,255,72,139,206,232,170,0,0,0,51,192,72,139,76,36,56,72,51,204,232,119,236,255,255,72,139,156,36,144,0,0,0,72,131,196,64,65,95,65,94,65,93,
      65,92,95,94,93,195,204,204,204,64,83,72,131,236,64,139,217,51,210,72,141,76,36,32,232,16,191,255,255,131,37,177,67,1,0,0,131,251,254,117,18,199,5,162,67,1,0,1,0,0,0,255,21,76,7,1,0,235,21,131,251,253,117,20,199,5,139,67,1,0,1,0,0,0,255,21,173,
      6,1,0,139,216,235,23,131,251,252,117,18,72,139,68,36,40,199,5,109,67,1,0,1,0,0,0,139,88,12,128,124,36,56,0,116,12,72,139,76,36,32,131,161,168,3,0,0,253,139,195,72,131,196,64,91,195,204,204,204,72,137,92,36,8,72,137,108,36,16,72,137,116,36,24,
      87,72,131,236,32,72,141,89,24,72,139,241,189,1,1,0,0,72,139,203,68,139,197,51,210,232,111,168,255,255,51,192,72,141,126,12,72,137,70,4,185,6,0,0,0,72,137,134,32,2,0,0,15,183,192,102,243,171,72,141,61,28,44,1,0,72,43,254,138,4,31,136,3,72,255,
      195,72,131,237,1,117,242,72,141,142,25,1,0,0,186,0,1,0,0,138,4,57,136,1,72,255,193,72,131,234,1,117,242,72,139,92,36,48,72,139,108,36,56,72,139,116,36,64,72,131,196,32,95,195,72,137,92,36,16,72,137,116,36,24,87,72,131,236,32,72,139,242,72,139,
      249,139,5,149,51,1,0,133,129,168,3,0,0,116,19,72,131,185,144,0,0,0,0,116,9,72,139,153,136,0,0,0,235,100,185,5,0,0,0,232,20,242,255,255,144,72,139,159,136,0,0,0,72,137,92,36,48,72,59,30,116,62,72,133,219,116,34,131,200,255,240,15,193,3,131,248,
      1,117,22,72,141,5,114,43,1,0,72,139,76,36,48,72,59,200,116,5,232,159,5,0,0,72,139,6,72,137,135,136,0,0,0,72,137,68,36,48,240,255,0,72,139,92,36,48,185,5,0,0,0,232,214,241,255,255,72,133,219,116,19,72,139,195,72,139,92,36,56,72,139,116,36,64,
      72,131,196,32,95,195,232,97,201,255,255,144,72,137,92,36,8,76,137,76,36,32,76,137,68,36,24,85,86,87,72,139,236,72,131,236,64,64,138,242,139,217,73,139,209,73,139,200,232,31,255,255,255,139,203,232,8,254,255,255,72,139,77,48,139,248,76,139,129,
      136,0,0,0,65,59,64,4,117,7,51,192,233,184,0,0,0,185,40,2,0,0,232,216,34,0,0,72,139,216,72,133,192,15,132,149,0,0,0,72,139,69,48,186,4,0,0,0,72,139,203,72,139,128,136,0,0,0,68,141,66,124,15,16,0,15,17,1,15,16,72,16,15,17,73,16,15,16,64,32,15,
      17,65,32,15,16,72,48,15,17,73,48,15,16,64,64,15,17,65,64,15,16,72,80,15,17,73,80,15,16,64,96,15,17,65,96,73,3,200,15,16,72,112,73,3,192,15,17,73,240,72,131,234,1,117,182,15,16,0,15,17,1,15,16,72,16,15,17,73,16,72,139,64,32,72,137,65,32,139,207,
      33,19,72,139,211,232,153,250,255,255,139,248,131,248,255,117,37,232,193,15,0,0,199,0,22,0,0,0,131,207,255,72,139,203,232,96,4,0,0,139,199,72,139,92,36,96,72,131,196,64,95,94,93,195,64,132,246,117,5,232,55,33,0,0,72,139,69,48,72,139,136,136,0,
      0,0,131,200,255,240,15,193,1,131,248,1,117,28,72,139,69,48,72,139,136,136,0,0,0,72,141,5,226,41,1,0,72,59,200,116,5,232,20,4,0,0,199,3,1,0,0,0,72,139,203,72,139,69,48,51,219,72,137,136,136,0,0,0,72,139,69,48,139,136,168,3,0,0,133,13,134,49,1,
      0,117,132,72,141,69,48,72,137,69,240,76,141,77,228,72,141,69,56,72,137,69,248,76,141,69,240,141,67,5,72,141,85,232,137,69,228,72,141,77,224,137,69,232,232,2,2,0,0,64,132,246,15,132,77,255,255,255,72,139,69,56,72,139,8,72,137,13,11,48,1,0,233,
      58,255,255,255,204,204,72,137,92,36,16,72,137,116,36,24,85,72,141,172,36,128,249,255,255,72,129,236,128,7,0,0,72,139,5,27,41,1,0,72,51,196,72,137,133,112,6,0,0,72,139,217,139,73,4,129,249,233,253,0,0,15,132,61,1,0,0,72,141,84,36,80,255,21,44,
      3,1,0,133,192,15,132,42,1,0,0,51,192,72,141,76,36,112,190,0,1,0,0,136,1,255,192,72,255,193,59,198,114,245,138,68,36,86,72,141,84,36,86,198,68,36,112,32,235,32,68,15,182,66,1,15,182,200,235,11,59,206,115,12,198,68,12,112,32,255,193,65,59,200,
      118,240,72,131,194,2,138,2,132,192,117,220,139,67,4,76,141,68,36,112,131,100,36,48,0,68,139,206,137,68,36,40,186,1,0,0,0,72,141,133,112,2,0,0,51,201,72,137,68,36,32,232,57,53,0,0,131,100,36,64,0,76,141,76,36,112,139,67,4,68,139,198,72,139,147,
      32,2,0,0,51,201,137,68,36,56,72,141,69,112,137,116,36,48,72,137,68,36,40,137,116,36,32,232,150,54,0,0,131,100,36,64,0,76,141,76,36,112,139,67,4,65,184,0,2,0,0,72,139,147,32,2,0,0,51,201,137,68,36,56,72,141,133,112,1,0,0,137,116,36,48,72,137,
      68,36,40,137,116,36,32,232,93,54,0,0,184,1,0,0,0,72,141,149,112,2,0,0,246,2,1,116,11,128,76,24,24,16,138,76,5,111,235,21,246,2,2,116,14,128,76,24,24,32,138,140,5,111,1,0,0,235,2,50,201,136,140,24,24,1,0,0,72,131,194,2,72,255,192,72,131,238,1,
      117,199,235,67,51,210,190,0,1,0,0,141,74,1,68,141,66,159,65,141,64,32,131,248,25,119,10,128,76,11,24,16,141,66,32,235,18,65,131,248,25,119,10,128,76,11,24,32,141,66,224,235,2,50,192,136,132,11,24,1,0,0,255,194,72,255,193,59,214,114,199,72,139,
      141,112,6,0,0,72,51,204,232,8,231,255,255,76,141,156,36,128,7,0,0,73,139,91,24,73,139,115,32,73,139,227,93,195,204,204,204,72,137,92,36,8,76,137,76,36,32,87,72,131,236,32,73,139,249,73,139,216,139,10,232,220,237,255,255,144,72,139,3,72,139,8,
      72,139,129,136,0,0,0,72,131,192,24,72,139,13,19,62,1,0,72,133,201,116,111,72,133,192,116,93,65,184,2,0,0,0,69,139,200,65,141,80,126,15,16,0,15,17,1,15,16,72,16,15,17,73,16,15,16,64,32,15,17,65,32,15,16,72,48,15,17,73,48,15,16,64,64,15,17,65,
      64,15,16,72,80,15,17,73,80,15,16,64,96,15,17,65,96,72,3,202,15,16,72,112,15,17,73,240,72,3,194,73,131,233,1,117,182,138,0,136,1,235,39,51,210,65,184,1,1,0,0,232,7,163,255,255,232,94,12,0,0,199,0,22,0,0,0,232,255,1,0,0,65,184,2,0,0,0,65,141,80,
      126,72,139,3,72,139,8,72,139,129,136,0,0,0,72,5,25,1,0,0,72,139,13,115,61,1,0,72,133,201,116,94,72,133,192,116,76,15,16,0,15,17,1,15,16,72,16,15,17,73,16,15,16,64,32,15,17,65,32,15,16,72,48,15,17,73,48,15,16,64,64,15,17,65,64,15,16,72,80,15,
      17,73,80,15,16,64,96,15,17,65,96,72,3,202,15,16,72,112,15,17,73,240,72,3,194,73,131,232,1,117,182,235,29,51,210,65,184,0,1,0,0,232,112,162,255,255,232,199,11,0,0,199,0,22,0,0,0,232,104,1,0,0,72,139,67,8,72,139,8,72,139,17,131,200,255,240,15,
      193,2,131,248,1,117,27,72,139,67,8,72,139,8,72,141,5,12,38,1,0,72,57,1,116,8,72,139,9,232,59,0,0,0,72,139,3,72,139,16,72,139,67,8,72,139,8,72,139,130,136,0,0,0,72,137,1,72,139,3,72,139,8,72,139,129,136,0,0,0,240,255,0,139,15,232,101,236,255,
      255,72,139,92,36,48,72,131,196,32,95,195,204,204,72,133,201,116,55,83,72,131,236,32,76,139,193,51,210,72,139,13,54,56,1,0,255,21,128,0,1,0,133,192,117,23,232,43,11,0,0,72,139,216,255,21,6,0,1,0,139,200,232,171,11,0,0,137,3,72,131,196,32,91,195,
      204,204,204,72,137,92,36,8,72,137,116,36,16,87,72,131,236,32,72,139,242,72,139,249,72,59,202,116,84,72,139,217,72,139,3,72,133,192,116,10,255,21,25,101,1,0,132,192,116,9,72,131,195,16,72,59,222,117,229,72,59,222,116,49,72,59,223,116,40,72,131,
      195,248,72,131,123,248,0,116,16,72,139,3,72,133,192,116,8,51,201,255,21,231,100,1,0,72,131,235,16,72,141,67,8,72,59,199,117,220,50,192,235,2,176,1,72,139,92,36,48,72,139,116,36,56,72,131,196,32,95,195,72,137,92,36,8,87,72,131,236,32,72,139,218,
      72,139,249,72,59,202,116,26,72,139,67,248,72,133,192,116,8,51,201,255,21,158,100,1,0,72,131,235,16,72,59,223,117,230,72,139,92,36,48,176,1,72,131,196,32,95,195,72,131,236,56,72,131,100,36,32,0,69,51,201,69,51,192,51,210,51,201,232,179,1,0,0,
      72,131,196,56,195,204,204,72,131,236,40,185,23,0,0,0,255,21,169,255,0,0,133,192,116,7,185,5,0,0,0,205,41,65,184,1,0,0,0,186,23,4,0,192,65,141,72,1,232,26,0,0,0,255,21,172,254,0,0,72,139,200,186,23,4,0,192,72,131,196,40,72,255,37,33,0,1,0,204,
      72,137,92,36,16,72,137,116,36,24,85,87,65,86,72,141,172,36,16,251,255,255,72,129,236,240,5,0,0,72,139,5,20,36,1,0,72,51,196,72,137,133,224,4,0,0,65,139,248,139,242,139,217,131,249,255,116,5,232,185,130,255,255,51,210,72,141,76,36,112,65,184,
      152,0,0,0,232,63,160,255,255,51,210,72,141,77,16,65,184,208,4,0,0,232,46,160,255,255,72,141,68,36,112,72,137,68,36,72,72,141,77,16,72,141,69,16,72,137,68,36,80,255,21,81,255,0,0,76,139,181,8,1,0,0,72,141,84,36,64,73,139,206,69,51,192,255,21,
      65,255,0,0,72,133,192,116,54,72,131,100,36,56,0,72,141,76,36,88,72,139,84,36,64,76,139,200,72,137,76,36,48,77,139,198,72,141,76,36,96,72,137,76,36,40,72,141,77,16,72,137,76,36,32,51,201,255,21,30,255,0,0,72,139,133,8,5,0,0,72,137,133,8,1,0,0,
      72,141,133,8,5,0,0,72,131,192,8,137,116,36,112,72,137,133,168,0,0,0,72,139,133,8,5,0,0,72,137,69,128,137,124,36,116,255,21,93,254,0,0,51,201,139,248,255,21,3,255,0,0,72,141,76,36,72,255,21,40,255,0,0,133,192,117,16,133,255,117,12,131,251,255,
      116,7,139,203,232,196,129,255,255,72,139,141,224,4,0,0,72,51,204,232,141,226,255,255,76,141,156,36,240,5,0,0,73,139,91,40,73,139,115,48,73,139,227,65,94,95,93,195,204,72,137,13,229,57,1,0,195,72,137,92,36,8,72,137,108,36,16,72,137,116,36,24,
      87,72,131,236,48,65,139,217,73,139,248,72,139,242,72,139,233,232,139,235,255,255,72,133,192,116,61,72,139,128,184,3,0,0,72,133,192,116,49,72,139,84,36,96,68,139,203,72,137,84,36,32,76,139,199,72,139,214,72,139,205,255,21,106,98,1,0,72,139,92,
      36,64,72,139,108,36,72,72,139,116,36,80,72,131,196,48,95,195,76,139,21,102,34,1,0,68,139,203,65,139,202,76,139,199,76,51,21,102,57,1,0,131,225,63,73,211,202,72,139,214,77,133,210,116,15,72,139,76,36,96,73,139,194,72,137,76,36,32,235,174,72,139,
      68,36,96,72,139,205,72,137,68,36,32,232,167,253,255,255,204,204,204,72,137,13,53,57,1,0,72,137,13,54,57,1,0,72,137,13,55,57,1,0,72,137,13,56,57,1,0,195,204,204,204,76,139,220,72,131,236,40,184,3,0,0,0,77,141,75,16,77,141,67,8,137,68,36,56,73,
      141,83,24,137,68,36,64,73,141,75,8,232,115,2,0,0,72,131,196,40,195,204,204,72,137,92,36,32,86,87,65,84,65,85,65,86,72,131,236,64,139,217,69,51,237,68,33,108,36,120,65,182,1,68,136,116,36,112,131,249,2,116,33,131,249,4,116,76,131,249,6,116,23,
      131,249,8,116,66,131,249,11,116,61,131,249,15,116,8,141,65,235,131,248,1,119,125,131,233,2,15,132,175,0,0,0,131,233,4,15,132,139,0,0,0,131,233,9,15,132,148,0,0,0,131,233,6,15,132,130,0,0,0,131,249,1,116,116,51,255,233,143,0,0,0,232,50,234,255,
      255,76,139,232,72,133,192,117,24,131,200,255,72,139,156,36,136,0,0,0,72,131,196,64,65,94,65,93,65,92,95,94,195,72,139,0,72,139,13,180,118,0,0,72,193,225,4,72,3,200,235,9,57,88,4,116,11,72,131,192,16,72,59,193,117,242,51,192,72,133,192,117,18,
      232,189,6,0,0,199,0,22,0,0,0,232,94,252,255,255,235,174,72,141,120,8,69,50,246,68,136,116,36,112,235,34,72,141,61,15,56,1,0,235,25,72,141,61,254,55,1,0,235,16,72,141,61,5,56,1,0,235,7,72,141,61,228,55,1,0,72,131,164,36,128,0,0,0,0,69,132,246,
      116,11,185,3,0,0,0,232,84,231,255,255,144,72,139,55,69,132,246,116,18,72,139,5,164,32,1,0,139,200,131,225,63,72,51,240,72,211,206,72,131,254,1,15,132,148,0,0,0,72,133,246,15,132,3,1,0,0,65,188,16,9,0,0,131,251,11,119,61,65,15,163,220,115,55,
      73,139,69,8,72,137,132,36,128,0,0,0,72,137,68,36,48,73,131,101,8,0,131,251,8,117,83,232,181,231,255,255,139,64,16,137,68,36,120,137,68,36,32,232,165,231,255,255,199,64,16,140,0,0,0,131,251,8,117,50,72,139,5,194,117,0,0,72,193,224,4,73,3,69,0,
      72,139,13,187,117,0,0,72,193,225,4,72,3,200,72,137,68,36,40,72,59,193,116,29,72,131,96,8,0,72,131,192,16,235,235,72,139,5,0,32,1,0,72,137,7,235,6,65,188,16,9,0,0,69,132,246,116,10,185,3,0,0,0,232,162,230,255,255,72,131,254,1,117,7,51,192,233,
      142,254,255,255,131,251,8,117,25,232,47,231,255,255,139,80,16,139,203,72,139,198,76,139,5,168,95,1,0,65,255,208,235,14,139,203,72,139,198,72,139,21,151,95,1,0,255,210,131,251,11,119,200,65,15,163,220,115,194,72,139,132,36,128,0,0,0,73,137,69,
      8,131,251,8,117,177,232,236,230,255,255,139,76,36,120,137,72,16,235,163,69,132,246,116,8,141,78,3,232,50,230,255,255,185,3,0,0,0,232,40,160,255,255,144,204,204,204,72,137,92,36,8,76,137,76,36,32,87,72,131,236,32,73,139,249,139,10,232,239,229,
      255,255,144,72,139,29,71,31,1,0,139,203,131,225,63,72,51,29,99,54,1,0,72,211,203,139,15,232,237,229,255,255,72,139,195,72,139,92,36,48,72,131,196,32,95,195,204,204,204,72,137,13,81,54,1,0,195,72,139,21,17,31,1,0,139,202,72,51,21,64,54,1,0,131,
      225,63,72,211,202,72,133,210,15,149,192,195,204,204,204,72,139,21,241,30,1,0,76,139,193,139,202,72,51,21,29,54,1,0,131,225,63,72,211,202,72,133,210,117,3,51,192,195,73,139,200,72,139,194,72,255,37,178,94,1,0,204,204,177,1,233,201,0,0,0,204,64,
      83,72,131,236,32,72,139,217,72,133,201,117,10,72,131,196,32,91,233,176,0,0,0,232,47,0,0,0,133,192,117,33,139,67,20,193,232,11,168,1,116,19,72,139,203,232,21,62,0,0,139,200,232,162,48,0,0,133,192,117,4,51,192,235,3,131,200,255,72,131,196,32,91,
      195,204,72,137,92,36,8,72,137,116,36,16,87,72,131,236,32,72,139,217,139,73,20,139,193,36,3,60,2,117,75,246,193,192,116,70,139,59,43,123,8,131,99,16,0,72,139,115,8,72,137,51,133,255,126,50,72,139,203,232,190,61,0,0,139,200,68,139,199,72,139,214,
      232,101,49,0,0,59,248,116,10,240,131,75,20,16,131,200,255,235,17,139,67,20,193,232,2,168,1,116,5,240,131,99,20,253,51,192,72,139,92,36,48,72,139,116,36,56,72,131,196,32,95,195,204,204,136,76,36,8,85,72,139,236,72,131,236,64,131,101,40,0,72,141,
      69,40,131,101,32,0,76,141,77,224,72,137,69,232,76,141,69,232,72,141,69,16,72,137,69,240,72,141,85,228,72,141,69,32,72,137,69,248,72,141,77,24,184,8,0,0,0,137,69,224,137,69,228,232,176,0,0,0,128,125,16,0,139,69,32,15,69,69,40,72,131,196,64,93,
      195,204,204,204,72,137,92,36,8,76,137,76,36,32,87,72,131,236,32,73,139,249,73,139,216,72,139,10,232,95,1,0,0,144,72,139,83,8,72,139,3,72,139,0,72,133,192,116,90,139,72,20,139,193,193,232,13,168,1,116,78,139,193,36,3,60,2,117,5,246,193,192,117,
      10,15,186,225,11,114,4,255,2,235,55,72,139,67,16,128,56,0,117,15,72,139,3,72,139,8,139,65,20,209,232,168,1,116,31,72,139,3,72,139,8,232,101,254,255,255,131,248,255,116,8,72,139,67,8,255,0,235,7,72,139,67,24,131,8,255,72,139,15,232,249,0,0,0,
      72,139,92,36,48,72,131,196,32,95,195,204,204,72,137,92,36,8,76,137,76,36,32,86,87,65,86,72,131,236,96,73,139,241,73,139,248,139,10,232,125,227,255,255,144,72,139,29,37,52,1,0,72,99,5,22,52,1,0,76,141,52,195,72,137,92,36,56,73,59,222,15,132,136,
      0,0,0,72,139,3,72,137,68,36,32,72,139,23,72,133,192,116,33,139,72,20,139,193,193,232,13,168,1,116,21,139,193,36,3,60,2,117,5,246,193,192,117,14,15,186,225,11,114,8,255,2,72,131,195,8,235,187,72,139,87,16,72,139,79,8,72,139,7,76,141,68,36,32,
      76,137,68,36,64,72,137,68,36,72,72,137,76,36,80,72,137,84,36,88,72,139,68,36,32,72,137,68,36,40,72,137,68,36,48,76,141,76,36,40,76,141,68,36,64,72,141,84,36,48,72,141,140,36,136,0,0,0,232,158,254,255,255,235,169,139,14,232,233,226,255,255,72,
      139,156,36,128,0,0,0,72,131,196,96,65,94,95,94,195,72,131,193,48,72,255,37,1,246,0,0,204,72,131,193,48,72,255,37,85,247,0,0,204,72,139,196,72,137,88,8,72,137,104,16,72,137,112,24,72,137,120,32,65,86,72,131,236,32,139,5,45,51,1,0,51,219,191,3,
      0,0,0,133,192,117,7,184,0,2,0,0,235,5,59,199,15,76,199,72,99,200,186,8,0,0,0,137,5,8,51,1,0,232,71,2,0,0,51,201,72,137,5,2,51,1,0,232,17,246,255,255,72,57,29,246,50,1,0,117,47,186,8,0,0,0,137,61,225,50,1,0,72,139,207,232,29,2,0,0,51,201,72,137,
      5,216,50,1,0,232,231,245,255,255,72,57,29,204,50,1,0,117,5,131,200,255,235,117,72,139,235,72,141,53,91,34,1,0,76,141,53,60,34,1,0,73,141,78,48,69,51,192,186,160,15,0,0,232,187,221,255,255,72,139,5,156,50,1,0,76,141,5,253,45,1,0,72,139,213,72,
      193,250,6,76,137,52,3,72,139,197,131,224,63,72,141,12,192,73,139,4,208,72,139,76,200,40,72,131,193,2,72,131,249,2,119,6,199,6,254,255,255,255,72,255,197,73,131,198,88,72,131,195,8,72,131,198,88,72,131,239,1,117,158,51,192,72,139,92,36,48,72,
      139,108,36,56,72,139,116,36,64,72,139,124,36,72,72,131,196,32,65,94,195,204,64,83,72,131,236,32,232,17,252,255,255,232,132,58,0,0,51,219,72,139,13,27,50,1,0,72,139,12,11,232,38,59,0,0,72,139,5,11,50,1,0,72,139,12,3,72,131,193,48,255,21,141,244,
      0,0,72,131,195,8,72,131,251,24,117,209,72,139,13,236,49,1,0,232,251,244,255,255,72,131,37,223,49,1,0,0,72,131,196,32,91,195,204,64,83,72,131,236,32,51,219,137,92,36,48,101,72,139,4,37,96,0,0,0,72,139,72,32,57,89,8,124,17,72,141,76,36,48,232,
      88,222,255,255,131,124,36,48,1,116,5,187,1,0,0,0,139,195,72,131,196,32,91,195,72,131,236,40,232,31,227,255,255,72,133,192,117,9,72,141,5,23,34,1,0,235,4,72,131,192,32,72,131,196,40,195,72,131,236,40,232,255,226,255,255,72,133,192,117,9,72,141,
      5,251,33,1,0,235,4,72,131,192,36,72,131,196,40,195,72,137,92,36,8,87,72,131,236,32,139,249,232,215,226,255,255,72,133,192,117,9,72,141,5,211,33,1,0,235,4,72,131,192,36,137,56,232,190,226,255,255,72,141,29,187,33,1,0,72,133,192,116,4,72,141,88,
      32,139,207,232,15,0,0,0,137,3,72,139,92,36,48,72,131,196,32,95,195,204,204,51,192,76,141,13,187,138,0,0,73,139,209,68,141,64,8,59,10,116,43,255,192,73,3,208,131,248,45,114,242,141,65,237,131,248,17,119,6,184,13,0,0,0,195,129,193,68,255,255,255,
      184,22,0,0,0,131,249,14,65,15,70,192,195,65,139,68,193,4,195,204,204,204,64,83,72,131,236,32,76,139,194,72,139,217,72,133,201,116,14,51,210,72,141,66,224,72,247,243,73,59,192,114,67,73,15,175,216,184,1,0,0,0,72,133,219,72,15,68,216,235,21,232,
      66,58,0,0,133,192,116,40,72,139,203,232,122,153,255,255,133,192,116,28,72,139,13,215,43,1,0,76,139,195,186,8,0,0,0,255,21,17,244,0,0,72,133,192,116,209,235,13,232,193,254,255,255,199,0,12,0,0,0,51,192,72,131,196,32,91,195,204,204,204,233,7,3,
      0,0,204,204,204,72,139,196,72,137,88,8,72,137,104,16,72,137,112,24,72,137,120,32,65,86,72,131,236,48,51,219,65,139,232,72,139,250,72,139,241,72,133,201,117,34,56,90,40,116,12,72,139,74,16,232,33,243,255,255,136,95,40,72,137,95,16,72,137,95,24,
      72,137,95,32,233,14,1,0,0,56,25,117,85,72,57,90,24,117,70,56,90,40,116,12,72,139,74,16,232,245,242,255,255,136,95,40,185,2,0,0,0,232,180,16,0,0,72,137,71,16,72,139,203,72,247,216,27,210,247,210,131,226,12,15,148,193,133,210,15,148,192,136,71,
      40,72,137,79,24,133,210,116,7,139,218,233,190,0,0,0,72,139,71,16,102,137,24,235,158,65,131,201,255,137,92,36,40,76,139,198,72,137,92,36,32,139,205,65,141,81,10,232,85,12,0,0,76,99,240,133,192,117,22,255,21,192,242,0,0,139,200,232,21,254,255,
      255,232,208,253,255,255,139,24,235,125,72,139,79,24,76,59,241,118,67,56,95,40,116,12,72,139,79,16,232,101,242,255,255,136,95,40,75,141,12,54,232,37,16,0,0,72,137,71,16,72,139,203,72,247,216,27,210,247,210,131,226,12,73,15,68,206,133,210,15,148,
      192,136,71,40,72,137,79,24,133,210,15,133,108,255,255,255,72,139,71,16,65,131,201,255,137,76,36,40,76,139,198,139,205,72,137,68,36,32,65,141,81,10,232,205,11,0,0,72,99,200,133,192,15,132,116,255,255,255,72,255,201,72,137,79,32,72,139,108,36,
      72,139,195,72,139,92,36,64,72,139,116,36,80,72,139,124,36,88,72,131,196,48,65,94,195,204,204,72,139,196,72,137,88,8,72,137,104,16,72,137,112,24,72,137,120,32,65,86,72,131,236,64,51,219,69,139,240,72,139,250,72,139,241,72,133,201,117,34,56,90,
      40,116,12,72,139,74,16,232,169,241,255,255,136,95,40,72,137,95,16,72,137,95,24,72,137,95,32,233,34,1,0,0,102,57,25,117,84,72,57,90,24,117,70,56,90,40,116,12,72,139,74,16,232,124,241,255,255,136,95,40,185,1,0,0,0,232,59,15,0,0,72,137,71,16,72,
      139,203,72,247,216,27,210,247,210,131,226,12,15,148,193,133,210,15,148,192,136,71,40,72,137,79,24,133,210,116,7,139,218,233,209,0,0,0,72,139,71,16,136,24,235,158,72,137,92,36,56,65,131,201,255,72,137,92,36,48,76,139,198,137,92,36,40,51,210,65,
      139,206,72,137,92,36,32,232,48,11,0,0,72,99,232,133,192,117,25,255,21,63,241,0,0,139,200,232,148,252,255,255,232,79,252,255,255,139,24,233,133,0,0,0,72,139,79,24,72,59,233,118,66,56,95,40,116,12,72,139,79,16,232,225,240,255,255,136,95,40,72,
      139,205,232,162,14,0,0,72,137,71,16,72,139,203,72,247,216,27,210,247,210,131,226,12,72,15,68,205,133,210,15,148,192,136,71,40,72,137,79,24,133,210,15,133,98,255,255,255,72,139,71,16,65,131,201,255,72,137,92,36,56,76,139,198,72,137,92,36,48,51,
      210,137,76,36,40,65,139,206,72,137,68,36,32,232,157,10,0,0,72,99,200,133,192,15,132,105,255,255,255,72,255,201,72,137,79,32,72,139,108,36,88,139,195,72,139,92,36,80,72,139,116,36,96,72,139,124,36,104,72,131,196,64,65,94,195,204,204,72,137,92,
      36,8,72,137,84,36,16,85,86,87,65,84,65,85,65,86,65,87,72,139,236,72,131,236,96,51,255,72,139,217,72,133,210,117,22,232,121,251,255,255,141,95,22,137,24,232,27,241,255,255,139,195,233,160,1,0,0,15,87,192,72,137,58,72,139,1,243,15,127,69,224,72,
      137,125,240,72,133,192,116,86,72,141,85,80,102,199,69,80,42,63,72,139,200,64,136,125,82,232,31,59,0,0,72,139,11,72,133,192,117,16,76,141,77,224,69,51,192,51,210,232,141,1,0,0,235,12,76,141,69,224,72,139,208,232,7,3,0,0,139,240,133,192,117,9,
      72,131,195,8,72,139,3,235,178,76,139,101,232,76,139,125,224,233,248,0,0,0,76,139,125,224,76,139,207,76,139,101,232,73,139,215,73,139,196,72,137,125,80,73,43,199,76,139,199,76,139,240,73,193,254,3,73,255,198,72,141,72,7,72,193,233,3,77,59,252,
      72,15,71,207,72,131,206,255,72,133,201,116,37,76,139,18,72,139,198,72,255,192,65,56,60,2,117,247,73,255,193,72,131,194,8,76,3,200,73,255,192,76,59,193,117,223,76,137,77,80,65,184,1,0,0,0,73,139,209,73,139,206,232,168,153,255,255,72,139,216,72,
      133,192,116,118,74,141,20,240,77,139,247,72,137,85,216,72,139,194,72,137,85,88,77,59,252,116,86,72,139,203,73,43,207,72,137,77,208,77,139,6,76,139,238,73,255,197,67,56,60,40,117,247,72,43,208,73,255,197,72,3,85,80,77,139,205,72,139,200,232,63,
      57,0,0,133,192,15,133,131,0,0,0,72,139,69,88,72,139,77,208,72,139,85,216,74,137,4,49,73,3,197,73,131,198,8,72,137,69,88,77,59,244,117,180,72,139,69,72,139,247,72,137,24,51,201,232,179,238,255,255,73,139,220,77,139,247,73,43,223,72,131,195,7,
      72,193,235,3,77,59,252,72,15,71,223,72,133,219,116,20,73,139,14,232,142,238,255,255,72,255,199,77,141,118,8,72,59,251,117,236,73,139,207,232,122,238,255,255,139,198,72,139,156,36,160,0,0,0,72,131,196,96,65,95,65,94,65,93,65,92,95,94,93,195,69,
      51,201,72,137,124,36,32,69,51,192,51,210,51,201,232,104,239,255,255,204,204,204,204,72,137,92,36,8,72,137,108,36,16,72,137,116,36,24,87,65,84,65,85,65,86,65,87,72,131,236,48,72,131,205,255,73,139,249,51,246,77,139,240,76,139,234,76,139,225,72,
      255,197,64,56,52,41,117,247,186,1,0,0,0,73,139,198,72,3,234,72,247,208,72,59,232,118,32,141,66,11,72,139,92,36,96,72,139,108,36,104,72,139,116,36,112,72,131,196,48,65,95,65,94,65,93,65,92,95,195,77,141,120,1,76,3,253,73,139,207,232,247,249,255,
      255,72,139,216,77,133,246,116,25,77,139,206,77,139,197,73,139,215,72,139,200,232,10,56,0,0,133,192,15,133,216,0,0,0,77,43,254,74,141,12,51,73,139,215,76,139,205,77,139,196,232,237,55,0,0,133,192,15,133,187,0,0,0,72,139,79,8,68,141,120,8,76,139,
      119,16,73,59,206,15,133,157,0,0,0,72,57,55,117,43,65,139,215,141,72,4,232,148,249,255,255,51,201,72,137,7,232,98,237,255,255,72,139,15,72,133,201,116,66,72,141,65,32,72,137,79,8,72,137,71,16,235,109,76,43,55,72,184,255,255,255,255,255,255,255,
      127,73,193,254,3,76,59,240,119,30,72,139,15,75,141,44,54,72,139,213,77,139,199,232,72,6,0,0,72,133,192,117,34,51,201,232,24,237,255,255,72,139,203,232,16,237,255,255,190,12,0,0,0,51,201,232,4,237,255,255,139,198,233,2,255,255,255,74,141,12,240,
      72,137,7,72,137,79,8,72,141,12,232,72,137,79,16,51,201,232,227,236,255,255,72,139,79,8,72,137,25,76,1,127,8,235,203,69,51,201,72,137,116,36,32,69,51,192,51,210,51,201,232,222,237,255,255,204,204,72,137,92,36,32,85,86,87,65,84,65,85,65,86,65,
      87,72,141,172,36,208,253,255,255,72,129,236,48,3,0,0,72,139,5,54,18,1,0,72,51,196,72,137,133,32,2,0,0,77,139,224,72,139,241,72,187,1,8,0,0,0,32,0,0,72,59,209,116,34,138,2,44,47,60,45,119,10,72,15,190,192,72,15,163,195,114,16,72,139,206,232,253,
      59,0,0,72,139,208,72,59,198,117,222,68,138,2,65,128,248,58,117,30,72,141,70,1,72,59,208,116,21,77,139,204,69,51,192,51,210,72,139,206,232,239,253,255,255,233,86,2,0,0,65,128,232,47,51,255,65,128,248,45,119,12,73,15,190,192,72,15,163,195,176,
      1,114,3,64,138,199,72,43,214,72,137,125,160,72,255,194,72,137,125,168,246,216,72,137,125,176,72,141,76,36,48,72,137,125,184,77,27,237,72,137,125,192,76,35,234,64,136,125,200,51,210,232,225,163,255,255,72,139,68,36,56,65,191,233,253,0,0,68,57,
      120,12,117,24,64,56,124,36,72,116,12,72,139,68,36,48,131,160,168,3,0,0,253,69,139,199,235,58,232,83,210,255,255,133,192,117,27,64,56,124,36,72,116,12,72,139,68,36,48,131,160,168,3,0,0,253,65,184,1,0,0,0,235,22,64,56,124,36,72,116,12,72,139,68,
      36,48,131,160,168,3,0,0,253,68,139,199,72,141,85,160,72,139,206,232,30,248,255,255,72,139,77,176,76,141,69,208,133,192,137,124,36,40,72,137,124,36,32,72,15,69,207,69,51,201,51,210,255,21,248,234,0,0,72,139,216,72,131,248,255,117,23,77,139,204,
      69,51,192,51,210,72,139,206,232,243,252,255,255,139,248,233,71,1,0,0,77,139,116,36,8,77,43,52,36,73,193,254,3,51,210,72,137,124,36,112,72,141,76,36,80,72,137,124,36,120,72,137,125,128,72,137,125,136,72,137,125,144,64,136,125,152,232,253,162,
      255,255,72,139,68,36,88,68,57,120,12,117,24,64,56,124,36,104,116,12,72,139,68,36,80,131,160,168,3,0,0,253,69,139,199,235,58,232,117,209,255,255,133,192,117,27,64,56,124,36,104,116,12,72,139,68,36,80,131,160,168,3,0,0,253,65,184,1,0,0,0,235,22,
      64,56,124,36,104,116,12,72,139,68,36,80,131,160,168,3,0,0,253,68,139,199,72,141,84,36,112,72,141,77,252,232,182,248,255,255,76,139,125,128,133,192,73,139,207,72,15,69,207,128,57,46,117,17,138,65,1,132,192,116,32,60,46,117,6,64,56,121,2,116,22,
      77,139,204,77,139,197,72,139,214,232,29,252,255,255,139,248,133,192,117,91,51,255,64,56,125,152,116,8,73,139,207,232,79,234,255,255,72,141,85,208,72,139,203,255,21,238,233,0,0,65,191,233,253,0,0,133,192,15,133,13,255,255,255,73,139,4,36,73,139,
      84,36,8,72,43,208,72,193,250,3,76,59,242,116,41,73,43,214,74,141,12,240,76,141,13,97,0,0,0,65,184,8,0,0,0,232,186,48,0,0,235,14,128,125,152,0,116,8,73,139,207,232,246,233,255,255,72,139,203,255,21,137,233,0,0,128,125,200,0,116,9,72,139,77,176,
      232,222,233,255,255,139,199,72,139,141,32,2,0,0,72,51,204,232,1,207,255,255,72,139,156,36,136,3,0,0,72,129,196,48,3,0,0,65,95,65,94,65,93,65,92,95,94,93,195,204,204,72,59,202,115,4,131,200,255,195,51,192,72,59,202,15,151,192,195,204,204,72,137,
      92,36,16,72,137,124,36,24,85,72,141,172,36,112,254,255,255,72,129,236,144,2,0,0,72,139,5,23,15,1,0,72,51,196,72,137,133,128,1,0,0,65,139,248,72,139,218,65,184,5,1,0,0,72,141,84,36,112,255,21,150,233,0,0,133,192,117,20,255,21,132,233,0,0,139,
      200,232,217,244,255,255,51,192,233,160,0,0,0,72,131,100,36,96,0,72,141,76,36,32,72,139,199,72,137,92,36,64,51,210,72,137,68,36,72,72,137,68,36,88,72,137,92,36,80,198,68,36,104,0,232,16,161,255,255,72,139,68,36,40,65,184,233,253,0,0,68,57,64,
      12,117,21,128,124,36,56,0,116,71,72,139,68,36,32,131,160,168,3,0,0,253,235,57,232,133,207,255,255,133,192,117,26,56,68,36,56,116,12,72,139,68,36,32,131,160,168,3,0,0,253,65,184,1,0,0,0,235,22,128,124,36,56,0,116,12,72,139,68,36,32,131,160,168,
      3,0,0,253,69,51,192,72,141,84,36,64,72,141,76,36,112,232,42,0,0,0,139,68,36,96,72,139,141,128,1,0,0,72,51,204,232,199,205,255,255,76,141,156,36,144,2,0,0,73,139,91,24,73,139,123,32,73,139,227,93,195,204,204,72,137,92,36,8,72,137,108,36,16,72,
      137,116,36,24,87,72,131,236,64,51,219,65,139,232,72,139,250,72,139,241,72,133,201,117,25,56,90,40,116,3,136,90,40,72,137,90,16,72,137,90,24,72,137,90,32,233,189,0,0,0,102,57,25,117,48,72,57,90,24,117,34,56,90,40,116,3,136,90,40,232,119,243,255,
      255,185,34,0,0,0,137,8,136,95,40,72,137,95,24,139,217,233,144,0,0,0,72,139,66,16,136,24,235,194,72,137,92,36,56,65,131,201,255,72,137,92,36,48,76,139,198,137,92,36,40,51,210,139,205,72,137,92,36,32,232,255,1,0,0,72,99,208,133,192,117,22,255,
      21,14,232,0,0,139,200,232,99,243,255,255,232,30,243,255,255,139,24,235,72,72,139,79,24,72,59,209,118,10,56,95,40,116,144,136,95,40,235,139,72,139,71,16,65,131,201,255,72,137,92,36,56,76,139,198,72,137,92,36,48,51,210,137,76,36,40,139,205,72,
      137,68,36,32,232,168,1,0,0,72,99,200,133,192,116,169,72,255,201,72,137,79,32,72,139,108,36,88,139,195,72,139,92,36,80,72,139,116,36,96,72,131,196,64,95,195,204,204,204,139,209,65,185,4,0,0,0,51,201,69,51,192,233,2,0,0,0,204,204,72,137,92,36,
      8,72,137,116,36,16,87,72,131,236,64,139,218,65,139,249,72,139,209,65,139,240,72,141,76,36,32,232,44,159,255,255,72,139,68,36,48,15,182,211,64,132,124,2,25,117,26,133,246,116,16,72,139,68,36,40,72,139,8,15,183,4,81,35,198,235,2,51,192,133,192,
      116,5,184,1,0,0,0,128,124,36,56,0,116,12,72,139,76,36,32,131,161,168,3,0,0,253,72,139,92,36,80,72,139,116,36,88,72,131,196,64,95,195,204,204,204,72,137,92,36,8,72,137,108,36,16,72,137,116,36,24,87,72,131,236,32,73,139,232,72,139,218,72,139,241,
      72,133,210,116,29,51,210,72,141,66,224,72,247,243,73,59,192,115,15,232,247,241,255,255,199,0,12,0,0,0,51,192,235,65,72,133,246,116,10,232,203,54,0,0,72,139,248,235,2,51,255,72,15,175,221,72,139,206,72,139,211,232,241,54,0,0,72,139,240,72,133,
      192,116,22,72,59,251,115,17,72,43,223,72,141,12,56,76,139,195,51,210,232,83,136,255,255,72,139,198,72,139,92,36,48,72,139,108,36,56,72,139,116,36,64,72,131,196,32,95,195,204,204,204,129,249,53,196,0,0,119,32,141,129,212,59,255,255,131,248,9,
      119,12,65,186,167,2,0,0,65,15,163,194,114,5,131,249,42,117,47,51,210,235,43,129,249,152,214,0,0,116,32,129,249,169,222,0,0,118,27,129,249,179,222,0,0,118,228,129,249,232,253,0,0,116,220,129,249,233,253,0,0,117,3,131,226,8,72,255,37,246,230,0,
      0,204,204,72,137,92,36,8,87,141,129,24,2,255,255,69,139,217,131,248,1,73,139,216,65,15,150,194,51,255,129,249,53,196,0,0,119,28,141,129,212,59,255,255,131,248,9,119,12,65,184,167,2,0,0,65,15,163,192,114,51,131,249,42,235,38,129,249,152,214,0,
      0,116,38,129,249,169,222,0,0,118,24,129,249,179,222,0,0,118,22,129,249,232,253,0,0,116,14,129,249,233,253,0,0,116,6,15,186,242,7,235,2,139,215,72,139,68,36,72,69,132,210,76,139,76,36,64,76,139,192,76,15,69,199,76,15,69,207,116,7,72,133,192,116,
      2,137,56,76,137,68,36,72,76,139,195,76,137,76,36,64,69,139,203,72,139,92,36,16,95,72,255,37,7,231,0,0,204,204,204,72,139,196,72,137,88,8,72,137,104,16,72,137,112,24,72,137,120,32,65,86,72,131,236,64,255,21,45,229,0,0,69,51,246,72,139,216,72,
      133,192,15,132,164,0,0,0,72,139,240,102,68,57,48,116,28,72,131,200,255,72,255,192,102,68,57,52,70,117,246,72,141,52,70,72,131,198,2,102,68,57,54,117,228,76,137,116,36,56,72,43,243,76,137,116,36,48,72,131,198,2,72,209,254,76,139,195,68,139,206,
      68,137,116,36,40,51,210,76,137,116,36,32,51,201,232,208,254,255,255,72,99,232,133,192,116,75,72,139,205,232,117,2,0,0,72,139,248,72,133,192,116,46,76,137,116,36,56,68,139,206,76,137,116,36,48,76,139,195,137,108,36,40,51,210,51,201,72,137,68,
      36,32,232,151,254,255,255,133,192,116,8,72,139,247,73,139,254,235,3,73,139,246,72,139,207,232,104,228,255,255,235,3,73,139,246,72,133,219,116,9,72,139,203,255,21,17,228,0,0,72,139,92,36,80,72,139,198,72,139,116,36,96,72,139,108,36,88,72,139,
      124,36,104,72,131,196,64,65,94,195,204,204,204,102,137,76,36,8,72,131,236,88,184,255,255,0,0,102,59,200,15,132,159,0,0,0,72,141,76,36,48,232,15,156,255,255,15,183,84,36,96,65,186,0,1,0,0,102,65,59,210,115,42,15,182,194,72,141,13,14,130,0,0,246,
      4,65,1,116,21,72,139,68,36,56,15,182,210,72,139,136,16,1,0,0,15,182,20,17,235,73,15,182,210,235,68,72,139,68,36,56,72,139,136,56,1,0,0,72,133,201,116,51,72,141,68,36,112,199,68,36,40,1,0,0,0,65,185,1,0,0,0,72,137,68,36,32,76,141,68,36,96,65,
      139,210,232,146,52,0,0,15,183,84,36,96,133,192,116,5,15,183,84,36,112,128,124,36,72,0,116,12,72,139,76,36,48,131,161,168,3,0,0,253,15,183,194,72,131,196,88,195,204,64,83,72,131,236,32,72,139,5,71,32,1,0,72,139,218,72,57,2,116,22,139,129,168,
      3,0,0,133,5,239,16,1,0,117,8,232,180,17,0,0,72,137,3,72,131,196,32,91,195,204,204,204,64,83,72,131,236,32,72,139,5,211,31,1,0,72,139,218,72,57,2,116,22,139,129,168,3,0,0,133,5,187,16,1,0,117,8,232,188,216,255,255,72,137,3,72,131,196,32,91,195,
      204,204,204,184,1,0,0,0,135,5,9,32,1,0,195,76,139,220,72,131,236,40,184,4,0,0,0,77,141,75,16,77,141,67,8,137,68,36,56,73,141,83,24,137,68,36,64,73,141,75,8,232,7,0,0,0,72,131,196,40,195,204,204,72,137,92,36,8,72,137,116,36,16,76,137,76,36,32,
      87,72,131,236,48,73,139,249,139,10,232,242,206,255,255,144,72,141,29,138,31,1,0,72,141,53,171,13,1,0,72,137,92,36,32,72,141,5,127,31,1,0,72,59,216,116,25,72,57,51,116,14,72,139,214,72,139,203,232,90,17,0,0,72,137,3,72,131,195,8,235,214,139,15,
      232,206,206,255,255,72,139,92,36,64,72,139,116,36,72,72,131,196,48,95,195,204,204,72,131,236,40,232,87,207,255,255,72,141,84,36,48,72,139,136,144,0,0,0,72,137,76,36,48,72,139,200,232,202,254,255,255,72,139,68,36,48,72,139,0,72,131,196,40,195,
      204,64,83,72,131,236,32,72,139,217,72,131,249,224,119,60,72,133,201,184,1,0,0,0,72,15,68,216,235,21,232,178,40,0,0,133,192,116,37,72,139,203,232,234,135,255,255,133,192,116,25,72,139,13,71,26,1,0,76,139,195,51,210,255,21,132,226,0,0,72,133,192,
      116,212,235,13,232,52,237,255,255,199,0,12,0,0,0,51,192,72,131,196,32,91,195,204,204,72,137,92,36,16,87,72,131,236,32,184,255,255,0,0,15,183,218,102,59,200,116,72,184,0,1,0,0,102,59,200,115,18,72,139,5,48,15,1,0,15,183,201,15,183,4,72,35,195,
      235,46,51,255,102,137,76,36,64,76,141,76,36,48,102,137,124,36,48,72,141,84,36,64,141,79,1,68,139,193,232,172,54,0,0,133,192,116,7,15,183,68,36,48,235,208,51,192,72,139,92,36,56,72,131,196,32,95,195,233,139,237,255,255,204,204,204,64,83,72,131,
      236,32,72,139,217,76,141,13,140,129,0,0,51,201,76,141,5,123,129,0,0,72,141,21,44,112,0,0,232,107,1,0,0,72,133,192,116,15,72,139,203,72,131,196,32,91,72,255,37,171,70,1,0,72,131,196,32,91,72,255,37,167,226,0,0,204,204,204,64,83,72,131,236,32,
      139,217,76,141,13,77,129,0,0,185,1,0,0,0,76,141,5,57,129,0,0,72,141,21,250,111,0,0,232,33,1,0,0,139,203,72,133,192,116,12,72,131,196,32,91,72,255,37,98,70,1,0,72,131,196,32,91,72,255,37,102,226,0,0,204,204,64,83,72,131,236,32,139,217,76,141,
      13,13,129,0,0,185,2,0,0,0,76,141,5,249,128,0,0,72,141,21,194,111,0,0,232,217,0,0,0,139,203,72,133,192,116,12,72,131,196,32,91,72,255,37,26,70,1,0,72,131,196,32,91,72,255,37,38,226,0,0,204,204,72,137,92,36,8,87,72,131,236,32,72,139,218,76,141,
      13,200,128,0,0,139,249,72,141,21,151,111,0,0,185,3,0,0,0,76,141,5,171,128,0,0,232,138,0,0,0,72,139,211,139,207,72,133,192,116,8,255,21,206,69,1,0,235,6,255,21,230,225,0,0,72,139,92,36,48,72,131,196,32,95,195,204,204,204,72,137,92,36,8,72,137,
      116,36,16,87,72,131,236,32,65,139,240,76,141,13,119,128,0,0,139,218,76,141,5,102,128,0,0,72,139,249,72,141,21,76,111,0,0,185,4,0,0,0,232,46,0,0,0,139,211,72,139,207,72,133,192,116,11,68,139,198,255,21,111,69,1,0,235,6,255,21,151,224,0,0,72,139,
      92,36,48,72,139,116,36,56,72,131,196,32,95,195,204,204,204,72,137,92,36,8,72,137,108,36,16,72,137,116,36,24,87,65,84,65,85,65,86,65,87,72,131,236,32,139,249,76,141,61,31,53,255,255,77,139,225,73,139,232,76,139,234,73,139,132,255,176,231,1,0,
      73,131,206,255,73,59,198,15,132,234,0,0,0,72,133,192,15,133,227,0,0,0,77,59,193,15,132,208,0,0,0,139,117,0,73,139,156,247,152,231,1,0,72,133,219,116,11,73,59,222,15,133,153,0,0,0,235,107,77,139,188,247,168,74,1,0,51,210,73,139,207,65,184,0,8,
      0,0,255,21,67,224,0,0,72,139,216,72,133,192,117,86,255,21,109,223,0,0,131,248,87,117,45,68,141,67,7,73,139,207,72,141,21,210,109,0,0,232,117,9,0,0,133,192,116,22,69,51,192,51,210,73,139,207,255,21,11,224,0,0,72,139,216,72,133,192,117,30,73,139,
      198,76,141,61,113,52,255,255,73,135,132,247,152,231,1,0,72,131,197,4,73,59,236,233,104,255,255,255,72,139,195,76,141,61,83,52,255,255,73,135,132,247,152,231,1,0,72,133,192,116,9,72,139,203,255,21,141,222,0,0,73,139,213,72,139,203,255,21,25,223,
      0,0,72,133,192,116,13,72,139,200,73,135,140,255,176,231,1,0,235,10,77,135,180,255,176,231,1,0,51,192,72,139,92,36,80,72,139,108,36,88,72,139,116,36,96,72,131,196,32,65,95,65,94,65,93,65,92,95,195,204,204,204,204,204,204,204,204,204,204,204,204,
      204,204,102,102,15,31,132,0,0,0,0,0,72,131,236,40,72,137,76,36,48,72,137,84,36,56,68,137,68,36,64,72,139,18,72,139,193,232,226,161,255,255,255,208,232,11,162,255,255,72,139,200,72,139,84,36,56,72,139,18,65,184,2,0,0,0,232,197,161,255,255,72,
      131,196,40,195,204,204,204,204,204,204,102,102,15,31,132,0,0,0,0,0,72,131,236,40,72,137,76,36,48,72,137,84,36,56,68,137,68,36,64,72,139,18,72,139,193,232,146,161,255,255,255,208,232,187,161,255,255,72,131,196,40,195,204,204,204,204,204,204,72,
      131,236,40,72,137,76,36,48,72,137,84,36,56,72,139,84,36,56,72,139,18,65,184,2,0,0,0,232,95,161,255,255,72,131,196,40,195,204,204,204,204,204,204,15,31,64,0,72,131,236,40,72,137,76,36,48,72,137,84,36,56,76,137,68,36,64,68,137,76,36,72,69,139,
      193,72,139,193,232,45,161,255,255,72,139,76,36,64,255,208,232,81,161,255,255,72,139,200,72,139,84,36,56,65,184,2,0,0,0,232,14,161,255,255,72,131,196,40,195,204,72,139,196,72,137,88,8,72,137,104,16,72,137,112,24,72,137,120,32,65,86,72,131,236,
      32,77,139,81,56,72,139,242,77,139,240,72,139,233,73,139,209,72,139,206,73,139,249,65,139,26,72,193,227,4,73,3,218,76,141,67,4,232,214,193,255,255,139,69,4,36,102,246,216,184,1,0,0,0,27,210,247,218,3,208,133,83,4,116,17,76,139,207,77,139,198,
      72,139,214,72,139,205,232,38,114,255,255,72,139,92,36,48,72,139,108,36,56,72,139,116,36,64,72,139,124,36,72,72,131,196,32,65,94,195,204,204,204,204,204,204,204,204,204,102,102,15,31,132,0,0,0,0,0,87,86,73,139,195,72,139,249,73,139,200,73,139,
      242,243,164,94,95,195,204,204,204,204,204,204,15,31,128,0,0,0,0,76,139,217,76,139,210,73,131,248,16,118,84,73,131,248,32,118,46,72,43,209,115,13,75,141,4,16,72,59,200,15,130,220,2,0,0,73,129,248,128,0,0,0,15,134,15,2,0,0,246,5,208,13,1,0,2,15,
      132,82,1,0,0,235,160,15,16,2,66,15,16,76,2,240,15,17,1,66,15,17,76,1,240,72,139,193,195,102,102,15,31,132,0,0,0,0,0,72,139,193,76,141,13,198,49,255,255,67,139,140,129,80,48,2,0,73,3,201,255,225,102,15,31,132,0,0,0,0,0,195,15,183,10,102,137,8,
      195,72,139,10,72,137,8,195,15,183,10,68,15,182,66,2,102,137,8,68,136,64,2,195,15,182,10,136,8,195,243,15,111,2,243,15,127,0,195,102,144,76,139,2,15,183,74,8,68,15,182,74,10,76,137,0,102,137,72,8,68,136,72,10,195,139,10,137,8,195,15,31,0,139,
      10,68,15,182,66,4,137,8,68,136,64,4,195,102,144,139,10,68,15,183,66,4,137,8,102,68,137,64,4,195,144,139,10,68,15,183,66,4,68,15,182,74,6,137,8,102,68,137,64,4,68,136,72,6,195,76,139,2,139,74,8,68,15,182,74,12,76,137,0,137,72,8,68,136,72,12,195,
      102,144,76,139,2,15,182,74,8,76,137,0,136,72,8,195,102,144,76,139,2,15,183,74,8,76,137,0,102,137,72,8,195,144,76,139,2,139,74,8,76,137,0,137,72,8,195,15,31,0,76,139,2,139,74,8,68,15,183,74,12,76,137,0,137,72,8,102,68,137,72,12,195,102,15,31,
      132,0,0,0,0,0,76,139,2,139,74,8,68,15,183,74,12,68,15,182,82,14,76,137,0,137,72,8,102,68,137,72,12,68,136,80,14,195,15,16,4,17,76,3,193,72,131,193,16,65,246,195,15,116,19,15,40,200,72,131,225,240,15,16,4,17,72,131,193,16,65,15,17,11,76,43,193,
      77,139,200,73,193,233,7,15,132,136,0,0,0,15,41,65,240,76,59,13,113,0,1,0,118,23,233,194,0,0,0,102,102,15,31,132,0,0,0,0,0,15,41,65,224,15,41,73,240,15,16,4,17,15,16,76,17,16,72,129,193,128,0,0,0,15,41,65,128,15,41,73,144,15,16,68,17,160,15,16,
      76,17,176,73,255,201,15,41,65,160,15,41,73,176,15,16,68,17,192,15,16,76,17,208,15,41,65,192,15,41,73,208,15,16,68,17,224,15,16,76,17,240,117,173,15,41,65,224,73,131,224,127,15,40,193,235,12,15,16,4,17,72,131,193,16,73,131,232,16,77,139,200,73,
      193,233,4,116,28,102,102,102,15,31,132,0,0,0,0,0,15,17,65,240,15,16,4,17,72,131,193,16,73,255,201,117,239,73,131,224,15,116,13,74,141,4,1,15,16,76,16,240,15,17,72,240,15,17,65,240,73,139,195,195,15,31,64,0,15,43,65,224,15,43,73,240,15,24,132,
      17,0,2,0,0,15,16,4,17,15,16,76,17,16,72,129,193,128,0,0,0,15,43,65,128,15,43,73,144,15,16,68,17,160,15,16,76,17,176,73,255,201,15,43,65,160,15,43,73,176,15,16,68,17,192,15,16,76,17,208,15,24,132,17,64,2,0,0,15,43,65,192,15,43,73,208,15,16,68,
      17,224,15,16,76,17,240,117,157,15,174,248,233,56,255,255,255,15,31,68,0,0,73,3,200,15,16,68,17,240,72,131,233,16,73,131,232,16,246,193,15,116,23,72,139,193,72,131,225,240,15,16,200,15,16,4,17,15,17,8,76,139,193,77,43,195,77,139,200,73,193,233,
      7,116,104,15,41,1,235,13,102,15,31,68,0,0,15,41,65,16,15,41,9,15,16,68,17,240,15,16,76,17,224,72,129,233,128,0,0,0,15,41,65,112,15,41,73,96,15,16,68,17,80,15,16,76,17,64,73,255,201,15,41,65,80,15,41,73,64,15,16,68,17,48,15,16,76,17,32,15,41,
      65,48,15,41,73,32,15,16,68,17,16,15,16,12,17,117,174,15,41,65,16,73,131,224,127,15,40,193,77,139,200,73,193,233,4,116,26,102,102,15,31,132,0,0,0,0,0,15,17,1,72,131,233,16,15,16,4,17,73,255,201,117,240,73,131,224,15,116,8,65,15,16,10,65,15,17,
      11,15,17,1,73,139,195,195,204,204,204,137,76,36,8,72,131,236,40,185,23,0,0,0,232,246,75,0,0,133,192,116,8,139,68,36,48,139,200,205,41,72,141,13,179,22,1,0,232,90,1,0,0,72,139,68,36,40,72,137,5,154,23,1,0,72,141,68,36,40,72,131,192,8,72,137,5,
      42,23,1,0,72,139,5,131,23,1,0,72,137,5,244,21,1,0,199,5,218,21,1,0,9,4,0,192,199,5,212,21,1,0,1,0,0,0,199,5,222,21,1,0,1,0,0,0,184,8,0,0,0,72,107,192,0,72,141,13,214,21,1,0,139,84,36,48,72,137,20,1,72,141,13,175,120,0,0,232,210,1,0,0,72,131,
      196,40,195,204,72,131,236,40,185,8,0,0,0,232,86,255,255,255,72,131,196,40,195,204,72,137,76,36,8,72,131,236,56,185,23,0,0,0,232,69,75,0,0,133,192,116,7,185,2,0,0,0,205,41,72,141,13,3,22,1,0,232,26,1,0,0,72,139,68,36,56,72,137,5,234,22,1,0,72,
      141,68,36,56,72,131,192,8,72,137,5,122,22,1,0,72,139,5,211,22,1,0,72,137,5,68,21,1,0,72,139,68,36,64,72,137,5,72,22,1,0,199,5,30,21,1,0,9,4,0,192,199,5,24,21,1,0,1,0,0,0,199,5,34,21,1,0,1,0,0,0,184,8,0,0,0,72,107,192,0,72,141,13,26,21,1,0,72,
      199,4,1,2,0,0,0,184,8,0,0,0,72,107,192,0,72,139,13,42,253,0,0,72,137,76,4,32,184,8,0,0,0,72,107,192,1,72,139,13,13,253,0,0,72,137,76,4,32,72,141,13,201,119,0,0,232,236,0,0,0,72,131,196,56,195,204,204,204,72,137,92,36,32,87,72,131,236,64,72,139,
      217,255,21,129,216,0,0,72,139,187,248,0,0,0,72,141,84,36,80,72,139,207,69,51,192,255,21,113,216,0,0,72,133,192,116,50,72,131,100,36,56,0,72,141,76,36,88,72,139,84,36,80,76,139,200,72,137,76,36,48,76,139,199,72,141,76,36,96,72,137,76,36,40,51,
      201,72,137,92,36,32,255,21,82,216,0,0,72,139,92,36,104,72,131,196,64,95,195,204,204,204,64,83,86,87,72,131,236,64,72,139,217,255,21,19,216,0,0,72,139,179,248,0,0,0,51,255,69,51,192,72,141,84,36,96,72,139,206,255,21,1,216,0,0,72,133,192,116,57,
      72,131,100,36,56,0,72,141,76,36,104,72,139,84,36,96,76,139,200,72,137,76,36,48,76,139,198,72,141,76,36,112,72,137,76,36,40,51,201,72,137,92,36,32,255,21,226,215,0,0,255,199,131,255,2,124,177,72,131,196,64,95,94,91,195,204,204,204,64,83,72,131,
      236,32,72,139,217,51,201,255,21,231,215,0,0,72,139,203,255,21,14,216,0,0,255,21,88,214,0,0,72,139,200,186,9,4,0,192,72,131,196,32,91,72,255,37,204,215,0,0,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,102,102,15,31,
      132,0,0,0,0,0,72,43,209,77,133,192,116,106,247,193,7,0,0,0,116,29,15,182,1,58,4,10,117,93,72,255,193,73,255,200,116,82,132,192,116,78,72,247,193,7,0,0,0,117,227,73,187,128,128,128,128,128,128,128,128,73,186,255,254,254,254,254,254,254,254,141,
      4,10,37,255,15,0,0,61,248,15,0,0,119,192,72,139,1,72,59,4,10,117,183,72,131,193,8,73,131,232,8,118,15,77,141,12,2,72,247,208,73,35,193,73,133,195,116,207,51,192,195,72,27,192,72,131,200,1,195,204,204,204,77,133,192,117,24,51,192,195,15,183,1,
      102,133,192,116,19,102,59,2,117,14,72,131,193,2,72,131,194,2,73,131,232,1,117,229,15,183,1,15,183,10,43,193,195,72,137,92,36,8,72,137,108,36,16,72,137,116,36,24,87,65,86,65,87,72,131,236,32,76,139,241,72,133,201,116,116,51,219,76,141,61,203,
      42,255,255,191,227,0,0,0,141,4,31,65,184,85,0,0,0,153,73,139,206,43,194,209,248,72,99,232,72,139,213,72,139,245,72,3,210,73,139,148,215,96,101,1,0,232,228,43,0,0,133,192,116,19,121,5,141,125,255,235,3,141,93,1,59,223,126,196,131,200,255,235,
      11,72,3,246,65,139,132,247,104,101,1,0,133,192,120,22,61,228,0,0,0,115,15,72,152,72,3,192,65,139,132,199,0,75,1,0,235,2,51,192,72,139,92,36,64,72,139,108,36,72,72,139,116,36,80,72,131,196,32,65,95,65,94,95,195,204,240,255,65,16,72,139,129,224,
      0,0,0,72,133,192,116,3,240,255,0,72,139,129,240,0,0,0,72,133,192,116,3,240,255,0,72,139,129,232,0,0,0,72,133,192,116,3,240,255,0,72,139,129,0,1,0,0,72,133,192,116,3,240,255,0,72,141,65,56,65,184,6,0,0,0,72,141,21,235,0,1,0,72,57,80,240,116,11,
      72,139,16,72,133,210,116,3,240,255,2,72,131,120,232,0,116,12,72,139,80,248,72,133,210,116,3,240,255,2,72,131,192,32,73,131,232,1,117,203,72,139,137,32,1,0,0,233,33,2,0,0,204,72,131,236,40,72,133,201,15,132,150,0,0,0,65,131,201,255,240,68,1,73,
      16,72,139,129,224,0,0,0,72,133,192,116,4,240,68,1,8,72,139,129,240,0,0,0,72,133,192,116,4,240,68,1,8,72,139,129,232,0,0,0,72,133,192,116,4,240,68,1,8,72,139,129,0,1,0,0,72,133,192,116,4,240,68,1,8,72,141,65,56,65,184,6,0,0,0,72,141,21,73,0,1,
      0,72,57,80,240,116,12,72,139,16,72,133,210,116,4,240,68,1,10,72,131,120,232,0,116,13,72,139,80,248,72,133,210,116,4,240,68,1,10,72,131,192,32,73,131,232,1,117,201,72,139,137,32,1,0,0,232,165,1,0,0,72,131,196,40,195,72,137,92,36,8,72,137,108,
      36,16,72,137,116,36,24,87,72,131,236,32,72,139,129,248,0,0,0,72,139,217,72,133,192,116,121,72,141,13,22,1,1,0,72,59,193,116,109,72,139,131,224,0,0,0,72,133,192,116,97,131,56,0,117,92,72,139,139,240,0,0,0,72,133,201,116,22,131,57,0,117,17,232,
      70,211,255,255,72,139,139,248,0,0,0,232,178,36,0,0,72,139,139,232,0,0,0,72,133,201,116,22,131,57,0,117,17,232,36,211,255,255,72,139,139,248,0,0,0,232,156,37,0,0,72,139,139,224,0,0,0,232,12,211,255,255,72,139,139,248,0,0,0,232,0,211,255,255,72,
      139,131,0,1,0,0,72,133,192,116,71,131,56,0,117,66,72,139,139,8,1,0,0,72,129,233,254,0,0,0,232,220,210,255,255,72,139,139,16,1,0,0,191,128,0,0,0,72,43,207,232,200,210,255,255,72,139,139,24,1,0,0,72,43,207,232,185,210,255,255,72,139,139,0,1,0,
      0,232,173,210,255,255,72,139,139,32,1,0,0,232,205,0,0,0,72,141,179,40,1,0,0,189,6,0,0,0,72,141,123,56,72,141,5,246,254,0,0,72,57,71,240,116,26,72,139,15,72,133,201,116,18,131,57,0,117,13,232,114,210,255,255,72,139,14,232,106,210,255,255,72,131,
      127,232,0,116,19,72,139,79,248,72,133,201,116,10,131,57,0,117,5,232,80,210,255,255,72,131,198,8,72,131,199,32,72,131,237,1,117,177,72,139,203,72,139,92,36,48,72,139,108,36,56,72,139,116,36,64,72,131,196,32,95,233,38,210,255,255,204,204,72,133,
      201,116,28,72,141,5,60,98,0,0,72,59,200,116,16,184,1,0,0,0,240,15,193,129,92,1,0,0,255,192,195,184,255,255,255,127,195,204,72,133,201,116,26,72,141,5,20,98,0,0,72,59,200,116,14,131,200,255,240,15,193,129,92,1,0,0,255,200,195,184,255,255,255,
      127,195,204,204,204,72,133,201,116,48,83,72,131,236,32,72,141,5,231,97,0,0,72,139,217,72,59,200,116,23,139,129,92,1,0,0,133,192,117,13,232,156,36,0,0,72,139,203,232,164,209,255,255,72,131,196,32,91,195,204,204,72,137,92,36,8,87,72,131,236,32,
      232,137,190,255,255,72,141,184,144,0,0,0,139,136,168,3,0,0,139,5,18,255,0,0,133,200,116,8,72,139,31,72,133,219,117,44,185,4,0,0,0,232,160,189,255,255,144,72,139,21,56,14,1,0,72,139,207,232,40,0,0,0,72,139,216,185,4,0,0,0,232,159,189,255,255,
      72,133,219,116,14,72,139,195,72,139,92,36,48,72,131,196,32,95,195,232,47,149,255,255,144,204,204,72,137,92,36,8,87,72,131,236,32,72,139,250,72,133,210,116,70,72,133,201,116,65,72,139,25,72,59,218,117,5,72,139,199,235,54,72,137,57,72,139,207,
      232,45,252,255,255,72,133,219,116,235,72,139,203,232,172,252,255,255,131,123,16,0,117,221,72,141,5,231,251,0,0,72,59,216,116,209,72,139,203,232,58,253,255,255,235,199,51,192,72,139,92,36,48,72,131,196,32,95,195,204,204,204,72,131,236,40,131,
      249,254,117,21,232,38,220,255,255,131,32,0,232,254,219,255,255,199,0,9,0,0,0,235,78,133,201,120,50,59,13,240,12,1,0,115,42,72,99,201,76,141,5,228,8,1,0,72,139,193,131,225,63,72,193,248,6,72,141,20,201,73,139,4,192,246,68,208,56,1,116,7,72,139,
      68,208,40,235,28,232,219,219,255,255,131,32,0,232,179,219,255,255,199,0,9,0,0,0,232,84,209,255,255,72,131,200,255,72,131,196,40,195,204,204,204,72,137,92,36,8,72,137,116,36,16,72,137,124,36,24,65,86,72,131,236,32,72,99,217,133,201,120,114,59,
      29,126,12,1,0,115,106,72,139,195,76,141,53,114,8,1,0,131,224,63,72,139,243,72,193,254,6,72,141,60,192,73,139,4,246,246,68,248,56,1,116,71,72,131,124,248,40,255,116,63,232,0,39,0,0,131,248,1,117,39,133,219,116,22,43,216,116,11,59,216,117,27,185,
      244,255,255,255,235,12,185,245,255,255,255,235,5,185,246,255,255,255,51,210,255,21,64,209,0,0,73,139,4,246,72,131,76,248,40,255,51,192,235,22,232,9,219,255,255,199,0,9,0,0,0,232,30,219,255,255,131,32,0,131,200,255,72,139,92,36,48,72,139,116,
      36,56,72,139,124,36,64,72,131,196,32,65,94,195,204,204,72,137,92,36,8,72,137,108,36,16,72,137,116,36,24,87,72,131,236,32,186,72,0,0,0,141,74,248,232,151,219,255,255,51,246,72,139,216,72,133,192,116,91,72,141,168,0,18,0,0,72,59,197,116,76,72,
      141,120,48,72,141,79,208,69,51,192,186,160,15,0,0,232,72,183,255,255,72,131,79,248,255,72,141,79,14,128,103,13,248,139,198,72,137,55,199,71,8,0,0,10,10,198,71,12,10,64,136,49,255,192,72,255,193,131,248,5,114,243,72,131,199,72,72,141,71,208,72,
      59,197,117,184,72,139,243,51,201,232,3,207,255,255,72,139,92,36,48,72,139,198,72,139,116,36,64,72,139,108,36,56,72,131,196,32,95,195,204,204,204,72,133,201,116,74,72,137,92,36,8,72,137,116,36,16,87,72,131,236,32,72,141,177,0,18,0,0,72,139,217,
      72,139,249,72,59,206,116,18,72,139,207,255,21,53,206,0,0,72,131,199,72,72,59,254,117,238,72,139,203,232,168,206,255,255,72,139,92,36,48,72,139,116,36,56,72,131,196,32,95,195,72,137,92,36,8,72,137,116,36,16,72,137,124,36,24,65,87,72,131,236,48,
      139,241,129,249,0,32,0,0,114,41,232,196,217,255,255,187,9,0,0,0,137,24,232,100,207,255,255,139,195,72,139,92,36,64,72,139,116,36,72,72,139,124,36,80,72,131,196,48,65,95,195,51,255,141,79,7,232,130,186,255,255,144,139,223,139,5,145,10,1,0,72,
      137,92,36,32,59,240,124,54,76,141,61,129,6,1,0,73,57,60,223,116,2,235,34,232,144,254,255,255,73,137,4,223,72,133,192,117,5,141,120,12,235,20,139,5,96,10,1,0,131,192,64,137,5,87,10,1,0,72,255,195,235,193,185,7,0,0,0,232,76,186,255,255,139,199,
      235,138,72,99,209,76,141,5,58,6,1,0,72,139,194,131,226,63,72,193,248,6,72,141,12,210,73,139,4,192,72,141,12,200,72,255,37,85,205,0,0,204,72,99,209,76,141,5,18,6,1,0,72,139,194,131,226,63,72,193,248,6,72,141,12,210,73,139,4,192,72,141,12,200,
      72,255,37,141,206,0,0,204,64,85,65,84,65,85,65,86,65,87,72,131,236,96,72,141,108,36,48,72,137,93,96,72,137,117,104,72,137,125,112,72,139,5,22,243,0,0,72,51,197,72,137,69,32,68,139,234,69,139,249,72,139,209,77,139,224,72,141,77,0,232,90,133,255,
      255,139,189,136,0,0,0,133,255,117,7,72,139,69,8,139,120,12,247,157,144,0,0,0,69,139,207,77,139,196,139,207,27,210,131,100,36,40,0,72,131,100,36,32,0,131,226,8,255,194,232,228,230,255,255,76,99,240,133,192,117,7,51,255,233,206,0,0,0,73,139,246,
      72,3,246,72,141,70,16,72,59,240,72,27,201,72,35,200,116,83,72,129,249,0,4,0,0,119,49,72,141,65,15,72,59,193,119,10,72,184,240,255,255,255,255,255,255,15,72,131,224,240,232,4,36,0,0,72,43,224,72,141,92,36,48,72,133,219,116,111,199,3,204,204,0,
      0,235,19,232,146,234,255,255,72,139,216,72,133,192,116,14,199,0,221,221,0,0,72,131,195,16,235,2,51,219,72,133,219,116,71,76,139,198,51,210,72,139,203,232,146,110,255,255,69,139,207,68,137,116,36,40,77,139,196,72,137,92,36,32,186,1,0,0,0,139,
      207,232,62,230,255,255,133,192,116,26,76,139,141,128,0,0,0,68,139,192,72,139,211,65,139,205,255,21,228,204,0,0,139,248,235,2,51,255,72,133,219,116,17,72,141,75,240,129,57,221,221,0,0,117,5,232,76,204,255,255,128,125,24,0,116,11,72,139,69,0,131,
      160,168,3,0,0,253,139,199,72,139,77,32,72,51,205,232,97,177,255,255,72,139,93,96,72,139,117,104,72,139,125,112,72,141,101,48,65,95,65,94,65,93,65,92,93,195,204,204,204,72,137,92,36,8,72,137,116,36,16,87,72,131,236,112,72,139,242,73,139,217,72,
      139,209,65,139,248,72,141,76,36,80,232,231,131,255,255,139,132,36,192,0,0,0,72,141,76,36,88,137,68,36,64,76,139,203,139,132,36,184,0,0,0,68,139,199,137,68,36,56,72,139,214,139,132,36,176,0,0,0,137,68,36,48,72,139,132,36,168,0,0,0,72,137,68,36,
      40,139,132,36,160,0,0,0,137,68,36,32,232,39,0,0,0,128,124,36,104,0,116,12,72,139,76,36,80,131,161,168,3,0,0,253,76,141,92,36,112,73,139,91,16,73,139,115,24,73,139,227,95,195,204,204,64,85,65,84,65,85,65,86,65,87,72,131,236,96,72,141,108,36,80,
      72,137,93,64,72,137,117,72,72,137,125,80,72,139,5,238,240,0,0,72,51,197,72,137,69,8,72,99,93,96,77,139,249,72,137,85,0,69,139,232,72,139,249,133,219,126,20,72,139,211,73,139,201,232,163,34,0,0,59,195,141,88,1,124,2,139,216,68,139,117,120,69,
      133,246,117,7,72,139,7,68,139,112,12,247,157,128,0,0,0,68,139,203,77,139,199,65,139,206,27,210,131,100,36,40,0,72,131,100,36,32,0,131,226,8,255,194,232,168,228,255,255,76,99,224,133,192,15,132,54,2,0,0,73,139,196,73,184,240,255,255,255,255,255,
      255,15,72,3,192,72,141,72,16,72,59,193,72,27,210,72,35,209,116,83,72,129,250,0,4,0,0,119,46,72,141,66,15,72,59,194,119,3,73,139,192,72,131,224,240,232,200,33,0,0,72,43,224,72,141,116,36,80,72,133,246,15,132,206,1,0,0,199,6,204,204,0,0,235,22,
      72,139,202,232,79,232,255,255,72,139,240,72,133,192,116,14,199,0,221,221,0,0,72,131,198,16,235,2,51,246,72,133,246,15,132,159,1,0,0,68,137,100,36,40,68,139,203,77,139,199,72,137,116,36,32,186,1,0,0,0,65,139,206,232,3,228,255,255,133,192,15,132,
      122,1,0,0,72,131,100,36,64,0,69,139,204,72,131,100,36,56,0,76,139,198,72,131,100,36,48,0,65,139,213,76,139,125,0,131,100,36,40,0,73,139,207,72,131,100,36,32,0,232,117,178,255,255,72,99,248,133,192,15,132,61,1,0,0,186,0,4,0,0,68,133,234,116,82,
      139,69,112,133,192,15,132,42,1,0,0,59,248,15,143,32,1,0,0,72,131,100,36,64,0,69,139,204,72,131,100,36,56,0,76,139,198,72,131,100,36,48,0,65,139,213,137,68,36,40,73,139,207,72,139,69,104,72,137,68,36,32,232,29,178,255,255,139,248,133,192,15,133,
      232,0,0,0,233,225,0,0,0,72,139,207,72,3,201,72,141,65,16,72,59,200,72,27,201,72,35,200,116,83,72,59,202,119,53,72,141,65,15,72,59,193,119,10,72,184,240,255,255,255,255,255,255,15,72,131,224,240,232,148,32,0,0,72,43,224,72,141,92,36,80,72,133,
      219,15,132,154,0,0,0,199,3,204,204,0,0,235,19,232,30,231,255,255,72,139,216,72,133,192,116,14,199,0,221,221,0,0,72,131,195,16,235,2,51,219,72,133,219,116,114,72,131,100,36,64,0,69,139,204,72,131,100,36,56,0,76,139,198,72,131,100,36,48,0,65,139,
      213,137,124,36,40,73,139,207,72,137,92,36,32,232,115,177,255,255,133,192,116,49,72,131,100,36,56,0,51,210,72,33,84,36,48,68,139,207,139,69,112,76,139,195,65,139,206,133,192,117,101,33,84,36,40,72,33,84,36,32,232,244,226,255,255,139,248,133,192,
      117,96,72,141,75,240,129,57,221,221,0,0,117,5,232,197,200,255,255,51,255,72,133,246,116,17,72,141,78,240,129,57,221,221,0,0,117,5,232,173,200,255,255,139,199,72,139,77,8,72,51,205,232,211,173,255,255,72,139,93,64,72,139,117,72,72,139,125,80,
      72,141,101,16,65,95,65,94,65,93,65,92,93,195,137,68,36,40,72,139,69,104,72,137,68,36,32,235,149,72,141,75,240,129,57,221,221,0,0,117,167,232,101,200,255,255,235,160,204,204,204,72,131,236,40,232,19,190,255,255,51,201,132,192,15,148,193,139,193,
      72,131,196,40,195,204,137,76,36,8,72,131,236,56,72,99,209,131,250,254,117,13,232,131,211,255,255,199,0,9,0,0,0,235,108,133,201,120,88,59,21,117,4,1,0,115,80,72,139,202,76,141,5,105,0,1,0,131,225,63,72,139,194,72,193,248,6,72,141,12,201,73,139,
      4,192,246,68,200,56,1,116,45,72,141,68,36,64,137,84,36,80,137,84,36,88,76,141,76,36,80,72,141,84,36,88,72,137,68,36,32,76,141,68,36,32,72,141,76,36,72,232,29,0,0,0,235,19,232,26,211,255,255,199,0,9,0,0,0,232,187,200,255,255,131,200,255,72,131,
      196,56,195,204,204,204,72,137,92,36,8,76,137,76,36,32,87,72,131,236,32,73,139,249,73,139,216,139,10,232,168,249,255,255,144,72,139,3,72,99,8,72,139,209,72,139,193,72,193,248,6,76,141,5,212,255,0,0,131,226,63,72,141,20,210,73,139,4,192,246,68,
      208,56,1,116,36,232,165,246,255,255,72,139,200,255,21,20,199,0,0,51,219,133,192,117,30,232,197,210,255,255,72,139,216,255,21,128,199,0,0,137,3,232,149,210,255,255,199,0,9,0,0,0,131,203,255,139,15,232,109,249,255,255,139,195,72,139,92,36,48,72,
      131,196,32,95,195,72,137,92,36,16,72,137,116,36,24,137,76,36,8,87,65,84,65,85,65,86,65,87,72,131,236,32,69,139,240,76,139,250,72,99,217,131,251,254,117,24,232,106,210,255,255,131,32,0,232,66,210,255,255,199,0,9,0,0,0,233,143,0,0,0,133,201,120,
      115,59,29,49,3,1,0,115,107,72,139,195,72,139,243,72,193,254,6,76,141,45,30,255,0,0,131,224,63,76,141,36,192,73,139,68,245,0,66,246,68,224,56,1,116,70,139,203,232,191,248,255,255,131,207,255,73,139,68,245,0,66,246,68,224,56,1,117,21,232,234,209,
      255,255,199,0,9,0,0,0,232,255,209,255,255,131,32,0,235,15,69,139,198,73,139,215,139,203,232,65,0,0,0,139,248,139,203,232,172,248,255,255,139,199,235,27,232,219,209,255,255,131,32,0,232,179,209,255,255,199,0,9,0,0,0,232,84,199,255,255,131,200,
      255,72,139,92,36,88,72,139,116,36,96,72,131,196,32,65,95,65,94,65,93,65,92,95,195,204,72,137,92,36,32,85,86,87,65,84,65,85,65,86,65,87,72,139,236,72,131,236,96,51,219,69,139,240,76,99,225,72,139,250,69,133,192,15,132,158,2,0,0,72,133,210,117,
      31,232,119,209,255,255,137,24,232,80,209,255,255,199,0,22,0,0,0,232,241,198,255,255,131,200,255,233,124,2,0,0,73,139,196,72,141,13,55,254,0,0,131,224,63,77,139,236,73,193,253,6,76,141,60,192,74,139,12,233,66,15,190,116,249,57,141,70,255,60,1,
      119,9,65,139,198,247,208,168,1,116,175,66,246,68,249,56,32,116,14,51,210,65,139,204,68,141,66,2,232,53,29,0,0,65,139,204,72,137,93,224,232,217,11,0,0,133,192,15,132,11,1,0,0,72,141,5,222,253,0,0,74,139,4,232,66,56,92,248,56,15,141,245,0,0,0,
      232,114,178,255,255,72,139,136,144,0,0,0,72,57,153,56,1,0,0,117,22,72,141,5,179,253,0,0,74,139,4,232,66,56,92,248,57,15,132,202,0,0,0,72,141,5,157,253,0,0,74,139,12,232,72,141,85,240,74,139,76,249,40,255,21,34,197,0,0,133,192,15,132,168,0,0,
      0,64,132,246,15,132,129,0,0,0,64,254,206,64,128,254,1,15,135,46,1,0,0,78,141,36,55,72,137,93,208,76,139,247,73,59,252,15,131,16,1,0,0,139,117,212,65,15,183,6,15,183,200,102,137,69,240,232,217,30,0,0,15,183,77,240,102,59,193,117,54,131,198,2,
      137,117,212,102,131,249,10,117,27,185,13,0,0,0,232,186,30,0,0,185,13,0,0,0,102,59,193,117,22,255,198,137,117,212,255,195,73,131,198,2,77,59,244,15,131,192,0,0,0,235,177,255,21,224,196,0,0,137,69,208,233,176,0,0,0,69,139,206,72,141,77,208,76,
      139,199,65,139,212,232,58,1,0,0,242,15,16,0,139,88,8,233,151,0,0,0,72,141,5,211,252,0,0,74,139,12,232,66,56,92,249,56,125,77,139,206,64,132,246,116,50,131,233,1,116,25,131,249,1,117,121,69,139,206,72,141,77,208,76,139,199,65,139,212,232,233,
      6,0,0,235,189,69,139,206,72,141,77,208,76,139,199,65,139,212,232,241,7,0,0,235,169,69,139,206,72,141,77,208,76,139,199,65,139,212,232,189,5,0,0,235,149,74,139,76,249,40,76,141,77,212,51,192,69,139,198,72,33,68,36,32,72,139,215,72,137,69,208,
      137,69,216,255,21,224,197,0,0,133,192,117,9,255,21,46,196,0,0,137,69,208,139,93,216,242,15,16,69,208,242,15,17,69,224,72,139,69,224,72,193,232,32,133,192,117,100,139,69,224,133,192,116,45,131,248,5,117,27,232,29,207,255,255,199,0,9,0,0,0,232,
      50,207,255,255,199,0,5,0,0,0,233,194,253,255,255,139,77,224,232,63,207,255,255,233,181,253,255,255,72,141,5,247,251,0,0,74,139,4,232,66,246,68,248,56,64,116,5,128,63,26,116,31,232,221,206,255,255,199,0,28,0,0,0,232,242,206,255,255,131,32,0,233,
      133,253,255,255,139,69,228,43,195,235,2,51,192,72,139,156,36,184,0,0,0,72,131,196,96,65,95,65,94,65,93,65,92,95,94,93,195,204,72,137,92,36,8,85,86,87,65,84,65,85,65,86,65,87,72,141,108,36,217,72,129,236,0,1,0,0,72,139,5,209,232,0,0,72,51,196,
      72,137,69,23,72,99,242,77,139,248,72,139,198,72,137,77,247,72,137,69,239,72,141,13,138,24,255,255,131,224,63,69,139,233,77,3,232,76,137,69,223,76,139,230,76,137,109,175,73,193,252,6,76,141,52,192,74,139,132,225,224,226,1,0,74,139,68,240,40,72,
      137,69,183,255,21,215,194,0,0,51,210,72,141,76,36,80,137,69,167,232,208,122,255,255,72,139,76,36,88,69,51,219,68,137,93,151,65,139,219,137,93,155,73,139,255,139,81,12,65,139,203,137,76,36,64,137,85,171,77,59,253,15,131,226,3,0,0,72,139,198,73,
      139,247,72,193,248,6,72,137,69,231,138,15,65,191,1,0,0,0,136,76,36,68,68,137,92,36,72,129,250,233,253,0,0,15,133,112,1,0,0,76,141,61,235,23,255,255,65,139,211,77,139,140,199,224,226,1,0,73,139,243,75,141,4,241,68,56,92,48,62,116,11,255,194,72,
      255,198,72,131,254,5,124,238,72,133,246,15,142,224,0,0,0,75,139,132,231,224,226,1,0,76,139,69,175,76,43,199,66,15,182,76,240,62,70,15,190,188,57,208,216,1,0,65,255,199,69,139,239,68,43,234,77,99,213,77,59,208,15,143,120,2,0,0,72,141,69,255,73,
      139,211,76,43,200,79,141,4,241,72,141,77,255,72,3,202,72,255,194,66,138,68,1,62,136,1,72,59,214,124,234,69,133,237,126,21,72,141,77,255,77,139,194,72,3,206,72,139,215,232,32,229,255,255,69,51,219,73,139,211,76,141,5,67,23,255,255,75,139,140,
      224,224,226,1,0,72,3,202,72,255,194,70,136,92,241,62,72,59,214,124,232,72,141,69,255,76,137,93,191,72,137,69,199,76,141,77,191,65,139,195,72,141,85,199,65,131,255,4,72,141,76,36,72,15,148,192,255,192,68,139,192,68,139,248,232,247,25,0,0,72,131,
      248,255,15,132,215,0,0,0,65,141,69,255,76,139,109,175,72,99,240,72,3,247,233,230,0,0,0,15,182,7,73,139,213,72,43,215,74,15,190,180,56,208,216,1,0,141,78,1,72,99,193,72,59,194,15,143,228,1,0,0,131,249,4,76,137,93,207,65,139,195,72,137,125,215,
      15,148,192,76,141,77,207,255,192,72,141,85,215,68,139,192,72,141,76,36,72,139,216,232,143,25,0,0,72,131,248,255,116,115,72,3,247,68,139,251,233,138,0,0,0,72,141,5,123,22,255,255,74,139,148,224,224,226,1,0,66,138,76,242,61,246,193,4,116,27,66,
      138,68,242,62,128,225,251,136,69,7,138,7,66,136,76,242,61,72,141,85,7,136,69,8,235,31,232,113,222,255,255,15,182,15,51,210,102,57,20,72,125,45,72,255,198,73,59,245,15,131,178,1,0,0,72,139,215,65,184,2,0,0,0,72,141,76,36,72,232,223,21,0,0,131,
      248,255,117,34,128,125,143,0,233,139,1,0,0,77,139,199,72,141,76,36,72,72,139,215,232,193,21,0,0,131,248,255,15,132,175,1,0,0,139,77,167,72,141,69,15,51,219,76,141,68,36,72,72,137,92,36,56,72,141,126,1,72,137,92,36,48,69,139,207,199,68,36,40,
      5,0,0,0,51,210,72,137,68,36,32,232,105,218,255,255,139,240,133,192,15,132,210,1,0,0,72,139,77,183,76,141,76,36,76,68,139,192,72,137,92,36,32,72,141,85,15,255,21,8,194,0,0,69,51,219,133,192,15,132,163,1,0,0,68,139,124,36,64,139,223,43,93,223,
      65,3,223,137,93,155,57,116,36,76,15,130,241,0,0,0,128,124,36,68,10,117,73,72,139,77,183,65,141,67,13,76,141,76,36,76,102,137,68,36,68,69,141,67,1,76,137,92,36,32,72,141,84,36,68,255,21,182,193,0,0,69,51,219,133,192,15,132,241,0,0,0,131,124,36,
      76,1,15,130,174,0,0,0,65,255,199,255,195,68,137,124,36,64,137,93,155,72,139,247,73,59,253,15,131,224,0,0,0,72,139,69,231,139,85,171,233,4,253,255,255,65,139,211,77,133,192,126,45,72,43,254,72,141,29,1,21,255,255,138,4,55,255,194,74,139,140,227,
      224,226,1,0,72,3,206,72,255,198,66,136,68,241,62,72,99,194,73,59,192,124,224,139,93,155,65,3,216,235,76,69,139,203,72,133,210,126,66,76,139,109,239,77,139,195,77,139,213,65,131,229,63,73,193,250,6,78,141,28,237,0,0,0,0,77,3,221,65,138,4,56,65,
      255,193,75,139,140,215,224,226,1,0,73,3,200,73,255,192,66,136,68,217,62,73,99,193,72,59,194,124,222,69,51,219,3,218,137,93,155,68,56,93,143,139,76,36,64,235,73,138,7,76,141,5,119,20,255,255,75,139,140,224,224,226,1,0,255,195,137,93,155,66,136,
      68,241,62,75,139,132,224,224,226,1,0,66,128,76,240,61,4,56,85,143,235,204,255,21,12,191,0,0,137,69,151,139,76,36,64,128,125,143,0,235,8,139,76,36,64,68,56,93,143,116,12,72,139,68,36,80,131,160,168,3,0,0,253,72,139,69,247,242,15,16,69,151,242,
      15,17,0,137,72,8,72,139,77,23,72,51,204,232,205,163,255,255,72,139,156,36,64,1,0,0,72,129,196,0,1,0,0,65,95,65,94,65,93,65,92,95,94,93,195,255,21,172,190,0,0,137,69,151,139,76,36,64,56,93,143,235,169,72,137,92,36,8,72,137,108,36,24,86,87,65,
      86,184,80,20,0,0,232,120,21,0,0,72,43,224,72,139,5,230,227,0,0,72,51,196,72,137,132,36,64,20,0,0,76,99,210,72,139,249,73,139,194,65,139,233,72,193,248,6,72,141,13,124,246,0,0,65,131,226,63,73,3,232,73,139,240,72,139,4,193,75,141,20,210,76,139,
      116,208,40,51,192,72,137,7,137,71,8,76,59,197,115,111,72,141,92,36,64,72,59,245,115,36,138,6,72,255,198,60,10,117,9,255,71,8,198,3,13,72,255,195,136,3,72,255,195,72,141,132,36,63,20,0,0,72,59,216,114,215,72,131,100,36,32,0,72,141,68,36,64,43,
      216,76,141,76,36,48,68,139,195,72,141,84,36,64,73,139,206,255,21,143,191,0,0,133,192,116,18,139,68,36,48,1,71,4,59,195,114,15,72,59,245,114,155,235,8,255,21,203,189,0,0,137,7,72,139,199,72,139,140,36,64,20,0,0,72,51,204,232,182,162,255,255,76,
      141,156,36,80,20,0,0,73,139,91,32,73,139,107,48,73,139,227,65,94,95,94,195,204,204,72,137,92,36,8,72,137,108,36,24,86,87,65,86,184,80,20,0,0,232,116,20,0,0,72,43,224,72,139,5,226,226,0,0,72,51,196,72,137,132,36,64,20,0,0,76,99,210,72,139,249,
      73,139,194,65,139,233,72,193,248,6,72,141,13,120,245,0,0,65,131,226,63,73,3,232,73,139,240,72,139,4,193,75,141,20,210,76,139,116,208,40,51,192,72,137,7,137,71,8,76,59,197,15,131,130,0,0,0,72,141,92,36,64,72,59,245,115,49,15,183,6,72,131,198,
      2,102,131,248,10,117,16,131,71,8,2,185,13,0,0,0,102,137,11,72,131,195,2,102,137,3,72,131,195,2,72,141,132,36,62,20,0,0,72,59,216,114,202,72,131,100,36,32,0,72,141,68,36,64,72,43,216,76,141,76,36,48,72,209,251,72,141,84,36,64,3,219,73,139,206,
      68,139,195,255,21,116,190,0,0,133,192,116,18,139,68,36,48,1,71,4,59,195,114,15,72,59,245,114,136,235,8,255,21,176,188,0,0,137,7,72,139,199,72,139,140,36,64,20,0,0,72,51,204,232,155,161,255,255,76,141,156,36,80,20,0,0,73,139,91,32,73,139,107,
      48,73,139,227,65,94,95,94,195,204,204,204,72,137,92,36,8,72,137,108,36,24,86,87,65,84,65,86,65,87,184,112,20,0,0,232,84,19,0,0,72,43,224,72,139,5,194,225,0,0,72,51,196,72,137,132,36,96,20,0,0,76,99,210,72,139,217,73,139,194,69,139,241,72,193,
      248,6,72,141,13,88,244,0,0,65,131,226,63,77,3,240,77,139,248,73,139,248,72,139,4,193,75,141,20,210,76,139,100,208,40,51,192,72,137,3,77,59,198,137,67,8,15,131,206,0,0,0,72,141,68,36,80,73,59,254,115,45,15,183,15,72,131,199,2,102,131,249,10,117,
      12,186,13,0,0,0,102,137,16,72,131,192,2,102,137,8,72,131,192,2,72,141,140,36,248,6,0,0,72,59,193,114,206,72,131,100,36,56,0,72,141,76,36,80,72,131,100,36,48,0,76,141,68,36,80,72,43,193,199,68,36,40,85,13,0,0,72,141,140,36,0,7,0,0,72,209,248,
      72,137,76,36,32,68,139,200,185,233,253,0,0,51,210,232,122,213,255,255,139,232,133,192,116,73,51,246,133,192,116,51,72,131,100,36,32,0,72,141,148,36,0,7,0,0,139,206,76,141,76,36,64,68,139,197,72,3,209,73,139,204,68,43,198,255,21,11,189,0,0,133,
      192,116,24,3,116,36,64,59,245,114,205,139,199,65,43,199,137,67,4,73,59,254,233,52,255,255,255,255,21,65,187,0,0,137,3,72,139,195,72,139,140,36,96,20,0,0,72,51,204,232,44,160,255,255,76,141,156,36,112,20,0,0,73,139,91,48,73,139,107,64,73,139,
      227,65,95,65,94,65,92,95,94,195,72,131,236,40,72,133,201,117,21,232,30,198,255,255,199,0,22,0,0,0,232,191,187,255,255,131,200,255,235,3,139,65,24,72,131,196,40,195,204,204,72,137,92,36,8,87,72,131,236,48,131,100,36,32,0,185,8,0,0,0,232,215,166,
      255,255,144,187,3,0,0,0,137,92,36,36,59,29,111,247,0,0,116,109,72,99,251,72,139,5,107,247,0,0,72,139,12,248,72,133,201,117,2,235,84,139,65,20,193,232,13,168,1,116,25,72,139,13,79,247,0,0,72,139,12,249,232,126,20,0,0,131,248,255,116,4,255,68,
      36,32,72,139,5,54,247,0,0,72,139,12,248,72,131,193,48,255,21,184,185,0,0,72,139,13,33,247,0,0,72,139,12,249,232,44,186,255,255,72,139,5,17,247,0,0,72,131,36,248,0,255,195,235,135,185,8,0,0,0,232,106,166,255,255,139,68,36,32,72,139,92,36,64,72,
      131,196,48,95,195,204,204,204,64,83,72,131,236,32,139,65,20,72,139,217,193,232,13,168,1,116,39,139,65,20,193,232,6,168,1,116,29,72,139,73,8,232,218,185,255,255,240,129,99,20,191,254,255,255,51,192,72,137,67,8,72,137,3,137,67,16,72,131,196,32,
      91,195,72,131,236,40,131,249,254,117,13,232,2,197,255,255,199,0,9,0,0,0,235,66,133,201,120,46,59,13,244,245,0,0,115,38,72,99,201,72,141,21,232,241,0,0,72,139,193,131,225,63,72,193,248,6,72,141,12,201,72,139,4,194,15,182,68,200,56,131,224,64,
      235,18,232,195,196,255,255,199,0,9,0,0,0,232,100,186,255,255,51,192,72,131,196,40,195,204,139,5,38,252,0,0,195,204,204,204,204,204,204,204,204,204,204,204,204,204,65,84,65,85,65,86,72,129,236,80,4,0,0,72,139,5,212,222,0,0,72,51,196,72,137,132,
      36,16,4,0,0,77,139,225,77,139,240,76,139,233,72,133,201,117,26,72,133,210,116,21,232,101,196,255,255,199,0,22,0,0,0,232,6,186,255,255,233,56,3,0,0,77,133,246,116,230,77,133,228,116,225,72,131,250,2,15,130,36,3,0,0,72,137,156,36,72,4,0,0,72,137,
      172,36,64,4,0,0,72,137,180,36,56,4,0,0,72,137,188,36,48,4,0,0,76,137,188,36,40,4,0,0,76,141,122,255,77,15,175,254,76,3,249,51,201,72,137,76,36,32,102,102,102,15,31,132,0,0,0,0,0,51,210,73,139,199,73,43,197,73,247,246,72,141,88,1,72,131,251,8,
      15,135,139,0,0,0,77,59,253,118,101,75,141,52,46,73,139,221,72,139,254,73,59,247,119,32,15,31,0,72,139,211,72,139,207,73,139,196,255,21,241,29,1,0,133,192,72,15,79,223,73,3,254,73,59,255,118,227,77,139,198,73,139,215,73,59,223,116,30,73,43,223,
      15,31,68,0,0,15,182,2,15,182,12,19,136,4,19,136,10,72,141,82,1,73,131,232,1,117,234,77,43,254,77,59,253,119,164,72,139,76,36,32,72,131,233,1,72,137,76,36,32,15,136,37,2,0,0,76,139,108,204,48,76,139,188,204,32,2,0,0,233,92,255,255,255,72,209,
      235,73,139,205,73,15,175,222,73,139,196,74,141,52,43,72,139,214,255,21,114,29,1,0,133,192,126,41,77,139,206,76,139,198,76,59,238,116,30,15,31,0,65,15,182,0,73,139,208,72,43,211,15,182,10,136,2,65,136,8,73,255,192,73,131,233,1,117,229,73,139,
      215,73,139,205,73,139,196,255,21,54,29,1,0,133,192,126,42,77,139,198,73,139,215,77,59,239,116,31,77,139,205,77,43,207,144,15,182,2,65,15,182,12,17,65,136,4,17,136,10,72,141,82,1,73,131,232,1,117,232,73,139,215,72,139,206,73,139,196,255,21,249,
      28,1,0,133,192,126,45,77,139,198,73,139,215,73,59,247,116,34,76,139,206,77,43,207,15,31,64,0,15,182,2,65,15,182,12,17,65,136,4,17,136,10,72,141,82,1,73,131,232,1,117,232,73,139,221,73,139,255,102,144,72,59,243,118,29,73,3,222,72,59,222,115,21,
      72,139,214,72,139,203,73,139,196,255,21,164,28,1,0,133,192,126,229,235,30,73,3,222,73,59,223,119,22,72,139,214,72,139,203,73,139,196,255,21,135,28,1,0,133,192,126,229,15,31,0,72,139,239,73,43,254,72,59,254,118,19,72,139,214,72,139,207,73,139,
      196,255,21,102,28,1,0,133,192,127,226,72,59,251,114,56,77,139,198,72,139,215,116,30,76,139,203,76,43,207,15,182,2,65,15,182,12,17,65,136,4,17,136,10,72,141,82,1,73,131,232,1,117,232,72,59,247,72,139,195,72,15,69,198,72,139,240,233,101,255,255,
      255,72,59,245,115,32,73,43,238,72,59,238,118,24,72,139,214,72,139,205,73,139,196,255,21,9,28,1,0,133,192,116,229,235,30,15,31,0,73,43,238,73,59,237,118,19,72,139,214,72,139,205,73,139,196,255,21,233,27,1,0,133,192,116,229,73,139,207,72,139,197,
      72,43,203,73,43,197,72,59,193,72,139,76,36,32,124,43,76,59,237,115,21,76,137,108,204,48,72,137,172,204,32,2,0,0,72,255,193,72,137,76,36,32,73,59,223,15,131,255,253,255,255,76,139,235,233,116,253,255,255,73,59,223,115,21,72,137,92,204,48,76,137,
      188,204,32,2,0,0,72,255,193,72,137,76,36,32,76,59,237,15,131,212,253,255,255,76,139,253,233,73,253,255,255,72,139,188,36,48,4,0,0,72,139,180,36,56,4,0,0,72,139,172,36,64,4,0,0,72,139,156,36,72,4,0,0,76,139,188,36,40,4,0,0,72,139,140,36,16,4,
      0,0,72,51,204,232,241,154,255,255,72,129,196,80,4,0,0,65,94,65,93,65,92,195,204,204,204,72,137,92,36,8,87,72,131,236,32,69,51,210,73,139,216,76,139,218,77,133,201,117,44,72,133,201,117,44,72,133,210,116,20,232,213,192,255,255,187,22,0,0,0,137,
      24,232,117,182,255,255,68,139,211,72,139,92,36,48,65,139,194,72,131,196,32,95,195,72,133,201,116,217,77,133,219,116,212,77,133,201,117,5,68,136,17,235,222,72,133,219,117,5,68,136,17,235,192,72,43,217,72,139,209,77,139,195,73,139,249,73,131,249,
      255,117,20,138,4,19,136,2,72,255,194,132,192,116,40,73,131,232,1,117,238,235,32,138,4,19,136,2,72,255,194,132,192,116,12,73,131,232,1,116,6,72,131,239,1,117,232,72,133,255,117,3,68,136,18,77,133,192,117,137,73,131,249,255,117,14,70,136,84,25,
      255,69,141,80,80,233,117,255,255,255,68,136,17,232,51,192,255,255,187,34,0,0,0,233,89,255,255,255,204,204,204,204,204,204,204,204,204,204,204,204,204,72,137,92,36,8,72,137,116,36,16,87,76,139,210,72,141,53,43,10,255,255,65,131,226,15,72,139,
      250,73,43,250,72,139,218,76,139,193,15,87,219,73,141,66,255,243,15,111,15,72,131,248,14,119,115,139,132,134,204,248,0,0,72,3,198,255,224,102,15,115,217,1,235,96,102,15,115,217,2,235,89,102,15,115,217,3,235,82,102,15,115,217,4,235,75,102,15,115,
      217,5,235,68,102,15,115,217,6,235,61,102,15,115,217,7,235,54,102,15,115,217,8,235,47,102,15,115,217,9,235,40,102,15,115,217,10,235,33,102,15,115,217,11,235,26,102,15,115,217,12,235,19,102,15,115,217,13,235,12,102,15,115,217,14,235,5,102,15,115,
      217,15,15,87,192,65,185,15,0,0,0,102,15,116,193,102,15,215,192,133,192,15,132,51,1,0,0,15,188,208,77,133,210,117,6,69,141,89,242,235,20,69,51,219,139,194,185,16,0,0,0,73,43,202,72,59,193,65,15,146,195,65,139,193,43,194,65,59,193,15,135,207,0,
      0,0,139,140,134,8,249,0,0,72,3,206,255,225,102,15,115,249,1,102,15,115,217,1,233,180,0,0,0,102,15,115,249,2,102,15,115,217,2,233,165,0,0,0,102,15,115,249,3,102,15,115,217,3,233,150,0,0,0,102,15,115,249,4,102,15,115,217,4,233,135,0,0,0,102,15,
      115,249,5,102,15,115,217,5,235,123,102,15,115,249,6,102,15,115,217,6,235,111,102,15,115,249,7,102,15,115,217,7,235,99,102,15,115,249,8,102,15,115,217,8,235,87,102,15,115,249,9,102,15,115,217,9,235,75,102,15,115,249,10,102,15,115,217,10,235,63,
      102,15,115,249,11,102,15,115,217,11,235,51,102,15,115,249,12,102,15,115,217,12,235,39,102,15,115,249,13,102,15,115,217,13,235,27,102,15,115,249,14,102,15,115,217,14,235,15,102,15,115,249,15,102,15,115,217,15,235,3,15,87,201,69,133,219,15,133,
      230,0,0,0,243,15,111,87,16,102,15,111,194,102,15,116,195,102,15,215,192,133,192,117,53,72,139,211,73,139,200,72,139,92,36,16,72,139,116,36,24,95,233,211,1,0,0,77,133,210,117,208,68,56,87,1,15,132,172,0,0,0,72,139,92,36,16,72,139,116,36,24,95,
      233,180,1,0,0,15,188,200,139,193,73,43,194,72,131,192,16,72,131,248,16,119,185,68,43,201,65,131,249,15,119,121,66,139,140,142,72,249,0,0,72,3,206,255,225,102,15,115,250,1,235,101,102,15,115,250,2,235,94,102,15,115,250,3,235,87,102,15,115,250,
      4,235,80,102,15,115,250,5,235,73,102,15,115,250,6,235,66,102,15,115,250,7,235,59,102,15,115,250,8,235,52,102,15,115,250,9,235,45,102,15,115,250,10,235,38,102,15,115,250,11,235,31,102,15,115,250,12,235,24,102,15,115,250,13,235,17,102,15,115,250,
      14,235,10,102,15,115,250,15,235,3,15,87,210,102,15,235,209,102,15,111,202,65,15,182,0,132,192,116,52,15,31,132,0,0,0,0,0,15,190,192,102,15,110,192,102,15,96,192,102,15,96,192,102,15,112,192,0,102,15,116,193,102,15,215,192,133,192,117,26,65,15,
      182,64,1,73,255,192,132,192,117,212,51,192,72,139,92,36,16,72,139,116,36,24,95,195,72,139,92,36,16,73,139,192,72,139,116,36,24,95,195,15,31,0,2,246,0,0,9,246,0,0,16,246,0,0,23,246,0,0,30,246,0,0,37,246,0,0,44,246,0,0,51,246,0,0,58,246,0,0,65,
      246,0,0,72,246,0,0,79,246,0,0,86,246,0,0,93,246,0,0,100,246,0,0,190,246,0,0,205,246,0,0,220,246,0,0,235,246,0,0,250,246,0,0,6,247,0,0,18,247,0,0,30,247,0,0,42,247,0,0,54,247,0,0,66,247,0,0,78,247,0,0,90,247,0,0,102,247,0,0,114,247,0,0,126,247,
      0,0,252,247,0,0,3,248,0,0,10,248,0,0,17,248,0,0,24,248,0,0,31,248,0,0,38,248,0,0,45,248,0,0,52,248,0,0,59,248,0,0,66,248,0,0,73,248,0,0,80,248,0,0,87,248,0,0,94,248,0,0,101,248,0,0,72,131,236,88,72,139,5,149,214,0,0,72,51,196,72,137,68,36,64,
      51,192,76,139,202,72,131,248,32,76,139,193,115,119,198,68,4,32,0,72,255,192,72,131,248,32,124,240,138,2,235,31,15,182,208,72,193,234,3,15,182,192,131,224,7,15,182,76,20,32,15,171,193,73,255,193,136,76,20,32,65,138,1,132,192,117,221,235,31,65,
      15,182,193,186,1,0,0,0,65,15,182,201,131,225,7,72,193,232,3,211,226,132,84,4,32,117,31,73,255,192,69,138,8,69,132,201,117,217,51,192,72,139,76,36,64,72,51,204,232,170,149,255,255,72,131,196,88,195,73,139,192,235,233,232,31,216,255,255,204,204,
      204,69,51,192,233,0,0,0,0,72,137,92,36,8,87,72,131,236,64,72,139,218,72,139,249,72,133,201,117,20,232,146,187,255,255,199,0,22,0,0,0,232,51,177,255,255,51,192,235,96,72,133,219,116,231,72,59,251,115,242,73,139,208,72,141,76,36,32,232,24,104,
      255,255,72,139,76,36,48,72,141,83,255,131,121,8,0,116,36,72,255,202,72,59,250,119,10,15,182,2,246,68,8,25,4,117,238,72,139,203,72,43,202,72,139,211,131,225,1,72,43,209,72,255,202,128,124,36,56,0,116,12,72,139,76,36,32,131,161,168,3,0,0,253,72,
      139,194,72,139,92,36,80,72,131,196,64,95,195,72,131,236,40,72,133,201,117,25,232,10,187,255,255,199,0,22,0,0,0,232,171,176,255,255,72,131,200,255,72,131,196,40,195,76,139,193,51,210,72,139,13,230,231,0,0,72,131,196,40,72,255,37,59,176,0,0,204,
      204,204,72,137,92,36,8,87,72,131,236,32,72,139,218,72,139,249,72,133,201,117,10,72,139,202,232,59,205,255,255,235,31,72,133,219,117,7,232,99,175,255,255,235,17,72,131,251,224,118,45,232,166,186,255,255,199,0,12,0,0,0,51,192,72,139,92,36,48,72,
      131,196,32,95,195,232,222,245,255,255,133,192,116,223,72,139,203,232,22,85,255,255,133,192,116,211,72,139,13,115,231,0,0,76,139,203,76,139,199,51,210,255,21,189,175,0,0,72,133,192,116,209,235,196,204,204,72,137,92,36,8,72,137,108,36,16,72,137,
      116,36,24,87,72,131,236,80,73,99,217,73,139,248,139,242,72,139,233,69,133,201,126,20,72,139,211,73,139,200,232,225,104,255,255,59,195,141,88,1,124,2,139,216,72,131,100,36,64,0,68,139,203,72,131,100,36,56,0,76,139,199,72,131,100,36,48,0,139,214,
      139,132,36,136,0,0,0,72,139,205,137,68,36,40,72,139,132,36,128,0,0,0,72,137,68,36,32,232,6,151,255,255,72,139,92,36,96,72,139,108,36,104,72,139,116,36,112,72,131,196,80,95,195,204,72,133,201,15,132,0,1,0,0,83,72,131,236,32,72,139,217,72,139,
      73,24,72,59,13,40,220,0,0,116,5,232,101,174,255,255,72,139,75,32,72,59,13,30,220,0,0,116,5,232,83,174,255,255,72,139,75,40,72,59,13,20,220,0,0,116,5,232,65,174,255,255,72,139,75,48,72,59,13,10,220,0,0,116,5,232,47,174,255,255,72,139,75,56,72,
      59,13,0,220,0,0,116,5,232,29,174,255,255,72,139,75,64,72,59,13,246,219,0,0,116,5,232,11,174,255,255,72,139,75,72,72,59,13,236,219,0,0,116,5,232,249,173,255,255,72,139,75,104,72,59,13,250,219,0,0,116,5,232,231,173,255,255,72,139,75,112,72,59,
      13,240,219,0,0,116,5,232,213,173,255,255,72,139,75,120,72,59,13,230,219,0,0,116,5,232,195,173,255,255,72,139,139,128,0,0,0,72,59,13,217,219,0,0,116,5,232,174,173,255,255,72,139,139,136,0,0,0,72,59,13,204,219,0,0,116,5,232,153,173,255,255,72,
      139,139,144,0,0,0,72,59,13,191,219,0,0,116,5,232,132,173,255,255,72,131,196,32,91,195,204,204,72,133,201,116,102,83,72,131,236,32,72,139,217,72,139,9,72,59,13,9,219,0,0,116,5,232,94,173,255,255,72,139,75,8,72,59,13,255,218,0,0,116,5,232,76,173,
      255,255,72,139,75,16,72,59,13,245,218,0,0,116,5,232,58,173,255,255,72,139,75,88,72,59,13,43,219,0,0,116,5,232,40,173,255,255,72,139,75,96,72,59,13,33,219,0,0,116,5,232,22,173,255,255,72,131,196,32,91,195,72,133,201,15,132,254,0,0,0,72,137,92,
      36,8,72,137,108,36,16,86,72,131,236,32,189,7,0,0,0,72,139,217,139,213,232,225,0,0,0,72,141,75,56,139,213,232,214,0,0,0,141,117,5,139,214,72,141,75,112,232,200,0,0,0,72,141,139,208,0,0,0,139,214,232,186,0,0,0,72,141,139,48,1,0,0,141,85,251,232,
      171,0,0,0,72,139,139,64,1,0,0,232,167,172,255,255,72,139,139,72,1,0,0,232,155,172,255,255,72,139,139,80,1,0,0,232,143,172,255,255,72,141,139,96,1,0,0,139,213,232,121,0,0,0,72,141,139,152,1,0,0,139,213,232,107,0,0,0,72,141,139,208,1,0,0,139,214,
      232,93,0,0,0,72,141,139,48,2,0,0,139,214,232,79,0,0,0,72,141,139,144,2,0,0,141,85,251,232,64,0,0,0,72,139,139,160,2,0,0,232,60,172,255,255,72,139,139,168,2,0,0,232,48,172,255,255,72,139,139,176,2,0,0,232,36,172,255,255,72,139,139,184,2,0,0,232,
      24,172,255,255,72,139,92,36,48,72,139,108,36,56,72,131,196,32,94,195,72,137,92,36,8,72,137,116,36,16,87,72,131,236,32,51,255,72,141,4,209,72,139,217,72,139,242,72,185,255,255,255,255,255,255,255,31,72,35,241,72,59,216,72,15,71,247,72,133,246,
      116,20,72,139,11,232,204,171,255,255,72,255,199,72,141,91,8,72,59,254,117,236,72,139,92,36,48,72,139,116,36,56,72,131,196,32,95,195,204,204,204,204,204,204,204,204,204,204,102,102,15,31,132,0,0,0,0,0,72,43,209,73,131,248,8,114,34,246,193,7,116,
      20,102,144,138,1,58,4,17,117,44,72,255,193,73,255,200,246,193,7,117,238,77,139,200,73,193,233,3,117,31,77,133,192,116,15,138,1,58,4,17,117,12,72,255,193,73,255,200,117,241,72,51,192,195,27,192,131,216,255,195,144,73,193,233,2,116,55,72,139,1,
      72,59,4,17,117,91,72,139,65,8,72,59,68,17,8,117,76,72,139,65,16,72,59,68,17,16,117,61,72,139,65,24,72,59,68,17,24,117,46,72,131,193,32,73,255,201,117,205,73,131,224,31,77,139,200,73,193,233,3,116,155,72,139,1,72,59,4,17,117,27,72,131,193,8,73,
      255,201,117,238,73,131,224,7,235,131,72,131,193,8,72,131,193,8,72,131,193,8,72,139,12,10,72,15,200,72,15,201,72,59,193,27,192,131,216,255,195,204,72,255,37,73,171,0,0,204,69,51,201,233,0,0,0,0,72,137,92,36,8,72,137,108,36,16,72,137,116,36,24,
      87,72,131,236,80,51,237,73,139,240,72,139,250,72,139,217,72,133,210,15,132,56,1,0,0,77,133,192,15,132,47,1,0,0,64,56,42,117,17,72,133,201,15,132,40,1,0,0,102,137,41,233,32,1,0,0,73,139,209,72,141,76,36,48,232,108,98,255,255,72,139,68,36,56,129,
      120,12,233,253,0,0,117,34,76,141,13,47,237,0,0,76,139,198,72,139,215,72,139,203,232,129,2,0,0,72,139,200,131,200,255,133,201,15,72,200,235,25,72,57,168,56,1,0,0,117,42,72,133,219,116,6,15,182,7,102,137,3,185,1,0,0,0,64,56,108,36,72,116,12,72,
      139,68,36,48,131,160,168,3,0,0,253,139,193,233,178,0,0,0,15,182,15,72,141,84,36,56,232,12,5,0,0,133,192,116,82,72,139,76,36,56,68,139,73,8,65,131,249,1,126,47,65,59,241,124,42,139,73,12,139,197,72,133,219,76,139,199,186,9,0,0,0,15,149,192,137,
      68,36,40,72,137,92,36,32,232,131,195,255,255,72,139,76,36,56,133,192,117,15,72,99,65,8,72,59,240,114,62,64,56,111,1,116,56,139,73,8,235,131,139,197,65,185,1,0,0,0,72,133,219,76,139,199,15,149,192,137,68,36,40,65,141,81,8,72,139,68,36,56,72,137,
      92,36,32,139,72,12,232,59,195,255,255,133,192,15,133,75,255,255,255,232,194,180,255,255,131,201,255,199,0,42,0,0,0,233,61,255,255,255,72,137,45,49,236,0,0,51,192,72,139,92,36,96,72,139,108,36,104,72,139,116,36,112,72,131,196,80,95,195,204,204,
      76,139,218,76,139,209,77,133,192,117,3,51,192,195,65,15,183,10,77,141,82,2,65,15,183,19,77,141,91,2,141,65,191,131,248,25,68,141,73,32,141,66,191,68,15,71,201,131,248,25,141,74,32,65,139,193,15,71,202,43,193,117,11,69,133,201,116,6,73,131,232,
      1,117,196,195,204,139,5,206,235,0,0,195,204,204,204,204,204,204,204,204,204,204,204,102,102,15,31,132,0,0,0,0,0,72,131,236,16,76,137,20,36,76,137,92,36,8,77,51,219,76,141,84,36,24,76,43,208,77,15,66,211,101,76,139,28,37,16,0,0,0,77,59,211,242,
      115,23,102,65,129,226,0,240,77,141,155,0,240,255,255,65,198,3,0,77,59,211,242,117,239,76,139,20,36,76,139,92,36,8,72,131,196,16,242,195,204,204,204,51,192,56,1,116,14,72,59,194,116,9,72,255,192,128,60,8,0,117,242,195,204,204,204,233,3,0,0,0,
      204,204,204,72,137,92,36,8,72,137,116,36,16,87,72,131,236,32,72,99,217,65,139,248,139,203,72,139,242,232,133,215,255,255,72,131,248,255,117,17,232,142,179,255,255,199,0,9,0,0,0,72,131,200,255,235,83,68,139,207,76,141,68,36,72,72,139,214,72,139,
      200,255,21,130,169,0,0,133,192,117,15,255,21,72,168,0,0,139,200,232,157,179,255,255,235,211,72,139,68,36,72,72,131,248,255,116,200,72,139,211,76,141,5,74,224,0,0,131,226,63,72,139,203,72,193,249,6,72,141,20,210,73,139,12,200,128,100,209,56,253,
      72,139,92,36,48,72,139,116,36,56,72,131,196,32,95,195,204,204,204,64,83,72,131,236,48,72,139,217,72,141,76,36,32,232,17,3,0,0,72,131,248,4,119,26,139,84,36,32,185,253,255,0,0,129,250,255,255,0,0,15,71,209,72,133,219,116,3,102,137,19,72,131,196,
      48,91,195,204,204,204,72,137,92,36,16,72,137,108,36,24,87,65,84,65,85,65,86,65,87,72,131,236,32,72,139,58,69,51,237,77,139,225,73,139,232,76,139,242,76,139,249,72,133,201,15,132,238,0,0,0,72,139,217,77,133,192,15,132,161,0,0,0,68,56,47,117,8,
      65,184,1,0,0,0,235,29,68,56,111,1,117,8,65,184,2,0,0,0,235,15,138,71,2,246,216,77,27,192,73,247,216,73,131,192,3,77,139,204,72,141,76,36,80,72,139,215,232,112,2,0,0,72,139,208,72,131,248,255,116,117,72,133,192,116,103,139,76,36,80,129,249,255,
      255,0,0,118,57,72,131,253,1,118,71,129,193,0,0,255,255,65,184,0,216,0,0,139,193,137,76,36,80,193,232,10,72,255,205,102,65,11,192,102,137,3,184,255,3,0,0,102,35,200,72,131,195,2,184,0,220,0,0,102,11,200,102,137,11,72,3,250,72,131,195,2,72,131,
      237,1,15,133,95,255,255,255,73,43,223,73,137,62,72,209,251,72,139,195,235,27,73,139,253,102,68,137,43,235,233,73,137,62,232,226,177,255,255,199,0,42,0,0,0,72,131,200,255,72,139,92,36,88,72,139,108,36,96,72,131,196,32,65,95,65,94,65,93,65,92,
      95,195,73,139,221,68,56,47,117,8,65,184,1,0,0,0,235,29,68,56,111,1,117,8,65,184,2,0,0,0,235,15,138,71,2,246,216,77,27,192,73,247,216,73,131,192,3,77,139,204,72,139,215,51,201,232,142,1,0,0,72,131,248,255,116,153,72,133,192,116,131,72,131,248,
      4,117,3,72,255,195,72,3,248,72,255,195,235,173,204,204,102,137,76,36,8,72,131,236,40,232,58,3,0,0,133,192,116,31,76,141,68,36,56,186,1,0,0,0,72,141,76,36,48,232,118,3,0,0,133,192,116,7,15,183,68,36,48,235,5,184,255,255,0,0,72,131,196,40,195,
      204,72,137,92,36,16,72,137,76,36,8,87,72,131,236,32,72,139,217,72,133,201,117,30,232,16,177,255,255,199,0,22,0,0,0,232,177,166,255,255,131,200,255,72,139,92,36,56,72,131,196,32,95,195,139,65,20,193,232,12,168,1,116,7,232,208,5,0,0,235,225,232,
      17,175,255,255,144,72,139,203,232,16,0,0,0,139,248,72,139,203,232,10,175,255,255,139,199,235,200,204,204,72,137,92,36,8,87,72,131,236,32,72,139,217,72,133,201,117,21,232,177,176,255,255,199,0,22,0,0,0,232,82,166,255,255,131,200,255,235,81,139,
      65,20,131,207,255,193,232,13,168,1,116,58,232,103,172,255,255,72,139,203,139,248,232,53,235,255,255,72,139,203,232,81,234,255,255,139,200,232,134,3,0,0,133,192,121,5,131,207,255,235,19,72,139,75,40,72,133,201,116,10,232,15,165,255,255,72,131,
      99,40,0,72,139,203,232,58,5,0,0,139,199,72,139,92,36,48,72,131,196,32,95,195,204,64,83,72,131,236,64,72,99,217,72,141,76,36,32,232,221,92,255,255,141,67,1,61,0,1,0,0,119,19,72,139,68,36,40,72,139,8,15,183,4,89,37,0,128,0,0,235,2,51,192,128,124,
      36,56,0,116,12,72,139,76,36,32,131,161,168,3,0,0,253,72,131,196,64,91,195,204,72,137,92,36,16,85,86,87,65,86,65,87,72,131,236,64,72,139,5,45,202,0,0,72,51,196,72,137,68,36,48,69,51,210,76,141,29,99,231,0,0,77,133,201,72,141,61,171,39,0,0,72,
      139,194,76,139,250,77,15,69,217,72,133,210,65,141,106,1,72,15,69,250,68,139,245,77,15,69,240,72,247,216,72,27,246,72,35,241,77,133,246,117,12,72,199,192,254,255,255,255,233,78,1,0,0,102,69,57,83,6,117,104,68,15,182,15,72,255,199,69,132,201,120,
      23,72,133,246,116,3,68,137,14,69,132,201,65,15,149,194,73,139,194,233,36,1,0,0,65,138,193,36,224,60,192,117,5,65,176,2,235,30,65,138,193,36,240,60,224,117,5,65,176,3,235,16,65,138,193,36,248,60,240,15,133,233,0,0,0,65,176,4,65,15,182,192,185,
      7,0,0,0,43,200,139,213,211,226,65,138,216,43,213,65,35,209,235,41,69,138,67,4,65,139,19,65,138,91,6,65,141,64,254,60,2,15,135,182,0,0,0,64,58,221,15,130,173,0,0,0,65,58,216,15,131,164,0,0,0,15,182,235,73,59,238,68,139,205,77,15,67,206,235,30,
      15,182,15,72,255,199,138,193,36,192,60,128,15,133,131,0,0,0,139,194,131,225,63,193,224,6,139,209,11,208,72,139,199,73,43,199,73,59,193,114,215,76,59,205,115,28,65,15,182,192,65,42,217,102,65,137,67,4,15,182,195,102,65,137,67,6,65,137,19,233,
      3,255,255,255,141,130,0,40,255,255,61,255,7,0,0,118,62,129,250,0,0,17,0,115,54,65,15,182,192,199,68,36,32,128,0,0,0,199,68,36,36,0,8,0,0,199,68,36,40,0,0,1,0,59,84,132,24,114,20,72,133,246,116,2,137,22,247,218,77,137,19,72,27,192,72,35,197,235,
      18,77,137,19,232,75,174,255,255,199,0,42,0,0,0,72,131,200,255,72,139,76,36,48,72,51,204,232,24,136,255,255,72,139,92,36,120,72,131,196,64,65,95,65,94,95,94,93,195,204,204,204,64,83,72,131,236,64,72,139,5,7,210,0,0,51,219,72,131,248,254,117,46,
      72,137,92,36,48,68,141,67,3,137,92,36,40,72,141,13,19,118,0,0,69,51,201,68,137,68,36,32,186,0,0,0,64,255,21,8,162,0,0,72,137,5,209,209,0,0,72,131,248,255,15,149,195,139,195,72,131,196,64,91,195,204,204,72,139,196,72,137,88,8,72,137,104,16,72,
      137,112,24,87,72,131,236,64,72,131,96,216,0,73,139,248,77,139,200,139,242,68,139,194,72,139,233,72,139,209,72,139,13,143,209,0,0,255,21,25,164,0,0,139,216,133,192,117,106,255,21,109,162,0,0,131,248,6,117,95,72,139,13,113,209,0,0,72,131,249,253,
      119,6,255,21,133,161,0,0,72,131,100,36,48,0,72,141,13,128,117,0,0,131,100,36,40,0,65,184,3,0,0,0,69,51,201,68,137,68,36,32,186,0,0,0,64,255,21,106,161,0,0,72,131,100,36,32,0,76,139,207,72,139,200,72,137,5,39,209,0,0,68,139,198,72,139,213,255,
      21,171,163,0,0,139,216,72,139,108,36,88,139,195,72,139,92,36,80,72,139,116,36,96,72,131,196,64,95,195,204,204,72,131,236,40,72,139,13,245,208,0,0,72,131,249,253,119,6,255,21,9,161,0,0,72,131,196,40,195,137,76,36,8,72,131,236,56,72,99,209,131,
      250,254,117,21,232,251,172,255,255,131,32,0,232,211,172,255,255,199,0,9,0,0,0,235,116,133,201,120,88,59,21,197,221,0,0,115,80,72,139,202,76,141,5,185,217,0,0,131,225,63,72,139,194,72,193,248,6,72,141,12,201,73,139,4,192,246,68,200,56,1,116,45,
      72,141,68,36,64,137,84,36,80,137,84,36,88,76,141,76,36,80,72,141,84,36,88,72,137,68,36,32,76,141,68,36,32,72,141,76,36,72,232,229,0,0,0,235,27,232,138,172,255,255,131,32,0,232,98,172,255,255,199,0,9,0,0,0,232,3,162,255,255,131,200,255,72,131,
      196,56,195,204,204,204,72,137,92,36,8,87,72,131,236,32,72,99,249,139,207,232,36,208,255,255,72,131,248,255,117,4,51,219,235,90,72,139,5,43,217,0,0,185,2,0,0,0,131,255,1,117,9,64,132,184,200,0,0,0,117,13,59,249,117,32,246,128,128,0,0,0,1,116,
      23,232,238,207,255,255,185,1,0,0,0,72,139,216,232,225,207,255,255,72,59,195,116,190,139,207,232,213,207,255,255,72,139,200,255,21,244,159,0,0,133,192,117,170,255,21,186,160,0,0,139,216,139,207,232,49,208,255,255,72,139,215,76,141,5,199,216,0,
      0,131,226,63,72,139,207,72,193,249,6,72,141,20,210,73,139,12,200,198,68,209,56,0,133,219,116,12,139,203,232,225,171,255,255,131,200,255,235,2,51,192,72,139,92,36,48,72,131,196,32,95,195,204,204,204,72,137,92,36,8,76,137,76,36,32,87,72,131,236,
      32,73,139,249,73,139,216,139,10,232,48,210,255,255,144,72,139,3,72,99,8,72,139,209,72,139,193,72,193,248,6,76,141,5,92,216,0,0,131,226,63,72,141,20,210,73,139,4,192,246,68,208,56,1,116,9,232,245,254,255,255,139,216,235,14,232,56,171,255,255,
      199,0,9,0,0,0,131,203,255,139,15,232,16,210,255,255,139,195,72,139,92,36,48,72,131,196,32,95,195,204,204,204,131,73,24,255,51,192,72,137,1,72,137,65,8,137,65,16,72,137,65,28,72,137,65,40,135,65,20,195,72,131,236,88,102,15,127,116,36,32,131,61,
      139,226,0,0,0,15,133,233,2,0,0,102,15,40,216,102,15,40,224,102,15,115,211,52,102,72,15,126,192,102,15,251,29,63,115,0,0,102,15,40,232,102,15,84,45,3,115,0,0,102,15,47,45,251,114,0,0,15,132,133,2,0,0,102,15,40,208,243,15,230,243,102,15,87,237,
      102,15,47,197,15,134,47,2,0,0,102,15,219,21,39,115,0,0,242,15,92,37,175,115,0,0,102,15,47,53,55,116,0,0,15,132,216,1,0,0,102,15,84,37,137,116,0,0,76,139,200,72,35,5,15,115,0,0,76,35,13,24,115,0,0,73,209,225,73,3,193,102,72,15,110,200,102,15,
      47,37,37,116,0,0,15,130,223,0,0,0,72,193,232,44,102,15,235,21,115,115,0,0,102,15,235,13,107,115,0,0,76,141,13,212,132,0,0,242,15,92,202,242,65,15,89,12,193,102,15,40,209,102,15,40,193,76,141,13,155,116,0,0,242,15,16,29,179,115,0,0,242,15,16,
      13,123,115,0,0,242,15,89,218,242,15,89,202,242,15,89,194,102,15,40,224,242,15,88,29,131,115,0,0,242,15,88,13,75,115,0,0,242,15,89,224,242,15,89,218,242,15,89,200,242,15,88,29,87,115,0,0,242,15,88,202,242,15,89,220,242,15,88,203,242,15,16,45,
      195,114,0,0,242,15,89,13,123,114,0,0,242,15,89,238,242,15,92,233,242,65,15,16,4,193,72,141,21,54,124,0,0,242,15,16,20,194,242,15,16,37,137,114,0,0,242,15,89,230,242,15,88,196,242,15,88,213,242,15,88,194,102,15,111,116,36,32,72,131,196,88,195,
      102,102,102,102,102,102,15,31,132,0,0,0,0,0,242,15,16,21,120,114,0,0,242,15,92,5,128,114,0,0,242,15,88,208,102,15,40,200,242,15,94,202,242,15,16,37,124,115,0,0,242,15,16,45,148,115,0,0,102,15,40,240,242,15,89,241,242,15,88,201,102,15,40,209,
      242,15,89,209,242,15,89,226,242,15,89,234,242,15,88,37,64,115,0,0,242,15,88,45,88,115,0,0,242,15,89,209,242,15,89,226,242,15,89,210,242,15,89,209,242,15,89,234,242,15,16,21,220,113,0,0,242,15,88,229,242,15,92,230,242,15,16,53,188,113,0,0,102,
      15,40,216,102,15,219,29,64,115,0,0,242,15,92,195,242,15,88,224,102,15,40,195,102,15,40,204,242,15,89,226,242,15,89,194,242,15,89,206,242,15,89,222,242,15,88,196,242,15,88,193,242,15,88,195,102,15,111,116,36,32,72,131,196,88,195,102,15,235,21,
      193,113,0,0,242,15,92,21,185,113,0,0,242,15,16,234,102,15,219,21,29,113,0,0,102,72,15,126,208,102,15,115,213,52,102,15,250,45,59,114,0,0,243,15,230,245,233,241,253,255,255,102,144,117,30,242,15,16,13,150,112,0,0,68,139,5,207,114,0,0,232,98,6,
      0,0,235,72,15,31,132,0,0,0,0,0,242,15,16,13,152,112,0,0,68,139,5,181,114,0,0,232,68,6,0,0,235,42,102,102,15,31,132,0,0,0,0,0,72,59,5,105,112,0,0,116,23,72,59,5,80,112,0,0,116,206,72,11,5,119,112,0,0,102,72,15,110,192,102,144,102,15,111,116,36,
      32,72,131,196,88,195,15,31,68,0,0,72,51,192,197,225,115,208,52,196,225,249,126,192,197,225,251,29,91,112,0,0,197,250,230,243,197,249,219,45,31,112,0,0,197,249,47,45,23,112,0,0,15,132,65,2,0,0,197,209,239,237,197,249,47,197,15,134,227,1,0,0,197,
      249,219,21,75,112,0,0,197,251,92,37,211,112,0,0,197,249,47,53,91,113,0,0,15,132,142,1,0,0,197,249,219,13,61,112,0,0,197,249,219,29,69,112,0,0,197,225,115,243,1,197,225,212,201,196,225,249,126,200,197,217,219,37,143,113,0,0,197,249,47,37,71,113,
      0,0,15,130,177,0,0,0,72,193,232,44,197,233,235,21,149,112,0,0,197,241,235,13,141,112,0,0,76,141,13,246,129,0,0,197,243,92,202,196,193,115,89,12,193,76,141,13,197,113,0,0,197,243,89,193,197,251,16,29,217,112,0,0,197,251,16,45,161,112,0,0,196,
      226,241,169,29,184,112,0,0,196,226,241,169,45,79,112,0,0,242,15,16,224,196,226,241,169,29,146,112,0,0,197,251,89,224,196,226,209,185,200,196,226,225,185,204,197,243,89,13,188,111,0,0,197,251,16,45,244,111,0,0,196,226,201,171,233,242,65,15,16,
      4,193,72,141,21,114,121,0,0,242,15,16,20,194,197,235,88,213,196,226,201,185,5,192,111,0,0,197,251,88,194,197,249,111,116,36,32,72,131,196,88,195,144,197,251,16,21,200,111,0,0,197,251,92,5,208,111,0,0,197,235,88,208,197,251,94,202,197,251,16,
      37,208,112,0,0,197,251,16,45,232,112,0,0,197,251,89,241,197,243,88,201,197,243,89,209,196,226,233,169,37,163,112,0,0,196,226,233,169,45,186,112,0,0,197,235,89,209,197,219,89,226,197,235,89,210,197,235,89,209,197,211,89,234,197,219,88,229,197,
      219,92,230,197,249,219,29,182,112,0,0,197,251,92,195,197,219,88,224,197,219,89,13,22,111,0,0,197,219,89,37,30,111,0,0,197,227,89,5,22,111,0,0,197,227,89,29,254,110,0,0,197,251,88,196,197,251,88,193,197,251,88,195,197,249,111,116,36,32,72,131,
      196,88,195,197,233,235,21,47,111,0,0,197,235,92,21,39,111,0,0,197,209,115,210,52,197,233,219,21,138,110,0,0,197,249,40,194,197,209,250,45,174,111,0,0,197,250,230,245,233,64,254,255,255,15,31,68,0,0,117,46,197,251,16,13,6,110,0,0,68,139,5,63,
      112,0,0,232,210,3,0,0,197,249,111,116,36,32,72,131,196,88,195,102,102,102,102,102,102,102,15,31,132,0,0,0,0,0,197,251,16,13,248,109,0,0,68,139,5,21,112,0,0,232,164,3,0,0,197,249,111,116,36,32,72,131,196,88,195,144,72,59,5,201,109,0,0,116,39,
      72,59,5,176,109,0,0,116,206,72,11,5,215,109,0,0,102,72,15,110,200,68,139,5,227,111,0,0,232,110,3,0,0,235,4,15,31,64,0,197,249,111,116,36,32,72,131,196,88,195,204,72,139,196,83,72,131,236,80,242,15,16,132,36,128,0,0,0,139,217,242,15,16,140,36,
      136,0,0,0,186,192,255,0,0,137,72,200,72,139,140,36,144,0,0,0,242,15,17,64,224,242,15,17,72,232,242,15,17,88,216,76,137,64,208,232,40,7,0,0,72,141,76,36,32,232,86,160,255,255,133,192,117,7,139,203,232,139,3,0,0,242,15,16,68,36,64,72,131,196,80,
      91,195,204,204,204,72,137,92,36,8,72,137,116,36,16,87,72,131,236,32,139,217,72,139,242,131,227,31,139,249,246,193,8,116,20,64,132,246,121,15,185,1,0,0,0,232,103,7,0,0,131,227,247,235,87,185,4,0,0,0,64,132,249,116,17,72,15,186,230,9,115,10,232,
      76,7,0,0,131,227,251,235,60,64,246,199,1,116,22,72,15,186,230,10,115,15,185,8,0,0,0,232,48,7,0,0,131,227,254,235,32,64,246,199,2,116,26,72,15,186,230,11,115,19,64,246,199,16,116,10,185,16,0,0,0,232,14,7,0,0,131,227,253,64,246,199,16,116,20,72,
      15,186,230,12,115,13,185,32,0,0,0,232,244,6,0,0,131,227,239,72,139,116,36,56,51,192,133,219,72,139,92,36,48,15,148,192,72,131,196,32,95,195,204,204,72,139,196,85,83,86,87,65,86,72,141,104,201,72,129,236,240,0,0,0,15,41,112,200,72,139,5,89,190,
      0,0,72,51,196,72,137,69,239,139,242,76,139,241,186,192,255,0,0,185,128,31,0,0,65,139,249,73,139,216,232,8,6,0,0,139,77,95,72,137,68,36,64,72,137,92,36,80,242,15,16,68,36,80,72,139,84,36,64,242,15,17,68,36,72,232,225,254,255,255,242,15,16,117,
      119,133,192,117,64,131,125,127,2,117,17,139,69,191,131,224,227,242,15,17,117,175,131,200,3,137,69,191,68,139,69,95,72,141,68,36,72,72,137,68,36,40,72,141,84,36,64,72,141,69,111,68,139,206,72,141,76,36,96,72,137,68,36,32,232,68,2,0,0,232,175,
      158,255,255,132,192,116,52,133,255,116,48,72,139,68,36,64,77,139,198,242,15,16,68,36,72,139,207,242,15,16,93,111,139,85,103,72,137,68,36,48,242,15,17,68,36,40,242,15,17,116,36,32,232,245,253,255,255,235,28,139,207,232,208,1,0,0,72,139,76,36,
      64,186,192,255,0,0,232,73,5,0,0,242,15,16,68,36,72,72,139,77,239,72,51,204,232,255,124,255,255,15,40,180,36,224,0,0,0,72,129,196,240,0,0,0,65,94,95,94,91,93,195,204,204,204,204,204,204,204,204,204,64,83,72,131,236,16,69,51,192,51,201,68,137,
      5,134,218,0,0,69,141,72,1,65,139,193,15,162,137,4,36,184,0,16,0,24,137,76,36,8,35,200,137,92,36,4,137,84,36,12,59,200,117,44,51,201,15,1,208,72,193,226,32,72,11,208,72,137,84,36,32,72,139,68,36,32,68,139,5,70,218,0,0,36,6,60,6,69,15,68,193,68,
      137,5,55,218,0,0,68,137,5,52,218,0,0,51,192,72,131,196,16,91,195,72,139,196,72,131,236,104,15,41,112,232,15,40,241,65,139,209,15,40,216,65,131,232,1,116,42,65,131,248,1,117,105,68,137,64,216,15,87,210,242,15,17,80,208,69,139,200,242,15,17,64,
      200,199,64,192,33,0,0,0,199,64,184,8,0,0,0,235,45,199,68,36,64,1,0,0,0,15,87,192,242,15,17,68,36,56,65,185,2,0,0,0,242,15,17,92,36,48,199,68,36,40,34,0,0,0,199,68,36,32,4,0,0,0,72,139,140,36,144,0,0,0,242,15,17,116,36,120,76,139,68,36,120,232,
      215,253,255,255,15,40,198,15,40,116,36,80,72,131,196,104,195,204,204,72,131,236,56,72,141,5,157,132,0,0,65,185,27,0,0,0,72,137,68,36,32,232,77,255,255,255,72,131,196,56,195,204,204,204,204,204,204,204,204,204,204,204,204,204,204,102,102,15,31,
      132,0,0,0,0,0,72,131,236,8,15,174,28,36,139,4,36,72,131,196,8,195,137,76,36,8,15,174,84,36,8,195,15,174,92,36,8,185,192,255,255,255,33,76,36,8,15,174,84,36,8,195,102,15,46,5,74,132,0,0,115,20,102,15,46,5,72,132,0,0,118,10,242,72,15,45,200,242,
      72,15,42,193,195,204,204,204,72,131,236,40,131,249,1,116,21,141,65,254,131,248,1,119,24,232,86,161,255,255,199,0,34,0,0,0,235,11,232,73,161,255,255,199,0,33,0,0,0,72,131,196,40,195,204,204,72,131,236,72,131,100,36,48,0,72,139,68,36,120,72,137,
      68,36,40,72,139,68,36,112,72,137,68,36,32,232,6,0,0,0,72,131,196,72,195,204,72,139,196,72,137,88,16,72,137,112,24,72,137,120,32,72,137,72,8,85,72,139,236,72,131,236,32,72,139,218,65,139,241,51,210,191,13,0,0,192,137,81,4,72,139,69,16,137,80,
      8,72,139,69,16,137,80,12,65,246,192,16,116,13,72,139,69,16,191,143,0,0,192,131,72,4,1,65,246,192,2,116,13,72,139,69,16,191,147,0,0,192,131,72,4,2,65,246,192,1,116,13,72,139,69,16,191,145,0,0,192,131,72,4,4,65,246,192,4,116,13,72,139,69,16,191,
      142,0,0,192,131,72,4,8,65,246,192,8,116,13,72,139,69,16,191,144,0,0,192,131,72,4,16,72,139,77,16,72,139,3,72,193,232,7,193,224,4,247,208,51,65,8,131,224,16,49,65,8,72,139,77,16,72,139,3,72,193,232,9,193,224,3,247,208,51,65,8,131,224,8,49,65,
      8,72,139,77,16,72,139,3,72,193,232,10,193,224,2,247,208,51,65,8,131,224,4,49,65,8,72,139,77,16,72,139,3,72,193,232,11,3,192,247,208,51,65,8,131,224,2,49,65,8,139,3,72,139,77,16,72,193,232,12,247,208,51,65,8,131,224,1,49,65,8,232,151,2,0,0,72,
      139,208,168,1,116,8,72,139,77,16,131,73,12,16,246,194,4,116,8,72,139,77,16,131,73,12,8,246,194,8,116,8,72,139,69,16,131,72,12,4,246,194,16,116,8,72,139,69,16,131,72,12,2,246,194,32,116,8,72,139,69,16,131,72,12,1,139,3,185,0,96,0,0,72,35,193,
      116,62,72,61,0,32,0,0,116,38,72,61,0,64,0,0,116,14,72,59,193,117,48,72,139,69,16,131,8,3,235,39,72,139,69,16,131,32,254,72,139,69,16,131,8,2,235,23,72,139,69,16,131,32,253,72,139,69,16,131,8,1,235,7,72,139,69,16,131,32,252,72,139,69,16,129,230,
      255,15,0,0,193,230,5,129,32,31,0,254,255,72,139,69,16,9,48,72,139,69,16,72,139,117,56,131,72,32,1,131,125,64,0,116,51,72,139,69,16,186,225,255,255,255,33,80,32,72,139,69,48,139,8,72,139,69,16,137,72,16,72,139,69,16,131,72,96,1,72,139,69,16,33,
      80,96,72,139,69,16,139,14,137,72,80,235,72,72,139,77,16,65,184,227,255,255,255,139,65,32,65,35,192,131,200,2,137,65,32,72,139,69,48,72,139,8,72,139,69,16,72,137,72,16,72,139,69,16,131,72,96,1,72,139,85,16,139,66,96,65,35,192,131,200,2,137,66,
      96,72,139,69,16,72,139,22,72,137,80,80,232,188,0,0,0,51,210,76,141,77,16,139,207,68,141,66,1,255,21,130,148,0,0,72,139,77,16,139,65,8,168,16,116,8,72,15,186,51,7,139,65,8,168,8,116,8,72,15,186,51,9,139,65,8,168,4,116,8,72,15,186,51,10,139,65,
      8,168,2,116,8,72,15,186,51,11,139,65,8,168,1,116,5,72,15,186,51,12,139,1,131,224,3,116,48,131,232,1,116,31,131,232,1,116,14,131,248,1,117,40,72,129,11,0,96,0,0,235,31,72,15,186,51,13,72,15,186,43,14,235,19,72,15,186,51,14,72,15,186,43,13,235,
      7,72,129,35,255,159,255,255,131,125,64,0,116,7,139,65,80,137,6,235,7,72,139,65,80,72,137,6,72,139,92,36,56,72,139,116,36,64,72,139,124,36,72,72,131,196,32,93,195,204,204,204,64,83,72,131,236,32,232,61,252,255,255,139,216,131,227,63,232,77,252,
      255,255,139,195,72,131,196,32,91,195,204,204,204,72,137,92,36,24,72,137,116,36,32,87,72,131,236,32,72,139,218,72,139,249,232,14,252,255,255,139,240,137,68,36,56,139,203,247,209,129,201,127,128,255,255,35,200,35,251,11,207,137,76,36,48,128,61,
      173,193,0,0,0,116,37,246,193,64,116,32,232,241,251,255,255,235,33,198,5,152,193,0,0,0,139,76,36,48,131,225,191,232,220,251,255,255,139,116,36,56,235,8,131,225,191,232,206,251,255,255,139,198,72,139,92,36,64,72,139,116,36,72,72,131,196,32,95,
      195,72,131,236,40,232,163,251,255,255,131,224,63,72,131,196,40,195,204,204,204,64,83,72,131,236,32,72,139,217,232,138,251,255,255,131,227,63,11,195,139,200,72,131,196,32,91,233,137,251,255,255,204,204,204,204,204,204,204,204,204,204,204,204,
      204,204,204,102,102,15,31,132,0,0,0,0,0,255,224,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,102,102,15,31,132,0,0,0,0,0,255,37,42,247,0,0,72,137,84,36,16,85,72,131,236,32,72,139,234,72,184,0,0,0,0,0,0,0,0,
      72,131,196,32,93,195,204,64,85,72,139,234,72,139,1,51,201,129,56,5,0,0,192,15,148,193,139,193,93,195,204,64,85,72,131,236,32,72,139,234,138,77,64,72,131,196,32,93,233,94,19,255,255,204,64,85,72,131,236,32,72,139,234,138,77,32,232,76,19,255,255,
      144,72,131,196,32,93,195,204,64,85,72,131,236,32,72,139,234,72,131,196,32,93,233,53,21,255,255,204,64,85,72,131,236,48,72,139,234,72,139,1,139,16,72,137,76,36,40,137,84,36,32,76,141,13,218,27,255,255,76,139,69,112,139,85,104,72,139,77,96,232,
      42,20,255,255,144,72,131,196,48,93,195,204,64,83,85,72,131,236,72,72,139,234,72,137,77,80,72,137,77,72,232,163,85,255,255,72,139,141,128,0,0,0,72,137,72,112,72,139,69,72,72,139,8,72,139,89,56,232,136,85,255,255,72,137,88,104,232,127,85,255,255,
      139,141,184,0,0,0,137,72,120,72,139,77,72,198,68,36,56,1,72,131,100,36,48,0,131,100,36,40,0,72,139,133,160,0,0,0,72,137,68,36,32,76,139,141,152,0,0,0,76,139,133,144,0,0,0,72,139,149,136,0,0,0,72,139,9,232,101,100,255,255,232,52,85,255,255,72,
      131,96,112,0,199,69,64,1,0,0,0,184,1,0,0,0,72,131,196,72,93,91,195,204,64,83,85,72,131,236,40,72,139,234,72,137,77,56,72,137,77,48,128,125,88,0,116,108,72,139,69,48,72,139,8,72,137,77,40,72,139,69,40,129,56,99,115,109,224,117,85,72,139,69,40,
      131,120,24,4,117,75,72,139,69,40,129,120,32,32,5,147,25,116,26,72,139,69,40,129,120,32,33,5,147,25,116,13,72,139,69,40,129,120,32,34,5,147,25,117,36,232,182,84,255,255,72,139,77,40,72,137,72,32,72,139,69,48,72,139,88,8,232,161,84,255,255,72,
      137,88,40,232,136,72,255,255,144,199,69,32,0,0,0,0,139,69,32,72,131,196,40,93,91,195,204,64,85,72,131,236,32,72,139,234,51,201,72,131,196,32,93,233,4,124,255,255,204,64,85,72,131,236,32,72,139,234,72,139,69,72,139,8,72,131,196,32,93,233,234,
      123,255,255,204,64,85,72,131,236,32,72,139,234,72,139,1,139,8,232,162,54,255,255,144,72,131,196,32,93,195,204,204,204,204,204,204,204,204,204,204,204,64,85,72,131,236,32,72,139,234,72,139,1,51,201,129,56,5,0,0,192,15,148,193,139,193,72,131,196,
      32,93,195,204,64,85,72,131,236,32,72,139,234,72,137,141,128,0,0,0,76,141,77,32,68,139,133,232,0,0,0,72,139,149,248,0,0,0,232,113,97,255,255,144,72,131,196,32,93,195,204,64,83,85,72,131,236,40,72,139,234,72,139,77,72,232,138,44,255,255,131,125,
      32,0,117,58,72,139,157,248,0,0,0,129,59,99,115,109,224,117,43,131,123,24,4,117,37,139,67,32,45,32,5,147,25,131,248,2,119,24,72,139,75,40,232,53,46,255,255,133,192,116,11,178,1,72,139,203,232,179,45,255,255,144,232,137,83,255,255,72,139,77,48,
      72,137,72,32,232,124,83,255,255,72,139,77,56,72,137,72,40,232,111,83,255,255,139,141,224,0,0,0,137,72,120,72,131,196,40,93,91,195,204,64,85,72,131,236,48,72,139,234,232,60,46,255,255,144,72,131,196,48,93,195,204,64,85,72,131,236,48,72,139,234,
      232,58,83,255,255,131,120,48,0,126,8,232,47,83,255,255,255,72,48,72,131,196,48,93,195,204,64,85,72,131,236,32,72,139,234,185,7,0,0,0,72,131,196,32,93,233,161,122,255,255,204,64,85,72,131,236,32,72,139,234,185,5,0,0,0,72,131,196,32,93,233,136,
      122,255,255,204,64,85,72,131,236,32,72,139,234,128,125,112,0,116,11,185,3,0,0,0,232,110,122,255,255,144,72,131,196,32,93,195,204,64,85,72,131,236,32,72,139,234,72,139,77,72,72,139,9,72,131,196,32,93,233,128,151,255,255,204,64,85,72,131,236,32,
      72,139,234,72,139,133,152,0,0,0,139,8,72,131,196,32,93,233,47,122,255,255,204,64,85,72,131,236,32,72,139,234,72,139,69,88,139,8,72,131,196,32,93,233,21,122,255,255,204,64,85,72,131,236,32,72,139,234,185,4,0,0,0,72,131,196,32,93,233,252,121,255,
      255,204,64,85,72,131,236,32,72,139,234,72,139,69,72,139,8,72,131,196,32,93,233,194,191,255,255,204,64,85,72,131,236,32,72,139,234,139,77,80,72,131,196,32,93,233,171,191,255,255,204,64,85,72,131,236,32,72,139,234,185,8,0,0,0,72,131,196,32,93,
      233,178,121,255,255,204,64,85,72,131,236,32,72,139,234,72,139,77,48,72,131,196,32,93,233,206,150,255,255,204,64,85,72,131,236,32,72,139,234,72,139,1,129,56,5,0,0,192,116,12,129,56,29,0,0,192,116,4,51,192,235,5,184,1,0,0,0,72,131,196,32,93,195,
      204,204,204,204,204,204,204,204,204,204,255,37,74,189,0,0,204,204,204,204,204,204,204,204,204,204,255,37,66,189,0,0,204,204,204,204,204,204,204,204,204,204,255,37,58,189,0,0,204,204,204,204,204,204,204,204,204,204,255,37,170,141,0,0,72,141,5,
      19,189,0,0,233,24,0,0,0,72,141,5,15,189,0,0,233,12,0,0,0,72,141,5,11,189,0,0,233,0,0,0,0,81,82,65,80,65,81,72,131,236,72,102,15,127,4,36,102,15,127,76,36,16,102,15,127,84,36,32,102,15,127,92,36,48,72,139,208,72,141,13,51,134,0,0,232,178,28,255,
      255,102,15,111,4,36,102,15,111,76,36,16,102,15,111,84,36,32,102,15,111,92,36,48,72,131,196,72,65,89,65,88,90,89,255,224,72,141,5,188,188,0,0,233,36,0,0,0,72,141,5,184,188,0,0,233,24,0,0,0,72,141,5,180,188,0,0,233,12,0,0,0,72,141,5,176,188,0,
      0,233,0,0,0,0,81,82,65,80,65,81,72,131,236,72,102,15,127,4,36,102,15,127,76,36,16,102,15,127,84,36,32,102,15,127,92,36,48,72,139,208,72,141,13,208,133,0,0,232,47,28,255,255,102,15,111,4,36,102,15,111,76,36,16,102,15,111,84,36,32,102,15,111,92,
      36,48,72,131,196,72,65,89,65,88,90,89,255,224,72,141,5,97,188,0,0,233,0,0,0,0,81,82,65,80,65,81,72,131,236,72,102,15,127,4,36,102,15,127,76,36,16,102,15,127,84,36,32,102,15,127,92,36,48,72,139,208,72,141,13,145,133,0,0,232,208,27,255,255,102,
      15,111,4,36,102,15,111,76,36,16,102,15,111,84,36,32,102,15,111,92,36,48,72,131,196,72,65,89,65,88,90,89,255,224,72,141,5,18,188,0,0,233,12,0,0,0,72,141,5,14,188,0,0,233,0,0,0,0,81,82,65,80,65,81,72,131,236,72,102,15,127,4,36,102,15,127,76,36,
      16,102,15,127,84,36,32,102,15,127,92,36,48,72,139,208,72,141,13,70,133,0,0,232,101,27,255,255,102,15,111,4,36,102,15,111,76,36,16,102,15,111,84,36,32,102,15,111,92,36,48,72,131,196,72,65,89,65,88,90,89,255,224,204,204,204,204,204,204,204,204,
      204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,
      204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,
      204,204,204,204,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,101,0,45,0,115,0,121,0,110,0,99,0,104,0,45,0,108,0,49,0,45,0,50,0,45,0,48,0,46,0,100,0,108,0,108,0,0,0,0,0,0,0,0,0,
      107,0,101,0,114,0,110,0,101,0,108,0,51,0,50,0,46,0,100,0,108,0,108,0,0,0,0,0,0,0,0,0,83,108,101,101,112,67,111,110,100,105,116,105,111,110,86,97,114,105,97,98,108,101,67,83,0,0,0,0,0,0,0,0,87,97,107,101,65,108,108,67,111,110,100,105,116,105,
      111,110,86,97,114,105,97,98,108,101,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,240,160,1,128,1,0,0,0,136,48,0,128,1,0,0,0,116,48,0,128,1,0,0,0,85,110,107,110,111,119,110,32,101,120,99,101,
      112,116,105,111,110,0,0,0,0,0,0,0,104,161,1,128,1,0,0,0,136,48,0,128,1,0,0,0,116,48,0,128,1,0,0,0,98,97,100,32,97,108,108,111,99,97,116,105,111,110,0,0,232,161,1,128,1,0,0,0,136,48,0,128,1,0,0,0,116,48,0,128,1,0,0,0,98,97,100,32,97,114,114,97,
      121,32,110,101,119,32,108,101,110,103,116,104,0,0,0,0,112,162,1,128,1,0,0,0,144,51,0,128,1,0,0,0,0,0,0,0,0,0,0,0,75,0,69,0,82,0,78,0,69,0,76,0,51,0,50,0,46,0,68,0,76,0,76,0,0,0,0,0,0,0,0,0,65,99,113,117,105,114,101,83,82,87,76,111,99,107,69,
      120,99,108,117,115,105,118,101,0,82,101,108,101,97,115,101,83,82,87,76,111,99,107,69,120,99,108,117,115,105,118,101,0,48,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,40,208,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,16,2,128,1,0,0,0,16,16,2,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,16,2,128,1,0,0,0,24,16,2,128,1,
      0,0,0,32,16,2,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,254,255,253,255,254,255,252,255,254,255,253,255,254,255,251,25,18,25,11,25,18,25,4,25,18,25,11,25,18,25,0,41,0,0,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,15,0,0,0,0,0,0,0,32,5,
      147,25,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,116,
      73,0,128,1,0,0,0,192,78,0,128,1,0,0,0,0,0,0,0,0,0,0,0,12,79,0,128,1,0,0,0,0,0,0,0,0,0,0,0,176,144,0,128,1,0,0,0,228,144,0,128,1,0,0,0,248,78,0,128,1,0,0,0,248,78,0,128,1,0,0,0,72,150,0,128,1,0,0,0,144,150,0,128,1,0,0,0,0,151,0,128,1,0,0,0,28,
      151,0,128,1,0,0,0,0,0,0,0,0,0,0,0,76,79,0,128,1,0,0,0,40,151,0,128,1,0,0,0,100,151,0,128,1,0,0,0,140,157,0,128,1,0,0,0,200,157,0,128,1,0,0,0,4,160,0,128,1,0,0,0,248,78,0,128,1,0,0,0,72,160,0,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,248,78,
      0,128,1,0,0,0,0,0,0,0,0,0,0,0,148,79,0,128,1,0,0,0,0,0,0,0,0,0,0,0,84,79,0,128,1,0,0,0,248,78,0,128,1,0,0,0,252,78,0,128,1,0,0,0,212,78,0,128,1,0,0,0,248,78,0,128,1,0,0,0,109,0,115,0,99,0,111,0,114,0,101,0,101,0,46,0,100,0,108,0,108,0,0,0,67,
      111,114,69,120,105,116,80,114,111,99,101,115,115,0,0,0,0,0,0,0,0,0,0,5,0,0,192,11,0,0,0,0,0,0,0,0,0,0,0,29,0,0,192,4,0,0,0,0,0,0,0,0,0,0,0,150,0,0,192,4,0,0,0,0,0,0,0,0,0,0,0,141,0,0,192,8,0,0,0,0,0,0,0,0,0,0,0,142,0,0,192,8,0,0,0,0,0,0,0,0,
      0,0,0,143,0,0,192,8,0,0,0,0,0,0,0,0,0,0,0,144,0,0,192,8,0,0,0,0,0,0,0,0,0,0,0,145,0,0,192,8,0,0,0,0,0,0,0,0,0,0,0,146,0,0,192,8,0,0,0,0,0,0,0,0,0,0,0,147,0,0,192,8,0,0,0,0,0,0,0,0,0,0,0,180,2,0,192,8,0,0,0,0,0,0,0,0,0,0,0,181,2,0,192,8,0,0,0,
      0,0,0,0,0,0,0,0,12,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,9,0,0,0,0,0,0,0,232,162,1,128,1,0,0,0,136,48,0,128,1,0,0,0,116,48,0,128,1,0,0,0,98,97,100,32,101,120,99,101,112,116,105,111,110,0,0,0,240,44,1,128,1,0,0,0,8,0,0,0,0,0,0,0,0,45,1,128,1,0,0,0,7,
      0,0,0,0,0,0,0,8,45,1,128,1,0,0,0,8,0,0,0,0,0,0,0,24,45,1,128,1,0,0,0,9,0,0,0,0,0,0,0,40,45,1,128,1,0,0,0,10,0,0,0,0,0,0,0,56,45,1,128,1,0,0,0,10,0,0,0,0,0,0,0,72,45,1,128,1,0,0,0,12,0,0,0,0,0,0,0,88,45,1,128,1,0,0,0,9,0,0,0,0,0,0,0,100,45,1,
      128,1,0,0,0,6,0,0,0,0,0,0,0,112,45,1,128,1,0,0,0,9,0,0,0,0,0,0,0,128,45,1,128,1,0,0,0,9,0,0,0,0,0,0,0,144,45,1,128,1,0,0,0,7,0,0,0,0,0,0,0,152,45,1,128,1,0,0,0,10,0,0,0,0,0,0,0,168,45,1,128,1,0,0,0,11,0,0,0,0,0,0,0,184,45,1,128,1,0,0,0,9,0,0,
      0,0,0,0,0,194,45,1,128,1,0,0,0,0,0,0,0,0,0,0,0,196,45,1,128,1,0,0,0,4,0,0,0,0,0,0,0,208,45,1,128,1,0,0,0,7,0,0,0,0,0,0,0,216,45,1,128,1,0,0,0,1,0,0,0,0,0,0,0,220,45,1,128,1,0,0,0,2,0,0,0,0,0,0,0,224,45,1,128,1,0,0,0,2,0,0,0,0,0,0,0,228,45,1,
      128,1,0,0,0,1,0,0,0,0,0,0,0,232,45,1,128,1,0,0,0,2,0,0,0,0,0,0,0,236,45,1,128,1,0,0,0,2,0,0,0,0,0,0,0,240,45,1,128,1,0,0,0,2,0,0,0,0,0,0,0,248,45,1,128,1,0,0,0,8,0,0,0,0,0,0,0,4,46,1,128,1,0,0,0,2,0,0,0,0,0,0,0,8,46,1,128,1,0,0,0,1,0,0,0,0,0,
      0,0,12,46,1,128,1,0,0,0,2,0,0,0,0,0,0,0,16,46,1,128,1,0,0,0,2,0,0,0,0,0,0,0,20,46,1,128,1,0,0,0,1,0,0,0,0,0,0,0,24,46,1,128,1,0,0,0,1,0,0,0,0,0,0,0,28,46,1,128,1,0,0,0,1,0,0,0,0,0,0,0,32,46,1,128,1,0,0,0,3,0,0,0,0,0,0,0,36,46,1,128,1,0,0,0,1,
      0,0,0,0,0,0,0,40,46,1,128,1,0,0,0,1,0,0,0,0,0,0,0,44,46,1,128,1,0,0,0,1,0,0,0,0,0,0,0,48,46,1,128,1,0,0,0,2,0,0,0,0,0,0,0,52,46,1,128,1,0,0,0,1,0,0,0,0,0,0,0,56,46,1,128,1,0,0,0,2,0,0,0,0,0,0,0,60,46,1,128,1,0,0,0,1,0,0,0,0,0,0,0,64,46,1,128,
      1,0,0,0,2,0,0,0,0,0,0,0,68,46,1,128,1,0,0,0,1,0,0,0,0,0,0,0,72,46,1,128,1,0,0,0,1,0,0,0,0,0,0,0,76,46,1,128,1,0,0,0,1,0,0,0,0,0,0,0,80,46,1,128,1,0,0,0,2,0,0,0,0,0,0,0,84,46,1,128,1,0,0,0,2,0,0,0,0,0,0,0,88,46,1,128,1,0,0,0,2,0,0,0,0,0,0,0,92,
      46,1,128,1,0,0,0,2,0,0,0,0,0,0,0,96,46,1,128,1,0,0,0,2,0,0,0,0,0,0,0,100,46,1,128,1,0,0,0,2,0,0,0,0,0,0,0,104,46,1,128,1,0,0,0,2,0,0,0,0,0,0,0,108,46,1,128,1,0,0,0,3,0,0,0,0,0,0,0,112,46,1,128,1,0,0,0,3,0,0,0,0,0,0,0,116,46,1,128,1,0,0,0,2,0,
      0,0,0,0,0,0,120,46,1,128,1,0,0,0,2,0,0,0,0,0,0,0,124,46,1,128,1,0,0,0,2,0,0,0,0,0,0,0,128,46,1,128,1,0,0,0,9,0,0,0,0,0,0,0,144,46,1,128,1,0,0,0,9,0,0,0,0,0,0,0,160,46,1,128,1,0,0,0,7,0,0,0,0,0,0,0,168,46,1,128,1,0,0,0,8,0,0,0,0,0,0,0,184,46,
      1,128,1,0,0,0,20,0,0,0,0,0,0,0,208,46,1,128,1,0,0,0,8,0,0,0,0,0,0,0,224,46,1,128,1,0,0,0,18,0,0,0,0,0,0,0,248,46,1,128,1,0,0,0,28,0,0,0,0,0,0,0,24,47,1,128,1,0,0,0,29,0,0,0,0,0,0,0,56,47,1,128,1,0,0,0,28,0,0,0,0,0,0,0,88,47,1,128,1,0,0,0,29,
      0,0,0,0,0,0,0,120,47,1,128,1,0,0,0,28,0,0,0,0,0,0,0,152,47,1,128,1,0,0,0,35,0,0,0,0,0,0,0,192,47,1,128,1,0,0,0,26,0,0,0,0,0,0,0,224,47,1,128,1,0,0,0,32,0,0,0,0,0,0,0,8,48,1,128,1,0,0,0,31,0,0,0,0,0,0,0,40,48,1,128,1,0,0,0,38,0,0,0,0,0,0,0,80,
      48,1,128,1,0,0,0,26,0,0,0,0,0,0,0,112,48,1,128,1,0,0,0,15,0,0,0,0,0,0,0,128,48,1,128,1,0,0,0,3,0,0,0,0,0,0,0,132,48,1,128,1,0,0,0,5,0,0,0,0,0,0,0,144,48,1,128,1,0,0,0,15,0,0,0,0,0,0,0,160,48,1,128,1,0,0,0,35,0,0,0,0,0,0,0,196,48,1,128,1,0,0,
      0,6,0,0,0,0,0,0,0,208,48,1,128,1,0,0,0,9,0,0,0,0,0,0,0,224,48,1,128,1,0,0,0,14,0,0,0,0,0,0,0,240,48,1,128,1,0,0,0,26,0,0,0,0,0,0,0,16,49,1,128,1,0,0,0,28,0,0,0,0,0,0,0,48,49,1,128,1,0,0,0,37,0,0,0,0,0,0,0,88,49,1,128,1,0,0,0,36,0,0,0,0,0,0,0,
      128,49,1,128,1,0,0,0,37,0,0,0,0,0,0,0,168,49,1,128,1,0,0,0,43,0,0,0,0,0,0,0,216,49,1,128,1,0,0,0,26,0,0,0,0,0,0,0,248,49,1,128,1,0,0,0,32,0,0,0,0,0,0,0,32,50,1,128,1,0,0,0,34,0,0,0,0,0,0,0,72,50,1,128,1,0,0,0,40,0,0,0,0,0,0,0,120,50,1,128,1,
      0,0,0,42,0,0,0,0,0,0,0,168,50,1,128,1,0,0,0,27,0,0,0,0,0,0,0,200,50,1,128,1,0,0,0,12,0,0,0,0,0,0,0,216,50,1,128,1,0,0,0,17,0,0,0,0,0,0,0,240,50,1,128,1,0,0,0,11,0,0,0,0,0,0,0,194,45,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,51,1,128,1,0,0,0,17,0,0,0,0,
      0,0,0,24,51,1,128,1,0,0,0,27,0,0,0,0,0,0,0,56,51,1,128,1,0,0,0,18,0,0,0,0,0,0,0,80,51,1,128,1,0,0,0,28,0,0,0,0,0,0,0,112,51,1,128,1,0,0,0,25,0,0,0,0,0,0,0,194,45,1,128,1,0,0,0,0,0,0,0,0,0,0,0,8,46,1,128,1,0,0,0,1,0,0,0,0,0,0,0,28,46,1,128,1,
      0,0,0,1,0,0,0,0,0,0,0,80,46,1,128,1,0,0,0,2,0,0,0,0,0,0,0,72,46,1,128,1,0,0,0,1,0,0,0,0,0,0,0,40,46,1,128,1,0,0,0,1,0,0,0,0,0,0,0,208,46,1,128,1,0,0,0,8,0,0,0,0,0,0,0,144,51,1,128,1,0,0,0,21,0,0,0,0,0,0,0,95,95,98,97,115,101,100,40,0,0,0,0,0,
      0,0,0,95,95,99,100,101,99,108,0,95,95,112,97,115,99,97,108,0,0,0,0,0,0,0,0,95,95,115,116,100,99,97,108,108,0,0,0,0,0,0,0,95,95,116,104,105,115,99,97,108,108,0,0,0,0,0,0,95,95,102,97,115,116,99,97,108,108,0,0,0,0,0,0,95,95,118,101,99,116,111,
      114,99,97,108,108,0,0,0,0,95,95,99,108,114,99,97,108,108,0,0,0,95,95,101,97,98,105,0,0,0,0,0,0,95,95,115,119,105,102,116,95,49,0,0,0,0,0,0,0,95,95,115,119,105,102,116,95,50,0,0,0,0,0,0,0,95,95,112,116,114,54,52,0,95,95,114,101,115,116,114,105,
      99,116,0,0,0,0,0,0,95,95,117,110,97,108,105,103,110,101,100,0,0,0,0,0,114,101,115,116,114,105,99,116,40,0,0,0,32,110,101,119,0,0,0,0,0,0,0,0,32,100,101,108,101,116,101,0,61,0,0,0,62,62,0,0,60,60,0,0,33,0,0,0,61,61,0,0,33,61,0,0,91,93,0,0,0,0,
      0,0,111,112,101,114,97,116,111,114,0,0,0,0,45,62,0,0,42,0,0,0,43,43,0,0,45,45,0,0,45,0,0,0,43,0,0,0,38,0,0,0,45,62,42,0,47,0,0,0,37,0,0,0,60,0,0,0,60,61,0,0,62,0,0,0,62,61,0,0,44,0,0,0,40,41,0,0,126,0,0,0,94,0,0,0,124,0,0,0,38,38,0,0,124,124,
      0,0,42,61,0,0,43,61,0,0,45,61,0,0,47,61,0,0,37,61,0,0,62,62,61,0,60,60,61,0,38,61,0,0,124,61,0,0,94,61,0,0,96,118,102,116,97,98,108,101,39,0,0,0,0,0,0,0,96,118,98,116,97,98,108,101,39,0,0,0,0,0,0,0,96,118,99,97,108,108,39,0,96,116,121,112,101,
      111,102,39,0,0,0,0,0,0,0,0,96,108,111,99,97,108,32,115,116,97,116,105,99,32,103,117,97,114,100,39,0,0,0,0,96,115,116,114,105,110,103,39,0,0,0,0,0,0,0,0,96,118,98,97,115,101,32,100,101,115,116,114,117,99,116,111,114,39,0,0,0,0,0,0,96,118,101,
      99,116,111,114,32,100,101,108,101,116,105,110,103,32,100,101,115,116,114,117,99,116,111,114,39,0,0,0,0,96,100,101,102,97,117,108,116,32,99,111,110,115,116,114,117,99,116,111,114,32,99,108,111,115,117,114,101,39,0,0,0,96,115,99,97,108,97,114,
      32,100,101,108,101,116,105,110,103,32,100,101,115,116,114,117,99,116,111,114,39,0,0,0,0,96,118,101,99,116,111,114,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,96,118,101,99,116,111,114,32,100,101,115,
      116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,0,96,118,101,99,116,111,114,32,118,98,97,115,101,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,0,0,96,118,105,114,116,117,97,108,
      32,100,105,115,112,108,97,99,101,109,101,110,116,32,109,97,112,39,0,0,0,0,0,0,96,101,104,32,118,101,99,116,111,114,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,0,0,0,0,0,96,101,104,32,118,101,99,116,
      111,114,32,100,101,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,96,101,104,32,118,101,99,116,111,114,32,118,98,97,115,101,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,96,99,111,
      112,121,32,99,111,110,115,116,114,117,99,116,111,114,32,99,108,111,115,117,114,101,39,0,0,0,0,0,0,96,117,100,116,32,114,101,116,117,114,110,105,110,103,39,0,96,69,72,0,96,82,84,84,73,0,0,0,0,0,0,0,96,108,111,99,97,108,32,118,102,116,97,98,108,
      101,39,0,96,108,111,99,97,108,32,118,102,116,97,98,108,101,32,99,111,110,115,116,114,117,99,116,111,114,32,99,108,111,115,117,114,101,39,0,32,110,101,119,91,93,0,0,0,0,0,0,32,100,101,108,101,116,101,91,93,0,0,0,0,0,0,0,96,111,109,110,105,32,
      99,97,108,108,115,105,103,39,0,0,96,112,108,97,99,101,109,101,110,116,32,100,101,108,101,116,101,32,99,108,111,115,117,114,101,39,0,0,0,0,0,0,96,112,108,97,99,101,109,101,110,116,32,100,101,108,101,116,101,91,93,32,99,108,111,115,117,114,101,
      39,0,0,0,0,96,109,97,110,97,103,101,100,32,118,101,99,116,111,114,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,96,109,97,110,97,103,101,100,32,118,101,99,116,111,114,32,100,101,115,116,114,117,99,116,
      111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,0,96,101,104,32,118,101,99,116,111,114,32,99,111,112,121,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,96,101,104,32,118,101,99,116,111,114,32,118,98,
      97,115,101,32,99,111,112,121,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,0,0,96,100,121,110,97,109,105,99,32,105,110,105,116,105,97,108,105,122,101,114,32,102,111,114,32,39,0,0,0,0,0,0,96,100,121,110,
      97,109,105,99,32,97,116,101,120,105,116,32,100,101,115,116,114,117,99,116,111,114,32,102,111,114,32,39,0,0,0,0,0,0,0,0,96,118,101,99,116,111,114,32,99,111,112,121,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,
      39,0,0,0,0,0,0,96,118,101,99,116,111,114,32,118,98,97,115,101,32,99,111,112,121,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,0,0,0,0,0,96,109,97,110,97,103,101,100,32,118,101,99,116,111,114,32,99,111,
      112,121,32,99,111,110,115,116,114,117,99,116,111,114,32,105,116,101,114,97,116,111,114,39,0,0,0,0,0,0,96,108,111,99,97,108,32,115,116,97,116,105,99,32,116,104,114,101,97,100,32,103,117,97,114,100,39,0,0,0,0,0,111,112,101,114,97,116,111,114,32,
      34,34,32,0,0,0,0,111,112,101,114,97,116,111,114,32,99,111,95,97,119,97,105,116,0,0,0,0,0,0,0,111,112,101,114,97,116,111,114,60,61,62,0,0,0,0,0,32,84,121,112,101,32,68,101,115,99,114,105,112,116,111,114,39,0,0,0,0,0,0,0,32,66,97,115,101,32,67,
      108,97,115,115,32,68,101,115,99,114,105,112,116,111,114,32,97,116,32,40,0,0,0,0,0,32,66,97,115,101,32,67,108,97,115,115,32,65,114,114,97,121,39,0,0,0,0,0,0,32,67,108,97,115,115,32,72,105,101,114,97,114,99,104,121,32,68,101,115,99,114,105,112,
      116,111,114,39,0,0,0,0,32,67,111,109,112,108,101,116,101,32,79,98,106,101,99,116,32,76,111,99,97,116,111,114,39,0,0,0,0,0,0,0,96,97,110,111,110,121,109,111,117,115,32,110,97,109,101,115,112,97,99,101,39,0,0,0,0,0,0,0,0,0,0,0,80,52,1,128,1,0,
      0,0,144,52,1,128,1,0,0,0,208,52,1,128,1,0,0,0,16,53,1,128,1,0,0,0,96,53,1,128,1,0,0,0,192,53,1,128,1,0,0,0,16,54,1,128,1,0,0,0,80,54,1,128,1,0,0,0,144,54,1,128,1,0,0,0,208,54,1,128,1,0,0,0,16,55,1,128,1,0,0,0,80,55,1,128,1,0,0,0,160,55,1,128,
      1,0,0,0,0,56,1,128,1,0,0,0,80,56,1,128,1,0,0,0,160,56,1,128,1,0,0,0,184,56,1,128,1,0,0,0,208,56,1,128,1,0,0,0,224,56,1,128,1,0,0,0,40,57,1,128,1,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,101,0,45,0,
      100,0,97,0,116,0,101,0,116,0,105,0,109,0,101,0,45,0,108,0,49,0,45,0,49,0,45,0,49,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,101,0,45,0,102,0,105,0,98,0,101,0,114,0,115,0,45,0,108,0,49,0,45,0,49,0,45,
      0,49,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,101,0,45,0,102,0,105,0,108,0,101,0,45,0,108,0,49,0,45,0,50,0,45,0,50,0,0,0,0,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,
      0,45,0,99,0,111,0,114,0,101,0,45,0,108,0,111,0,99,0,97,0,108,0,105,0,122,0,97,0,116,0,105,0,111,0,110,0,45,0,108,0,49,0,45,0,50,0,45,0,49,0,0,0,0,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,101,
      0,45,0,108,0,111,0,99,0,97,0,108,0,105,0,122,0,97,0,116,0,105,0,111,0,110,0,45,0,111,0,98,0,115,0,111,0,108,0,101,0,116,0,101,0,45,0,108,0,49,0,45,0,50,0,45,0,48,0,0,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,
      99,0,111,0,114,0,101,0,45,0,112,0,114,0,111,0,99,0,101,0,115,0,115,0,116,0,104,0,114,0,101,0,97,0,100,0,115,0,45,0,108,0,49,0,45,0,49,0,45,0,50,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,101,
      0,45,0,115,0,116,0,114,0,105,0,110,0,103,0,45,0,108,0,49,0,45,0,49,0,45,0,48,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,101,0,45,0,115,0,121,0,110,0,99,0,104,0,45,0,108,0,49,0,45,0,50,0,45,0,
      48,0,0,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,101,0,45,0,115,0,121,0,115,0,105,0,110,0,102,0,111,0,45,0,108,0,49,0,45,0,50,0,45,0,49,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,
      105,0,110,0,45,0,99,0,111,0,114,0,101,0,45,0,119,0,105,0,110,0,114,0,116,0,45,0,108,0,49,0,45,0,49,0,45,0,48,0,0,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,99,0,111,0,114,0,101,0,45,0,120,0,115,0,116,0,97,0,116,
      0,101,0,45,0,108,0,50,0,45,0,49,0,45,0,48,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,114,0,116,0,99,0,111,0,114,0,101,0,45,0,110,0,116,0,117,0,115,0,101,0,114,0,45,0,119,0,105,0,110,0,100,0,111,0,119,0,45,0,108,
      0,49,0,45,0,49,0,45,0,48,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,115,0,101,0,99,0,117,0,114,0,105,0,116,0,121,0,45,0,115,0,121,0,115,0,116,0,101,0,109,0,102,0,117,0,110,0,99,0,116,0,105,0,111,0,110,0,115,0,45,
      0,108,0,49,0,45,0,49,0,45,0,48,0,0,0,0,0,0,0,0,0,0,0,0,0,101,0,120,0,116,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,110,0,116,0,117,0,115,0,101,0,114,0,45,0,100,0,105,0,97,0,108,0,111,0,103,0,98,0,111,0,120,0,45,0,108,0,49,0,45,0,49,0,45,
      0,48,0,0,0,0,0,0,0,0,0,0,0,0,0,101,0,120,0,116,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,110,0,116,0,117,0,115,0,101,0,114,0,45,0,119,0,105,0,110,0,100,0,111,0,119,0,115,0,116,0,97,0,116,0,105,0,111,0,110,0,45,0,108,0,49,0,45,0,49,0,45,
      0,48,0,0,0,0,0,97,0,100,0,118,0,97,0,112,0,105,0,51,0,50,0,0,0,0,0,0,0,0,0,107,0,101,0,114,0,110,0,101,0,108,0,51,0,50,0,0,0,0,0,0,0,0,0,110,0,116,0,100,0,108,0,108,0,0,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,119,0,105,0,110,0,45,0,
      97,0,112,0,112,0,109,0,111,0,100,0,101,0,108,0,45,0,114,0,117,0,110,0,116,0,105,0,109,0,101,0,45,0,108,0,49,0,45,0,49,0,45,0,50,0,0,0,0,0,117,0,115,0,101,0,114,0,51,0,50,0,0,0,0,0,97,0,112,0,105,0,45,0,109,0,115,0,45,0,0,0,101,0,120,0,116,0,
      45,0,109,0,115,0,45,0,0,0,16,0,0,0,0,0,0,0,65,114,101,70,105,108,101,65,112,105,115,65,78,83,73,0,1,0,0,0,16,0,0,0,70,108,115,65,108,108,111,99,0,0,0,0,0,0,0,0,1,0,0,0,16,0,0,0,70,108,115,70,114,101,101,0,1,0,0,0,16,0,0,0,70,108,115,71,101,116,
      86,97,108,117,101,0,0,0,0,0,1,0,0,0,16,0,0,0,70,108,115,83,101,116,86,97,108,117,101,0,0,0,0,0,7,0,0,0,16,0,0,0,73,110,105,116,105,97,108,105,122,101,67,114,105,116,105,99,97,108,83,101,99,116,105,111,110,69,120,0,0,0,0,0,3,0,0,0,16,0,0,0,76,
      67,77,97,112,83,116,114,105,110,103,69,120,0,0,0,3,0,0,0,16,0,0,0,76,111,99,97,108,101,78,97,109,101,84,111,76,67,73,68,0,0,0,0,18,0,0,0,65,112,112,80,111,108,105,99,121,71,101,116,80,114,111,99,101,115,115,84,101,114,109,105,110,97,116,105,
      111,110,77,101,116,104,111,100,0,0,0,0,112,58,1,128,1,0,0,0,128,58,1,128,1,0,0,0,144,58,1,128,1,0,0,0,160,58,1,128,1,0,0,0,106,0,97,0,45,0,74,0,80,0,0,0,0,0,0,0,122,0,104,0,45,0,67,0,78,0,0,0,0,0,0,0,107,0,111,0,45,0,75,0,82,0,0,0,0,0,0,0,122,
      0,104,0,45,0,84,0,87,0,0,0,0,0,0,0,112,61,1,128,1,0,0,0,116,61,1,128,1,0,0,0,120,61,1,128,1,0,0,0,124,61,1,128,1,0,0,0,128,61,1,128,1,0,0,0,132,61,1,128,1,0,0,0,136,61,1,128,1,0,0,0,140,61,1,128,1,0,0,0,148,61,1,128,1,0,0,0,160,61,1,128,1,0,
      0,0,168,61,1,128,1,0,0,0,184,61,1,128,1,0,0,0,196,61,1,128,1,0,0,0,208,61,1,128,1,0,0,0,220,61,1,128,1,0,0,0,224,61,1,128,1,0,0,0,228,61,1,128,1,0,0,0,232,61,1,128,1,0,0,0,236,61,1,128,1,0,0,0,240,61,1,128,1,0,0,0,244,61,1,128,1,0,0,0,248,61,
      1,128,1,0,0,0,252,61,1,128,1,0,0,0,0,62,1,128,1,0,0,0,4,62,1,128,1,0,0,0,8,62,1,128,1,0,0,0,16,62,1,128,1,0,0,0,24,62,1,128,1,0,0,0,36,62,1,128,1,0,0,0,44,62,1,128,1,0,0,0,236,61,1,128,1,0,0,0,52,62,1,128,1,0,0,0,60,62,1,128,1,0,0,0,68,62,1,
      128,1,0,0,0,80,62,1,128,1,0,0,0,96,62,1,128,1,0,0,0,104,62,1,128,1,0,0,0,120,62,1,128,1,0,0,0,132,62,1,128,1,0,0,0,136,62,1,128,1,0,0,0,144,62,1,128,1,0,0,0,160,62,1,128,1,0,0,0,184,62,1,128,1,0,0,0,1,0,0,0,0,0,0,0,200,62,1,128,1,0,0,0,208,62,
      1,128,1,0,0,0,216,62,1,128,1,0,0,0,224,62,1,128,1,0,0,0,232,62,1,128,1,0,0,0,240,62,1,128,1,0,0,0,248,62,1,128,1,0,0,0,0,63,1,128,1,0,0,0,16,63,1,128,1,0,0,0,32,63,1,128,1,0,0,0,48,63,1,128,1,0,0,0,72,63,1,128,1,0,0,0,96,63,1,128,1,0,0,0,112,
      63,1,128,1,0,0,0,136,63,1,128,1,0,0,0,144,63,1,128,1,0,0,0,152,63,1,128,1,0,0,0,160,63,1,128,1,0,0,0,168,63,1,128,1,0,0,0,176,63,1,128,1,0,0,0,184,63,1,128,1,0,0,0,192,63,1,128,1,0,0,0,200,63,1,128,1,0,0,0,208,63,1,128,1,0,0,0,216,63,1,128,1,
      0,0,0,224,63,1,128,1,0,0,0,232,63,1,128,1,0,0,0,248,63,1,128,1,0,0,0,16,64,1,128,1,0,0,0,32,64,1,128,1,0,0,0,168,63,1,128,1,0,0,0,48,64,1,128,1,0,0,0,64,64,1,128,1,0,0,0,80,64,1,128,1,0,0,0,96,64,1,128,1,0,0,0,120,64,1,128,1,0,0,0,136,64,1,128,
      1,0,0,0,160,64,1,128,1,0,0,0,180,64,1,128,1,0,0,0,188,64,1,128,1,0,0,0,200,64,1,128,1,0,0,0,224,64,1,128,1,0,0,0,8,65,1,128,1,0,0,0,32,65,1,128,1,0,0,0,83,117,110,0,77,111,110,0,84,117,101,0,87,101,100,0,84,104,117,0,70,114,105,0,83,97,116,0,
      83,117,110,100,97,121,0,0,77,111,110,100,97,121,0,0,0,0,0,0,84,117,101,115,100,97,121,0,87,101,100,110,101,115,100,97,121,0,0,0,0,0,0,0,84,104,117,114,115,100,97,121,0,0,0,0,70,114,105,100,97,121,0,0,0,0,0,0,83,97,116,117,114,100,97,121,0,0,
      0,0,74,97,110,0,70,101,98,0,77,97,114,0,65,112,114,0,77,97,121,0,74,117,110,0,74,117,108,0,65,117,103,0,83,101,112,0,79,99,116,0,78,111,118,0,68,101,99,0,0,0,0,0,74,97,110,117,97,114,121,0,70,101,98,114,117,97,114,121,0,0,0,0,77,97,114,99,104,
      0,0,0,65,112,114,105,108,0,0,0,74,117,110,101,0,0,0,0,74,117,108,121,0,0,0,0,65,117,103,117,115,116,0,0,0,0,0,0,83,101,112,116,101,109,98,101,114,0,0,0,0,0,0,0,79,99,116,111,98,101,114,0,78,111,118,101,109,98,101,114,0,0,0,0,0,0,0,0,68,101,99,
      101,109,98,101,114,0,0,0,0,65,77,0,0,80,77,0,0,0,0,0,0,77,77,47,100,100,47,121,121,0,0,0,0,0,0,0,0,100,100,100,100,44,32,77,77,77,77,32,100,100,44,32,121,121,121,121,0,0,0,0,0,72,72,58,109,109,58,115,115,0,0,0,0,0,0,0,0,83,0,117,0,110,0,0,0,
      77,0,111,0,110,0,0,0,84,0,117,0,101,0,0,0,87,0,101,0,100,0,0,0,84,0,104,0,117,0,0,0,70,0,114,0,105,0,0,0,83,0,97,0,116,0,0,0,83,0,117,0,110,0,100,0,97,0,121,0,0,0,0,0,77,0,111,0,110,0,100,0,97,0,121,0,0,0,0,0,84,0,117,0,101,0,115,0,100,0,97,
      0,121,0,0,0,87,0,101,0,100,0,110,0,101,0,115,0,100,0,97,0,121,0,0,0,0,0,0,0,84,0,104,0,117,0,114,0,115,0,100,0,97,0,121,0,0,0,0,0,0,0,0,0,70,0,114,0,105,0,100,0,97,0,121,0,0,0,0,0,83,0,97,0,116,0,117,0,114,0,100,0,97,0,121,0,0,0,0,0,0,0,0,0,
      74,0,97,0,110,0,0,0,70,0,101,0,98,0,0,0,77,0,97,0,114,0,0,0,65,0,112,0,114,0,0,0,77,0,97,0,121,0,0,0,74,0,117,0,110,0,0,0,74,0,117,0,108,0,0,0,65,0,117,0,103,0,0,0,83,0,101,0,112,0,0,0,79,0,99,0,116,0,0,0,78,0,111,0,118,0,0,0,68,0,101,0,99,0,
      0,0,74,0,97,0,110,0,117,0,97,0,114,0,121,0,0,0,70,0,101,0,98,0,114,0,117,0,97,0,114,0,121,0,0,0,0,0,0,0,0,0,77,0,97,0,114,0,99,0,104,0,0,0,0,0,0,0,65,0,112,0,114,0,105,0,108,0,0,0,0,0,0,0,74,0,117,0,110,0,101,0,0,0,0,0,0,0,0,0,74,0,117,0,108,
      0,121,0,0,0,0,0,0,0,0,0,65,0,117,0,103,0,117,0,115,0,116,0,0,0,0,0,83,0,101,0,112,0,116,0,101,0,109,0,98,0,101,0,114,0,0,0,0,0,0,0,79,0,99,0,116,0,111,0,98,0,101,0,114,0,0,0,78,0,111,0,118,0,101,0,109,0,98,0,101,0,114,0,0,0,0,0,0,0,0,0,68,0,
      101,0,99,0,101,0,109,0,98,0,101,0,114,0,0,0,0,0,65,0,77,0,0,0,0,0,80,0,77,0,0,0,0,0,0,0,0,0,77,0,77,0,47,0,100,0,100,0,47,0,121,0,121,0,0,0,0,0,0,0,0,0,100,0,100,0,100,0,100,0,44,0,32,0,77,0,77,0,77,0,77,0,32,0,100,0,100,0,44,0,32,0,121,0,121,
      0,121,0,121,0,0,0,72,0,72,0,58,0,109,0,109,0,58,0,115,0,115,0,0,0,0,0,0,0,0,0,101,0,110,0,45,0,85,0,83,0,0,0,0,0,0,0,1,0,0,0,22,0,0,0,2,0,0,0,2,0,0,0,3,0,0,0,2,0,0,0,4,0,0,0,24,0,0,0,5,0,0,0,13,0,0,0,6,0,0,0,9,0,0,0,7,0,0,0,12,0,0,0,8,0,0,0,
      12,0,0,0,9,0,0,0,12,0,0,0,10,0,0,0,7,0,0,0,11,0,0,0,8,0,0,0,12,0,0,0,22,0,0,0,13,0,0,0,22,0,0,0,15,0,0,0,2,0,0,0,16,0,0,0,13,0,0,0,17,0,0,0,18,0,0,0,18,0,0,0,2,0,0,0,33,0,0,0,13,0,0,0,53,0,0,0,2,0,0,0,65,0,0,0,13,0,0,0,67,0,0,0,2,0,0,0,80,0,
      0,0,17,0,0,0,82,0,0,0,13,0,0,0,83,0,0,0,13,0,0,0,87,0,0,0,22,0,0,0,89,0,0,0,11,0,0,0,108,0,0,0,13,0,0,0,109,0,0,0,32,0,0,0,112,0,0,0,28,0,0,0,114,0,0,0,9,0,0,0,128,0,0,0,10,0,0,0,129,0,0,0,10,0,0,0,130,0,0,0,9,0,0,0,131,0,0,0,22,0,0,0,132,0,
      0,0,13,0,0,0,145,0,0,0,41,0,0,0,158,0,0,0,13,0,0,0,161,0,0,0,2,0,0,0,164,0,0,0,11,0,0,0,167,0,0,0,13,0,0,0,183,0,0,0,17,0,0,0,206,0,0,0,2,0,0,0,215,0,0,0,11,0,0,0,89,4,0,0,42,0,0,0,24,7,0,0,12,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,40,0,40,0,40,0,40,0,40,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,72,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,
      132,0,132,0,132,0,132,0,132,0,132,0,132,0,132,0,132,0,132,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,129,0,129,0,129,0,129,0,129,0,129,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,16,0,16,0,16,0,16,0,16,0,16,0,
      130,0,130,0,130,0,130,0,130,0,130,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,16,0,16,0,16,0,16,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,
      146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,
      207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,
      19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,91,
      92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,
      155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,
      216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,
      149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,
      210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,
      23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,65,66,67,68,69,70,71,
      72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,
      169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,
      230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,0,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,40,0,40,0,40,0,40,0,40,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,
      0,32,0,32,0,32,0,32,0,32,0,72,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,132,0,132,0,132,0,132,0,132,0,132,0,132,0,132,0,132,0,132,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,129,1,129,1,129,1,129,1,129,1,129,1,1,1,
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,16,0,16,0,16,0,16,0,16,0,16,0,130,1,130,1,130,1,130,1,130,1,130,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,16,0,16,0,16,0,16,0,
      32,0,32,0,32,0,32,0,32,0,32,0,40,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,8,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,18,1,16,0,16,0,48,0,16,0,16,
      0,16,0,16,0,20,0,20,0,16,0,18,1,16,0,16,0,16,0,20,0,18,1,16,0,16,0,16,0,16,0,16,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,16,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,
      1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,16,0,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,1,1,0,0,0,0,144,52,1,128,1,0,0,0,80,54,1,128,1,0,0,0,184,56,1,128,1,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,0,0,0,1,0,
      0,0,2,0,0,0,224,231,1,128,1,0,0,0,128,232,1,128,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,64,89,1,128,1,0,0,0,2,0,0,0,0,0,0,0,72,89,1,128,1,0,0,0,3,0,0,0,0,0,0,0,80,89,1,128,1,0,0,0,4,0,0,0,0,0,0,0,88,89,1,128,1,0,0,0,5,0,0,0,0,0,0,0,104,89,1,
      128,1,0,0,0,6,0,0,0,0,0,0,0,112,89,1,128,1,0,0,0,7,0,0,0,0,0,0,0,120,89,1,128,1,0,0,0,8,0,0,0,0,0,0,0,128,89,1,128,1,0,0,0,9,0,0,0,0,0,0,0,136,89,1,128,1,0,0,0,10,0,0,0,0,0,0,0,144,89,1,128,1,0,0,0,11,0,0,0,0,0,0,0,152,89,1,128,1,0,0,0,12,0,
      0,0,0,0,0,0,160,89,1,128,1,0,0,0,13,0,0,0,0,0,0,0,168,89,1,128,1,0,0,0,14,0,0,0,0,0,0,0,176,89,1,128,1,0,0,0,15,0,0,0,0,0,0,0,184,89,1,128,1,0,0,0,16,0,0,0,0,0,0,0,192,89,1,128,1,0,0,0,17,0,0,0,0,0,0,0,200,89,1,128,1,0,0,0,18,0,0,0,0,0,0,0,208,
      89,1,128,1,0,0,0,19,0,0,0,0,0,0,0,216,89,1,128,1,0,0,0,20,0,0,0,0,0,0,0,224,89,1,128,1,0,0,0,21,0,0,0,0,0,0,0,232,89,1,128,1,0,0,0,22,0,0,0,0,0,0,0,240,89,1,128,1,0,0,0,24,0,0,0,0,0,0,0,248,89,1,128,1,0,0,0,25,0,0,0,0,0,0,0,0,90,1,128,1,0,0,
      0,26,0,0,0,0,0,0,0,8,90,1,128,1,0,0,0,27,0,0,0,0,0,0,0,16,90,1,128,1,0,0,0,28,0,0,0,0,0,0,0,24,90,1,128,1,0,0,0,29,0,0,0,0,0,0,0,32,90,1,128,1,0,0,0,30,0,0,0,0,0,0,0,40,90,1,128,1,0,0,0,31,0,0,0,0,0,0,0,48,90,1,128,1,0,0,0,32,0,0,0,0,0,0,0,56,
      90,1,128,1,0,0,0,33,0,0,0,0,0,0,0,64,90,1,128,1,0,0,0,34,0,0,0,0,0,0,0,72,90,1,128,1,0,0,0,35,0,0,0,0,0,0,0,80,90,1,128,1,0,0,0,36,0,0,0,0,0,0,0,88,90,1,128,1,0,0,0,37,0,0,0,0,0,0,0,96,90,1,128,1,0,0,0,38,0,0,0,0,0,0,0,104,90,1,128,1,0,0,0,39,
      0,0,0,0,0,0,0,112,90,1,128,1,0,0,0,41,0,0,0,0,0,0,0,120,90,1,128,1,0,0,0,42,0,0,0,0,0,0,0,128,90,1,128,1,0,0,0,43,0,0,0,0,0,0,0,136,90,1,128,1,0,0,0,44,0,0,0,0,0,0,0,144,90,1,128,1,0,0,0,45,0,0,0,0,0,0,0,152,90,1,128,1,0,0,0,47,0,0,0,0,0,0,0,
      160,90,1,128,1,0,0,0,54,0,0,0,0,0,0,0,168,90,1,128,1,0,0,0,55,0,0,0,0,0,0,0,176,90,1,128,1,0,0,0,56,0,0,0,0,0,0,0,184,90,1,128,1,0,0,0,57,0,0,0,0,0,0,0,192,90,1,128,1,0,0,0,62,0,0,0,0,0,0,0,200,90,1,128,1,0,0,0,63,0,0,0,0,0,0,0,208,90,1,128,
      1,0,0,0,64,0,0,0,0,0,0,0,216,90,1,128,1,0,0,0,65,0,0,0,0,0,0,0,224,90,1,128,1,0,0,0,67,0,0,0,0,0,0,0,232,90,1,128,1,0,0,0,68,0,0,0,0,0,0,0,240,90,1,128,1,0,0,0,70,0,0,0,0,0,0,0,248,90,1,128,1,0,0,0,71,0,0,0,0,0,0,0,0,91,1,128,1,0,0,0,73,0,0,
      0,0,0,0,0,8,91,1,128,1,0,0,0,74,0,0,0,0,0,0,0,16,91,1,128,1,0,0,0,75,0,0,0,0,0,0,0,24,91,1,128,1,0,0,0,78,0,0,0,0,0,0,0,32,91,1,128,1,0,0,0,79,0,0,0,0,0,0,0,40,91,1,128,1,0,0,0,80,0,0,0,0,0,0,0,48,91,1,128,1,0,0,0,86,0,0,0,0,0,0,0,56,91,1,128,
      1,0,0,0,87,0,0,0,0,0,0,0,64,91,1,128,1,0,0,0,90,0,0,0,0,0,0,0,72,91,1,128,1,0,0,0,101,0,0,0,0,0,0,0,80,91,1,128,1,0,0,0,127,0,0,0,0,0,0,0,88,91,1,128,1,0,0,0,1,4,0,0,0,0,0,0,96,91,1,128,1,0,0,0,2,4,0,0,0,0,0,0,112,91,1,128,1,0,0,0,3,4,0,0,0,
      0,0,0,128,91,1,128,1,0,0,0,4,4,0,0,0,0,0,0,160,58,1,128,1,0,0,0,5,4,0,0,0,0,0,0,144,91,1,128,1,0,0,0,6,4,0,0,0,0,0,0,160,91,1,128,1,0,0,0,7,4,0,0,0,0,0,0,176,91,1,128,1,0,0,0,8,4,0,0,0,0,0,0,192,91,1,128,1,0,0,0,9,4,0,0,0,0,0,0,32,65,1,128,1,
      0,0,0,11,4,0,0,0,0,0,0,208,91,1,128,1,0,0,0,12,4,0,0,0,0,0,0,224,91,1,128,1,0,0,0,13,4,0,0,0,0,0,0,240,91,1,128,1,0,0,0,14,4,0,0,0,0,0,0,0,92,1,128,1,0,0,0,15,4,0,0,0,0,0,0,16,92,1,128,1,0,0,0,16,4,0,0,0,0,0,0,32,92,1,128,1,0,0,0,17,4,0,0,0,
      0,0,0,112,58,1,128,1,0,0,0,18,4,0,0,0,0,0,0,144,58,1,128,1,0,0,0,19,4,0,0,0,0,0,0,48,92,1,128,1,0,0,0,20,4,0,0,0,0,0,0,64,92,1,128,1,0,0,0,21,4,0,0,0,0,0,0,80,92,1,128,1,0,0,0,22,4,0,0,0,0,0,0,96,92,1,128,1,0,0,0,24,4,0,0,0,0,0,0,112,92,1,128,
      1,0,0,0,25,4,0,0,0,0,0,0,128,92,1,128,1,0,0,0,26,4,0,0,0,0,0,0,144,92,1,128,1,0,0,0,27,4,0,0,0,0,0,0,160,92,1,128,1,0,0,0,28,4,0,0,0,0,0,0,176,92,1,128,1,0,0,0,29,4,0,0,0,0,0,0,192,92,1,128,1,0,0,0,30,4,0,0,0,0,0,0,208,92,1,128,1,0,0,0,31,4,
      0,0,0,0,0,0,224,92,1,128,1,0,0,0,32,4,0,0,0,0,0,0,240,92,1,128,1,0,0,0,33,4,0,0,0,0,0,0,0,93,1,128,1,0,0,0,34,4,0,0,0,0,0,0,16,93,1,128,1,0,0,0,35,4,0,0,0,0,0,0,32,93,1,128,1,0,0,0,36,4,0,0,0,0,0,0,48,93,1,128,1,0,0,0,37,4,0,0,0,0,0,0,64,93,
      1,128,1,0,0,0,38,4,0,0,0,0,0,0,80,93,1,128,1,0,0,0,39,4,0,0,0,0,0,0,96,93,1,128,1,0,0,0,41,4,0,0,0,0,0,0,112,93,1,128,1,0,0,0,42,4,0,0,0,0,0,0,128,93,1,128,1,0,0,0,43,4,0,0,0,0,0,0,144,93,1,128,1,0,0,0,44,4,0,0,0,0,0,0,160,93,1,128,1,0,0,0,45,
      4,0,0,0,0,0,0,184,93,1,128,1,0,0,0,47,4,0,0,0,0,0,0,200,93,1,128,1,0,0,0,50,4,0,0,0,0,0,0,216,93,1,128,1,0,0,0,52,4,0,0,0,0,0,0,232,93,1,128,1,0,0,0,53,4,0,0,0,0,0,0,248,93,1,128,1,0,0,0,54,4,0,0,0,0,0,0,8,94,1,128,1,0,0,0,55,4,0,0,0,0,0,0,24,
      94,1,128,1,0,0,0,56,4,0,0,0,0,0,0,40,94,1,128,1,0,0,0,57,4,0,0,0,0,0,0,56,94,1,128,1,0,0,0,58,4,0,0,0,0,0,0,72,94,1,128,1,0,0,0,59,4,0,0,0,0,0,0,88,94,1,128,1,0,0,0,62,4,0,0,0,0,0,0,104,94,1,128,1,0,0,0,63,4,0,0,0,0,0,0,120,94,1,128,1,0,0,0,
      64,4,0,0,0,0,0,0,136,94,1,128,1,0,0,0,65,4,0,0,0,0,0,0,152,94,1,128,1,0,0,0,67,4,0,0,0,0,0,0,168,94,1,128,1,0,0,0,68,4,0,0,0,0,0,0,192,94,1,128,1,0,0,0,69,4,0,0,0,0,0,0,208,94,1,128,1,0,0,0,70,4,0,0,0,0,0,0,224,94,1,128,1,0,0,0,71,4,0,0,0,0,
      0,0,240,94,1,128,1,0,0,0,73,4,0,0,0,0,0,0,0,95,1,128,1,0,0,0,74,4,0,0,0,0,0,0,16,95,1,128,1,0,0,0,75,4,0,0,0,0,0,0,32,95,1,128,1,0,0,0,76,4,0,0,0,0,0,0,48,95,1,128,1,0,0,0,78,4,0,0,0,0,0,0,64,95,1,128,1,0,0,0,79,4,0,0,0,0,0,0,80,95,1,128,1,0,
      0,0,80,4,0,0,0,0,0,0,96,95,1,128,1,0,0,0,82,4,0,0,0,0,0,0,112,95,1,128,1,0,0,0,86,4,0,0,0,0,0,0,128,95,1,128,1,0,0,0,87,4,0,0,0,0,0,0,144,95,1,128,1,0,0,0,90,4,0,0,0,0,0,0,160,95,1,128,1,0,0,0,101,4,0,0,0,0,0,0,176,95,1,128,1,0,0,0,107,4,0,0,
      0,0,0,0,192,95,1,128,1,0,0,0,108,4,0,0,0,0,0,0,208,95,1,128,1,0,0,0,129,4,0,0,0,0,0,0,224,95,1,128,1,0,0,0,1,8,0,0,0,0,0,0,240,95,1,128,1,0,0,0,4,8,0,0,0,0,0,0,128,58,1,128,1,0,0,0,7,8,0,0,0,0,0,0,0,96,1,128,1,0,0,0,9,8,0,0,0,0,0,0,16,96,1,128,
      1,0,0,0,10,8,0,0,0,0,0,0,32,96,1,128,1,0,0,0,12,8,0,0,0,0,0,0,48,96,1,128,1,0,0,0,16,8,0,0,0,0,0,0,64,96,1,128,1,0,0,0,19,8,0,0,0,0,0,0,80,96,1,128,1,0,0,0,20,8,0,0,0,0,0,0,96,96,1,128,1,0,0,0,22,8,0,0,0,0,0,0,112,96,1,128,1,0,0,0,26,8,0,0,0,
      0,0,0,128,96,1,128,1,0,0,0,29,8,0,0,0,0,0,0,152,96,1,128,1,0,0,0,44,8,0,0,0,0,0,0,168,96,1,128,1,0,0,0,59,8,0,0,0,0,0,0,192,96,1,128,1,0,0,0,62,8,0,0,0,0,0,0,208,96,1,128,1,0,0,0,67,8,0,0,0,0,0,0,224,96,1,128,1,0,0,0,107,8,0,0,0,0,0,0,248,96,
      1,128,1,0,0,0,1,12,0,0,0,0,0,0,8,97,1,128,1,0,0,0,4,12,0,0,0,0,0,0,24,97,1,128,1,0,0,0,7,12,0,0,0,0,0,0,40,97,1,128,1,0,0,0,9,12,0,0,0,0,0,0,56,97,1,128,1,0,0,0,10,12,0,0,0,0,0,0,72,97,1,128,1,0,0,0,12,12,0,0,0,0,0,0,88,97,1,128,1,0,0,0,26,12,
      0,0,0,0,0,0,104,97,1,128,1,0,0,0,59,12,0,0,0,0,0,0,128,97,1,128,1,0,0,0,107,12,0,0,0,0,0,0,144,97,1,128,1,0,0,0,1,16,0,0,0,0,0,0,160,97,1,128,1,0,0,0,4,16,0,0,0,0,0,0,176,97,1,128,1,0,0,0,7,16,0,0,0,0,0,0,192,97,1,128,1,0,0,0,9,16,0,0,0,0,0,
      0,208,97,1,128,1,0,0,0,10,16,0,0,0,0,0,0,224,97,1,128,1,0,0,0,12,16,0,0,0,0,0,0,240,97,1,128,1,0,0,0,26,16,0,0,0,0,0,0,0,98,1,128,1,0,0,0,59,16,0,0,0,0,0,0,16,98,1,128,1,0,0,0,1,20,0,0,0,0,0,0,32,98,1,128,1,0,0,0,4,20,0,0,0,0,0,0,48,98,1,128,
      1,0,0,0,7,20,0,0,0,0,0,0,64,98,1,128,1,0,0,0,9,20,0,0,0,0,0,0,80,98,1,128,1,0,0,0,10,20,0,0,0,0,0,0,96,98,1,128,1,0,0,0,12,20,0,0,0,0,0,0,112,98,1,128,1,0,0,0,26,20,0,0,0,0,0,0,128,98,1,128,1,0,0,0,59,20,0,0,0,0,0,0,152,98,1,128,1,0,0,0,1,24,
      0,0,0,0,0,0,168,98,1,128,1,0,0,0,9,24,0,0,0,0,0,0,184,98,1,128,1,0,0,0,10,24,0,0,0,0,0,0,200,98,1,128,1,0,0,0,12,24,0,0,0,0,0,0,216,98,1,128,1,0,0,0,26,24,0,0,0,0,0,0,232,98,1,128,1,0,0,0,59,24,0,0,0,0,0,0,0,99,1,128,1,0,0,0,1,28,0,0,0,0,0,0,
      16,99,1,128,1,0,0,0,9,28,0,0,0,0,0,0,32,99,1,128,1,0,0,0,10,28,0,0,0,0,0,0,48,99,1,128,1,0,0,0,26,28,0,0,0,0,0,0,64,99,1,128,1,0,0,0,59,28,0,0,0,0,0,0,88,99,1,128,1,0,0,0,1,32,0,0,0,0,0,0,104,99,1,128,1,0,0,0,9,32,0,0,0,0,0,0,120,99,1,128,1,
      0,0,0,10,32,0,0,0,0,0,0,136,99,1,128,1,0,0,0,59,32,0,0,0,0,0,0,152,99,1,128,1,0,0,0,1,36,0,0,0,0,0,0,168,99,1,128,1,0,0,0,9,36,0,0,0,0,0,0,184,99,1,128,1,0,0,0,10,36,0,0,0,0,0,0,200,99,1,128,1,0,0,0,59,36,0,0,0,0,0,0,216,99,1,128,1,0,0,0,1,40,
      0,0,0,0,0,0,232,99,1,128,1,0,0,0,9,40,0,0,0,0,0,0,248,99,1,128,1,0,0,0,10,40,0,0,0,0,0,0,8,100,1,128,1,0,0,0,1,44,0,0,0,0,0,0,24,100,1,128,1,0,0,0,9,44,0,0,0,0,0,0,40,100,1,128,1,0,0,0,10,44,0,0,0,0,0,0,56,100,1,128,1,0,0,0,1,48,0,0,0,0,0,0,
      72,100,1,128,1,0,0,0,9,48,0,0,0,0,0,0,88,100,1,128,1,0,0,0,10,48,0,0,0,0,0,0,104,100,1,128,1,0,0,0,1,52,0,0,0,0,0,0,120,100,1,128,1,0,0,0,9,52,0,0,0,0,0,0,136,100,1,128,1,0,0,0,10,52,0,0,0,0,0,0,152,100,1,128,1,0,0,0,1,56,0,0,0,0,0,0,168,100,
      1,128,1,0,0,0,10,56,0,0,0,0,0,0,184,100,1,128,1,0,0,0,1,60,0,0,0,0,0,0,200,100,1,128,1,0,0,0,10,60,0,0,0,0,0,0,216,100,1,128,1,0,0,0,1,64,0,0,0,0,0,0,232,100,1,128,1,0,0,0,10,64,0,0,0,0,0,0,248,100,1,128,1,0,0,0,10,68,0,0,0,0,0,0,8,101,1,128,
      1,0,0,0,10,72,0,0,0,0,0,0,24,101,1,128,1,0,0,0,10,76,0,0,0,0,0,0,40,101,1,128,1,0,0,0,10,80,0,0,0,0,0,0,56,101,1,128,1,0,0,0,4,124,0,0,0,0,0,0,72,101,1,128,1,0,0,0,26,124,0,0,0,0,0,0,88,101,1,128,1,0,0,0,97,0,114,0,0,0,0,0,98,0,103,0,0,0,0,0,
      99,0,97,0,0,0,0,0,122,0,104,0,45,0,67,0,72,0,83,0,0,0,0,0,99,0,115,0,0,0,0,0,100,0,97,0,0,0,0,0,100,0,101,0,0,0,0,0,101,0,108,0,0,0,0,0,101,0,110,0,0,0,0,0,101,0,115,0,0,0,0,0,102,0,105,0,0,0,0,0,102,0,114,0,0,0,0,0,104,0,101,0,0,0,0,0,104,0,
      117,0,0,0,0,0,105,0,115,0,0,0,0,0,105,0,116,0,0,0,0,0,106,0,97,0,0,0,0,0,107,0,111,0,0,0,0,0,110,0,108,0,0,0,0,0,110,0,111,0,0,0,0,0,112,0,108,0,0,0,0,0,112,0,116,0,0,0,0,0,114,0,111,0,0,0,0,0,114,0,117,0,0,0,0,0,104,0,114,0,0,0,0,0,115,0,107,
      0,0,0,0,0,115,0,113,0,0,0,0,0,115,0,118,0,0,0,0,0,116,0,104,0,0,0,0,0,116,0,114,0,0,0,0,0,117,0,114,0,0,0,0,0,105,0,100,0,0,0,0,0,117,0,107,0,0,0,0,0,98,0,101,0,0,0,0,0,115,0,108,0,0,0,0,0,101,0,116,0,0,0,0,0,108,0,118,0,0,0,0,0,108,0,116,0,
      0,0,0,0,102,0,97,0,0,0,0,0,118,0,105,0,0,0,0,0,104,0,121,0,0,0,0,0,97,0,122,0,0,0,0,0,101,0,117,0,0,0,0,0,109,0,107,0,0,0,0,0,97,0,102,0,0,0,0,0,107,0,97,0,0,0,0,0,102,0,111,0,0,0,0,0,104,0,105,0,0,0,0,0,109,0,115,0,0,0,0,0,107,0,107,0,0,0,0,
      0,107,0,121,0,0,0,0,0,115,0,119,0,0,0,0,0,117,0,122,0,0,0,0,0,116,0,116,0,0,0,0,0,112,0,97,0,0,0,0,0,103,0,117,0,0,0,0,0,116,0,97,0,0,0,0,0,116,0,101,0,0,0,0,0,107,0,110,0,0,0,0,0,109,0,114,0,0,0,0,0,115,0,97,0,0,0,0,0,109,0,110,0,0,0,0,0,103,
      0,108,0,0,0,0,0,107,0,111,0,107,0,0,0,115,0,121,0,114,0,0,0,100,0,105,0,118,0,0,0,0,0,0,0,0,0,0,0,97,0,114,0,45,0,83,0,65,0,0,0,0,0,0,0,98,0,103,0,45,0,66,0,71,0,0,0,0,0,0,0,99,0,97,0,45,0,69,0,83,0,0,0,0,0,0,0,99,0,115,0,45,0,67,0,90,0,0,0,
      0,0,0,0,100,0,97,0,45,0,68,0,75,0,0,0,0,0,0,0,100,0,101,0,45,0,68,0,69,0,0,0,0,0,0,0,101,0,108,0,45,0,71,0,82,0,0,0,0,0,0,0,102,0,105,0,45,0,70,0,73,0,0,0,0,0,0,0,102,0,114,0,45,0,70,0,82,0,0,0,0,0,0,0,104,0,101,0,45,0,73,0,76,0,0,0,0,0,0,0,
      104,0,117,0,45,0,72,0,85,0,0,0,0,0,0,0,105,0,115,0,45,0,73,0,83,0,0,0,0,0,0,0,105,0,116,0,45,0,73,0,84,0,0,0,0,0,0,0,110,0,108,0,45,0,78,0,76,0,0,0,0,0,0,0,110,0,98,0,45,0,78,0,79,0,0,0,0,0,0,0,112,0,108,0,45,0,80,0,76,0,0,0,0,0,0,0,112,0,116,
      0,45,0,66,0,82,0,0,0,0,0,0,0,114,0,111,0,45,0,82,0,79,0,0,0,0,0,0,0,114,0,117,0,45,0,82,0,85,0,0,0,0,0,0,0,104,0,114,0,45,0,72,0,82,0,0,0,0,0,0,0,115,0,107,0,45,0,83,0,75,0,0,0,0,0,0,0,115,0,113,0,45,0,65,0,76,0,0,0,0,0,0,0,115,0,118,0,45,0,
      83,0,69,0,0,0,0,0,0,0,116,0,104,0,45,0,84,0,72,0,0,0,0,0,0,0,116,0,114,0,45,0,84,0,82,0,0,0,0,0,0,0,117,0,114,0,45,0,80,0,75,0,0,0,0,0,0,0,105,0,100,0,45,0,73,0,68,0,0,0,0,0,0,0,117,0,107,0,45,0,85,0,65,0,0,0,0,0,0,0,98,0,101,0,45,0,66,0,89,
      0,0,0,0,0,0,0,115,0,108,0,45,0,83,0,73,0,0,0,0,0,0,0,101,0,116,0,45,0,69,0,69,0,0,0,0,0,0,0,108,0,118,0,45,0,76,0,86,0,0,0,0,0,0,0,108,0,116,0,45,0,76,0,84,0,0,0,0,0,0,0,102,0,97,0,45,0,73,0,82,0,0,0,0,0,0,0,118,0,105,0,45,0,86,0,78,0,0,0,0,
      0,0,0,104,0,121,0,45,0,65,0,77,0,0,0,0,0,0,0,97,0,122,0,45,0,65,0,90,0,45,0,76,0,97,0,116,0,110,0,0,0,0,0,101,0,117,0,45,0,69,0,83,0,0,0,0,0,0,0,109,0,107,0,45,0,77,0,75,0,0,0,0,0,0,0,116,0,110,0,45,0,90,0,65,0,0,0,0,0,0,0,120,0,104,0,45,0,90,
      0,65,0,0,0,0,0,0,0,122,0,117,0,45,0,90,0,65,0,0,0,0,0,0,0,97,0,102,0,45,0,90,0,65,0,0,0,0,0,0,0,107,0,97,0,45,0,71,0,69,0,0,0,0,0,0,0,102,0,111,0,45,0,70,0,79,0,0,0,0,0,0,0,104,0,105,0,45,0,73,0,78,0,0,0,0,0,0,0,109,0,116,0,45,0,77,0,84,0,0,
      0,0,0,0,0,115,0,101,0,45,0,78,0,79,0,0,0,0,0,0,0,109,0,115,0,45,0,77,0,89,0,0,0,0,0,0,0,107,0,107,0,45,0,75,0,90,0,0,0,0,0,0,0,107,0,121,0,45,0,75,0,71,0,0,0,0,0,0,0,115,0,119,0,45,0,75,0,69,0,0,0,0,0,0,0,117,0,122,0,45,0,85,0,90,0,45,0,76,0,
      97,0,116,0,110,0,0,0,0,0,116,0,116,0,45,0,82,0,85,0,0,0,0,0,0,0,98,0,110,0,45,0,73,0,78,0,0,0,0,0,0,0,112,0,97,0,45,0,73,0,78,0,0,0,0,0,0,0,103,0,117,0,45,0,73,0,78,0,0,0,0,0,0,0,116,0,97,0,45,0,73,0,78,0,0,0,0,0,0,0,116,0,101,0,45,0,73,0,78,
      0,0,0,0,0,0,0,107,0,110,0,45,0,73,0,78,0,0,0,0,0,0,0,109,0,108,0,45,0,73,0,78,0,0,0,0,0,0,0,109,0,114,0,45,0,73,0,78,0,0,0,0,0,0,0,115,0,97,0,45,0,73,0,78,0,0,0,0,0,0,0,109,0,110,0,45,0,77,0,78,0,0,0,0,0,0,0,99,0,121,0,45,0,71,0,66,0,0,0,0,0,
      0,0,103,0,108,0,45,0,69,0,83,0,0,0,0,0,0,0,107,0,111,0,107,0,45,0,73,0,78,0,0,0,0,0,115,0,121,0,114,0,45,0,83,0,89,0,0,0,0,0,100,0,105,0,118,0,45,0,77,0,86,0,0,0,0,0,113,0,117,0,122,0,45,0,66,0,79,0,0,0,0,0,110,0,115,0,45,0,90,0,65,0,0,0,0,0,
      0,0,109,0,105,0,45,0,78,0,90,0,0,0,0,0,0,0,97,0,114,0,45,0,73,0,81,0,0,0,0,0,0,0,100,0,101,0,45,0,67,0,72,0,0,0,0,0,0,0,101,0,110,0,45,0,71,0,66,0,0,0,0,0,0,0,101,0,115,0,45,0,77,0,88,0,0,0,0,0,0,0,102,0,114,0,45,0,66,0,69,0,0,0,0,0,0,0,105,
      0,116,0,45,0,67,0,72,0,0,0,0,0,0,0,110,0,108,0,45,0,66,0,69,0,0,0,0,0,0,0,110,0,110,0,45,0,78,0,79,0,0,0,0,0,0,0,112,0,116,0,45,0,80,0,84,0,0,0,0,0,0,0,115,0,114,0,45,0,83,0,80,0,45,0,76,0,97,0,116,0,110,0,0,0,0,0,115,0,118,0,45,0,70,0,73,0,
      0,0,0,0,0,0,97,0,122,0,45,0,65,0,90,0,45,0,67,0,121,0,114,0,108,0,0,0,0,0,115,0,101,0,45,0,83,0,69,0,0,0,0,0,0,0,109,0,115,0,45,0,66,0,78,0,0,0,0,0,0,0,117,0,122,0,45,0,85,0,90,0,45,0,67,0,121,0,114,0,108,0,0,0,0,0,113,0,117,0,122,0,45,0,69,
      0,67,0,0,0,0,0,97,0,114,0,45,0,69,0,71,0,0,0,0,0,0,0,122,0,104,0,45,0,72,0,75,0,0,0,0,0,0,0,100,0,101,0,45,0,65,0,84,0,0,0,0,0,0,0,101,0,110,0,45,0,65,0,85,0,0,0,0,0,0,0,101,0,115,0,45,0,69,0,83,0,0,0,0,0,0,0,102,0,114,0,45,0,67,0,65,0,0,0,0,
      0,0,0,115,0,114,0,45,0,83,0,80,0,45,0,67,0,121,0,114,0,108,0,0,0,0,0,115,0,101,0,45,0,70,0,73,0,0,0,0,0,0,0,113,0,117,0,122,0,45,0,80,0,69,0,0,0,0,0,97,0,114,0,45,0,76,0,89,0,0,0,0,0,0,0,122,0,104,0,45,0,83,0,71,0,0,0,0,0,0,0,100,0,101,0,45,
      0,76,0,85,0,0,0,0,0,0,0,101,0,110,0,45,0,67,0,65,0,0,0,0,0,0,0,101,0,115,0,45,0,71,0,84,0,0,0,0,0,0,0,102,0,114,0,45,0,67,0,72,0,0,0,0,0,0,0,104,0,114,0,45,0,66,0,65,0,0,0,0,0,0,0,115,0,109,0,106,0,45,0,78,0,79,0,0,0,0,0,97,0,114,0,45,0,68,0,
      90,0,0,0,0,0,0,0,122,0,104,0,45,0,77,0,79,0,0,0,0,0,0,0,100,0,101,0,45,0,76,0,73,0,0,0,0,0,0,0,101,0,110,0,45,0,78,0,90,0,0,0,0,0,0,0,101,0,115,0,45,0,67,0,82,0,0,0,0,0,0,0,102,0,114,0,45,0,76,0,85,0,0,0,0,0,0,0,98,0,115,0,45,0,66,0,65,0,45,
      0,76,0,97,0,116,0,110,0,0,0,0,0,115,0,109,0,106,0,45,0,83,0,69,0,0,0,0,0,97,0,114,0,45,0,77,0,65,0,0,0,0,0,0,0,101,0,110,0,45,0,73,0,69,0,0,0,0,0,0,0,101,0,115,0,45,0,80,0,65,0,0,0,0,0,0,0,102,0,114,0,45,0,77,0,67,0,0,0,0,0,0,0,115,0,114,0,45,
      0,66,0,65,0,45,0,76,0,97,0,116,0,110,0,0,0,0,0,115,0,109,0,97,0,45,0,78,0,79,0,0,0,0,0,97,0,114,0,45,0,84,0,78,0,0,0,0,0,0,0,101,0,110,0,45,0,90,0,65,0,0,0,0,0,0,0,101,0,115,0,45,0,68,0,79,0,0,0,0,0,0,0,115,0,114,0,45,0,66,0,65,0,45,0,67,0,121,
      0,114,0,108,0,0,0,0,0,115,0,109,0,97,0,45,0,83,0,69,0,0,0,0,0,97,0,114,0,45,0,79,0,77,0,0,0,0,0,0,0,101,0,110,0,45,0,74,0,77,0,0,0,0,0,0,0,101,0,115,0,45,0,86,0,69,0,0,0,0,0,0,0,115,0,109,0,115,0,45,0,70,0,73,0,0,0,0,0,97,0,114,0,45,0,89,0,69,
      0,0,0,0,0,0,0,101,0,110,0,45,0,67,0,66,0,0,0,0,0,0,0,101,0,115,0,45,0,67,0,79,0,0,0,0,0,0,0,115,0,109,0,110,0,45,0,70,0,73,0,0,0,0,0,97,0,114,0,45,0,83,0,89,0,0,0,0,0,0,0,101,0,110,0,45,0,66,0,90,0,0,0,0,0,0,0,101,0,115,0,45,0,80,0,69,0,0,0,
      0,0,0,0,97,0,114,0,45,0,74,0,79,0,0,0,0,0,0,0,101,0,110,0,45,0,84,0,84,0,0,0,0,0,0,0,101,0,115,0,45,0,65,0,82,0,0,0,0,0,0,0,97,0,114,0,45,0,76,0,66,0,0,0,0,0,0,0,101,0,110,0,45,0,90,0,87,0,0,0,0,0,0,0,101,0,115,0,45,0,69,0,67,0,0,0,0,0,0,0,97,
      0,114,0,45,0,75,0,87,0,0,0,0,0,0,0,101,0,110,0,45,0,80,0,72,0,0,0,0,0,0,0,101,0,115,0,45,0,67,0,76,0,0,0,0,0,0,0,97,0,114,0,45,0,65,0,69,0,0,0,0,0,0,0,101,0,115,0,45,0,85,0,89,0,0,0,0,0,0,0,97,0,114,0,45,0,66,0,72,0,0,0,0,0,0,0,101,0,115,0,45,
      0,80,0,89,0,0,0,0,0,0,0,97,0,114,0,45,0,81,0,65,0,0,0,0,0,0,0,101,0,115,0,45,0,66,0,79,0,0,0,0,0,0,0,101,0,115,0,45,0,83,0,86,0,0,0,0,0,0,0,101,0,115,0,45,0,72,0,78,0,0,0,0,0,0,0,101,0,115,0,45,0,78,0,73,0,0,0,0,0,0,0,101,0,115,0,45,0,80,0,82,
      0,0,0,0,0,0,0,122,0,104,0,45,0,67,0,72,0,84,0,0,0,0,0,115,0,114,0,0,0,0,0,88,91,1,128,1,0,0,0,66,0,0,0,0,0,0,0,168,90,1,128,1,0,0,0,44,0,0,0,0,0,0,0,160,115,1,128,1,0,0,0,113,0,0,0,0,0,0,0,64,89,1,128,1,0,0,0,0,0,0,0,0,0,0,0,176,115,1,128,1,
      0,0,0,216,0,0,0,0,0,0,0,192,115,1,128,1,0,0,0,218,0,0,0,0,0,0,0,208,115,1,128,1,0,0,0,177,0,0,0,0,0,0,0,224,115,1,128,1,0,0,0,160,0,0,0,0,0,0,0,240,115,1,128,1,0,0,0,143,0,0,0,0,0,0,0,0,116,1,128,1,0,0,0,207,0,0,0,0,0,0,0,16,116,1,128,1,0,0,
      0,213,0,0,0,0,0,0,0,32,116,1,128,1,0,0,0,210,0,0,0,0,0,0,0,48,116,1,128,1,0,0,0,169,0,0,0,0,0,0,0,64,116,1,128,1,0,0,0,185,0,0,0,0,0,0,0,80,116,1,128,1,0,0,0,196,0,0,0,0,0,0,0,96,116,1,128,1,0,0,0,220,0,0,0,0,0,0,0,112,116,1,128,1,0,0,0,67,0,
      0,0,0,0,0,0,128,116,1,128,1,0,0,0,204,0,0,0,0,0,0,0,144,116,1,128,1,0,0,0,191,0,0,0,0,0,0,0,160,116,1,128,1,0,0,0,200,0,0,0,0,0,0,0,144,90,1,128,1,0,0,0,41,0,0,0,0,0,0,0,176,116,1,128,1,0,0,0,155,0,0,0,0,0,0,0,200,116,1,128,1,0,0,0,107,0,0,0,
      0,0,0,0,80,90,1,128,1,0,0,0,33,0,0,0,0,0,0,0,224,116,1,128,1,0,0,0,99,0,0,0,0,0,0,0,72,89,1,128,1,0,0,0,1,0,0,0,0,0,0,0,240,116,1,128,1,0,0,0,68,0,0,0,0,0,0,0,0,117,1,128,1,0,0,0,125,0,0,0,0,0,0,0,16,117,1,128,1,0,0,0,183,0,0,0,0,0,0,0,80,89,
      1,128,1,0,0,0,2,0,0,0,0,0,0,0,40,117,1,128,1,0,0,0,69,0,0,0,0,0,0,0,104,89,1,128,1,0,0,0,4,0,0,0,0,0,0,0,56,117,1,128,1,0,0,0,71,0,0,0,0,0,0,0,72,117,1,128,1,0,0,0,135,0,0,0,0,0,0,0,112,89,1,128,1,0,0,0,5,0,0,0,0,0,0,0,88,117,1,128,1,0,0,0,72,
      0,0,0,0,0,0,0,120,89,1,128,1,0,0,0,6,0,0,0,0,0,0,0,104,117,1,128,1,0,0,0,162,0,0,0,0,0,0,0,120,117,1,128,1,0,0,0,145,0,0,0,0,0,0,0,136,117,1,128,1,0,0,0,73,0,0,0,0,0,0,0,152,117,1,128,1,0,0,0,179,0,0,0,0,0,0,0,168,117,1,128,1,0,0,0,171,0,0,0,
      0,0,0,0,80,91,1,128,1,0,0,0,65,0,0,0,0,0,0,0,184,117,1,128,1,0,0,0,139,0,0,0,0,0,0,0,128,89,1,128,1,0,0,0,7,0,0,0,0,0,0,0,200,117,1,128,1,0,0,0,74,0,0,0,0,0,0,0,136,89,1,128,1,0,0,0,8,0,0,0,0,0,0,0,216,117,1,128,1,0,0,0,163,0,0,0,0,0,0,0,232,
      117,1,128,1,0,0,0,205,0,0,0,0,0,0,0,248,117,1,128,1,0,0,0,172,0,0,0,0,0,0,0,8,118,1,128,1,0,0,0,201,0,0,0,0,0,0,0,24,118,1,128,1,0,0,0,146,0,0,0,0,0,0,0,40,118,1,128,1,0,0,0,186,0,0,0,0,0,0,0,56,118,1,128,1,0,0,0,197,0,0,0,0,0,0,0,72,118,1,128,
      1,0,0,0,180,0,0,0,0,0,0,0,88,118,1,128,1,0,0,0,214,0,0,0,0,0,0,0,104,118,1,128,1,0,0,0,208,0,0,0,0,0,0,0,120,118,1,128,1,0,0,0,75,0,0,0,0,0,0,0,136,118,1,128,1,0,0,0,192,0,0,0,0,0,0,0,152,118,1,128,1,0,0,0,211,0,0,0,0,0,0,0,144,89,1,128,1,0,
      0,0,9,0,0,0,0,0,0,0,168,118,1,128,1,0,0,0,209,0,0,0,0,0,0,0,184,118,1,128,1,0,0,0,221,0,0,0,0,0,0,0,200,118,1,128,1,0,0,0,215,0,0,0,0,0,0,0,216,118,1,128,1,0,0,0,202,0,0,0,0,0,0,0,232,118,1,128,1,0,0,0,181,0,0,0,0,0,0,0,248,118,1,128,1,0,0,0,
      193,0,0,0,0,0,0,0,8,119,1,128,1,0,0,0,212,0,0,0,0,0,0,0,24,119,1,128,1,0,0,0,164,0,0,0,0,0,0,0,40,119,1,128,1,0,0,0,173,0,0,0,0,0,0,0,56,119,1,128,1,0,0,0,223,0,0,0,0,0,0,0,72,119,1,128,1,0,0,0,147,0,0,0,0,0,0,0,88,119,1,128,1,0,0,0,224,0,0,
      0,0,0,0,0,104,119,1,128,1,0,0,0,187,0,0,0,0,0,0,0,120,119,1,128,1,0,0,0,206,0,0,0,0,0,0,0,136,119,1,128,1,0,0,0,225,0,0,0,0,0,0,0,152,119,1,128,1,0,0,0,219,0,0,0,0,0,0,0,168,119,1,128,1,0,0,0,222,0,0,0,0,0,0,0,184,119,1,128,1,0,0,0,217,0,0,0,
      0,0,0,0,200,119,1,128,1,0,0,0,198,0,0,0,0,0,0,0,96,90,1,128,1,0,0,0,35,0,0,0,0,0,0,0,216,119,1,128,1,0,0,0,101,0,0,0,0,0,0,0,152,90,1,128,1,0,0,0,42,0,0,0,0,0,0,0,232,119,1,128,1,0,0,0,108,0,0,0,0,0,0,0,120,90,1,128,1,0,0,0,38,0,0,0,0,0,0,0,
      248,119,1,128,1,0,0,0,104,0,0,0,0,0,0,0,152,89,1,128,1,0,0,0,10,0,0,0,0,0,0,0,8,120,1,128,1,0,0,0,76,0,0,0,0,0,0,0,184,90,1,128,1,0,0,0,46,0,0,0,0,0,0,0,24,120,1,128,1,0,0,0,115,0,0,0,0,0,0,0,160,89,1,128,1,0,0,0,11,0,0,0,0,0,0,0,40,120,1,128,
      1,0,0,0,148,0,0,0,0,0,0,0,56,120,1,128,1,0,0,0,165,0,0,0,0,0,0,0,72,120,1,128,1,0,0,0,174,0,0,0,0,0,0,0,88,120,1,128,1,0,0,0,77,0,0,0,0,0,0,0,104,120,1,128,1,0,0,0,182,0,0,0,0,0,0,0,120,120,1,128,1,0,0,0,188,0,0,0,0,0,0,0,56,91,1,128,1,0,0,0,
      62,0,0,0,0,0,0,0,136,120,1,128,1,0,0,0,136,0,0,0,0,0,0,0,0,91,1,128,1,0,0,0,55,0,0,0,0,0,0,0,152,120,1,128,1,0,0,0,127,0,0,0,0,0,0,0,168,89,1,128,1,0,0,0,12,0,0,0,0,0,0,0,168,120,1,128,1,0,0,0,78,0,0,0,0,0,0,0,192,90,1,128,1,0,0,0,47,0,0,0,0,
      0,0,0,184,120,1,128,1,0,0,0,116,0,0,0,0,0,0,0,8,90,1,128,1,0,0,0,24,0,0,0,0,0,0,0,200,120,1,128,1,0,0,0,175,0,0,0,0,0,0,0,216,120,1,128,1,0,0,0,90,0,0,0,0,0,0,0,176,89,1,128,1,0,0,0,13,0,0,0,0,0,0,0,232,120,1,128,1,0,0,0,79,0,0,0,0,0,0,0,136,
      90,1,128,1,0,0,0,40,0,0,0,0,0,0,0,248,120,1,128,1,0,0,0,106,0,0,0,0,0,0,0,64,90,1,128,1,0,0,0,31,0,0,0,0,0,0,0,8,121,1,128,1,0,0,0,97,0,0,0,0,0,0,0,184,89,1,128,1,0,0,0,14,0,0,0,0,0,0,0,24,121,1,128,1,0,0,0,80,0,0,0,0,0,0,0,192,89,1,128,1,0,
      0,0,15,0,0,0,0,0,0,0,40,121,1,128,1,0,0,0,149,0,0,0,0,0,0,0,56,121,1,128,1,0,0,0,81,0,0,0,0,0,0,0,200,89,1,128,1,0,0,0,16,0,0,0,0,0,0,0,72,121,1,128,1,0,0,0,82,0,0,0,0,0,0,0,176,90,1,128,1,0,0,0,45,0,0,0,0,0,0,0,88,121,1,128,1,0,0,0,114,0,0,
      0,0,0,0,0,208,90,1,128,1,0,0,0,49,0,0,0,0,0,0,0,104,121,1,128,1,0,0,0,120,0,0,0,0,0,0,0,24,91,1,128,1,0,0,0,58,0,0,0,0,0,0,0,120,121,1,128,1,0,0,0,130,0,0,0,0,0,0,0,208,89,1,128,1,0,0,0,17,0,0,0,0,0,0,0,64,91,1,128,1,0,0,0,63,0,0,0,0,0,0,0,136,
      121,1,128,1,0,0,0,137,0,0,0,0,0,0,0,152,121,1,128,1,0,0,0,83,0,0,0,0,0,0,0,216,90,1,128,1,0,0,0,50,0,0,0,0,0,0,0,168,121,1,128,1,0,0,0,121,0,0,0,0,0,0,0,112,90,1,128,1,0,0,0,37,0,0,0,0,0,0,0,184,121,1,128,1,0,0,0,103,0,0,0,0,0,0,0,104,90,1,128,
      1,0,0,0,36,0,0,0,0,0,0,0,200,121,1,128,1,0,0,0,102,0,0,0,0,0,0,0,216,121,1,128,1,0,0,0,142,0,0,0,0,0,0,0,160,90,1,128,1,0,0,0,43,0,0,0,0,0,0,0,232,121,1,128,1,0,0,0,109,0,0,0,0,0,0,0,248,121,1,128,1,0,0,0,131,0,0,0,0,0,0,0,48,91,1,128,1,0,0,
      0,61,0,0,0,0,0,0,0,8,122,1,128,1,0,0,0,134,0,0,0,0,0,0,0,32,91,1,128,1,0,0,0,59,0,0,0,0,0,0,0,24,122,1,128,1,0,0,0,132,0,0,0,0,0,0,0,200,90,1,128,1,0,0,0,48,0,0,0,0,0,0,0,40,122,1,128,1,0,0,0,157,0,0,0,0,0,0,0,56,122,1,128,1,0,0,0,119,0,0,0,
      0,0,0,0,72,122,1,128,1,0,0,0,117,0,0,0,0,0,0,0,88,122,1,128,1,0,0,0,85,0,0,0,0,0,0,0,216,89,1,128,1,0,0,0,18,0,0,0,0,0,0,0,104,122,1,128,1,0,0,0,150,0,0,0,0,0,0,0,120,122,1,128,1,0,0,0,84,0,0,0,0,0,0,0,136,122,1,128,1,0,0,0,151,0,0,0,0,0,0,0,
      224,89,1,128,1,0,0,0,19,0,0,0,0,0,0,0,152,122,1,128,1,0,0,0,141,0,0,0,0,0,0,0,248,90,1,128,1,0,0,0,54,0,0,0,0,0,0,0,168,122,1,128,1,0,0,0,126,0,0,0,0,0,0,0,232,89,1,128,1,0,0,0,20,0,0,0,0,0,0,0,184,122,1,128,1,0,0,0,86,0,0,0,0,0,0,0,240,89,1,
      128,1,0,0,0,21,0,0,0,0,0,0,0,200,122,1,128,1,0,0,0,87,0,0,0,0,0,0,0,216,122,1,128,1,0,0,0,152,0,0,0,0,0,0,0,232,122,1,128,1,0,0,0,140,0,0,0,0,0,0,0,248,122,1,128,1,0,0,0,159,0,0,0,0,0,0,0,8,123,1,128,1,0,0,0,168,0,0,0,0,0,0,0,248,89,1,128,1,
      0,0,0,22,0,0,0,0,0,0,0,24,123,1,128,1,0,0,0,88,0,0,0,0,0,0,0,0,90,1,128,1,0,0,0,23,0,0,0,0,0,0,0,40,123,1,128,1,0,0,0,89,0,0,0,0,0,0,0,40,91,1,128,1,0,0,0,60,0,0,0,0,0,0,0,56,123,1,128,1,0,0,0,133,0,0,0,0,0,0,0,72,123,1,128,1,0,0,0,167,0,0,0,
      0,0,0,0,88,123,1,128,1,0,0,0,118,0,0,0,0,0,0,0,104,123,1,128,1,0,0,0,156,0,0,0,0,0,0,0,16,90,1,128,1,0,0,0,25,0,0,0,0,0,0,0,120,123,1,128,1,0,0,0,91,0,0,0,0,0,0,0,88,90,1,128,1,0,0,0,34,0,0,0,0,0,0,0,136,123,1,128,1,0,0,0,100,0,0,0,0,0,0,0,152,
      123,1,128,1,0,0,0,190,0,0,0,0,0,0,0,168,123,1,128,1,0,0,0,195,0,0,0,0,0,0,0,184,123,1,128,1,0,0,0,176,0,0,0,0,0,0,0,200,123,1,128,1,0,0,0,184,0,0,0,0,0,0,0,216,123,1,128,1,0,0,0,203,0,0,0,0,0,0,0,232,123,1,128,1,0,0,0,199,0,0,0,0,0,0,0,24,90,
      1,128,1,0,0,0,26,0,0,0,0,0,0,0,248,123,1,128,1,0,0,0,92,0,0,0,0,0,0,0,88,101,1,128,1,0,0,0,227,0,0,0,0,0,0,0,8,124,1,128,1,0,0,0,194,0,0,0,0,0,0,0,32,124,1,128,1,0,0,0,189,0,0,0,0,0,0,0,56,124,1,128,1,0,0,0,166,0,0,0,0,0,0,0,80,124,1,128,1,0,
      0,0,153,0,0,0,0,0,0,0,32,90,1,128,1,0,0,0,27,0,0,0,0,0,0,0,104,124,1,128,1,0,0,0,154,0,0,0,0,0,0,0,120,124,1,128,1,0,0,0,93,0,0,0,0,0,0,0,224,90,1,128,1,0,0,0,51,0,0,0,0,0,0,0,136,124,1,128,1,0,0,0,122,0,0,0,0,0,0,0,72,91,1,128,1,0,0,0,64,0,
      0,0,0,0,0,0,152,124,1,128,1,0,0,0,138,0,0,0,0,0,0,0,8,91,1,128,1,0,0,0,56,0,0,0,0,0,0,0,168,124,1,128,1,0,0,0,128,0,0,0,0,0,0,0,16,91,1,128,1,0,0,0,57,0,0,0,0,0,0,0,184,124,1,128,1,0,0,0,129,0,0,0,0,0,0,0,40,90,1,128,1,0,0,0,28,0,0,0,0,0,0,0,
      200,124,1,128,1,0,0,0,94,0,0,0,0,0,0,0,216,124,1,128,1,0,0,0,110,0,0,0,0,0,0,0,48,90,1,128,1,0,0,0,29,0,0,0,0,0,0,0,232,124,1,128,1,0,0,0,95,0,0,0,0,0,0,0,240,90,1,128,1,0,0,0,53,0,0,0,0,0,0,0,248,124,1,128,1,0,0,0,124,0,0,0,0,0,0,0,72,90,1,
      128,1,0,0,0,32,0,0,0,0,0,0,0,8,125,1,128,1,0,0,0,98,0,0,0,0,0,0,0,56,90,1,128,1,0,0,0,30,0,0,0,0,0,0,0,24,125,1,128,1,0,0,0,96,0,0,0,0,0,0,0,232,90,1,128,1,0,0,0,52,0,0,0,0,0,0,0,40,125,1,128,1,0,0,0,158,0,0,0,0,0,0,0,64,125,1,128,1,0,0,0,123,
      0,0,0,0,0,0,0,128,90,1,128,1,0,0,0,39,0,0,0,0,0,0,0,88,125,1,128,1,0,0,0,105,0,0,0,0,0,0,0,104,125,1,128,1,0,0,0,111,0,0,0,0,0,0,0,120,125,1,128,1,0,0,0,3,0,0,0,0,0,0,0,136,125,1,128,1,0,0,0,226,0,0,0,0,0,0,0,152,125,1,128,1,0,0,0,144,0,0,0,
      0,0,0,0,168,125,1,128,1,0,0,0,161,0,0,0,0,0,0,0,184,125,1,128,1,0,0,0,178,0,0,0,0,0,0,0,200,125,1,128,1,0,0,0,170,0,0,0,0,0,0,0,216,125,1,128,1,0,0,0,70,0,0,0,0,0,0,0,232,125,1,128,1,0,0,0,112,0,0,0,0,0,0,0,97,0,102,0,45,0,122,0,97,0,0,0,0,0,
      0,0,97,0,114,0,45,0,97,0,101,0,0,0,0,0,0,0,97,0,114,0,45,0,98,0,104,0,0,0,0,0,0,0,97,0,114,0,45,0,100,0,122,0,0,0,0,0,0,0,97,0,114,0,45,0,101,0,103,0,0,0,0,0,0,0,97,0,114,0,45,0,105,0,113,0,0,0,0,0,0,0,97,0,114,0,45,0,106,0,111,0,0,0,0,0,0,0,
      97,0,114,0,45,0,107,0,119,0,0,0,0,0,0,0,97,0,114,0,45,0,108,0,98,0,0,0,0,0,0,0,97,0,114,0,45,0,108,0,121,0,0,0,0,0,0,0,97,0,114,0,45,0,109,0,97,0,0,0,0,0,0,0,97,0,114,0,45,0,111,0,109,0,0,0,0,0,0,0,97,0,114,0,45,0,113,0,97,0,0,0,0,0,0,0,97,0,
      114,0,45,0,115,0,97,0,0,0,0,0,0,0,97,0,114,0,45,0,115,0,121,0,0,0,0,0,0,0,97,0,114,0,45,0,116,0,110,0,0,0,0,0,0,0,97,0,114,0,45,0,121,0,101,0,0,0,0,0,0,0,97,0,122,0,45,0,97,0,122,0,45,0,99,0,121,0,114,0,108,0,0,0,0,0,97,0,122,0,45,0,97,0,122,
      0,45,0,108,0,97,0,116,0,110,0,0,0,0,0,98,0,101,0,45,0,98,0,121,0,0,0,0,0,0,0,98,0,103,0,45,0,98,0,103,0,0,0,0,0,0,0,98,0,110,0,45,0,105,0,110,0,0,0,0,0,0,0,98,0,115,0,45,0,98,0,97,0,45,0,108,0,97,0,116,0,110,0,0,0,0,0,99,0,97,0,45,0,101,0,115,
      0,0,0,0,0,0,0,99,0,115,0,45,0,99,0,122,0,0,0,0,0,0,0,99,0,121,0,45,0,103,0,98,0,0,0,0,0,0,0,100,0,97,0,45,0,100,0,107,0,0,0,0,0,0,0,100,0,101,0,45,0,97,0,116,0,0,0,0,0,0,0,100,0,101,0,45,0,99,0,104,0,0,0,0,0,0,0,100,0,101,0,45,0,100,0,101,0,
      0,0,0,0,0,0,100,0,101,0,45,0,108,0,105,0,0,0,0,0,0,0,100,0,101,0,45,0,108,0,117,0,0,0,0,0,0,0,100,0,105,0,118,0,45,0,109,0,118,0,0,0,0,0,101,0,108,0,45,0,103,0,114,0,0,0,0,0,0,0,101,0,110,0,45,0,97,0,117,0,0,0,0,0,0,0,101,0,110,0,45,0,98,0,122,
      0,0,0,0,0,0,0,101,0,110,0,45,0,99,0,97,0,0,0,0,0,0,0,101,0,110,0,45,0,99,0,98,0,0,0,0,0,0,0,101,0,110,0,45,0,103,0,98,0,0,0,0,0,0,0,101,0,110,0,45,0,105,0,101,0,0,0,0,0,0,0,101,0,110,0,45,0,106,0,109,0,0,0,0,0,0,0,101,0,110,0,45,0,110,0,122,
      0,0,0,0,0,0,0,101,0,110,0,45,0,112,0,104,0,0,0,0,0,0,0,101,0,110,0,45,0,116,0,116,0,0,0,0,0,0,0,101,0,110,0,45,0,117,0,115,0,0,0,0,0,0,0,101,0,110,0,45,0,122,0,97,0,0,0,0,0,0,0,101,0,110,0,45,0,122,0,119,0,0,0,0,0,0,0,101,0,115,0,45,0,97,0,114,
      0,0,0,0,0,0,0,101,0,115,0,45,0,98,0,111,0,0,0,0,0,0,0,101,0,115,0,45,0,99,0,108,0,0,0,0,0,0,0,101,0,115,0,45,0,99,0,111,0,0,0,0,0,0,0,101,0,115,0,45,0,99,0,114,0,0,0,0,0,0,0,101,0,115,0,45,0,100,0,111,0,0,0,0,0,0,0,101,0,115,0,45,0,101,0,99,
      0,0,0,0,0,0,0,101,0,115,0,45,0,101,0,115,0,0,0,0,0,0,0,101,0,115,0,45,0,103,0,116,0,0,0,0,0,0,0,101,0,115,0,45,0,104,0,110,0,0,0,0,0,0,0,101,0,115,0,45,0,109,0,120,0,0,0,0,0,0,0,101,0,115,0,45,0,110,0,105,0,0,0,0,0,0,0,101,0,115,0,45,0,112,0,
      97,0,0,0,0,0,0,0,101,0,115,0,45,0,112,0,101,0,0,0,0,0,0,0,101,0,115,0,45,0,112,0,114,0,0,0,0,0,0,0,101,0,115,0,45,0,112,0,121,0,0,0,0,0,0,0,101,0,115,0,45,0,115,0,118,0,0,0,0,0,0,0,101,0,115,0,45,0,117,0,121,0,0,0,0,0,0,0,101,0,115,0,45,0,118,
      0,101,0,0,0,0,0,0,0,101,0,116,0,45,0,101,0,101,0,0,0,0,0,0,0,101,0,117,0,45,0,101,0,115,0,0,0,0,0,0,0,102,0,97,0,45,0,105,0,114,0,0,0,0,0,0,0,102,0,105,0,45,0,102,0,105,0,0,0,0,0,0,0,102,0,111,0,45,0,102,0,111,0,0,0,0,0,0,0,102,0,114,0,45,0,
      98,0,101,0,0,0,0,0,0,0,102,0,114,0,45,0,99,0,97,0,0,0,0,0,0,0,102,0,114,0,45,0,99,0,104,0,0,0,0,0,0,0,102,0,114,0,45,0,102,0,114,0,0,0,0,0,0,0,102,0,114,0,45,0,108,0,117,0,0,0,0,0,0,0,102,0,114,0,45,0,109,0,99,0,0,0,0,0,0,0,103,0,108,0,45,0,
      101,0,115,0,0,0,0,0,0,0,103,0,117,0,45,0,105,0,110,0,0,0,0,0,0,0,104,0,101,0,45,0,105,0,108,0,0,0,0,0,0,0,104,0,105,0,45,0,105,0,110,0,0,0,0,0,0,0,104,0,114,0,45,0,98,0,97,0,0,0,0,0,0,0,104,0,114,0,45,0,104,0,114,0,0,0,0,0,0,0,104,0,117,0,45,
      0,104,0,117,0,0,0,0,0,0,0,104,0,121,0,45,0,97,0,109,0,0,0,0,0,0,0,105,0,100,0,45,0,105,0,100,0,0,0,0,0,0,0,105,0,115,0,45,0,105,0,115,0,0,0,0,0,0,0,105,0,116,0,45,0,99,0,104,0,0,0,0,0,0,0,105,0,116,0,45,0,105,0,116,0,0,0,0,0,0,0,106,0,97,0,45,
      0,106,0,112,0,0,0,0,0,0,0,107,0,97,0,45,0,103,0,101,0,0,0,0,0,0,0,107,0,107,0,45,0,107,0,122,0,0,0,0,0,0,0,107,0,110,0,45,0,105,0,110,0,0,0,0,0,0,0,107,0,111,0,107,0,45,0,105,0,110,0,0,0,0,0,107,0,111,0,45,0,107,0,114,0,0,0,0,0,0,0,107,0,121,
      0,45,0,107,0,103,0,0,0,0,0,0,0,108,0,116,0,45,0,108,0,116,0,0,0,0,0,0,0,108,0,118,0,45,0,108,0,118,0,0,0,0,0,0,0,109,0,105,0,45,0,110,0,122,0,0,0,0,0,0,0,109,0,107,0,45,0,109,0,107,0,0,0,0,0,0,0,109,0,108,0,45,0,105,0,110,0,0,0,0,0,0,0,109,0,
      110,0,45,0,109,0,110,0,0,0,0,0,0,0,109,0,114,0,45,0,105,0,110,0,0,0,0,0,0,0,109,0,115,0,45,0,98,0,110,0,0,0,0,0,0,0,109,0,115,0,45,0,109,0,121,0,0,0,0,0,0,0,109,0,116,0,45,0,109,0,116,0,0,0,0,0,0,0,110,0,98,0,45,0,110,0,111,0,0,0,0,0,0,0,110,
      0,108,0,45,0,98,0,101,0,0,0,0,0,0,0,110,0,108,0,45,0,110,0,108,0,0,0,0,0,0,0,110,0,110,0,45,0,110,0,111,0,0,0,0,0,0,0,110,0,115,0,45,0,122,0,97,0,0,0,0,0,0,0,112,0,97,0,45,0,105,0,110,0,0,0,0,0,0,0,112,0,108,0,45,0,112,0,108,0,0,0,0,0,0,0,112,
      0,116,0,45,0,98,0,114,0,0,0,0,0,0,0,112,0,116,0,45,0,112,0,116,0,0,0,0,0,0,0,113,0,117,0,122,0,45,0,98,0,111,0,0,0,0,0,113,0,117,0,122,0,45,0,101,0,99,0,0,0,0,0,113,0,117,0,122,0,45,0,112,0,101,0,0,0,0,0,114,0,111,0,45,0,114,0,111,0,0,0,0,0,
      0,0,114,0,117,0,45,0,114,0,117,0,0,0,0,0,0,0,115,0,97,0,45,0,105,0,110,0,0,0,0,0,0,0,115,0,101,0,45,0,102,0,105,0,0,0,0,0,0,0,115,0,101,0,45,0,110,0,111,0,0,0,0,0,0,0,115,0,101,0,45,0,115,0,101,0,0,0,0,0,0,0,115,0,107,0,45,0,115,0,107,0,0,0,
      0,0,0,0,115,0,108,0,45,0,115,0,105,0,0,0,0,0,0,0,115,0,109,0,97,0,45,0,110,0,111,0,0,0,0,0,115,0,109,0,97,0,45,0,115,0,101,0,0,0,0,0,115,0,109,0,106,0,45,0,110,0,111,0,0,0,0,0,115,0,109,0,106,0,45,0,115,0,101,0,0,0,0,0,115,0,109,0,110,0,45,0,
      102,0,105,0,0,0,0,0,115,0,109,0,115,0,45,0,102,0,105,0,0,0,0,0,115,0,113,0,45,0,97,0,108,0,0,0,0,0,0,0,115,0,114,0,45,0,98,0,97,0,45,0,99,0,121,0,114,0,108,0,0,0,0,0,115,0,114,0,45,0,98,0,97,0,45,0,108,0,97,0,116,0,110,0,0,0,0,0,115,0,114,0,
      45,0,115,0,112,0,45,0,99,0,121,0,114,0,108,0,0,0,0,0,115,0,114,0,45,0,115,0,112,0,45,0,108,0,97,0,116,0,110,0,0,0,0,0,115,0,118,0,45,0,102,0,105,0,0,0,0,0,0,0,115,0,118,0,45,0,115,0,101,0,0,0,0,0,0,0,115,0,119,0,45,0,107,0,101,0,0,0,0,0,0,0,
      115,0,121,0,114,0,45,0,115,0,121,0,0,0,0,0,116,0,97,0,45,0,105,0,110,0,0,0,0,0,0,0,116,0,101,0,45,0,105,0,110,0,0,0,0,0,0,0,116,0,104,0,45,0,116,0,104,0,0,0,0,0,0,0,116,0,110,0,45,0,122,0,97,0,0,0,0,0,0,0,116,0,114,0,45,0,116,0,114,0,0,0,0,0,
      0,0,116,0,116,0,45,0,114,0,117,0,0,0,0,0,0,0,117,0,107,0,45,0,117,0,97,0,0,0,0,0,0,0,117,0,114,0,45,0,112,0,107,0,0,0,0,0,0,0,117,0,122,0,45,0,117,0,122,0,45,0,99,0,121,0,114,0,108,0,0,0,0,0,117,0,122,0,45,0,117,0,122,0,45,0,108,0,97,0,116,0,
      110,0,0,0,0,0,118,0,105,0,45,0,118,0,110,0,0,0,0,0,0,0,120,0,104,0,45,0,122,0,97,0,0,0,0,0,0,0,122,0,104,0,45,0,99,0,104,0,115,0,0,0,0,0,122,0,104,0,45,0,99,0,104,0,116,0,0,0,0,0,122,0,104,0,45,0,99,0,110,0,0,0,0,0,0,0,122,0,104,0,45,0,104,0,
      107,0,0,0,0,0,0,0,122,0,104,0,45,0,109,0,111,0,0,0,0,0,0,0,122,0,104,0,45,0,115,0,103,0,0,0,0,0,0,0,122,0,104,0,45,0,116,0,119,0,0,0,0,0,0,0,122,0,117,0,45,0,122,0,97,0,0,0,0,0,0,0,67,0,79,0,78,0,79,0,85,0,84,0,36,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,240,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,240,127,0,0,0,0,0,0,0,0,0,0,0,0,0,0,248,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,0,0,0,0,0,0,0,0,0,255,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,15,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,240,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,0,0,0,0,0,0,0,0,0,0,14,229,38,21,123,203,219,63,0,0,0,0,0,0,0,0,0,0,0,0,120,203,219,63,0,0,0,0,0,0,0,0,53,149,113,40,55,169,168,62,0,0,0,0,0,0,0,0,0,0,0,80,19,68,211,63,0,0,0,0,0,0,0,0,37,62,
      98,222,63,239,3,62,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,0,0,0,0,0,0,0,0,0,0,0,0,0,240,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,224,63,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,96,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,224,63,0,0,0,0,0,0,0,0,85,
      85,85,85,85,85,213,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,208,63,0,0,0,0,0,0,0,0,154,153,153,153,153,153,201,63,0,0,0,0,0,0,0,0,85,85,85,85,85,85,197,63,0,0,0,0,0,0,0,0,0,0,0,0,0,248,143,192,0,0,0,0,0,0,0,0,253,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,176,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,238,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,241,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,16,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,127,0,0,0,0,0,0,0,0,230,84,85,85,85,85,181,63,0,0,0,0,0,0,0,0,212,198,186,153,153,153,
      137,63,0,0,0,0,0,0,0,0,159,81,241,7,35,73,98,63,0,0,0,0,0,0,0,0,240,255,93,200,52,128,60,63,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,1,0,0,0,2,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,144,158,189,91,63,0,0,0,112,212,175,107,
      63,0,0,0,96,149,185,116,63,0,0,0,160,118,148,123,63,0,0,0,160,77,52,129,63,0,0,0,80,8,155,132,63,0,0,0,192,113,254,135,63,0,0,0,128,144,94,139,63,0,0,0,240,106,187,142,63,0,0,0,160,131,10,145,63,0,0,0,224,181,181,146,63,0,0,0,80,79,95,148,63,
      0,0,0,0,83,7,150,63,0,0,0,208,195,173,151,63,0,0,0,240,164,82,153,63,0,0,0,32,249,245,154,63,0,0,0,112,195,151,156,63,0,0,0,160,6,56,158,63,0,0,0,176,197,214,159,63,0,0,0,160,1,186,160,63,0,0,0,32,225,135,161,63,0,0,0,192,2,85,162,63,0,0,0,192,
      103,33,163,63,0,0,0,144,17,237,163,63,0,0,0,128,1,184,164,63,0,0,0,224,56,130,165,63,0,0,0,16,185,75,166,63,0,0,0,64,131,20,167,63,0,0,0,192,152,220,167,63,0,0,0,208,250,163,168,63,0,0,0,192,170,106,169,63,0,0,0,208,169,48,170,63,0,0,0,32,249,
      245,170,63,0,0,0,0,154,186,171,63,0,0,0,144,141,126,172,63,0,0,0,16,213,65,173,63,0,0,0,160,113,4,174,63,0,0,0,112,100,198,174,63,0,0,0,176,174,135,175,63,0,0,0,192,40,36,176,63,0,0,0,240,38,132,176,63,0,0,0,144,210,227,176,63,0,0,0,48,44,67,
      177,63,0,0,0,64,52,162,177,63,0,0,0,96,235,0,178,63,0,0,0,16,82,95,178,63,0,0,0,224,104,189,178,63,0,0,0,80,48,27,179,63,0,0,0,224,168,120,179,63,0,0,0,48,211,213,179,63,0,0,0,160,175,50,180,63,0,0,0,208,62,143,180,63,0,0,0,32,129,235,180,63,
      0,0,0,48,119,71,181,63,0,0,0,96,33,163,181,63,0,0,0,64,128,254,181,63,0,0,0,64,148,89,182,63,0,0,0,240,93,180,182,63,0,0,0,176,221,14,183,63,0,0,0,0,20,105,183,63,0,0,0,96,1,195,183,63,0,0,0,48,166,28,184,63,0,0,0,0,3,118,184,63,0,0,0,48,24,
      207,184,63,0,0,0,64,230,39,185,63,0,0,0,144,109,128,185,63,0,0,0,160,174,216,185,63,0,0,0,208,169,48,186,63,0,0,0,160,95,136,186,63,0,0,0,112,208,223,186,63,0,0,0,176,252,54,187,63,0,0,0,208,228,141,187,63,0,0,0,48,137,228,187,63,0,0,0,64,234,
      58,188,63,0,0,0,112,8,145,188,63,0,0,0,16,228,230,188,63,0,0,0,160,125,60,189,63,0,0,0,128,213,145,189,63,0,0,0,0,236,230,189,63,0,0,0,160,193,59,190,63,0,0,0,176,86,144,190,63,0,0,0,160,171,228,190,63,0,0,0,192,192,56,191,63,0,0,0,128,150,140,
      191,63,0,0,0,48,45,224,191,63,0,0,0,160,194,25,192,63,0,0,0,112,79,67,192,63,0,0,0,96,189,108,192,63,0,0,0,128,12,150,192,63,0,0,0,0,61,191,192,63,0,0,0,16,79,232,192,63,0,0,0,240,66,17,193,63,0,0,0,160,24,58,193,63,0,0,0,128,208,98,193,63,0,
      0,0,144,106,139,193,63,0,0,0,16,231,179,193,63,0,0,0,48,70,220,193,63,0,0,0,16,136,4,194,63,0,0,0,224,172,44,194,63,0,0,0,208,180,84,194,63,0,0,0,240,159,124,194,63,0,0,0,128,110,164,194,63,0,0,0,176,32,204,194,63,0,0,0,144,182,243,194,63,0,
      0,0,80,48,27,195,63,0,0,0,32,142,66,195,63,0,0,0,32,208,105,195,63,0,0,0,128,246,144,195,63,0,0,0,96,1,184,195,63,0,0,0,224,240,222,195,63,0,0,0,48,197,5,196,63,0,0,0,112,126,44,196,63,0,0,0,208,28,83,196,63,0,0,0,112,160,121,196,63,0,0,0,112,
      9,160,196,63,0,0,0,0,88,198,196,63,0,0,0,48,140,236,196,63,0,0,0,64,166,18,197,63,0,0,0,48,166,56,197,63,0,0,0,80,140,94,197,63,0,0,0,144,88,132,197,63,0,0,0,64,11,170,197,63,0,0,0,112,164,207,197,63,0,0,0,64,36,245,197,63,0,0,0,208,138,26,198,
      63,0,0,0,80,216,63,198,63,0,0,0,208,12,101,198,63,0,0,0,128,40,138,198,63,0,0,0,128,43,175,198,63,0,0,0,224,21,212,198,63,0,0,0,208,231,248,198,63,0,0,0,112,161,29,199,63,0,0,0,224,66,66,199,63,0,0,0,64,204,102,199,63,0,0,0,160,61,139,199,63,
      0,0,0,48,151,175,199,63,0,0,0,16,217,211,199,63,0,0,0,80,3,248,199,63,0,0,0,32,22,28,200,63,0,0,0,144,17,64,200,63,0,0,0,192,245,99,200,63,0,0,0,224,194,135,200,63,0,0,0,0,121,171,200,63,0,0,0,48,24,207,200,63,0,0,0,160,160,242,200,63,0,0,0,
      112,18,22,201,63,0,0,0,176,109,57,201,63,0,0,0,128,178,92,201,63,0,0,0,0,225,127,201,63,0,0,0,80,249,162,201,63,0,0,0,112,251,197,201,63,0,0,0,176,231,232,201,63,0,0,0,240,189,11,202,63,0,0,0,128,126,46,202,63,0,0,0,96,41,81,202,63,0,0,0,160,
      190,115,202,63,0,0,0,112,62,150,202,63,0,0,0,240,168,184,202,63,0,0,0,32,254,218,202,63,0,0,0,48,62,253,202,63,0,0,0,48,105,31,203,63,0,0,0,64,127,65,203,63,0,0,0,112,128,99,203,63,0,0,0,240,108,133,203,63,0,0,0,176,68,167,203,63,0,0,0,240,7,
      201,203,63,0,0,0,192,182,234,203,63,0,0,0,48,81,12,204,63,0,0,0,80,215,45,204,63,0,0,0,80,73,79,204,63,0,0,0,64,167,112,204,63,0,0,0,48,241,145,204,63,0,0,0,64,39,179,204,63,0,0,0,128,73,212,204,63,0,0,0,16,88,245,204,63,0,0,0,0,83,22,205,63,
      0,0,0,96,58,55,205,63,0,0,0,96,14,88,205,63,0,0,0,0,207,120,205,63,0,0,0,112,124,153,205,63,0,0,0,160,22,186,205,63,0,0,0,208,157,218,205,63,0,0,0,240,17,251,205,63,0,0,0,48,115,27,206,63,0,0,0,160,193,59,206,63,0,0,0,80,253,91,206,63,0,0,0,
      96,38,124,206,63,0,0,0,224,60,156,206,63,0,0,0,224,64,188,206,63,0,0,0,128,50,220,206,63,0,0,0,208,17,252,206,63,0,0,0,224,222,27,207,63,0,0,0,208,153,59,207,63,0,0,0,160,66,91,207,63,0,0,0,128,217,122,207,63,0,0,0,112,94,154,207,63,0,0,0,144,
      209,185,207,63,0,0,0,240,50,217,207,63,0,0,0,160,130,248,207,63,0,0,0,80,224,11,208,63,0,0,0,160,118,27,208,63,0,0,0,48,4,43,208,63,0,0,0,16,137,58,208,63,0,0,0,64,5,74,208,63,0,0,0,224,120,89,208,63,0,0,0,240,227,104,208,63,0,0,0,112,70,120,
      208,63,0,0,0,128,160,135,208,63,0,0,0,16,242,150,208,63,0,0,0,48,59,166,208,63,0,0,0,240,123,181,208,63,0,0,0,80,180,196,208,63,0,0,0,96,228,211,208,63,0,0,0,48,12,227,208,63,0,0,0,192,43,242,208,63,0,0,0,16,67,1,209,63,0,0,0,64,82,16,209,63,
      0,0,0,64,89,31,209,63,0,0,0,48,88,46,209,63,0,0,0,0,79,61,209,63,0,0,0,208,61,76,209,63,0,0,0,160,36,91,209,63,0,0,0,112,3,106,209,63,0,0,0,80,218,120,209,63,0,0,0,64,169,135,209,63,0,0,0,96,112,150,209,63,0,0,0,160,47,165,209,63,0,0,0,16,231,
      179,209,63,0,0,0,192,150,194,209,63,0,0,0,176,62,209,209,63,0,0,0,240,222,223,209,63,0,0,0,112,119,238,209,63,0,0,0,96,8,253,209,63,0,0,0,160,145,11,210,63,0,0,0,80,19,26,210,63,0,0,0,112,141,40,210,63,0,0,0,16,0,55,210,63,0,0,0,48,107,69,210,
      63,0,0,0,208,206,83,210,63,0,0,0,0,43,98,210,63,0,0,0,208,127,112,210,63,0,0,0,64,205,126,210,63,0,0,0,96,19,141,210,63,0,0,0,32,82,155,210,63,0,0,0,160,137,169,210,63,0,0,0,224,185,183,210,63,0,0,0,224,226,197,210,63,0,0,0,176,4,212,210,63,
      0,0,0,80,31,226,210,63,0,0,0,192,50,240,210,63,0,0,0,32,63,254,210,63,0,0,0,112,68,12,211,63,0,0,0,176,66,26,211,63,0,0,0,224,57,40,211,63,0,0,0,16,42,54,211,63,0,0,0,80,19,68,211,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,143,32,178,34,188,10,178,61,
      212,13,46,51,105,15,177,61,87,210,126,232,13,149,206,61,105,109,98,59,68,243,211,61,87,62,54,165,234,90,244,61,11,191,225,60,104,67,196,61,17,165,198,96,205,137,249,61,159,46,31,32,111,98,253,61,205,189,218,184,139,79,233,61,21,48,66,239,216,
      136,0,62,173,121,43,166,19,4,8,62,196,211,238,192,23,151,5,62,2,73,212,173,119,74,173,61,14,48,55,240,63,118,14,62,195,246,6,71,215,98,225,61,20,188,77,31,204,1,6,62,191,229,246,81,224,243,234,61,235,243,26,30,11,122,9,62,199,2,192,112,137,163,
      192,61,81,199,87,0,0,46,16,62,14,110,205,238,0,91,21,62,175,181,3,112,41,134,223,61,109,163,54,179,185,87,16,62,79,234,6,74,200,75,19,62,173,188,161,158,218,67,22,62,42,234,247,180,167,102,29,62,239,252,247,56,224,178,246,61,136,240,112,198,
      84,233,243,61,179,202,58,9,9,114,4,62,167,93,39,231,143,112,29,62,231,185,113,119,158,223,31,62,96,6,10,167,191,39,8,62,20,188,77,31,204,1,22,62,91,94,106,16,246,55,6,62,75,98,124,241,19,106,18,62,58,98,128,206,178,62,9,62,222,148,21,233,209,
      48,20,62,49,160,143,16,16,107,29,62,65,242,186,11,156,135,22,62,43,188,166,94,1,8,255,61,108,103,198,205,61,182,41,62,44,171,196,188,44,2,43,62,68,101,221,125,208,23,249,61,158,55,3,87,96,64,21,62,96,27,122,148,139,209,12,62,126,169,124,39,101,
      173,23,62,169,95,159,197,77,136,17,62,130,208,6,96,196,17,23,62,248,8,49,60,46,9,47,62,58,225,43,227,197,20,23,62,154,79,115,253,167,187,38,62,131,132,224,181,143,244,253,61,149,11,77,199,155,47,35,62,19,12,121,72,232,115,249,61,110,88,198,8,
      188,204,30,62,152,74,82,249,233,21,33,62,184,49,49,89,64,23,47,62,53,56,100,37,139,207,27,62,128,237,139,29,168,95,31,62,228,217,41,249,77,74,36,62,148,12,34,216,32,152,18,62,9,227,4,147,72,11,42,62,254,101,166,171,86,77,31,62,99,81,54,25,144,
      12,33,62,54,39,89,254,120,15,248,61,202,28,200,37,136,82,16,62,106,116,109,125,83,149,224,61,96,6,10,167,191,39,24,62,60,147,69,236,168,176,6,62,169,219,245,27,248,90,16,62,21,213,85,38,250,226,23,62,191,228,174,191,236,89,13,62,163,63,104,218,
      47,139,29,62,55,55,58,253,221,184,36,62,4,18,174,97,126,130,19,62,159,15,233,73,123,140,44,62,29,89,151,21,240,234,41,62,54,123,49,110,166,170,25,62,85,6,114,9,86,114,46,62,84,172,122,252,51,28,38,62,82,162,97,207,43,102,41,62,48,39,196,17,200,
      67,24,62,54,203,90,11,187,100,32,62,164,1,39,132,12,52,10,62,214,121,143,181,85,142,26,62,154,157,94,156,33,45,233,61,106,253,127,13,230,99,63,62,20,99,81,217,14,155,46,62,12,53,98,25,144,35,41,62,129,94,120,56,136,111,50,62,175,166,171,76,106,
      91,59,62,28,118,142,220,106,34,240,61,237,26,58,49,215,74,60,62,23,141,115,124,232,100,21,62,24,102,138,241,236,143,51,62,102,118,119,245,158,146,61,62,184,160,141,240,59,72,57,62,38,88,170,238,14,221,59,62,186,55,2,89,221,196,57,62,199,202,
      235,224,233,243,26,62,172,13,39,130,83,206,53,62,186,185,42,83,116,79,57,62,84,134,136,149,39,52,7,62,240,75,227,11,0,90,12,62,130,208,6,96,196,17,39,62,248,140,237,180,37,0,37,62,160,210,242,206,139,209,46,62,84,117,10,12,46,40,33,62,202,167,
      89,51,243,112,13,62,37,64,168,19,126,127,43,62,30,137,33,195,110,48,51,62,80,117,139,3,248,199,63,62,100,29,215,140,53,176,62,62,116,148,133,34,200,118,58,62,227,134,222,82,198,14,61,62,175,88,134,224,204,164,47,62,158,10,192,210,162,132,59,
      62,209,91,194,242,176,165,32,62,153,246,91,34,96,214,61,62,55,240,155,133,15,177,8,62,225,203,144,181,35,136,62,62,246,150,30,243,17,19,54,62,154,15,162,92,135,31,46,62,165,185,57,73,114,149,44,62,226,88,62,122,149,5,56,62,52,3,159,234,38,241,
      47,62,9,86,142,89,245,83,57,62,72,196,86,248,111,193,54,62,244,97,242,15,34,203,36,62,162,83,61,213,32,225,53,62,86,242,137,97,127,82,58,62,15,156,212,255,252,86,56,62,218,215,40,130,46,12,48,62,224,223,68,148,208,19,241,61,166,89,234,14,99,
      16,37,62,17,215,50,15,120,46,38,62,207,248,16,26,217,62,237,61,133,205,75,126,74,101,35,62,33,173,128,73,120,91,5,62,100,110,177,212,45,47,33,62,12,245,57,217,173,196,55,62,252,128,113,98,132,23,40,62,97,73,225,199,98,81,234,61,99,81,54,25,144,
      12,49,62,136,118,161,43,77,60,55,62,129,61,233,224,165,232,42,62,175,33,22,240,198,176,42,62,102,91,221,116,139,30,48,62,148,84,187,236,111,32,45,62,0,204,79,114,139,180,240,61,41,226,97,11,31,131,63,62,175,188,7,196,151,26,248,61,170,183,203,
      28,108,40,62,62,147,10,34,73,11,99,40,62,92,44,162,193,21,11,255,61,70,9,28,231,69,84,53,62,133,109,6,248,48,230,59,62,57,108,217,240,223,153,37,62,129,176,143,177,133,204,54,62,200,168,30,0,109,71,52,62,31,211,22,158,136,63,55,62,135,42,121,
      13,16,87,51,62,246,1,97,174,121,209,59,62,226,246,195,86,16,163,12,62,251,8,156,98,112,40,61,62,63,103,210,128,56,186,58,62,166,125,41,203,51,54,44,62,2,234,239,153,56,132,33,62,230,8,32,157,201,204,59,62,80,211,189,68,5,0,56,62,225,106,96,38,
      194,145,43,62,223,43,182,38,223,122,42,62,201,110,130,200,79,118,24,62,240,104,15,229,61,79,31,62,227,149,121,117,202,96,247,61,71,81,128,211,126,102,252,61,111,223,106,25,246,51,55,62,107,131,62,243,16,183,47,62,19,16,100,186,110,136,57,62,
      26,140,175,208,104,83,251,61,113,41,141,27,105,140,53,62,251,8,109,34,101,148,254,61,151,0,63,6,126,88,51,62,24,159,18,2,231,24,54,62,84,172,122,252,51,28,54,62,74,96,8,132,166,7,63,62,33,84,148,228,191,52,60,62,11,48,65,14,240,177,56,62,99,
      27,214,132,66,67,63,62,54,116,57,94,9,99,58,62,222,25,185,86,134,66,52,62,166,217,178,1,146,202,54,62,28,147,42,58,130,56,39,62,48,146,23,14,136,17,60,62,254,82,109,141,220,61,49,62,23,233,34,137,213,238,51,62,80,221,107,132,146,89,41,62,139,
      39,46,95,77,219,13,62,196,53,6,42,241,165,241,61,52,60,44,136,240,66,70,62,94,71,246,167,155,238,42,62,228,96,74,131,127,75,38,62,46,121,67,226,66,13,41,62,1,79,19,8,32,39,76,62,91,207,214,22,46,120,74,62,72,102,218,121,92,80,68,62,33,205,77,
      234,212,169,76,62,188,213,124,98,61,125,41,62,19,170,188,249,92,177,32,62,221,118,207,99,32,91,49,62,72,39,170,243,230,131,41,62,148,233,255,244,100,76,63,62,15,90,232,124,186,190,70,62,184,166,78,253,105,156,59,62,171,164,95,131,165,106,43,
      62,209,237,15,121,195,204,67,62,224,79,64,196,76,192,41,62,157,216,117,122,75,115,64,62,18,22,224,196,4,68,27,62,148,72,206,194,101,197,64,62,205,53,217,65,20,199,51,62,78,59,107,85,146,164,114,61,67,220,65,3,9,250,32,62,244,217,227,9,112,143,
      46,62,69,138,4,139,246,27,75,62,86,169,250,223,82,238,62,62,189,101,228,0,9,107,69,62,102,118,119,245,158,146,77,62,96,226,55,134,162,110,72,62,240,162,12,241,175,101,70,62,116,236,72,175,253,17,47,62,199,209,164,134,27,190,76,62,101,118,168,
      254,91,176,37,62,29,74,26,10,194,206,65,62,159,155,64,10,95,205,65,62,112,80,38,200,86,54,69,62,96,34,40,53,216,126,55,62,210,185,64,48,188,23,36,62,242,239,121,123,239,142,64,62,233,87,220,57,111,199,77,62,87,244,12,167,147,4,76,62,12,166,165,
      206,214,131,74,62,186,87,197,13,112,214,48,62,10,189,232,18,108,201,68,62,21,35,227,147,25,44,61,62,66,130,95,19,33,199,34,62,125,116,218,77,62,154,39,62,43,167,65,105,159,248,252,61,49,8,241,2,167,73,33,62,219,117,129,124,75,173,78,62,10,231,
      99,254,48,105,78,62,47,238,217,190,6,225,65,62,146,28,241,130,43,104,45,62,124,164,219,136,241,7,58,62,246,114,193,45,52,249,64,62,37,62,98,222,63,239,3,62,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64,32,224,31,224,31,224,255,63,240,7,252,1,127,192,255,
      63,18,250,1,170,28,161,255,63,32,248,129,31,248,129,255,63,181,219,160,172,16,99,255,63,113,66,74,158,101,68,255,63,181,10,35,68,246,37,255,63,8,31,124,240,193,7,255,63,2,142,69,248,199,233,254,63,192,236,1,179,7,204,254,63,235,1,186,122,128,
      174,254,63,103,183,240,171,49,145,254,63,228,80,151,165,26,116,254,63,116,229,1,201,58,87,254,63,115,26,220,121,145,58,254,63,30,30,30,30,30,30,254,63,30,224,1,30,224,1,254,63,138,134,248,227,214,229,253,63,202,29,160,220,1,202,253,63,219,129,
      185,118,96,174,253,63,138,127,30,35,242,146,253,63,52,44,184,84,182,119,253,63,178,114,117,128,172,92,253,63,29,212,65,29,212,65,253,63,26,91,252,163,44,39,253,63,116,192,110,143,181,12,253,63,198,191,68,92,110,242,252,63,11,155,3,137,86,216,
      252,63,231,203,1,150,109,190,252,63,145,225,94,5,179,164,252,63,66,138,251,90,38,139,252,63,28,199,113,28,199,113,252,63,134,73,13,209,148,88,252,63,240,248,195,1,143,63,252,63,28,160,46,57,181,38,252,63,224,192,129,3,7,14,252,63,139,141,134,
      238,131,245,251,63,247,6,148,137,43,221,251,63,123,62,136,101,253,196,251,63,208,186,193,20,249,172,251,63,35,255,24,43,30,149,251,63,139,51,218,61,108,125,251,63,5,238,190,227,226,101,251,63,79,27,232,180,129,78,251,63,206,6,216,74,72,55,251,
      63,217,128,108,64,54,32,251,63,164,34,217,49,75,9,251,63,40,175,161,188,134,242,250,63,94,144,148,127,232,219,250,63,27,112,197,26,112,197,250,63,253,235,135,47,29,175,250,63,190,99,106,96,239,152,250,63,89,225,48,81,230,130,250,63,109,26,208,
      166,1,109,250,63,74,138,104,7,65,87,250,63,26,164,65,26,164,65,250,63,160,28,197,135,42,44,250,63,2,75,122,249,211,22,250,63,26,160,1,26,160,1,250,63,217,51,16,149,142,236,249,63,45,104,107,23,159,215,249,63,2,161,228,78,209,194,249,63,218,16,
      85,234,36,174,249,63,154,153,153,153,153,153,249,63,255,192,142,13,47,133,249,63,114,184,12,248,228,112,249,63,174,119,227,11,187,92,249,63,224,233,214,252,176,72,249,63,230,44,155,127,198,52,249,63,41,226,208,73,251,32,249,63,213,144,1,18,79,
      13,249,63,250,24,156,143,193,249,248,63,63,55,241,122,82,230,248,63,211,24,48,141,1,211,248,63,58,255,98,128,206,191,248,63,170,243,107,15,185,172,248,63,156,137,1,246,192,153,248,63,74,176,171,240,229,134,248,63,185,146,192,188,39,116,248,63,
      24,134,97,24,134,97,248,63,20,6,120,194,0,79,248,63,221,190,178,122,151,60,248,63,160,164,130,1,74,42,248,63,24,24,24,24,24,24,248,63,6,24,96,128,1,6,248,63,64,127,1,253,5,244,247,63,29,79,90,81,37,226,247,63,244,5,125,65,95,208,247,63,124,1,
      46,146,179,190,247,63,195,236,224,8,34,173,247,63,139,57,182,107,170,155,247,63,200,164,120,129,76,138,247,63,13,198,154,17,8,121,247,63,177,169,52,228,220,103,247,63,109,117,1,194,202,86,247,63,70,23,93,116,209,69,247,63,141,254,65,197,240,
      52,247,63,188,222,70,127,40,36,247,63,9,124,156,109,120,19,247,63,112,129,11,92,224,2,247,63,23,96,242,22,96,242,246,63,199,55,67,107,247,225,246,63,97,200,129,38,166,209,246,63,23,108,193,22,108,193,246,63,61,26,163,10,73,177,246,63,144,114,
      83,209,60,161,246,63,192,208,136,58,71,145,246,63,23,104,129,22,104,129,246,63,26,103,1,54,159,113,246,63,249,34,81,106,236,97,246,63,163,74,59,133,79,82,246,63,100,33,11,89,200,66,246,63,222,192,138,184,86,51,246,63,64,98,1,119,250,35,246,63,
      148,174,49,104,179,20,246,63,6,22,88,96,129,5,246,63,252,45,41,52,100,246,245,63,231,21,208,184,91,231,245,63,165,226,236,195,103,216,245,63,87,16,147,43,136,201,245,63,145,250,71,198,188,186,245,63,192,90,1,107,5,172,245,63,170,204,35,241,97,
      157,245,63,237,88,129,48,210,142,245,63,96,5,88,1,86,128,245,63,58,107,80,60,237,113,245,63,226,82,124,186,151,99,245,63,85,85,85,85,85,85,245,63,254,130,187,230,37,71,245,63,235,15,244,72,9,57,245,63,75,5,168,86,255,42,245,63,21,248,226,234,
      7,29,245,63,197,196,17,225,34,15,245,63,21,80,1,21,80,1,245,63,155,76,221,98,143,243,244,63,57,5,47,167,224,229,244,63,76,44,220,190,67,216,244,63,110,175,37,135,184,202,244,63,225,143,166,221,62,189,244,63,91,191,82,160,214,175,244,63,74,1,
      118,173,127,162,244,63,103,208,178,227,57,149,244,63,128,72,1,34,5,136,244,63,123,20,174,71,225,122,244,63,102,96,89,52,206,109,244,63,154,207,245,199,203,96,244,63,202,118,199,226,217,83,244,63,251,217,98,101,248,70,244,63,77,238,171,48,39,
      58,244,63,135,31,213,37,102,45,244,63,81,89,94,38,181,32,244,63,20,20,20,20,20,20,244,63,102,101,14,209,130,7,244,63,251,19,176,63,1,251,243,63,7,175,165,66,143,238,243,63,2,169,228,188,44,226,243,63,198,117,170,145,217,213,243,63,231,171,123,
      164,149,201,243,63,85,41,35,217,96,189,243,63,20,59,177,19,59,177,243,63,34,200,122,56,36,165,243,63,99,127,24,44,28,153,243,63,142,8,102,211,34,141,243,63,20,56,129,19,56,129,243,63,238,69,201,209,91,117,243,63,72,7,222,243,141,105,243,63,248,
      42,159,95,206,93,243,63,193,120,43,251,28,82,243,63,70,19,224,172,121,70,243,63,178,188,87,91,228,58,243,63,250,29,106,237,92,47,243,63,191,16,43,74,227,35,243,63,182,235,233,88,119,24,243,63,144,209,48,1,25,13,243,63,96,2,196,42,200,1,243,63,
      104,47,161,189,132,246,242,63,75,209,254,161,78,235,242,63,151,128,75,192,37,224,242,63,160,80,45,1,10,213,242,63,160,44,129,77,251,201,242,63,17,55,90,142,249,190,242,63,64,43,1,173,4,180,242,63,5,193,243,146,28,169,242,63,158,18,228,41,65,
      158,242,63,165,4,184,91,114,147,242,63,19,176,136,18,176,136,242,63,77,206,161,56,250,125,242,63,53,39,129,184,80,115,242,63,39,1,214,124,179,104,242,63,241,146,128,112,34,94,242,63,178,119,145,126,157,83,242,63,146,36,73,146,36,73,242,63,91,
      96,23,151,183,62,242,63,223,188,154,120,86,52,242,63,42,18,160,34,1,42,242,63,120,251,33,129,183,31,242,63,230,85,72,128,121,21,242,63,217,192,103,12,71,11,242,63,18,32,1,18,32,1,242,63,112,31,193,125,4,247,241,63,76,184,127,60,244,236,241,63,
      116,184,63,59,239,226,241,63,189,74,46,103,245,216,241,63,29,129,162,173,6,207,241,63,89,224,28,252,34,197,241,63,41,237,70,64,74,187,241,63,227,186,242,103,124,177,241,63,150,123,26,97,185,167,241,63,158,17,224,25,1,158,241,63,156,162,140,128,
      83,148,241,63,219,43,144,131,176,138,241,63,18,24,129,17,24,129,241,63,132,214,27,25,138,119,241,63,121,115,66,137,6,110,241,63,1,50,252,80,141,100,241,63,13,39,117,95,30,91,241,63,201,213,253,163,185,81,241,63,59,205,10,14,95,72,241,63,36,71,
      52,141,14,63,241,63,17,200,53,17,200,53,241,63,172,192,237,137,139,44,241,63,51,48,93,231,88,35,241,63,38,72,167,25,48,26,241,63,17,17,17,17,17,17,241,63,128,16,1,190,251,7,241,63,17,240,254,16,240,254,240,63,162,37,179,250,237,245,240,63,144,
      156,230,107,245,236,240,63,17,96,130,85,6,228,240,63,150,70,143,168,32,219,240,63,58,158,53,86,68,210,240,63,59,218,188,79,113,201,240,63,113,65,139,134,167,192,240,63,200,157,37,236,230,183,240,63,181,236,46,114,47,175,240,63,167,16,104,10,
      129,166,240,63,96,131,175,166,219,157,240,63,84,9,1,57,63,149,240,63,226,101,117,179,171,140,240,63,132,16,66,8,33,132,240,63,226,234,184,41,159,123,240,63,198,247,71,10,38,115,240,63,251,18,121,156,181,106,240,63,252,169,241,210,77,98,240,63,
      134,117,114,160,238,89,240,63,4,52,215,247,151,81,240,63,197,100,22,204,73,73,240,63,16,4,65,16,4,65,240,63,252,71,130,183,198,56,240,63,26,94,31,181,145,48,240,63,233,41,119,252,100,40,240,63,8,4,2,129,64,32,240,63,55,122,81,54,36,24,240,63,
      16,16,16,16,16,16,240,63,128,0,1,2,4,8,240,63,0,0,0,0,0,0,240,63,0,0,0,0,0,0,0,0,108,111,103,49,48,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,63,67,255,255,255,255,255,255,63,195,76,30,0,128,1,0,0,0,180,30,0,128,1,0,0,0,194,30,0,128,1,0,0,
      0,0,31,0,128,1,0,0,0,194,31,0,128,1,0,0,0,0,0,0,0,0,0,0,0,76,30,0,128,1,0,0,0,180,30,0,128,1,0,0,0,194,30,0,128,1,0,0,0,44,76,0,128,1,0,0,0,12,32,0,128,1,0,0,0,0,0,0,0,0,0,0,0,69,0,66,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,69,0,66,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,92,0,120,0,54,0,52,0,92,0,69,0,109,0,98,0,101,0,100,0,100,0,101,0,100,0,66,0,114,0,111,0,119,0,115,0,101,0,114,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,46,0,100,0,108,0,108,0,0,0,123,
      0,70,0,51,0,48,0,49,0,55,0,50,0,50,0,54,0,45,0,70,0,69,0,50,0,65,0,45,0,52,0,50,0,57,0,53,0,45,0,56,0,66,0,68,0,70,0,45,0,48,0,48,0,67,0,51,0,65,0,57,0,65,0,55,0,69,0,52,0,67,0,53,0,125,0,0,0,123,0,50,0,67,0,68,0,56,0,65,0,48,0,48,0,55,0,45,
      0,69,0,49,0,56,0,57,0,45,0,52,0,48,0,57,0,68,0,45,0,65,0,50,0,67,0,56,0,45,0,57,0,65,0,70,0,52,0,69,0,70,0,51,0,67,0,55,0,50,0,65,0,65,0,125,0,0,0,123,0,48,0,68,0,53,0,48,0,66,0,70,0,69,0,67,0,45,0,67,0,68,0,54,0,65,0,45,0,52,0,70,0,57,0,65,
      0,45,0,57,0,54,0,52,0,67,0,45,0,67,0,55,0,52,0,49,0,54,0,69,0,51,0,65,0,67,0,66,0,49,0,48,0,125,0,0,0,123,0,54,0,53,0,67,0,51,0,53,0,66,0,49,0,52,0,45,0,54,0,67,0,49,0,68,0,45,0,52,0,49,0,50,0,50,0,45,0,65,0,67,0,52,0,54,0,45,0,55,0,49,0,52,
      0,56,0,67,0,67,0,57,0,68,0,54,0,52,0,57,0,55,0,125,0,0,0,123,0,66,0,69,0,53,0,57,0,69,0,56,0,70,0,68,0,45,0,48,0,56,0,57,0,65,0,45,0,52,0,49,0,49,0,66,0,45,0,65,0,51,0,66,0,48,0,45,0,48,0,53,0,49,0,68,0,57,0,69,0,52,0,49,0,55,0,56,0,49,0,56,
      0,125,0,0,0,0,0,0,0,0,0,0,0,83,0,111,0,102,0,116,0,119,0,97,0,114,0,101,0,92,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,92,0,69,0,100,0,103,0,101,0,85,0,112,0,100,0,97,0,116,0,101,0,92,0,67,0,108,0,105,0,101,0,110,0,116,0,83,0,116,
      0,97,0,116,0,101,0,92,0,0,0,0,0,98,0,101,0,116,0,97,0,0,0,100,0,101,0,118,0,0,0,99,0,97,0,110,0,97,0,114,0,121,0,0,0,105,0,110,0,116,0,101,0,114,0,110,0,97,0,108,0,0,0,92,0,0,0,92,0,83,0,116,0,114,0,105,0,110,0,103,0,70,0,105,0,108,0,101,0,73,
      0,110,0,102,0,111,0,92,0,48,0,52,0,48,0,57,0,48,0,52,0,66,0,48,0,92,0,80,0,114,0,111,0,100,0,117,0,99,0,116,0,86,0,101,0,114,0,115,0,105,0,111,0,110,0,0,0,32,0,0,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,46,0,87,0,101,0,98,0,86,0,
      105,0,101,0,119,0,50,0,82,0,117,0,110,0,116,0,105,0,109,0,101,0,46,0,83,0,116,0,97,0,98,0,108,0,101,0,95,0,56,0,119,0,101,0,107,0,121,0,98,0,51,0,100,0,56,0,98,0,98,0,119,0,101,0,0,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,46,0,87,
      0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,82,0,117,0,110,0,116,0,105,0,109,0,101,0,46,0,66,0,101,0,116,0,97,0,95,0,56,0,119,0,101,0,107,0,121,0,98,0,51,0,100,0,56,0,98,0,98,0,119,0,101,0,0,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,
      46,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,82,0,117,0,110,0,116,0,105,0,109,0,101,0,46,0,68,0,101,0,118,0,95,0,56,0,119,0,101,0,107,0,121,0,98,0,51,0,100,0,56,0,98,0,98,0,119,0,101,0,0,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,
      0,46,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,82,0,117,0,110,0,116,0,105,0,109,0,101,0,46,0,67,0,97,0,110,0,97,0,114,0,121,0,95,0,56,0,119,0,101,0,107,0,121,0,98,0,51,0,100,0,56,0,98,0,98,0,119,0,101,0,0,0,77,0,105,0,99,0,114,0,111,0,115,
      0,111,0,102,0,116,0,46,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,82,0,117,0,110,0,116,0,105,0,109,0,101,0,46,0,73,0,110,0,116,0,101,0,114,0,110,0,97,0,108,0,95,0,56,0,119,0,101,0,107,0,121,0,98,0,51,0,100,0,56,0,98,0,98,0,119,0,101,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,84,0,0,0,0,0,0,0,232,1,0,0,0,0,0,0,76,0,111,0,97,0,100,0,101,0,114,0,32,0,115,0,107,0,105,0,112,0,112,0,101,0,100,0,32,0,97,0,110,0,32,0,105,0,110,0,99,0,111,0,109,0,112,0,97,0,116,0,105,0,98,0,108,0,101,0,32,0,118,
      0,101,0,114,0,115,0,105,0,111,0,110,0,58,0,32,0,0,0,10,0,0,0,46,0,0,0,71,101,116,67,117,114,114,101,110,116,80,97,99,107,97,103,101,73,110,102,111,0,107,0,101,0,114,0,110,0,101,0,108,0,98,0,97,0,115,0,101,0,46,0,100,0,108,0,108,0,0,0,0,0,132,
      216,102,218,168,109,14,65,150,48,140,72,248,179,164,14,71,101,116,67,117,114,114,101,110,116,65,112,112,108,105,99,97,116,105,111,110,85,115,101,114,77,111,100,101,108,73,100,0,0,75,0,101,0,114,0,110,0,101,0,108,0,51,0,50,0,46,0,100,0,108,0,
      108,0,0,0,98,0,114,0,111,0,119,0,115,0,101,0,114,0,69,0,120,0,101,0,99,0,117,0,116,0,97,0,98,0,108,0,101,0,70,0,111,0,108,0,100,0,101,0,114,0,0,0,87,0,69,0,66,0,86,0,73,0,69,0,87,0,50,0,95,0,66,0,82,0,79,0,87,0,83,0,69,0,82,0,95,0,69,0,88,0,
      69,0,67,0,85,0,84,0,65,0,66,0,76,0,69,0,95,0,70,0,79,0,76,0,68,0,69,0,82,0,0,0,117,0,115,0,101,0,114,0,68,0,97,0,116,0,97,0,70,0,111,0,108,0,100,0,101,0,114,0,0,0,87,0,69,0,66,0,86,0,73,0,69,0,87,0,50,0,95,0,85,0,83,0,69,0,82,0,95,0,68,0,65,
      0,84,0,65,0,95,0,70,0,79,0,76,0,68,0,69,0,82,0,0,0,114,0,101,0,108,0,101,0,97,0,115,0,101,0,67,0,104,0,97,0,110,0,110,0,101,0,108,0,80,0,114,0,101,0,102,0,101,0,114,0,101,0,110,0,99,0,101,0,0,0,87,0,69,0,66,0,86,0,73,0,69,0,87,0,50,0,95,0,82,
      0,69,0,76,0,69,0,65,0,83,0,69,0,95,0,67,0,72,0,65,0,78,0,78,0,69,0,76,0,95,0,80,0,82,0,69,0,70,0,69,0,82,0,69,0,78,0,67,0,69,0,0,0,42,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,83,0,111,0,102,0,116,0,119,0,97,0,114,0,101,0,92,0,80,0,111,0,108,0,105,0,
      99,0,105,0,101,0,115,0,92,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,92,0,69,0,109,0,98,0,101,0,100,0,100,0,101,0,100,0,66,0,114,0,111,0,119,0,115,0,101,0,114,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,92,0,76,0,111,0,97,0,100,0,101,
      0,114,0,79,0,118,0,101,0,114,0,114,0,105,0,100,0,101,0,92,0,0,0,67,114,101,97,116,101,87,101,98,86,105,101,119,69,110,118,105,114,111,110,109,101,110,116,87,105,116,104,79,112,116,105,111,110,115,73,110,116,101,114,110,97,108,0,0,0,0,0,0,0,0,
      32,2,128,1,0,0,0,8,32,2,128,1,0,0,0,212,219,1,128,1,0,0,0,248,163,1,128,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,16,218,1,0,24,161,1,0,240,160,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,48,161,1,0,0,0,0,0,0,0,0,0,64,161,
      1,0,0,0,0,0,0,0,0,0,0,0,0,0,16,218,1,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,64,0,0,0,24,161,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,232,217,1,0,144,161,1,0,104,161,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,168,
      161,1,0,0,0,0,0,0,0,0,0,192,161,1,0,64,161,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,232,217,1,0,1,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,64,0,0,0,144,161,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,56,218,1,0,16,162,1,0,232,161,1,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,40,162,1,0,0,0,0,0,0,0,0,0,72,162,1,0,192,161,1,0,64,161,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,56,218,1,0,2,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,64,0,0,0,16,162,1,0,0,0,0,0,0,0,0,0,0,0,0,
      0,1,0,0,0,0,0,0,0,0,0,0,0,104,218,1,0,152,162,1,0,112,162,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,176,162,1,0,0,0,0,0,0,0,0,0,192,162,1,0,0,0,0,0,0,0,0,0,0,0,0,0,104,218,1,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,64,0,0,
      0,152,162,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,136,218,1,0,16,163,1,0,232,162,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,40,163,1,0,0,0,0,0,0,0,0,0,64,163,1,0,64,161,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,218,
      1,0,1,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,64,0,0,0,16,163,1,0,0,0,0,0,0,0,0,0,0,0,0,0,183,180,15,95,0,0,0,0,2,0,0,0,47,0,0,0,128,163,1,0,128,151,1,0,82,83,68,83,204,251,224,2,100,102,11,16,76,76,68,32,80,68,66,46,1,0,0,0,87,101,98,86,105,101,
      119,50,76,111,97,100,101,114,46,100,108,108,46,112,100,98,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,196,40,0,128,1,0,0,0,44,226,0,128,1,0,0,0,36,180,0,128,1,0,0,0,224,18,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,136,199,0,128,1,0,0,0,208,8,1,128,1,0,0,0,68,181,0,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,24,166,1,0,176,218,1,0,208,218,1,0,216,164,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,36,166,1,0,184,218,1,0,240,
      218,1,0,248,164,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,49,166,1,0,192,218,1,0,24,219,1,0,32,165,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,61,166,1,0,200,218,1,0,40,219,1,0,48,165,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,72,165,1,0,0,0,0,0,98,165,1,0,0,0,0,0,120,165,1,0,0,0,0,0,0,0,0,0,0,0,0,0,138,165,1,0,0,0,0,0,152,165,1,0,0,0,0,0,168,165,1,0,0,0,0,0,184,165,1,0,0,0,0,0,0,0,0,0,0,0,0,0,204,165,1,0,0,0,0,0,0,0,0,0,0,0,0,0,246,165,1,0,0,0,
      0,0,8,166,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,71,101,116,70,105,108,101,86,101,114,115,105,111,110,73,110,102,111,83,105,122,101,87,0,0,0,71,101,116,70,105,108,101,86,101,114,115,105,111,110,73,110,102,111,87,0,0,0,86,101,114,81,117,101,114,121,
      86,97,108,117,101,87,0,0,0,0,82,101,103,67,108,111,115,101,75,101,121,0,0,0,82,101,103,71,101,116,86,97,108,117,101,87,0,0,0,0,82,101,103,79,112,101,110,75,101,121,69,120,87,0,0,0,82,101,103,81,117,101,114,121,86,97,108,117,101,69,120,87,0,0,
      0,0,71,101,116,67,117,114,114,101,110,116,80,114,111,99,101,115,115,69,120,112,108,105,99,105,116,65,112,112,85,115,101,114,77,111,100,101,108,73,68,0,0,0,67,111,84,97,115,107,77,101,109,65,108,108,111,99,0,0,0,0,67,111,84,97,115,107,77,101,
      109,70,114,101,101,0,86,69,82,83,73,79,78,46,100,108,108,0,65,68,86,65,80,73,51,50,46,100,108,108,0,83,72,69,76,76,51,50,46,100,108,108,0,111,108,101,51,50,46,100,108,108,0,0,0,0,0,0,0,0,0,0,0,0,0,111,166,1,0,0,0,0,0,5,0,0,0,4,0,0,0,130,166,
      1,0,150,166,1,0,166,166,1,0,87,101,98,86,105,101,119,50,76,111,97,100,101,114,46,100,108,108,0,0,0,0,0,108,27,0,0,176,28,0,0,20,18,0,0,70,20,0,0,174,166,1,0,197,166,1,0,227,166,1,0,12,167,1,0,1,0,2,0,3,0,4,0,67,111,109,112,97,114,101,66,114,
      111,119,115,101,114,86,101,114,115,105,111,110,115,0,67,114,101,97,116,101,67,111,114,101,87,101,98,86,105,101,119,50,69,110,118,105,114,111,110,109,101,110,116,0,67,114,101,97,116,101,67,111,114,101,87,101,98,86,105,101,119,50,69,110,118,105,
      114,111,110,109,101,110,116,87,105,116,104,79,112,116,105,111,110,115,0,71,101,116,65,118,97,105,108,97,98,108,101,67,111,114,101,87,101,98,86,105,101,119,50,66,114,111,119,115,101,114,86,101,114,115,105,111,110,83,116,114,105,110,103,0,104,
      167,1,0,0,0,0,0,0,0,0,0,82,178,1,0,240,169,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,120,172,1,0,0,0,0,0,134,172,1,0,0,0,0,0,150,172,1,0,0,0,0,0,164,172,1,0,0,0,0,0,188,172,1,0,0,0,0,0,204,172,1,0,0,0,0,0,228,172,1,0,0,0,0,0,
      242,172,1,0,0,0,0,0,254,172,1,0,0,0,0,0,18,173,1,0,0,0,0,0,34,173,1,0,0,0,0,0,54,173,1,0,0,0,0,0,80,173,1,0,0,0,0,0,94,173,1,0,0,0,0,0,104,173,1,0,0,0,0,0,116,173,1,0,0,0,0,0,134,173,1,0,0,0,0,0,152,173,1,0,0,0,0,0,170,173,1,0,0,0,0,0,192,173,
      1,0,0,0,0,0,212,173,1,0,0,0,0,0,234,173,1,0,0,0,0,0,0,174,1,0,0,0,0,0,26,174,1,0,0,0,0,0,52,174,1,0,0,0,0,0,74,174,1,0,0,0,0,0,88,174,1,0,0,0,0,0,104,174,1,0,0,0,0,0,126,174,1,0,0,0,0,0,148,174,1,0,0,0,0,0,168,174,1,0,0,0,0,0,180,174,1,0,0,0,
      0,0,198,174,1,0,0,0,0,0,216,174,1,0,0,0,0,0,234,174,1,0,0,0,0,0,250,174,1,0,0,0,0,0,12,175,1,0,0,0,0,0,28,175,1,0,0,0,0,0,54,175,1,0,0,0,0,0,66,175,1,0,0,0,0,0,78,175,1,0,0,0,0,0,92,175,1,0,0,0,0,0,104,175,1,0,0,0,0,0,144,175,1,0,0,0,0,0,166,
      175,1,0,0,0,0,0,190,175,1,0,0,0,0,0,210,175,1,0,0,0,0,0,238,175,1,0,0,0,0,0,0,176,1,0,0,0,0,0,16,176,1,0,0,0,0,0,40,176,1,0,0,0,0,0,58,176,1,0,0,0,0,0,76,176,1,0,0,0,0,0,92,176,1,0,0,0,0,0,114,176,1,0,0,0,0,0,136,176,1,0,0,0,0,0,162,176,1,0,
      0,0,0,0,180,176,1,0,0,0,0,0,194,176,1,0,0,0,0,0,214,176,1,0,0,0,0,0,240,176,1,0,0,0,0,0,4,177,1,0,0,0,0,0,18,177,1,0,0,0,0,0,38,177,1,0,0,0,0,0,50,177,1,0,0,0,0,0,70,177,1,0,0,0,0,0,86,177,1,0,0,0,0,0,102,177,1,0,0,0,0,0,132,177,1,0,0,0,0,0,
      152,177,1,0,0,0,0,0,164,177,1,0,0,0,0,0,174,177,1,0,0,0,0,0,188,177,1,0,0,0,0,0,202,177,1,0,0,0,0,0,230,177,1,0,0,0,0,0,248,177,1,0,0,0,0,0,8,178,1,0,0,0,0,0,32,178,1,0,0,0,0,0,54,178,1,0,0,0,0,0,70,178,1,0,0,0,0,0,0,0,0,0,0,0,0,0,120,172,1,
      0,0,0,0,0,134,172,1,0,0,0,0,0,150,172,1,0,0,0,0,0,164,172,1,0,0,0,0,0,188,172,1,0,0,0,0,0,204,172,1,0,0,0,0,0,228,172,1,0,0,0,0,0,242,172,1,0,0,0,0,0,254,172,1,0,0,0,0,0,18,173,1,0,0,0,0,0,34,173,1,0,0,0,0,0,54,173,1,0,0,0,0,0,80,173,1,0,0,0,
      0,0,94,173,1,0,0,0,0,0,104,173,1,0,0,0,0,0,116,173,1,0,0,0,0,0,134,173,1,0,0,0,0,0,152,173,1,0,0,0,0,0,170,173,1,0,0,0,0,0,192,173,1,0,0,0,0,0,212,173,1,0,0,0,0,0,234,173,1,0,0,0,0,0,0,174,1,0,0,0,0,0,26,174,1,0,0,0,0,0,52,174,1,0,0,0,0,0,74,
      174,1,0,0,0,0,0,88,174,1,0,0,0,0,0,104,174,1,0,0,0,0,0,126,174,1,0,0,0,0,0,148,174,1,0,0,0,0,0,168,174,1,0,0,0,0,0,180,174,1,0,0,0,0,0,198,174,1,0,0,0,0,0,216,174,1,0,0,0,0,0,234,174,1,0,0,0,0,0,250,174,1,0,0,0,0,0,12,175,1,0,0,0,0,0,28,175,
      1,0,0,0,0,0,54,175,1,0,0,0,0,0,66,175,1,0,0,0,0,0,78,175,1,0,0,0,0,0,92,175,1,0,0,0,0,0,104,175,1,0,0,0,0,0,144,175,1,0,0,0,0,0,166,175,1,0,0,0,0,0,190,175,1,0,0,0,0,0,210,175,1,0,0,0,0,0,238,175,1,0,0,0,0,0,0,176,1,0,0,0,0,0,16,176,1,0,0,0,
      0,0,40,176,1,0,0,0,0,0,58,176,1,0,0,0,0,0,76,176,1,0,0,0,0,0,92,176,1,0,0,0,0,0,114,176,1,0,0,0,0,0,136,176,1,0,0,0,0,0,162,176,1,0,0,0,0,0,180,176,1,0,0,0,0,0,194,176,1,0,0,0,0,0,214,176,1,0,0,0,0,0,240,176,1,0,0,0,0,0,4,177,1,0,0,0,0,0,18,
      177,1,0,0,0,0,0,38,177,1,0,0,0,0,0,50,177,1,0,0,0,0,0,70,177,1,0,0,0,0,0,86,177,1,0,0,0,0,0,102,177,1,0,0,0,0,0,132,177,1,0,0,0,0,0,152,177,1,0,0,0,0,0,164,177,1,0,0,0,0,0,174,177,1,0,0,0,0,0,188,177,1,0,0,0,0,0,202,177,1,0,0,0,0,0,230,177,1,
      0,0,0,0,0,248,177,1,0,0,0,0,0,8,178,1,0,0,0,0,0,32,178,1,0,0,0,0,0,54,178,1,0,0,0,0,0,70,178,1,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,67,108,111,115,101,72,97,110,100,108,101,0,193,0,67,114,101,97,116,101,69,118,101,110,116,87,0,0,205,0,67,114,101,
      97,116,101,70,105,108,101,87,0,19,1,68,101,108,101,116,101,67,114,105,116,105,99,97,108,83,101,99,116,105,111,110,0,51,1,69,110,99,111,100,101,80,111,105,110,116,101,114,0,55,1,69,110,116,101,114,67,114,105,116,105,99,97,108,83,101,99,116,105,
      111,110,0,0,102,1,69,120,105,116,80,114,111,99,101,115,115,0,125,1,70,105,110,100,67,108,111,115,101,0,131,1,70,105,110,100,70,105,114,115,116,70,105,108,101,69,120,87,0,0,148,1,70,105,110,100,78,101,120,116,70,105,108,101,87,0,167,1,70,108,
      117,115,104,70,105,108,101,66,117,102,102,101,114,115,0,0,178,1,70,114,101,101,69,110,118,105,114,111,110,109,101,110,116,83,116,114,105,110,103,115,87,0,179,1,70,114,101,101,76,105,98,114,97,114,121,0,186,1,71,101,116,65,67,80,0,0,201,1,71,
      101,116,67,80,73,110,102,111,0,222,1,71,101,116,67,111,109,109,97,110,100,76,105,110,101,65,0,223,1,71,101,116,67,111,109,109,97,110,100,76,105,110,101,87,0,4,2,71,101,116,67,111,110,115,111,108,101,77,111,100,101,0,0,8,2,71,101,116,67,111,110,
      115,111,108,101,79,117,116,112,117,116,67,80,0,0,31,2,71,101,116,67,117,114,114,101,110,116,80,114,111,99,101,115,115,0,32,2,71,101,116,67,117,114,114,101,110,116,80,114,111,99,101,115,115,73,100,0,36,2,71,101,116,67,117,114,114,101,110,116,
      84,104,114,101,97,100,73,100,0,0,64,2,71,101,116,69,110,118,105,114,111,110,109,101,110,116,83,116,114,105,110,103,115,87,0,0,66,2,71,101,116,69,110,118,105,114,111,110,109,101,110,116,86,97,114,105,97,98,108,101,87,0,78,2,71,101,116,70,105,
      108,101,65,116,116,114,105,98,117,116,101,115,87,0,0,87,2,71,101,116,70,105,108,101,84,121,112,101,0,105,2,71,101,116,76,97,115,116,69,114,114,111,114,0,0,124,2,71,101,116,77,111,100,117,108,101,70,105,108,101,78,97,109,101,87,0,0,127,2,71,101,
      116,77,111,100,117,108,101,72,97,110,100,108,101,69,120,87,0,0,128,2,71,101,116,77,111,100,117,108,101,72,97,110,100,108,101,87,0,0,160,2,71,101,116,79,69,77,67,80,0,0,183,2,71,101,116,80,114,111,99,65,100,100,114,101,115,115,0,0,189,2,71,101,
      116,80,114,111,99,101,115,115,72,101,97,112,0,0,217,2,71,101,116,83,116,97,114,116,117,112,73,110,102,111,87,0,219,2,71,101,116,83,116,100,72,97,110,100,108,101,0,0,224,2,71,101,116,83,116,114,105,110,103,84,121,112,101,87,0,0,236,2,71,101,116,
      83,121,115,116,101,109,73,110,102,111,0,242,2,71,101,116,83,121,115,116,101,109,84,105,109,101,65,115,70,105,108,101,84,105,109,101,0,80,3,72,101,97,112,65,108,108,111,99,0,84,3,72,101,97,112,70,114,101,101,0,0,87,3,72,101,97,112,82,101,65,108,
      108,111,99,0,89,3,72,101,97,112,83,105,122,101,0,0,106,3,73,110,105,116,105,97,108,105,122,101,67,114,105,116,105,99,97,108,83,101,99,116,105,111,110,65,110,100,83,112,105,110,67,111,117,110,116,0,110,3,73,110,105,116,105,97,108,105,122,101,
      83,76,105,115,116,72,101,97,100,0,114,3,73,110,116,101,114,108,111,99,107,101,100,70,108,117,115,104,83,76,105,115,116,0,132,3,73,115,68,101,98,117,103,103,101,114,80,114,101,115,101,110,116,0,139,3,73,115,80,114,111,99,101,115,115,111,114,70,
      101,97,116,117,114,101,80,114,101,115,101,110,116,0,145,3,73,115,86,97,108,105,100,67,111,100,101,80,97,103,101,0,183,3,76,67,77,97,112,83,116,114,105,110,103,87,0,0,195,3,76,101,97,118,101,67,114,105,116,105,99,97,108,83,101,99,116,105,111,
      110,0,0,200,3,76,111,97,100,76,105,98,114,97,114,121,69,120,65,0,0,201,3,76,111,97,100,76,105,98,114,97,114,121,69,120,87,0,0,202,3,76,111,97,100,76,105,98,114,97,114,121,87,0,0,245,3,77,117,108,116,105,66,121,116,101,84,111,87,105,100,101,67,
      104,97,114,0,29,4,79,117,116,112,117,116,68,101,98,117,103,83,116,114,105,110,103,87,0,0,81,4,81,117,101,114,121,80,101,114,102,111,114,109,97,110,99,101,67,111,117,110,116,101,114,0,103,4,82,97,105,115,101,69,120,99,101,112,116,105,111,110,
      0,0,203,4,82,101,115,101,116,69,118,101,110,116,0,0,212,4,82,116,108,67,97,112,116,117,114,101,67,111,110,116,101,120,116,0,219,4,82,116,108,76,111,111,107,117,112,70,117,110,99,116,105,111,110,69,110,116,114,121,0,0,221,4,82,116,108,80,99,84,
      111,70,105,108,101,72,101,97,100,101,114,0,225,4,82,116,108,85,110,119,105,110,100,69,120,0,226,4,82,116,108,86,105,114,116,117,97,108,85,110,119,105,110,100,0,0,37,5,83,101,116,69,118,101,110,116,0,0,50,5,83,101,116,70,105,108,101,80,111,105,
      110,116,101,114,69,120,0,0,64,5,83,101,116,76,97,115,116,69,114,114,111,114,0,0,89,5,83,101,116,83,116,100,72,97,110,100,108,101,0,0,125,5,83,101,116,85,110,104,97,110,100,108,101,100,69,120,99,101,112,116,105,111,110,70,105,108,116,101,114,
      0,156,5,84,101,114,109,105,110,97,116,101,80,114,111,99,101,115,115,0,0,174,5,84,108,115,65,108,108,111,99,0,0,175,5,84,108,115,70,114,101,101,0,176,5,84,108,115,71,101,116,86,97,108,117,101,0,177,5,84,108,115,83,101,116,86,97,108,117,101,0,
      190,5,85,110,104,97,110,100,108,101,100,69,120,99,101,112,116,105,111,110,70,105,108,116,101,114,0,0,221,5,86,105,114,116,117,97,108,80,114,111,116,101,99,116,0,0,223,5,86,105,114,116,117,97,108,81,117,101,114,121,0,0,233,5,87,97,105,116,70,
      111,114,83,105,110,103,108,101,79,98,106,101,99,116,69,120,0,15,6,87,105,100,101,67,104,97,114,84,111,77,117,108,116,105,66,121,116,101,0,34,6,87,114,105,116,101,67,111,110,115,111,108,101,87,0,35,6,87,114,105,116,101,70,105,108,101,0,75,69,
      82,78,69,76,51,50,46,100,108,108,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,25,4,1,0,4,66,0,0,96,72,0,0,144,178,1,0,120,157,178,1,0,160,178,1,0,176,178,1,0,4,8,16,2,0,0,2,168,178,1,0,2,17,128,230,24,1,0,22,2,8,2,0,25,
      10,2,0,10,50,6,80,96,72,0,0,196,178,1,0,105,206,178,1,0,208,178,1,0,112,2,8,0,0,0,0,1,6,2,0,6,50,2,48,1,4,1,0,4,66,0,0,1,10,4,0,10,52,6,0,10,50,6,112,9,4,1,0,4,34,0,0,168,63,0,0,1,0,0,0,195,43,0,0,77,44,0,0,4,25,1,0,77,44,0,0,1,2,1,0,2,80,0,
      0,1,20,8,0,20,100,8,0,20,84,7,0,20,52,6,0,20,50,16,112,1,21,5,0,21,52,186,0,21,1,184,0,6,80,0,0,1,4,1,0,4,130,0,0,1,15,6,0,15,100,6,0,15,52,5,0,15,18,11,112,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,17,21,8,0,21,116,9,0,21,100,7,0,21,52,6,0,21,50,17,224,
      168,63,0,0,2,0,0,0,244,51,0,0,99,52,0,0,28,25,1,0,0,0,0,0,198,52,0,0,209,52,0,0,28,25,1,0,0,0,0,0,1,6,2,0,6,50,2,80,17,10,4,0,10,52,8,0,10,82,6,112,168,63,0,0,4,0,0,0,11,53,0,0,42,53,0,0,51,25,1,0,0,0,0,0,0,53,0,0,66,53,0,0,76,25,1,0,0,0,0,0,
      75,53,0,0,86,53,0,0,51,25,1,0,0,0,0,0,75,53,0,0,87,53,0,0,76,25,1,0,0,0,0,0,1,6,2,0,6,50,2,80,1,6,2,0,6,50,2,80,9,26,6,0,26,52,15,0,26,114,22,224,20,112,19,96,168,63,0,0,1,0,0,0,221,53,0,0,195,54,0,0,96,25,1,0,195,54,0,0,1,6,2,0,6,82,2,80,1,
      15,6,0,15,100,7,0,15,52,6,0,15,50,11,112,1,15,6,0,15,116,3,0,10,100,2,0,5,52,1,0,1,13,4,0,13,52,18,0,13,242,6,112,1,16,6,0,16,100,7,0,16,52,6,0,16,50,12,112,1,31,12,0,31,116,23,0,31,100,22,0,31,52,21,0,31,210,24,240,22,224,20,208,18,192,16,80,
      1,13,4,0,13,52,9,0,13,50,6,80,1,15,6,0,15,100,15,0,15,52,14,0,15,146,11,112,1,28,12,0,28,100,16,0,28,84,15,0,28,52,14,0,28,114,24,240,22,224,20,208,18,192,16,112,1,21,9,0,21,116,5,0,21,100,4,0,21,84,3,0,21,52,2,0,21,224,0,0,25,28,3,0,14,1,28,
      0,2,80,0,0,140,143,0,0,208,0,0,0,1,38,12,0,38,104,5,0,27,116,18,0,27,100,17,0,27,52,16,0,27,178,20,240,18,224,16,80,1,20,8,0,20,100,17,0,20,84,16,0,20,52,15,0,20,178,16,112,9,24,2,0,24,210,20,48,168,63,0,0,1,0,0,0,71,73,0,0,103,73,0,0,150,25,
      1,0,103,73,0,0,1,7,3,0,7,130,3,80,2,48,0,0,9,13,1,0,13,130,0,0,168,63,0,0,1,0,0,0,173,73,0,0,188,73,0,0,53,26,1,0,188,73,0,0,1,7,3,0,7,66,3,80,2,48,0,0,1,21,8,0,21,116,8,0,21,100,7,0,21,52,6,0,21,50,17,224,0,0,0,0,2,1,3,0,2,22,0,6,1,112,0,0,
      1,0,0,0,17,6,2,0,6,50,2,48,168,63,0,0,1,0,0,0,182,80,0,0,204,80,0,0,203,26,1,0,0,0,0,0,1,6,2,0,6,50,2,80,1,22,4,0,22,52,12,0,22,146,15,80,9,6,2,0,6,50,2,48,168,63,0,0,1,0,0,0,153,82,0,0,232,82,0,0,251,26,1,0,51,83,0,0,1,6,2,0,6,50,2,80,17,15,
      4,0,15,52,6,0,15,50,11,112,168,63,0,0,1,0,0,0,89,83,0,0,98,83,0,0,225,26,1,0,0,0,0,0,1,6,2,0,6,50,2,80,1,19,8,0,19,52,12,0,19,82,12,240,10,224,8,112,7,96,6,80,1,29,12,0,29,116,11,0,29,100,10,0,29,84,9,0,29,52,8,0,29,50,25,240,23,224,21,192,1,
      18,2,0,18,114,11,80,1,11,1,0,11,98,0,0,1,24,10,0,24,100,11,0,24,84,10,0,24,52,9,0,24,50,20,240,18,224,16,112,1,24,10,0,24,100,10,0,24,84,9,0,24,52,8,0,24,50,20,240,18,224,16,112,17,15,4,0,15,52,6,0,15,50,11,112,168,63,0,0,1,0,0,0,189,90,0,0,
      199,90,0,0,225,26,1,0,0,0,0,0,1,6,2,0,6,50,2,80,17,15,4,0,15,52,6,0,15,50,11,112,168,63,0,0,1,0,0,0,249,90,0,0,3,91,0,0,225,26,1,0,0,0,0,0,1,6,2,0,6,50,2,80,1,15,4,0,15,52,6,0,15,50,11,112,1,24,10,0,24,100,12,0,24,84,11,0,24,52,10,0,24,82,20,
      240,18,224,16,112,1,4,1,0,4,98,0,0,1,6,2,0,6,82,2,48,1,24,10,0,24,100,14,0,24,84,13,0,24,52,12,0,24,114,20,240,18,224,16,112,9,4,1,0,4,66,0,0,168,63,0,0,1,0,0,0,82,99,0,0,90,99,0,0,1,0,0,0,90,99,0,0,1,22,10,0,22,84,16,0,22,52,14,0,22,114,18,
      240,16,224,14,192,12,112,11,96,9,10,4,0,10,52,6,0,10,50,6,112,168,63,0,0,1,0,0,0,205,109,0,0,0,110,0,0,32,27,1,0,0,110,0,0,1,6,2,0,6,50,2,80,1,0,0,0,1,0,0,0,1,0,0,0,1,19,8,0,19,228,4,0,15,116,3,0,11,100,2,0,7,52,1,0,1,30,10,0,30,52,14,0,30,50,
      26,240,24,224,22,208,20,192,18,112,17,96,16,80,1,15,6,0,15,100,9,0,15,52,8,0,15,82,11,112,25,43,11,0,25,104,14,0,21,1,30,0,14,240,12,224,10,208,8,192,6,112,5,96,4,48,0,0,24,205,0,0,2,0,0,0,197,120,0,0,37,121,0,0,242,27,1,0,37,121,0,0,229,119,
      0,0,66,121,0,0,8,28,1,0,0,0,0,0,211,0,0,0,1,6,2,0,6,82,2,80,1,6,2,0,6,82,2,80,25,19,8,0,19,1,21,0,12,240,10,208,8,192,6,112,5,96,4,48,168,63,0,0,4,0,0,0,138,122,0,0,213,122,0,0,64,27,1,0,213,122,0,0,138,122,0,0,81,123,0,0,111,27,1,0,0,0,0,0,
      209,123,0,0,215,123,0,0,64,27,1,0,213,122,0,0,209,123,0,0,215,123,0,0,111,27,1,0,0,0,0,0,1,6,2,0,6,50,2,80,1,7,3,0,7,66,3,80,2,48,0,0,1,28,12,0,28,100,13,0,28,84,12,0,28,52,10,0,28,50,24,240,22,224,20,208,18,192,16,112,1,27,10,0,27,100,22,0,
      27,84,21,0,27,52,20,0,27,242,20,240,18,224,16,112,1,25,10,0,25,116,9,0,25,100,8,0,25,84,7,0,25,52,6,0,25,50,21,224,9,25,10,0,25,116,12,0,25,100,11,0,25,52,10,0,25,82,21,240,19,224,17,208,168,63,0,0,2,0,0,0,30,132,0,0,85,133,0,0,1,0,0,0,143,133,
      0,0,117,133,0,0,143,133,0,0,1,0,0,0,143,133,0,0,9,21,8,0,21,116,8,0,21,100,7,0,21,52,6,0,21,50,17,224,168,63,0,0,1,0,0,0,199,133,0,0,61,134,0,0,1,0,0,0,83,134,0,0,25,42,10,0,28,1,49,0,13,240,11,224,9,208,7,192,5,112,4,96,3,48,2,80,140,143,0,
      0,112,1,0,0,1,26,10,0,26,52,20,0,26,178,22,240,20,224,18,208,16,192,14,112,13,96,12,80,25,39,10,0,25,1,39,0,13,240,11,224,9,208,7,192,5,112,4,96,3,48,2,80,140,143,0,0,40,1,0,0,1,2,1,0,2,48,0,0,1,0,0,0,1,5,2,0,5,116,1,0,1,20,8,0,20,100,14,0,20,
      84,13,0,20,52,12,0,20,146,16,112,1,28,12,0,28,100,12,0,28,84,11,0,28,52,10,0,28,50,24,240,22,224,20,208,18,192,16,112,1,10,2,0,10,50,6,48,1,9,2,0,9,146,2,80,1,9,2,0,9,114,2,80,17,15,4,0,15,52,6,0,15,50,11,112,168,63,0,0,1,0,0,0,129,156,0,0,145,
      156,0,0,225,26,1,0,0,0,0,0,1,6,2,0,6,50,2,80,17,15,4,0,15,52,6,0,15,50,11,112,168,63,0,0,1,0,0,0,193,156,0,0,215,156,0,0,225,26,1,0,0,0,0,0,1,6,2,0,6,50,2,80,17,15,4,0,15,52,6,0,15,50,11,112,168,63,0,0,1,0,0,0,9,157,0,0,57,157,0,0,225,26,1,0,
      0,0,0,0,1,6,2,0,6,50,2,80,17,15,4,0,15,52,6,0,15,50,11,112,168,63,0,0,1,0,0,0,105,157,0,0,119,157,0,0,225,26,1,0,0,0,0,0,1,6,2,0,6,50,2,80,17,6,2,0,6,50,2,48,168,63,0,0,1,0,0,0,158,157,0,0,181,157,0,0,43,28,1,0,0,0,0,0,1,6,2,0,6,50,2,80,1,28,
      11,0,28,116,23,0,28,100,22,0,28,84,21,0,28,52,20,0,28,1,18,0,21,224,0,0,25,35,10,0,20,52,18,0,20,114,16,240,14,224,12,208,10,192,8,112,7,96,6,80,140,143,0,0,56,0,0,0,1,6,2,0,6,114,2,48,17,15,6,0,15,100,8,0,15,52,7,0,15,50,11,112,168,63,0,0,1,
      0,0,0,181,164,0,0,4,165,0,0,68,28,1,0,0,0,0,0,1,6,2,0,6,50,2,80,1,25,6,0,25,52,12,0,25,114,18,112,17,96,16,80,25,43,7,0,26,100,244,0,26,52,243,0,26,1,240,0,11,80,0,0,140,143,0,0,112,7,0,0,17,15,4,0,15,52,6,0,15,50,11,112,168,63,0,0,1,0,0,0,237,
      168,0,0,120,170,0,0,225,26,1,0,0,0,0,0,1,6,2,0,6,50,2,80,25,46,9,0,29,100,196,0,29,52,195,0,29,1,190,0,14,224,12,112,11,80,0,0,140,143,0,0,224,5,0,0,1,20,8,0,20,100,10,0,20,84,9,0,20,52,8,0,20,82,16,112,1,7,1,0,7,66,0,0,17,17,8,0,17,52,17,0,
      17,114,13,224,11,208,9,192,7,112,6,96,168,63,0,0,2,0,0,0,117,175,0,0,51,176,0,0,93,28,1,0,0,0,0,0,165,176,0,0,189,176,0,0,93,28,1,0,0,0,0,0,1,6,2,0,6,50,2,80,17,15,4,0,15,52,6,0,15,50,11,112,168,63,0,0,1,0,0,0,218,176,0,0,240,176,0,0,225,26,
      1,0,0,0,0,0,1,6,2,0,6,50,2,80,1,12,2,0,12,114,5,80,17,15,4,0,15,52,6,0,15,50,11,112,168,63,0,0,1,0,0,0,174,178,0,0,23,179,0,0,126,28,1,0,0,0,0,0,1,6,2,0,6,50,2,80,17,18,6,0,18,52,16,0,18,178,14,224,12,112,11,96,168,63,0,0,1,0,0,0,76,179,0,0,
      244,179,0,0,153,28,1,0,0,0,0,0,1,6,2,0,6,50,2,80,1,25,10,0,25,116,11,0,25,100,10,0,25,84,9,0,25,52,8,0,25,82,21,224,1,25,10,0,25,116,13,0,25,100,12,0,25,84,11,0,25,52,10,0,25,114,21,224,1,28,10,0,28,52,20,0,28,178,21,240,19,224,17,208,15,192,
      13,112,12,96,11,80,1,28,12,0,28,100,14,0,28,84,13,0,28,52,12,0,28,82,24,240,22,224,20,208,18,192,16,112,25,48,11,0,31,52,113,0,31,1,102,0,16,240,14,224,12,208,10,192,8,112,7,96,6,80,0,0,140,143,0,0,32,3,0,0,25,43,7,0,26,116,86,0,26,52,85,0,26,
      1,82,0,11,80,0,0,140,143,0,0,128,2,0,0,1,20,8,0,20,100,12,0,20,84,11,0,20,52,10,0,20,114,16,112,1,15,6,0,15,100,11,0,15,52,10,0,15,114,11,112,1,6,3,0,6,52,2,0,6,112,0,0,1,9,1,0,9,162,0,0,17,20,6,0,20,100,9,0,20,52,8,0,20,82,16,112,168,63,0,0,
      1,0,0,0,215,199,0,0,15,200,0,0,182,28,1,0,0,0,0,0,1,6,2,0,6,50,2,80,1,10,4,0,10,52,7,0,10,50,6,112,1,4,1,0,4,66,0,0,1,4,1,0,4,66,0,0,1,4,1,0,4,66,0,0,1,4,1,0,4,66,0,0,2,2,4,0,3,22,0,6,2,96,1,112,1,0,0,0,1,8,1,0,8,66,0,0,1,9,1,0,9,98,0,0,1,10,
      4,0,10,52,13,0,10,114,6,112,1,8,4,0,8,114,4,112,3,96,2,48,1,0,0,0,17,10,4,0,10,52,6,0,10,50,6,112,168,63,0,0,1,0,0,0,41,217,0,0,59,217,0,0,208,28,1,0,0,0,0,0,1,6,2,0,6,50,2,80,1,20,6,0,20,100,7,0,20,52,6,0,20,50,16,112,17,21,8,0,21,116,10,0,
      21,100,9,0,21,52,8,0,21,82,17,240,168,63,0,0,1,0,0,0,71,220,0,0,142,220,0,0,43,28,1,0,0,0,0,0,1,6,2,0,6,50,2,80,25,45,13,53,31,116,20,0,27,100,19,0,23,52,18,0,19,51,14,178,10,240,8,224,6,208,4,192,2,80,0,0,140,143,0,0,80,0,0,0,1,15,6,0,15,100,
      17,0,15,52,16,0,15,210,11,112,25,45,13,85,31,116,20,0,27,100,19,0,23,52,18,0,19,83,14,178,10,240,8,224,6,208,4,192,2,80,0,0,140,143,0,0,88,0,0,0,1,8,1,0,8,98,0,0,17,15,4,0,15,52,6,0,15,50,11,112,168,63,0,0,1,0,0,0,245,226,0,0,80,227,0,0,233,
      28,1,0,0,0,0,0,1,6,2,0,6,50,2,80,17,27,10,0,27,100,12,0,27,52,11,0,27,50,23,240,21,224,19,208,17,192,15,112,168,63,0,0,1,0,0,0,224,227,0,0,17,228,0,0,3,29,1,0,0,0,0,0,1,6,2,0,6,50,2,80,1,23,10,0,23,52,23,0,23,178,16,240,14,224,12,208,10,192,
      8,112,7,96,6,80,25,42,11,0,28,52,40,0,28,1,32,0,16,240,14,224,12,208,10,192,8,112,7,96,6,80,0,0,140,143,0,0,240,0,0,0,25,45,9,0,27,84,144,2,27,52,142,2,27,1,138,2,14,224,12,112,11,96,0,0,140,143,0,0,64,20,0,0,25,49,11,0,31,84,150,2,31,52,148,
      2,31,1,142,2,18,240,16,224,14,192,12,112,11,96,0,0,140,143,0,0,96,20,0,0,17,10,4,0,10,52,8,0,10,82,6,112,168,63,0,0,1,0,0,0,242,239,0,0,112,240,0,0,26,29,1,0,0,0,0,0,1,6,2,0,6,50,2,80,25,31,5,0,13,1,138,0,6,224,4,208,2,192,0,0,140,143,0,0,16,
      4,0,0,33,40,10,0,40,244,133,0,32,116,134,0,24,100,135,0,16,84,136,0,8,52,137,0,64,241,0,0,155,241,0,0,52,192,1,0,33,0,0,0,64,241,0,0,155,241,0,0,52,192,1,0,1,11,5,0,11,100,3,0,11,52,2,0,11,112,0,0,25,19,1,0,4,162,0,0,140,143,0,0,64,0,0,0,1,10,
      4,0,10,52,10,0,10,114,6,112,1,14,2,0,14,50,10,48,1,24,6,0,24,84,7,0,24,52,6,0,24,50,20,96,0,0,0,0,1,0,0,0,0,0,0,0,1,4,1,0,4,18,0,0,1,23,10,0,23,84,12,0,23,52,11,0,23,50,19,240,17,224,15,208,13,192,11,112,1,9,1,0,9,66,0,0,17,15,4,0,15,52,7,0,
      15,50,11,112,168,63,0,0,1,0,0,0,252,4,1,0,6,5,1,0,51,29,1,0,0,0,0,0,1,6,2,0,6,50,2,80,25,31,8,0,16,52,15,0,16,114,12,240,10,224,8,112,7,96,6,80,140,143,0,0,48,0,0,0,17,15,4,0,15,52,6,0,15,50,11,112,168,63,0,0,1,0,0,0,109,10,1,0,173,10,1,0,233,
      28,1,0,0,0,0,0,1,6,2,0,6,50,2,80,0,0,0,0,1,10,3,0,10,104,2,0,4,162,0,0,1,8,2,0,8,146,4,48,25,38,9,0,24,104,14,0,20,1,30,0,9,224,7,112,6,96,5,48,4,80,0,0,140,143,0,0,208,0,0,0,1,6,2,0,6,18,2,48,1,11,3,0,11,104,5,0,7,194,0,0,1,4,1,0,4,2,0,0,1,
      27,8,0,27,116,9,0,27,100,8,0,27,52,7,0,27,50,20,80,9,15,6,0,15,100,9,0,15,52,8,0,15,50,11,112,168,63,0,0,1,0,0,0,58,24,1,0,65,24,1,0,75,29,1,0,65,24,1,0,1,6,2,0,6,50,2,80,1,5,2,0,5,50,1,96,1,7,4,0,7,50,3,48,2,112,1,96,1,7,4,0,7,114,3,48,2,112,
      1,96,1,12,6,0,12,1,27,0,5,48,4,112,3,96,2,224,1,8,5,0,8,162,4,48,3,80,2,112,1,96,0,0,1,21,9,0,21,104,19,0,13,1,40,0,6,48,5,80,4,112,3,96,2,224,0,0,1,37,14,0,37,104,10,0,28,120,11,0,19,1,25,0,12,48,11,80,10,112,9,96,8,192,6,208,4,224,2,240,1,
      7,4,0,7,146,3,48,2,112,1,96,1,11,6,0,11,82,7,48,6,112,5,96,4,224,2,240,1,11,6,0,11,1,77,0,4,48,3,80,2,112,1,96,1,6,3,0,6,66,2,112,1,96,0,0,1,6,3,0,6,162,2,112,1,96,0,0,1,7,4,0,7,178,3,48,2,112,1,96,1,16,8,0,16,1,19,0,9,48,8,112,7,96,6,192,4,
      224,2,240,1,14,7,0,14,1,76,0,7,48,6,112,5,96,4,224,2,240,0,0,1,11,6,0,11,50,7,48,6,112,5,96,4,224,2,240,1,8,5,0,8,66,4,48,3,80,2,112,1,96,0,0,1,10,6,0,10,50,6,48,5,80,4,112,3,96,2,224,1,7,4,0,7,82,3,48,2,112,1,96,0,0,0,0,236,48,0,0,0,0,0,0,80,
      195,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,104,195,1,0,144,195,1,0,0,0,0,0,0,0,0,0,0,0,0,0,16,0,0,0,232,217,1,0,0,0,0,0,255,255,255,255,0,0,0,0,24,0,0,0,0,49,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,16,218,1,0,0,0,0,0,255,255,255,255,0,0,0,0,
      24,0,0,0,64,48,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,236,48,0,0,0,0,0,0,216,195,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,248,195,1,0,104,195,1,0,144,195,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,56,218,1,0,0,0,0,0,255,255,255,255,0,0,0,0,
      24,0,0,0,92,49,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,236,48,0,0,0,0,0,0,64,196,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,88,196,1,0,144,195,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,218,1,0,0,0,0,0,255,255,255,255,0,0,0,0,24,0,0,0,20,124,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,255,255,255,255,1,0,0,0,2,0,0,0,47,32,0,0,0,0,0,0,0,248,0,0,0,0,0,0,205,93,32,210,102,212,255,255,50,162,223,45,153,43,0,0,2,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,255,
      255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,16,16,16,16,16,16,16,16,16,
      16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,0,0,0,0,0,0,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,0,0,0,0,0,0,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,
      90,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,16,16,16,16,16,16,16,16,16,16,16,16,
      16,16,16,16,16,16,16,16,16,16,16,16,16,16,0,0,0,0,0,0,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,0,0,0,0,0,0,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,
      83,84,85,86,87,88,89,90,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,4,8,0,0,0,0,0,0,0,0,0,0,0,0,164,3,0,0,96,130,121,130,33,0,0,0,0,0,0,0,166,223,0,0,0,0,0,0,161,165,0,0,0,0,0,0,129,159,224,252,0,0,0,0,64,126,128,252,0,0,0,0,168,3,0,0,193,163,218,163,32,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,129,254,0,0,0,0,0,0,64,254,0,0,0,0,0,0,181,3,0,0,193,163,218,163,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,129,254,0,0,0,0,0,0,65,254,0,0,0,0,0,0,182,3,0,0,207,162,228,162,26,0,229,162,232,162,
      91,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,129,254,0,0,0,0,0,0,64,126,161,254,0,0,0,0,81,5,0,0,81,218,94,218,32,0,95,218,106,218,50,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,129,211,216,222,224,249,0,0,49,126,129,254,0,0,0,0,160,67,1,128,1,0,0,0,1,0,0,0,0,
      0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,248,214,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,248,214,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,248,214,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,248,214,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,248,214,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,48,216,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,70,1,128,1,0,0,0,160,71,1,128,1,0,0,0,176,58,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,144,213,1,128,
      1,0,0,0,80,208,1,128,1,0,0,0,67,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,2,32,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,32,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,12,0,0,0,8,0,0,0,162,72,1,128,1,0,0,0,0,0,0,0,0,0,0,0,117,152,0,0,254,255,255,255,0,0,0,0,0,0,0,0,200,216,1,128,1,0,0,0,80,237,1,128,1,0,0,0,80,237,
      1,128,1,0,0,0,80,237,1,128,1,0,0,0,80,237,1,128,1,0,0,0,80,237,1,128,1,0,0,0,80,237,1,128,1,0,0,0,80,237,1,128,1,0,0,0,80,237,1,128,1,0,0,0,80,237,1,128,1,0,0,0,127,127,127,127,127,127,127,127,204,216,1,128,1,0,0,0,84,237,1,128,1,0,0,0,84,237,
      1,128,1,0,0,0,84,237,1,128,1,0,0,0,84,237,1,128,1,0,0,0,84,237,1,128,1,0,0,0,84,237,1,128,1,0,0,0,84,237,1,128,1,0,0,0,46,0,0,0,46,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,0,0,0,0,0,0,0,0,254,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,96,33,1,128,
      1,0,0,0,0,0,0,0,0,0,0,0,46,63,65,86,98,97,100,95,97,108,108,111,99,64,115,116,100,64,64,0,0,0,0,0,96,33,1,128,1,0,0,0,0,0,0,0,0,0,0,0,46,63,65,86,101,120,99,101,112,116,105,111,110,64,115,116,100,64,64,0,0,0,0,0,96,33,1,128,1,0,0,0,0,0,0,0,0,
      0,0,0,46,63,65,86,98,97,100,95,97,114,114,97,121,95,110,101,119,95,108,101,110,103,116,104,64,115,116,100,64,64,0,0,96,33,1,128,1,0,0,0,0,0,0,0,0,0,0,0,46,63,65,86,116,121,112,101,95,105,110,102,111,64,64,0,96,33,1,128,1,0,0,0,0,0,0,0,0,0,0,
      0,46,63,65,86,98,97,100,95,101,120,99,101,112,116,105,111,110,64,115,116,100,64,64,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,182,29,1,128,1,0,0,0,194,29,1,128,1,0,0,0,206,29,1,128,1,0,0,0,0,0,0,0,0,0,0,0,45,30,1,128,1,
      0,0,0,57,30,1,128,1,0,0,0,69,30,1,128,1,0,0,0,81,30,1,128,1,0,0,0,0,0,0,0,0,0,0,0,176,30,1,128,1,0,0,0,0,0,0,0,0,0,0,0,15,31,1,128,1,0,0,0,27,31,1,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,16,0,0,77,16,0,0,4,194,1,0,77,16,0,0,226,16,0,0,12,194,1,0,226,16,0,0,20,18,0,0,24,194,1,0,20,18,0,0,151,19,0,0,36,194,1,0,151,19,0,0,70,20,0,0,52,194,1,0,70,20,0,
      0,233,22,0,0,68,194,1,0,233,22,0,0,108,27,0,0,92,194,1,0,108,27,0,0,9,28,0,0,124,194,1,0,9,28,0,0,176,28,0,0,136,194,1,0,191,28,0,0,240,29,0,0,152,194,1,0,240,29,0,0,76,30,0,0,168,194,1,0,76,30,0,0,180,30,0,0,220,178,1,0,194,30,0,0,255,30,0,
      0,4,194,1,0,0,31,0,0,193,31,0,0,180,194,1,0,194,31,0,0,11,32,0,0,168,194,1,0,16,32,0,0,205,32,0,0,192,194,1,0,205,32,0,0,138,33,0,0,136,194,1,0,138,33,0,0,115,35,0,0,204,194,1,0,115,35,0,0,88,37,0,0,224,194,1,0,88,37,0,0,144,37,0,0,4,194,1,0,
      144,37,0,0,28,38,0,0,244,194,1,0,28,38,0,0,78,38,0,0,168,194,1,0,78,38,0,0,194,38,0,0,4,195,1,0,224,38,0,0,18,39,0,0,168,194,1,0,18,39,0,0,135,39,0,0,20,195,1,0,136,39,0,0,216,39,0,0,12,194,1,0,216,39,0,0,47,40,0,0,12,194,1,0,48,40,0,0,150,40,
      0,0,36,195,1,0,152,40,0,0,171,40,0,0,128,178,1,0,196,40,0,0,148,41,0,0,228,178,1,0,148,41,0,0,251,41,0,0,212,178,1,0,252,41,0,0,92,42,0,0,212,178,1,0,92,42,0,0,191,42,0,0,212,178,1,0,192,42,0,0,1,43,0,0,220,178,1,0,4,43,0,0,44,43,0,0,220,178,
      1,0,44,43,0,0,104,43,0,0,212,178,1,0,104,43,0,0,127,43,0,0,220,178,1,0,128,43,0,0,186,43,0,0,212,178,1,0,188,43,0,0,84,44,0,0,240,178,1,0,84,44,0,0,141,44,0,0,220,178,1,0,144,44,0,0,180,44,0,0,212,178,1,0,180,44,0,0,253,44,0,0,212,178,1,0,0,
      45,0,0,41,45,0,0,212,178,1,0,44,45,0,0,183,45,0,0,212,178,1,0,184,45,0,0,24,46,0,0,24,179,1,0,24,46,0,0,45,46,0,0,220,178,1,0,48,46,0,0,100,46,0,0,220,178,1,0,100,46,0,0,148,46,0,0,220,178,1,0,148,46,0,0,168,46,0,0,220,178,1,0,168,46,0,0,208,
      46,0,0,220,178,1,0,208,46,0,0,229,46,0,0,220,178,1,0,240,46,0,0,58,48,0,0,44,179,1,0,64,48,0,0,114,48,0,0,212,178,1,0,136,48,0,0,202,48,0,0,228,178,1,0,0,49,0,0,60,49,0,0,212,178,1,0,92,49,0,0,152,49,0,0,212,178,1,0,152,49,0,0,184,49,0,0,60,
      179,1,0,184,49,0,0,216,49,0,0,60,179,1,0,216,49,0,0,121,51,0,0,68,179,1,0,144,51,0,0,187,51,0,0,212,178,1,0,188,51,0,0,210,52,0,0,100,179,1,0,212,52,0,0,88,53,0,0,168,179,1,0,88,53,0,0,168,53,0,0,220,178,1,0,168,53,0,0,217,54,0,0,12,180,1,0,
      220,54,0,0,25,55,0,0,60,180,1,0,28,55,0,0,183,55,0,0,228,178,1,0,184,55,0,0,81,56,0,0,76,180,1,0,84,56,0,0,236,56,0,0,92,180,1,0,236,56,0,0,122,57,0,0,104,180,1,0,124,57,0,0,38,58,0,0,212,178,1,0,40,58,0,0,187,58,0,0,220,178,1,0,188,58,0,0,145,
      61,0,0,120,180,1,0,148,61,0,0,64,62,0,0,148,180,1,0,92,62,0,0,119,62,0,0,220,178,1,0,144,62,0,0,204,62,0,0,228,178,1,0,204,62,0,0,8,63,0,0,228,178,1,0,8,63,0,0,168,63,0,0,160,180,1,0,168,63,0,0,179,65,0,0,176,180,1,0,180,65,0,0,214,66,0,0,204,
      180,1,0,164,68,0,0,214,68,0,0,220,178,1,0,216,68,0,0,247,69,0,0,228,180,1,0,28,70,0,0,118,71,0,0,248,180,1,0,120,71,0,0,138,71,0,0,220,178,1,0,140,71,0,0,164,71,0,0,212,178,1,0,164,71,0,0,182,71,0,0,220,178,1,0,184,71,0,0,208,71,0,0,212,178,
      1,0,208,71,0,0,10,72,0,0,212,178,1,0,12,72,0,0,95,72,0,0,228,178,1,0,96,72,0,0,31,73,0,0,20,181,1,0,32,73,0,0,113,73,0,0,40,181,1,0,116,73,0,0,225,73,0,0,84,181,1,0,232,73,0,0,23,74,0,0,212,178,1,0,60,74,0,0,162,74,0,0,228,178,1,0,164,74,0,0,
      49,75,0,0,128,181,1,0,52,75,0,0,89,75,0,0,212,178,1,0,92,75,0,0,134,75,0,0,212,178,1,0,176,75,0,0,216,75,0,0,220,178,1,0,216,75,0,0,241,75,0,0,220,178,1,0,244,75,0,0,4,76,0,0,220,178,1,0,4,76,0,0,24,76,0,0,220,178,1,0,24,76,0,0,42,76,0,0,220,
      178,1,0,44,76,0,0,70,76,0,0,220,178,1,0,96,76,0,0,112,76,0,0,152,181,1,0,128,76,0,0,16,78,0,0,164,181,1,0,212,78,0,0,247,78,0,0,220,178,1,0,252,78,0,0,12,79,0,0,220,178,1,0,12,79,0,0,73,79,0,0,212,178,1,0,84,79,0,0,148,79,0,0,212,178,1,0,148,
      79,0,0,239,79,0,0,220,178,1,0,4,80,0,0,57,80,0,0,220,178,1,0,60,80,0,0,76,80,0,0,220,178,1,0,76,80,0,0,96,80,0,0,220,178,1,0,96,80,0,0,112,80,0,0,220,178,1,0,112,80,0,0,159,80,0,0,212,178,1,0,168,80,0,0,220,80,0,0,168,181,1,0,8,81,0,0,99,81,
      0,0,212,178,1,0,100,81,0,0,174,81,0,0,212,178,1,0,188,81,0,0,120,82,0,0,208,181,1,0,120,82,0,0,57,83,0,0,220,181,1,0,60,83,0,0,116,83,0,0,4,182,1,0,116,83,0,0,250,84,0,0,48,182,1,0,252,84,0,0,89,85,0,0,212,178,1,0,92,85,0,0,27,87,0,0,68,182,
      1,0,68,87,0,0,140,87,0,0,96,182,1,0,140,87,0,0,198,87,0,0,104,182,1,0,216,87,0,0,136,89,0,0,112,182,1,0,136,89,0,0,158,90,0,0,136,182,1,0,160,90,0,0,219,90,0,0,160,182,1,0,220,90,0,0,23,91,0,0,204,182,1,0,32,91,0,0,88,91,0,0,220,178,1,0,88,91,
      0,0,196,91,0,0,228,178,1,0,196,91,0,0,222,91,0,0,220,178,1,0,224,91,0,0,250,91,0,0,220,178,1,0,252,91,0,0,61,92,0,0,248,182,1,0,64,92,0,0,70,93,0,0,4,183,1,0,72,93,0,0,172,93,0,0,24,179,1,0,172,93,0,0,233,93,0,0,228,178,1,0,0,94,0,0,130,95,0,
      0,24,179,1,0,132,95,0,0,151,95,0,0,28,183,1,0,152,95,0,0,1,96,0,0,36,183,1,0,4,96,0,0,216,96,0,0,136,182,1,0,216,96,0,0,30,97,0,0,220,178,1,0,32,97,0,0,71,98,0,0,44,183,1,0,136,98,0,0,35,99,0,0,60,180,1,0,64,99,0,0,96,99,0,0,68,183,1,0,100,102,
      0,0,142,102,0,0,28,183,1,0,144,102,0,0,53,109,0,0,100,183,1,0,192,109,0,0,13,110,0,0,124,183,1,0,32,110,0,0,56,110,0,0,168,183,1,0,64,110,0,0,65,110,0,0,172,183,1,0,80,110,0,0,81,110,0,0,176,183,1,0,140,110,0,0,226,110,0,0,220,178,1,0,228,110,
      0,0,43,111,0,0,220,178,1,0,44,111,0,0,78,111,0,0,220,178,1,0,80,111,0,0,105,111,0,0,220,178,1,0,108,111,0,0,43,112,0,0,60,180,1,0,44,112,0,0,121,112,0,0,212,178,1,0,124,112,0,0,155,112,0,0,220,178,1,0,164,112,0,0,139,113,0,0,180,183,1,0,140,
      113,0,0,90,114,0,0,200,183,1,0,92,114,0,0,35,115,0,0,224,183,1,0,156,116,0,0,22,117,0,0,212,178,1,0,160,118,0,0,134,121,0,0,240,183,1,0,136,121,0,0,216,123,0,0,72,184,1,0,20,124,0,0,80,124,0,0,212,178,1,0,80,124,0,0,215,124,0,0,212,178,1,0,216,
      124,0,0,8,125,0,0,228,178,1,0,8,125,0,0,242,125,0,0,184,184,1,0,244,125,0,0,124,126,0,0,24,179,1,0,124,126,0,0,16,129,0,0,212,184,1,0,16,129,0,0,77,130,0,0,236,184,1,0,80,130,0,0,146,131,0,0,236,184,1,0,148,131,0,0,149,133,0,0,4,185,1,0,152,
      133,0,0,89,134,0,0,68,185,1,0,92,134,0,0,107,139,0,0,112,185,1,0,108,139,0,0,61,140,0,0,144,185,1,0,64,140,0,0,47,143,0,0,168,185,1,0,48,143,0,0,139,143,0,0,200,185,1,0,140,143,0,0,169,143,0,0,220,178,1,0,192,143,0,0,225,143,0,0,208,185,1,0,
      228,143,0,0,67,144,0,0,212,178,1,0,68,144,0,0,118,144,0,0,220,178,1,0,120,144,0,0,175,144,0,0,212,178,1,0,176,144,0,0,225,144,0,0,212,185,1,0,228,144,0,0,37,145,0,0,212,178,1,0,40,145,0,0,98,145,0,0,220,178,1,0,100,145,0,0,172,145,0,0,212,178,
      1,0,172,145,0,0,242,145,0,0,212,178,1,0,244,145,0,0,58,146,0,0,212,178,1,0,60,146,0,0,141,146,0,0,228,178,1,0,144,146,0,0,241,146,0,0,60,180,1,0,244,146,0,0,208,147,0,0,220,185,1,0,208,147,0,0,32,148,0,0,228,178,1,0,32,148,0,0,110,148,0,0,212,
      178,1,0,112,148,0,0,70,150,0,0,240,185,1,0,72,150,0,0,144,150,0,0,212,178,1,0,144,150,0,0,199,150,0,0,212,178,1,0,0,151,0,0,28,151,0,0,220,178,1,0,40,151,0,0,97,151,0,0,220,178,1,0,100,151,0,0,134,151,0,0,220,178,1,0,136,151,0,0,92,152,0,0,60,
      180,1,0,92,152,0,0,3,153,0,0,212,178,1,0,4,153,0,0,208,153,0,0,60,180,1,0,208,153,0,0,17,154,0,0,212,178,1,0,20,154,0,0,52,154,0,0,12,186,1,0,52,154,0,0,155,154,0,0,228,178,1,0,156,154,0,0,105,155,0,0,20,186,1,0,108,155,0,0,97,156,0,0,28,186,
      1,0,100,156,0,0,163,156,0,0,36,186,1,0,164,156,0,0,233,156,0,0,80,186,1,0,236,156,0,0,75,157,0,0,124,186,1,0,76,157,0,0,137,157,0,0,168,186,1,0,140,157,0,0,199,157,0,0,212,186,1,0,200,157,0,0,8,158,0,0,228,178,1,0,8,158,0,0,245,158,0,0,252,186,
      1,0,248,158,0,0,4,160,0,0,236,184,1,0,4,160,0,0,41,160,0,0,220,178,1,0,44,160,0,0,72,160,0,0,220,178,1,0,72,160,0,0,168,160,0,0,220,178,1,0,168,160,0,0,97,163,0,0,24,187,1,0,100,163,0,0,225,163,0,0,56,187,1,0,228,163,0,0,116,164,0,0,24,179,1,
      0,116,164,0,0,44,165,0,0,64,187,1,0,44,165,0,0,234,166,0,0,112,187,1,0,236,166,0,0,205,168,0,0,128,187,1,0,208,168,0,0,138,170,0,0,156,187,1,0,140,170,0,0,201,170,0,0,12,186,1,0,204,170,0,0,76,171,0,0,60,180,1,0,76,171,0,0,136,171,0,0,228,178,
      1,0,136,171,0,0,166,171,0,0,28,183,1,0,168,171,0,0,239,171,0,0,220,178,1,0,240,171,0,0,75,173,0,0,200,187,1,0,84,173,0,0,2,174,0,0,232,187,1,0,36,174,0,0,82,174,0,0,252,187,1,0,84,174,0,0,190,176,0,0,4,188,1,0,192,176,0,0,5,177,0,0,72,188,1,
      0,104,177,0,0,179,177,0,0,212,178,1,0,180,177,0,0,46,178,0,0,60,180,1,0,48,178,0,0,141,178,0,0,116,188,1,0,144,178,0,0,42,179,0,0,124,188,1,0,44,179,0,0,12,180,0,0,168,188,1,0,36,180,0,0,67,181,0,0,236,184,1,0,68,181,0,0,159,181,0,0,212,178,
      1,0,160,181,0,0,220,181,0,0,212,178,1,0,220,181,0,0,252,181,0,0,220,178,1,0,252,181,0,0,28,182,0,0,220,178,1,0,28,182,0,0,106,182,0,0,228,178,1,0,180,182,0,0,41,183,0,0,212,178,1,0,52,183,0,0,170,184,0,0,216,188,1,0,172,184,0,0,54,186,0,0,240,
      188,1,0,56,186,0,0,65,188,0,0,8,189,1,0,68,188,0,0,203,189,0,0,32,189,1,0,204,189,0,0,218,192,0,0,60,189,1,0,240,192,0,0,14,194,0,0,96,189,1,0,16,194,0,0,33,195,0,0,124,189,1,0,56,195,0,0,173,195,0,0,144,189,1,0,176,195,0,0,69,196,0,0,24,179,
      1,0,164,196,0,0,81,197,0,0,160,189,1,0,84,197,0,0,85,198,0,0,240,188,1,0,88,198,0,0,19,199,0,0,172,189,1,0,20,199,0,0,69,199,0,0,212,178,1,0,72,199,0,0,121,199,0,0,212,178,1,0,136,199,0,0,182,199,0,0,252,187,1,0,184,199,0,0,38,200,0,0,180,189,
      1,0,40,200,0,0,87,200,0,0,220,178,1,0,88,200,0,0,182,200,0,0,212,178,1,0,184,200,0,0,36,201,0,0,228,189,1,0,44,201,0,0,113,201,0,0,212,178,1,0,116,201,0,0,186,201,0,0,212,178,1,0,188,201,0,0,2,202,0,0,212,178,1,0,4,202,0,0,85,202,0,0,228,178,
      1,0,88,202,0,0,185,202,0,0,60,180,1,0,188,202,0,0,8,204,0,0,240,185,1,0,32,204,0,0,96,204,0,0,240,189,1,0,112,204,0,0,154,204,0,0,248,189,1,0,160,204,0,0,198,204,0,0,0,190,1,0,208,204,0,0,23,205,0,0,8,190,1,0,24,205,0,0,157,205,0,0,236,184,1,
      0,176,205,0,0,195,205,0,0,16,190,1,0,208,205,0,0,165,209,0,0,28,190,1,0,168,209,0,0,67,210,0,0,32,190,1,0,68,210,0,0,87,210,0,0,220,178,1,0,88,210,0,0,41,211,0,0,40,190,1,0,44,211,0,0,153,211,0,0,48,190,1,0,156,211,0,0,13,212,0,0,60,190,1,0,
      16,212,0,0,68,212,0,0,212,178,1,0,96,212,0,0,221,212,0,0,72,190,1,0,12,213,0,0,187,213,0,0,136,182,1,0,72,214,0,0,240,214,0,0,220,178,1,0,240,214,0,0,102,216,0,0,24,179,1,0,184,216,0,0,238,216,0,0,12,186,1,0,240,216,0,0,94,217,0,0,76,190,1,0,
      96,217,0,0,197,217,0,0,228,178,1,0,200,217,0,0,61,218,0,0,220,178,1,0,64,218,0,0,250,218,0,0,128,181,1,0,252,218,0,0,161,219,0,0,24,179,1,0,164,219,0,0,244,219,0,0,120,190,1,0,244,219,0,0,156,220,0,0,136,190,1,0,236,220,0,0,121,222,0,0,188,190,
      1,0,124,222,0,0,18,223,0,0,228,190,1,0,20,223,0,0,41,226,0,0,244,190,1,0,44,226,0,0,67,226,0,0,220,178,1,0,68,226,0,0,213,226,0,0,28,191,1,0,216,226,0,0,100,227,0,0,36,191,1,0,100,227,0,0,79,228,0,0,80,191,1,0,80,228,0,0,51,231,0,0,136,191,1,
      0,52,231,0,0,32,236,0,0,160,191,1,0,32,236,0,0,34,237,0,0,196,191,1,0,36,237,0,0,61,238,0,0,196,191,1,0,64,238,0,0,176,239,0,0,228,191,1,0,176,239,0,0,214,239,0,0,220,178,1,0,216,239,0,0,137,240,0,0,8,192,1,0,140,240,0,0,204,240,0,0,212,178,
      1,0,204,240,0,0,43,241,0,0,220,178,1,0,64,241,0,0,155,241,0,0,52,192,1,0,155,241,0,0,191,244,0,0,76,192,1,0,191,244,0,0,221,244,0,0,112,192,1,0,224,244,0,0,179,245,0,0,228,178,1,0,192,245,0,0,136,249,0,0,128,192,1,0,136,249,0,0,38,250,0,0,144,
      192,1,0,48,250,0,0,196,250,0,0,160,192,1,0,196,250,0,0,253,250,0,0,220,178,1,0,0,251,0,0,122,251,0,0,228,178,1,0,124,251,0,0,3,252,0,0,220,185,1,0,4,252,0,0,14,253,0,0,172,192,1,0,16,253,0,0,124,253,0,0,12,186,1,0,124,253,0,0,132,254,0,0,180,
      192,1,0,132,254,0,0,220,254,0,0,60,180,1,0,240,254,0,0,183,255,0,0,200,192,1,0,200,255,0,0,70,1,1,0,220,185,1,0,176,1,1,0,1,2,1,0,208,192,1,0,36,2,1,0,189,2,1,0,60,180,1,0,192,2,1,0,249,2,1,0,36,183,1,0,252,2,1,0,114,4,1,0,216,192,1,0,116,4,
      1,0,175,4,1,0,240,192,1,0,176,4,1,0,18,5,1,0,248,192,1,0,20,5,1,0,151,5,1,0,228,178,1,0,152,5,1,0,227,5,1,0,56,187,1,0,228,5,1,0,185,7,1,0,36,193,1,0,188,7,1,0,14,8,1,0,56,187,1,0,16,8,1,0,206,8,1,0,124,189,1,0,208,8,1,0,236,8,1,0,220,178,1,
      0,236,8,1,0,141,9,1,0,28,191,1,0,144,9,1,0,77,10,1,0,228,178,1,0,80,10,1,0,193,10,1,0,64,193,1,0,224,10,1,0,139,16,1,0,112,193,1,0,140,16,1,0,241,16,1,0,124,193,1,0,244,16,1,0,174,17,1,0,60,180,1,0,176,17,1,0,215,18,1,0,132,193,1,0,224,18,1,
      0,80,19,1,0,164,193,1,0,80,19,1,0,230,19,1,0,172,193,1,0,232,19,1,0,8,20,1,0,28,183,1,0,32,20,1,0,48,20,1,0,184,193,1,0,112,20,1,0,158,20,1,0,220,178,1,0,160,20,1,0,199,20,1,0,60,179,1,0,200,20,1,0,213,23,1,0,192,193,1,0,216,23,1,0,245,23,1,
      0,212,178,1,0,248,23,1,0,116,24,1,0,212,193,1,0,116,24,1,0,133,24,1,0,220,178,1,0,136,24,1,0,167,24,1,0,212,178,1,0,192,24,1,0,194,24,1,0,88,179,1,0,224,24,1,0,230,24,1,0,96,179,1,0,230,24,1,0,4,25,1,0,180,178,1,0,4,25,1,0,28,25,1,0,16,179,1,
      0,28,25,1,0,51,25,1,0,160,179,1,0,51,25,1,0,76,25,1,0,252,179,1,0,76,25,1,0,96,25,1,0,4,180,1,0,96,25,1,0,150,25,1,0,52,180,1,0,150,25,1,0,53,26,1,0,72,181,1,0,53,26,1,0,203,26,1,0,116,181,1,0,203,26,1,0,225,26,1,0,200,181,1,0,225,26,1,0,251,
      26,1,0,160,186,1,0,225,26,1,0,251,26,1,0,204,186,1,0,225,26,1,0,251,26,1,0,108,188,1,0,225,26,1,0,251,26,1,0,192,187,1,0,225,26,1,0,251,26,1,0,116,186,1,0,225,26,1,0,251,26,1,0,196,182,1,0,225,26,1,0,251,26,1,0,40,182,1,0,225,26,1,0,251,26,1,
      0,240,182,1,0,225,26,1,0,251,26,1,0,72,186,1,0,251,26,1,0,22,27,1,0,252,181,1,0,32,27,1,0,64,27,1,0,160,183,1,0,64,27,1,0,111,27,1,0,164,184,1,0,111,27,1,0,242,27,1,0,172,184,1,0,242,27,1,0,8,28,1,0,56,184,1,0,8,28,1,0,43,28,1,0,64,184,1,0,43,
      28,1,0,68,28,1,0,180,190,1,0,43,28,1,0,68,28,1,0,244,186,1,0,68,28,1,0,93,28,1,0,104,187,1,0,93,28,1,0,126,28,1,0,64,188,1,0,126,28,1,0,153,28,1,0,160,188,1,0,153,28,1,0,182,28,1,0,208,188,1,0,182,28,1,0,208,28,1,0,220,189,1,0,208,28,1,0,233,
      28,1,0,112,190,1,0,233,28,1,0,3,29,1,0,100,193,1,0,233,28,1,0,3,29,1,0,72,191,1,0,3,29,1,0,26,29,1,0,128,191,1,0,26,29,1,0,51,29,1,0,44,192,1,0,51,29,1,0,75,29,1,0,28,193,1,0,75,29,1,0,119,29,1,0,252,193,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,48,0,128,1,0,0,0,60,48,0,128,1,0,0,0,192,24,1,128,1,0,0,0,224,24,1,128,1,0,0,0,224,24,1,128,1,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,190,77,0,0,187,77,0,0,231,77,0,0,183,77,0,0,196,77,0,0,212,77,0,0,228,77,0,0,180,77,0,0,236,77,0,0,200,77,0,0,0,78,0,0,240,77,0,0,192,77,0,0,208,77,0,0,224,77,0,0,176,77,0,0,8,78,0,0,0,0,0,0,0,0,0,0,0,0,0,0,80,206,0,0,111,206,
      0,0,81,206,0,0,95,206,0,0,152,206,0,0,160,206,0,0,176,206,0,0,192,206,0,0,88,206,0,0,240,206,0,0,0,207,0,0,128,206,0,0,16,207,0,0,216,206,0,0,32,207,0,0,64,207,0,0,117,206,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,16,0,0,0,24,0,0,128,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,1,0,1,0,0,0,48,0,0,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,9,4,0,0,72,0,0,0,96,64,2,0,32,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,5,52,0,0,0,86,0,83,0,95,0,86,0,69,0,82,0,83,0,73,0,79,0,78,0,95,0,73,0,78,0,70,0,79,0,0,0,0,0,189,4,
      239,254,0,0,1,0,9,0,0,0,0,0,67,2,9,0,0,0,0,0,67,2,63,0,0,0,0,0,0,0,4,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,4,0,0,1,0,83,0,116,0,114,0,105,0,110,0,103,0,70,0,105,0,108,0,101,0,73,0,110,0,102,0,111,0,0,0,92,4,0,0,1,0,48,0,52,0,48,0,57,0,48,
      0,52,0,98,0,48,0,0,0,76,0,22,0,1,0,67,0,111,0,109,0,112,0,97,0,110,0,121,0,78,0,97,0,109,0,101,0,0,0,0,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,32,0,67,0,111,0,114,0,112,0,111,0,114,0,97,0,116,0,105,0,111,0,110,0,0,0,134,0,47,0,
      1,0,70,0,105,0,108,0,101,0,68,0,101,0,115,0,99,0,114,0,105,0,112,0,116,0,105,0,111,0,110,0,0,0,0,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,32,0,69,0,100,0,103,0,101,0,32,0,69,0,109,0,98,0,101,0,100,0,100,0,101,0,100,0,32,0,66,0,114,
      0,111,0,119,0,115,0,101,0,114,0,32,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,32,0,76,0,111,0,97,0,100,0,101,0,114,0,0,0,0,0,52,0,10,0,1,0,70,0,105,0,108,0,101,0,86,0,101,0,114,0,115,0,105,0,111,0,110,0,0,0,0,0,48,0,46,0,57,0,46,0,53,0,55,0,57,
      0,46,0,48,0,0,0,70,0,19,0,1,0,73,0,110,0,116,0,101,0,114,0,110,0,97,0,108,0,78,0,97,0,109,0,101,0,0,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,76,0,111,0,97,0,100,0,101,0,114,0,46,0,100,0,108,0,108,0,0,0,0,0,144,0,54,0,1,0,76,0,101,0,103,
      0,97,0,108,0,67,0,111,0,112,0,121,0,114,0,105,0,103,0,104,0,116,0,0,0,67,0,111,0,112,0,121,0,114,0,105,0,103,0,104,0,116,0,32,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,32,0,67,0,111,0,114,0,112,0,111,0,114,0,97,0,116,0,105,0,111,
      0,110,0,46,0,32,0,65,0,108,0,108,0,32,0,114,0,105,0,103,0,104,0,116,0,115,0,32,0,114,0,101,0,115,0,101,0,114,0,118,0,101,0,100,0,46,0,0,0,78,0,19,0,1,0,79,0,114,0,105,0,103,0,105,0,110,0,97,0,108,0,70,0,105,0,108,0,101,0,110,0,97,0,109,0,101,
      0,0,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,76,0,111,0,97,0,100,0,101,0,114,0,46,0,100,0,108,0,108,0,0,0,0,0,126,0,47,0,1,0,80,0,114,0,111,0,100,0,117,0,99,0,116,0,78,0,97,0,109,0,101,0,0,0,0,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,
      0,116,0,32,0,69,0,100,0,103,0,101,0,32,0,69,0,109,0,98,0,101,0,100,0,100,0,101,0,100,0,32,0,66,0,114,0,111,0,119,0,115,0,101,0,114,0,32,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,32,0,76,0,111,0,97,0,100,0,101,0,114,0,0,0,0,0,56,0,10,0,1,0,80,
      0,114,0,111,0,100,0,117,0,99,0,116,0,86,0,101,0,114,0,115,0,105,0,111,0,110,0,0,0,48,0,46,0,57,0,46,0,53,0,55,0,57,0,46,0,48,0,0,0,60,0,10,0,1,0,67,0,111,0,109,0,112,0,97,0,110,0,121,0,83,0,104,0,111,0,114,0,116,0,78,0,97,0,109,0,101,0,0,0,77,
      0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,0,0,134,0,47,0,1,0,80,0,114,0,111,0,100,0,117,0,99,0,116,0,83,0,104,0,111,0,114,0,116,0,78,0,97,0,109,0,101,0,0,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,32,0,69,0,100,0,103,0,101,
      0,32,0,69,0,109,0,98,0,101,0,100,0,100,0,101,0,100,0,32,0,66,0,114,0,111,0,119,0,115,0,101,0,114,0,32,0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,32,0,76,0,111,0,97,0,100,0,101,0,114,0,0,0,0,0,110,0,41,0,1,0,76,0,97,0,115,0,116,0,67,0,104,0,97,
      0,110,0,103,0,101,0,0,0,101,0,50,0,56,0,55,0,54,0,97,0,100,0,99,0,56,0,98,0,54,0,57,0,54,0,55,0,54,0,54,0,53,0,56,0,100,0,53,0,102,0,55,0,98,0,97,0,56,0,55,0,102,0,49,0,101,0,56,0,98,0,52,0,56,0,54,0,49,0,97,0,55,0,101,0,54,0,101,0,0,0,0,0,40,
      0,2,0,1,0,79,0,102,0,102,0,105,0,99,0,105,0,97,0,108,0,32,0,66,0,117,0,105,0,108,0,100,0,0,0,49,0,0,0,68,0,0,0,1,0,86,0,97,0,114,0,70,0,105,0,108,0,101,0,73,0,110,0,102,0,111,0,0,0,0,0,36,0,4,0,0,0,84,0,114,0,97,0,110,0,115,0,108,0,97,0,116,
      0,105,0,111,0,110,0,0,0,0,0,9,4,176,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,1,0,68,1,0,0,208,160,216,160,224,160,0,161,8,161,16,161,40,161,48,161,56,161,88,161,96,161,24,162,48,162,56,162,216,162,224,162,232,162,184,163,192,163,208,163,224,163,232,163,240,163,
      248,163,0,164,8,164,16,164,24,164,40,164,48,164,56,164,64,164,72,164,80,164,88,164,96,164,120,164,136,164,152,164,160,164,168,164,176,164,184,164,200,165,208,165,216,165,240,165,0,166,16,166,32,166,48,166,64,166,80,166,96,166,112,166,128,166,
      144,166,160,166,176,166,192,166,208,166,224,166,240,166,0,167,16,167,32,167,48,167,64,167,80,167,96,167,112,167,128,167,144,167,160,167,176,167,192,167,208,167,224,167,240,167,0,168,16,168,32,168,48,168,64,168,80,168,96,168,112,168,128,168,144,
      168,160,168,176,168,192,168,208,168,224,168,240,168,0,169,16,169,32,169,48,169,64,169,80,169,96,169,112,169,128,169,144,169,160,169,176,169,192,169,208,169,224,169,240,169,0,170,16,170,32,170,48,170,64,170,80,170,96,170,112,170,128,170,144,170,
      160,170,176,170,192,170,208,170,224,170,240,170,0,171,16,171,32,171,48,171,64,171,80,171,96,171,112,171,128,171,144,171,160,171,176,171,192,171,208,171,224,171,240,171,0,172,16,172,32,172,48,172,64,172,80,172,96,172,112,172,128,172,144,172,160,
      172,176,172,192,172,208,172,224,172,0,48,1,0,232,0,0,0,176,163,184,163,192,163,200,163,208,163,216,163,224,163,232,163,240,163,248,163,0,164,8,164,16,164,24,164,32,164,40,164,48,164,56,164,64,164,72,164,80,170,88,170,96,170,104,170,176,170,184,
      170,192,170,200,170,208,170,216,170,224,170,232,170,240,170,248,170,0,171,8,171,16,171,24,171,32,171,40,171,48,171,56,171,64,171,72,171,80,171,88,171,96,171,104,171,112,171,120,171,128,171,136,171,144,171,152,171,160,171,168,171,176,171,184,
      171,192,171,200,171,208,171,216,171,224,171,232,171,240,171,248,171,0,172,16,172,24,172,32,172,40,172,48,172,56,172,64,172,72,172,80,172,88,172,96,172,104,172,112,172,120,172,128,172,136,172,144,172,152,172,160,172,168,172,176,172,184,172,192,
      172,200,172,208,172,216,172,224,172,232,172,240,172,248,172,0,173,8,173,16,173,24,173,32,173,40,173,48,173,56,173,64,173,72,173,80,173,88,173,96,173,104,173,0,0,0,64,1,0,180,0,0,0,168,170,176,170,184,170,232,170,240,170,8,171,24,171,40,171,56,
      171,72,171,88,171,104,171,120,171,136,171,152,171,168,171,184,171,200,171,216,171,232,171,248,171,8,172,24,172,40,172,56,172,72,172,88,172,104,172,120,172,136,172,152,172,168,172,184,172,200,172,216,172,232,172,248,172,8,173,24,173,40,173,56,
      173,72,173,88,173,104,173,120,173,136,173,152,173,168,173,184,173,200,173,216,173,232,173,248,173,8,174,24,174,40,174,56,174,72,174,88,174,104,174,120,174,136,174,152,174,168,174,184,174,200,174,216,174,232,174,248,174,8,175,24,175,40,175,56,
      175,72,175,88,175,104,175,120,175,136,175,152,175,168,175,184,175,200,175,216,175,232,175,248,175,0,0,0,80,1,0,48,1,0,0,8,160,24,160,40,160,56,160,72,160,88,160,104,160,120,160,136,160,152,160,168,160,184,160,200,160,216,160,232,160,248,160,
      8,161,24,161,40,161,56,161,72,161,88,161,104,161,120,161,136,161,152,161,168,161,184,161,200,161,216,161,232,161,248,161,8,162,24,162,40,162,56,162,72,162,88,162,104,162,120,162,136,162,152,162,168,162,184,162,200,162,216,162,232,162,248,162,
      8,163,24,163,40,163,56,163,72,163,88,163,104,163,120,163,136,163,152,163,168,163,184,163,200,163,216,163,232,163,248,163,8,164,24,164,40,164,56,164,72,164,88,164,104,164,120,164,136,164,152,164,168,164,184,164,200,164,216,164,232,164,248,164,
      8,165,24,165,40,165,56,165,72,165,88,165,104,165,120,165,136,165,152,165,168,165,184,165,200,165,216,165,232,165,248,165,8,166,24,166,40,166,56,166,72,166,88,166,104,166,120,166,136,166,152,166,168,166,184,166,200,166,216,166,232,166,248,166,
      8,167,24,167,40,167,56,167,72,167,88,167,104,167,120,167,136,167,152,167,168,167,184,167,200,167,216,167,232,167,248,167,8,168,24,168,40,168,56,168,72,168,88,168,104,168,120,168,136,168,152,168,168,168,184,168,200,168,216,168,232,168,248,168,
      8,169,24,169,40,169,56,169,0,96,1,0,92,1,0,0,96,165,112,165,128,165,144,165,160,165,176,165,192,165,208,165,224,165,240,165,0,166,16,166,32,166,48,166,64,166,80,166,96,166,112,166,128,166,144,166,160,166,176,166,192,166,208,166,224,166,240,166,
      0,167,16,167,32,167,48,167,64,167,80,167,96,167,112,167,128,167,144,167,160,167,176,167,192,167,208,167,224,167,240,167,0,168,16,168,32,168,48,168,64,168,80,168,96,168,112,168,128,168,144,168,160,168,176,168,192,168,208,168,224,168,240,168,0,
      169,16,169,32,169,48,169,64,169,80,169,96,169,112,169,128,169,144,169,160,169,176,169,192,169,208,169,224,169,240,169,0,170,16,170,32,170,48,170,64,170,80,170,96,170,112,170,128,170,144,170,160,170,176,170,192,170,208,170,224,170,240,170,0,171,
      16,171,32,171,48,171,64,171,80,171,96,171,112,171,128,171,144,171,160,171,176,171,192,171,208,171,224,171,240,171,0,172,16,172,32,172,48,172,64,172,80,172,96,172,112,172,128,172,144,172,160,172,176,172,192,172,208,172,224,172,240,172,0,173,16,
      173,32,173,48,173,64,173,80,173,96,173,112,173,128,173,144,173,160,173,176,173,192,173,208,173,224,173,240,173,0,174,16,174,32,174,48,174,64,174,80,174,96,174,112,174,128,174,144,174,160,174,176,174,192,174,208,174,224,174,240,174,0,175,16,175,
      32,175,48,175,64,175,80,175,96,175,112,175,128,175,144,175,160,175,176,175,192,175,208,175,224,175,240,175,0,112,1,0,124,0,0,0,0,160,16,160,32,160,48,160,64,160,80,160,96,160,112,160,128,160,144,160,160,160,176,160,192,160,208,160,224,160,240,
      160,0,161,16,161,32,161,48,161,64,161,80,161,96,161,112,161,128,161,144,161,160,161,176,161,192,161,208,161,224,161,240,161,0,162,16,162,32,162,48,162,64,162,80,162,96,162,112,162,128,162,144,162,160,162,176,162,192,162,208,162,224,162,240,162,
      0,163,16,163,32,163,48,163,64,163,80,163,96,163,112,163,128,163,144,163,0,144,1,0,28,0,0,0,176,168,184,168,192,168,200,168,208,168,224,168,232,168,240,168,248,168,0,169,0,160,1,0,32,0,0,0,200,160,208,160,216,160,224,160,200,163,208,163,216,163,
      224,163,8,164,16,164,24,164,0,0,0,208,1,0,100,0,0,0,144,165,216,165,248,165,24,166,56,166,88,166,136,166,160,166,168,166,176,166,232,166,240,166,16,168,48,168,56,168,64,168,72,168,80,168,88,168,96,168,104,168,112,168,120,168,136,168,144,168,
      152,168,160,168,168,168,176,168,184,168,192,168,232,169,16,170,56,170,104,170,136,170,208,170,216,170,224,170,240,170,248,170,0,171,8,171,24,171,40,171,48,171,0,16,2,0,20,0,0,0,0,160,8,160,16,160,24,160,32,160,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,144,33,0,0,0,2,2,0,48,130,33,132,6,9,42,134,
      72,134,247,13,1,7,2,160,130,33,117,48,130,33,113,2,1,1,49,15,48,13,6,9,96,134,72,1,101,3,4,2,1,5,0,48,92,6,10,43,6,1,4,1,130,55,2,1,4,160,78,48,76,48,23,6,10,43,6,1,4,1,130,55,2,1,15,48,9,3,1,0,160,4,162,2,128,0,48,49,48,13,6,9,96,134,72,1,101,
      3,4,2,1,5,0,4,32,5,72,198,180,189,50,108,140,26,237,33,118,87,83,244,148,225,96,69,77,141,12,179,31,78,92,53,95,191,99,175,203,160,130,11,118,48,130,4,254,48,130,3,230,160,3,2,1,2,2,19,51,0,0,3,38,174,206,237,249,188,228,123,146,0,0,0,0,3,38,
      48,13,6,9,42,134,72,134,247,13,1,1,11,5,0,48,126,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,
      99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,40,48,38,6,3,85,4,3,19,31,77,105,99,114,111,115,111,102,116,32,67,111,100,101,32,83,105,103,110,105,110,103,32,80,67,65,32,50,48,49,48,48,30,23,13,50,48,48,51,48,52,49,
      56,50,57,50,57,90,23,13,50,49,48,51,48,51,49,56,50,57,50,57,90,48,116,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,
      85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,30,48,28,6,3,85,4,3,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,48,130,1,34,48,13,6,9,42,134,72,134,247,
      13,1,1,1,5,0,3,130,1,15,0,48,130,1,10,2,130,1,1,0,157,151,52,196,187,94,209,154,120,61,91,52,110,201,189,177,184,239,129,212,28,172,248,191,142,142,71,48,26,149,207,120,191,191,159,38,105,139,19,93,57,202,126,142,88,84,185,206,17,212,208,205,
      211,26,140,255,185,108,99,52,81,162,5,201,69,142,183,215,180,105,248,218,255,188,75,16,196,17,175,240,123,105,131,185,30,72,71,9,57,157,133,151,198,25,66,58,182,77,181,197,110,50,255,121,103,43,125,250,206,41,154,57,90,207,22,85,220,117,65,171,
      171,44,188,82,128,111,61,176,36,58,235,106,29,13,123,44,78,205,220,109,23,143,173,173,4,184,46,150,244,98,36,108,174,221,33,235,149,14,162,80,51,92,174,200,191,83,96,100,190,186,89,88,97,217,180,70,206,211,235,215,62,186,242,205,194,111,190,
      239,189,34,113,24,69,217,203,116,45,210,22,46,124,208,133,93,154,151,148,63,196,113,132,51,188,141,179,202,35,240,124,49,156,45,235,243,229,106,99,83,229,16,210,105,7,81,109,224,193,83,90,123,229,35,167,251,246,199,148,196,41,54,249,119,43,21,
      25,205,2,3,1,0,1,163,130,1,125,48,130,1,121,48,31,6,3,85,29,37,4,24,48,22,6,10,43,6,1,4,1,130,55,61,6,1,6,8,43,6,1,5,5,7,3,3,48,29,6,3,85,29,14,4,22,4,20,93,115,187,145,194,122,246,93,30,217,78,200,45,47,224,3,171,166,220,18,48,84,6,3,85,29,
      17,4,77,48,75,164,73,48,71,49,45,48,43,6,3,85,4,11,19,36,77,105,99,114,111,115,111,102,116,32,73,114,101,108,97,110,100,32,79,112,101,114,97,116,105,111,110,115,32,76,105,109,105,116,101,100,49,22,48,20,6,3,85,4,5,19,13,50,51,48,56,54,53,43,
      52,53,56,52,57,52,48,31,6,3,85,29,35,4,24,48,22,128,20,230,252,95,123,187,34,0,88,228,114,78,181,244,33,116,35,50,230,239,172,48,86,6,3,85,29,31,4,79,48,77,48,75,160,73,160,71,134,69,104,116,116,112,58,47,47,99,114,108,46,109,105,99,114,111,
      115,111,102,116,46,99,111,109,47,112,107,105,47,99,114,108,47,112,114,111,100,117,99,116,115,47,77,105,99,67,111,100,83,105,103,80,67,65,95,50,48,49,48,45,48,55,45,48,54,46,99,114,108,48,90,6,8,43,6,1,5,5,7,1,1,4,78,48,76,48,74,6,8,43,6,1,5,
      5,7,48,2,134,62,104,116,116,112,58,47,47,119,119,119,46,109,105,99,114,111,115,111,102,116,46,99,111,109,47,112,107,105,47,99,101,114,116,115,47,77,105,99,67,111,100,83,105,103,80,67,65,95,50,48,49,48,45,48,55,45,48,54,46,99,114,116,48,12,6,
      3,85,29,19,1,1,255,4,2,48,0,48,13,6,9,42,134,72,134,247,13,1,1,11,5,0,3,130,1,1,0,25,230,236,173,12,220,113,193,44,133,221,29,114,163,227,132,144,60,251,69,216,164,102,158,189,152,206,125,10,96,37,108,139,224,131,122,101,83,179,137,15,34,23,
      164,207,117,245,99,97,1,21,191,183,96,50,121,61,252,54,207,218,117,125,145,245,52,164,36,144,46,150,81,0,71,6,202,248,182,181,238,152,202,181,175,17,61,40,17,67,135,136,153,35,29,177,49,202,27,254,177,89,150,139,35,251,91,105,223,51,115,80,9,
      165,239,178,166,55,54,73,117,245,202,251,143,211,117,228,83,33,26,147,75,128,104,246,4,22,223,40,237,96,146,33,250,155,111,230,183,209,218,109,255,231,59,235,202,49,188,136,214,87,153,105,125,96,233,253,129,54,131,25,105,11,19,198,165,79,129,
      156,150,113,217,228,0,246,112,164,54,143,14,19,174,139,205,251,173,87,81,148,91,88,106,169,216,152,104,201,141,95,109,173,122,84,252,99,160,246,110,51,78,24,14,31,160,18,235,152,142,142,86,155,211,144,203,100,88,123,31,103,66,166,145,239,120,
      247,245,46,254,178,253,21,40,23,93,179,45,48,130,6,112,48,130,4,88,160,3,2,1,2,2,10,97,12,82,76,0,0,0,0,0,3,48,13,6,9,42,134,72,134,247,13,1,1,11,5,0,48,129,136,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,
      110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,50,48,48,6,3,85,4,3,19,41,77,105,99,114,111,115,111,102,
      116,32,82,111,111,116,32,67,101,114,116,105,102,105,99,97,116,101,32,65,117,116,104,111,114,105,116,121,32,50,48,49,48,48,30,23,13,49,48,48,55,48,54,50,48,52,48,49,55,90,23,13,50,53,48,55,48,54,50,48,53,48,49,55,90,48,126,49,11,48,9,6,3,85,4,
      6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,
      49,40,48,38,6,3,85,4,3,19,31,77,105,99,114,111,115,111,102,116,32,67,111,100,101,32,83,105,103,110,105,110,103,32,80,67,65,32,50,48,49,48,48,130,1,34,48,13,6,9,42,134,72,134,247,13,1,1,1,5,0,3,130,1,15,0,48,130,1,10,2,130,1,1,0,233,14,100,80,
      121,103,181,196,227,253,9,0,76,158,148,172,247,86,104,234,68,216,207,197,88,79,169,165,118,124,109,69,186,211,57,146,180,164,30,249,249,101,130,228,23,210,143,253,68,156,8,232,101,147,206,44,85,132,191,125,8,227,46,43,168,65,43,24,183,162,75,
      110,73,76,107,21,7,222,209,210,194,137,30,113,148,205,181,127,75,180,175,8,216,204,136,214,107,23,148,58,147,206,38,63,236,230,254,52,152,87,213,29,93,73,246,178,42,46,213,133,187,89,63,248,144,180,43,131,116,202,43,179,59,70,227,240,70,73,193,
      23,102,84,201,28,189,29,196,85,98,87,114,248,103,185,37,32,52,222,93,166,165,149,94,171,40,128,205,213,178,158,229,3,181,99,211,178,20,200,193,200,138,38,10,89,127,7,236,255,14,237,128,18,53,76,18,166,190,82,91,245,166,218,224,139,11,72,119,
      214,133,71,213,16,185,198,232,170,238,139,106,45,5,92,96,198,180,42,91,156,35,28,95,69,227,26,20,30,111,55,203,25,51,128,106,137,77,163,106,102,99,120,147,213,48,207,149,31,2,3,1,0,1,163,130,1,227,48,130,1,223,48,16,6,9,43,6,1,4,1,130,55,21,
      1,4,3,2,1,0,48,29,6,3,85,29,14,4,22,4,20,230,252,95,123,187,34,0,88,228,114,78,181,244,33,116,35,50,230,239,172,48,25,6,9,43,6,1,4,1,130,55,20,2,4,12,30,10,0,83,0,117,0,98,0,67,0,65,48,11,6,3,85,29,15,4,4,3,2,1,134,48,15,6,3,85,29,19,1,1,255,
      4,5,48,3,1,1,255,48,31,6,3,85,29,35,4,24,48,22,128,20,213,246,86,203,143,232,162,92,98,104,209,61,148,144,91,215,206,154,24,196,48,86,6,3,85,29,31,4,79,48,77,48,75,160,73,160,71,134,69,104,116,116,112,58,47,47,99,114,108,46,109,105,99,114,111,
      115,111,102,116,46,99,111,109,47,112,107,105,47,99,114,108,47,112,114,111,100,117,99,116,115,47,77,105,99,82,111,111,67,101,114,65,117,116,95,50,48,49,48,45,48,54,45,50,51,46,99,114,108,48,90,6,8,43,6,1,5,5,7,1,1,4,78,48,76,48,74,6,8,43,6,1,
      5,5,7,48,2,134,62,104,116,116,112,58,47,47,119,119,119,46,109,105,99,114,111,115,111,102,116,46,99,111,109,47,112,107,105,47,99,101,114,116,115,47,77,105,99,82,111,111,67,101,114,65,117,116,95,50,48,49,48,45,48,54,45,50,51,46,99,114,116,48,129,
      157,6,3,85,29,32,4,129,149,48,129,146,48,129,143,6,9,43,6,1,4,1,130,55,46,3,48,129,129,48,61,6,8,43,6,1,5,5,7,2,1,22,49,104,116,116,112,58,47,47,119,119,119,46,109,105,99,114,111,115,111,102,116,46,99,111,109,47,80,75,73,47,100,111,99,115,47,
      67,80,83,47,100,101,102,97,117,108,116,46,104,116,109,48,64,6,8,43,6,1,5,5,7,2,2,48,52,30,50,32,29,0,76,0,101,0,103,0,97,0,108,0,95,0,80,0,111,0,108,0,105,0,99,0,121,0,95,0,83,0,116,0,97,0,116,0,101,0,109,0,101,0,110,0,116,0,46,32,29,48,13,6,
      9,42,134,72,134,247,13,1,1,11,5,0,3,130,2,1,0,26,116,239,87,79,41,123,196,22,133,120,184,80,211,34,252,9,157,172,130,151,248,52,255,42,44,151,149,18,229,228,191,207,191,147,200,227,52,169,219,129,184,220,30,0,190,210,53,111,175,229,127,121,149,
      119,229,2,212,241,235,216,205,78,30,27,97,162,194,90,35,26,240,140,168,98,81,69,103,8,227,63,60,30,147,248,48,133,23,200,57,64,166,215,14,179,33,41,229,165,161,105,140,34,147,204,116,152,231,161,71,67,242,83,172,192,15,48,105,127,254,210,37,
      32,109,111,97,211,223,7,213,217,114,0,44,105,134,118,61,81,219,166,57,72,201,55,97,109,7,221,83,25,203,167,214,97,194,191,226,131,171,15,224,107,155,149,214,125,40,81,176,137,74,81,164,154,108,200,183,31,74,26,14,105,169,215,220,193,126,209,
      73,112,170,182,173,187,114,71,99,23,250,166,214,162,166,134,236,168,16,68,155,99,182,178,105,137,6,199,70,134,122,24,63,232,197,29,33,213,123,249,2,35,45,197,65,203,191,29,76,200,22,239,177,156,127,252,34,75,73,138,110,21,227,166,127,118,91,
      209,83,121,145,133,157,213,210,219,61,115,53,243,60,174,84,178,82,71,106,192,170,19,149,210,142,17,218,153,103,94,50,140,251,55,133,209,220,117,133,156,135,198,90,87,133,194,191,221,13,143,140,155,45,235,180,238,207,39,211,181,94,105,250,164,
      22,4,1,167,36,103,115,207,77,79,182,222,5,86,151,122,247,233,82,77,244,119,5,79,133,198,216,11,241,142,237,66,9,209,13,118,227,35,86,120,34,38,54,190,202,177,140,110,170,29,228,133,218,71,51,98,143,164,201,145,51,95,113,30,64,175,152,101,201,
      34,232,66,33,37,138,28,45,96,217,55,137,65,137,42,22,15,215,97,60,148,104,96,82,239,214,71,153,160,128,64,238,21,129,119,62,156,224,83,24,26,80,29,56,149,155,30,102,51,19,39,57,23,120,135,54,206,78,195,95,178,245,61,71,83,182,224,229,219,11,
      97,61,42,215,146,44,206,55,90,62,64,66,49,164,31,16,8,194,86,156,191,36,93,81,2,157,106,121,210,23,211,218,193,148,142,7,123,37,113,68,171,6,106,230,212,198,223,35,154,150,117,197,49,130,21,129,48,130,21,125,2,1,1,48,129,149,48,126,49,11,48,
      9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,
      105,111,110,49,40,48,38,6,3,85,4,3,19,31,77,105,99,114,111,115,111,102,116,32,67,111,100,101,32,83,105,103,110,105,110,103,32,80,67,65,32,50,48,49,48,2,19,51,0,0,3,38,174,206,237,249,188,228,123,146,0,0,0,0,3,38,48,13,6,9,96,134,72,1,101,3,4,
      2,1,5,0,160,129,212,48,25,6,9,42,134,72,134,247,13,1,9,3,49,12,6,10,43,6,1,4,1,130,55,2,1,4,48,28,6,10,43,6,1,4,1,130,55,2,1,11,49,14,48,12,6,10,43,6,1,4,1,130,55,2,1,21,48,47,6,9,42,134,72,134,247,13,1,9,4,49,34,4,32,195,246,174,138,248,28,
      120,80,162,226,174,22,149,156,49,38,51,135,188,228,117,118,22,64,48,60,206,19,165,176,198,85,48,104,6,10,43,6,1,4,1,130,55,2,1,12,49,90,48,88,160,56,128,54,0,77,0,105,0,99,0,114,0,111,0,115,0,111,0,102,0,116,0,32,0,69,0,100,0,103,0,101,0,32,
      0,87,0,101,0,98,0,86,0,105,0,101,0,119,0,50,0,32,0,83,0,68,0,75,161,28,128,26,104,116,116,112,115,58,47,47,119,119,119,46,109,105,99,114,111,115,111,102,116,46,99,111,109,32,48,13,6,9,42,134,72,134,247,13,1,1,1,5,0,4,130,1,0,119,10,209,130,188,
      67,98,158,62,154,24,51,21,213,224,4,235,141,51,79,106,202,77,168,236,71,212,30,250,230,179,200,218,134,145,92,3,119,161,227,240,102,119,218,33,236,194,250,85,83,11,27,116,168,130,66,72,184,139,65,213,75,159,117,56,129,51,69,187,207,100,125,117,
      38,228,100,23,51,194,196,193,79,97,2,250,42,193,168,65,70,176,62,171,11,39,111,229,77,205,131,100,110,121,15,188,8,83,165,18,72,198,238,154,207,193,137,29,7,144,20,85,125,214,28,191,68,246,226,133,174,16,168,140,88,119,183,210,97,184,119,160,
      242,11,29,36,27,231,136,181,84,53,232,63,89,230,125,220,40,6,119,178,48,85,79,41,108,72,3,217,19,126,38,104,205,33,191,175,152,122,180,156,154,196,123,102,192,206,15,141,116,0,249,14,240,4,7,47,141,250,124,174,182,177,24,230,58,211,65,35,150,
      126,229,134,120,164,152,233,176,45,137,17,54,40,111,88,72,105,30,161,220,112,21,209,89,205,32,244,192,91,87,124,71,154,134,66,202,135,220,105,95,131,20,69,202,29,245,161,130,18,229,48,130,18,225,6,10,43,6,1,4,1,130,55,3,3,1,49,130,18,209,48,
      130,18,205,6,9,42,134,72,134,247,13,1,7,2,160,130,18,190,48,130,18,186,2,1,3,49,15,48,13,6,9,96,134,72,1,101,3,4,2,1,5,0,48,130,1,81,6,11,42,134,72,134,247,13,1,9,16,1,4,160,130,1,64,4,130,1,60,48,130,1,56,2,1,1,6,10,43,6,1,4,1,132,89,10,3,1,
      48,49,48,13,6,9,96,134,72,1,101,3,4,2,1,5,0,4,32,157,170,92,160,134,63,188,191,76,91,150,102,171,69,79,194,6,189,222,15,241,20,247,188,178,185,61,73,9,233,106,169,2,6,94,240,147,241,143,182,24,19,50,48,50,48,48,55,49,54,48,57,49,56,51,53,46,
      56,50,57,90,48,4,128,2,1,244,160,129,208,164,129,205,48,129,202,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,
      19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,37,48,35,6,3,85,4,11,19,28,77,105,99,114,111,115,111,102,116,32,65,109,101,114,105,99,97,32,79,112,101,114,97,116,105,111,110,115,49,38,48,36,6,3,85,4,11,
      19,29,84,104,97,108,101,115,32,84,83,83,32,69,83,78,58,68,54,66,68,45,69,51,69,55,45,49,54,56,53,49,37,48,35,6,3,85,4,3,19,28,77,105,99,114,111,115,111,102,116,32,84,105,109,101,45,83,116,97,109,112,32,83,101,114,118,105,99,101,160,130,14,60,
      48,130,4,241,48,130,3,217,160,3,2,1,2,2,19,51,0,0,1,30,14,188,229,75,22,162,3,27,0,0,0,0,1,30,48,13,6,9,42,134,72,134,247,13,1,1,11,5,0,48,124,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,
      49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,38,48,36,6,3,85,4,3,19,29,77,105,99,114,111,115,111,102,116,32,84,105,109,
      101,45,83,116,97,109,112,32,80,67,65,32,50,48,49,48,48,30,23,13,49,57,49,49,49,51,50,49,52,48,52,48,90,23,13,50,49,48,50,49,49,50,49,52,48,52,48,90,48,129,202,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,
      103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,37,48,35,6,3,85,4,11,19,28,77,105,99,114,111,115,111,102,116,
      32,65,109,101,114,105,99,97,32,79,112,101,114,97,116,105,111,110,115,49,38,48,36,6,3,85,4,11,19,29,84,104,97,108,101,115,32,84,83,83,32,69,83,78,58,68,54,66,68,45,69,51,69,55,45,49,54,56,53,49,37,48,35,6,3,85,4,3,19,28,77,105,99,114,111,115,
      111,102,116,32,84,105,109,101,45,83,116,97,109,112,32,83,101,114,118,105,99,101,48,130,1,34,48,13,6,9,42,134,72,134,247,13,1,1,1,5,0,3,130,1,15,0,48,130,1,10,2,130,1,1,0,206,19,183,24,16,162,252,245,240,92,174,172,237,252,27,117,170,149,95,158,
      180,177,99,9,88,130,205,235,220,47,237,26,86,94,20,74,39,115,42,142,107,228,81,96,100,31,91,234,119,0,145,240,9,197,135,250,198,100,56,216,112,230,182,152,238,249,194,182,59,110,42,223,27,190,163,80,209,167,176,197,119,125,186,212,121,193,101,
      84,95,156,74,17,236,240,8,135,215,158,156,26,190,44,81,248,103,122,64,172,189,212,215,47,19,193,87,65,83,221,86,16,235,220,83,34,204,234,164,169,200,114,29,138,227,224,207,75,130,220,91,146,32,242,113,172,108,106,253,215,235,196,237,181,204,
      41,5,195,2,159,106,27,21,38,99,9,165,21,204,118,198,175,161,31,160,189,55,77,230,155,199,69,153,172,71,182,199,180,19,209,153,24,64,16,152,152,148,181,81,116,18,1,85,99,169,189,112,156,197,235,239,237,71,241,241,203,173,221,127,118,211,54,14,
      196,127,13,124,39,192,74,51,245,157,84,166,183,159,150,152,119,119,85,118,33,181,125,204,163,158,125,120,41,252,128,118,105,20,187,2,3,1,0,1,163,130,1,27,48,130,1,23,48,29,6,3,85,29,14,4,22,4,20,140,19,40,231,145,120,70,224,75,251,126,155,63,
      217,175,39,218,102,138,91,48,31,6,3,85,29,35,4,24,48,22,128,20,213,99,58,92,138,49,144,243,67,123,124,70,27,197,51,104,90,133,109,85,48,86,6,3,85,29,31,4,79,48,77,48,75,160,73,160,71,134,69,104,116,116,112,58,47,47,99,114,108,46,109,105,99,114,
      111,115,111,102,116,46,99,111,109,47,112,107,105,47,99,114,108,47,112,114,111,100,117,99,116,115,47,77,105,99,84,105,109,83,116,97,80,67,65,95,50,48,49,48,45,48,55,45,48,49,46,99,114,108,48,90,6,8,43,6,1,5,5,7,1,1,4,78,48,76,48,74,6,8,43,6,1,
      5,5,7,48,2,134,62,104,116,116,112,58,47,47,119,119,119,46,109,105,99,114,111,115,111,102,116,46,99,111,109,47,112,107,105,47,99,101,114,116,115,47,77,105,99,84,105,109,83,116,97,80,67,65,95,50,48,49,48,45,48,55,45,48,49,46,99,114,116,48,12,6,
      3,85,29,19,1,1,255,4,2,48,0,48,19,6,3,85,29,37,4,12,48,10,6,8,43,6,1,5,5,7,3,8,48,13,6,9,42,134,72,134,247,13,1,1,11,5,0,3,130,1,1,0,46,100,220,142,117,150,35,144,130,19,34,84,13,37,232,118,52,135,60,221,176,222,136,128,69,32,239,131,152,200,
      219,177,246,134,62,235,244,47,191,249,227,20,230,71,228,204,143,102,76,159,247,155,91,154,83,38,134,189,99,190,104,101,223,23,168,83,191,40,70,1,10,54,161,201,112,185,7,28,62,165,106,147,239,64,70,178,15,166,208,135,70,97,161,64,12,150,16,155,
      163,86,11,86,173,99,127,42,210,139,28,126,134,88,128,100,120,76,114,72,145,118,105,55,238,238,62,189,114,3,243,228,48,210,190,93,212,154,8,84,6,93,110,77,117,33,255,67,30,66,201,33,119,7,201,224,18,148,104,179,182,168,0,23,154,84,7,227,107,81,
      85,171,26,210,163,88,49,211,230,239,17,117,97,5,101,175,81,1,158,249,189,149,65,211,105,189,13,169,81,126,18,238,164,110,125,203,82,205,40,128,120,22,152,16,38,145,8,214,57,172,173,104,188,176,48,136,71,119,6,14,202,158,210,193,162,84,31,30,
      189,112,253,87,54,179,157,162,73,198,133,69,232,116,225,210,93,126,33,109,48,130,6,113,48,130,4,89,160,3,2,1,2,2,10,97,9,129,42,0,0,0,0,0,2,48,13,6,9,42,134,72,134,247,13,1,1,11,5,0,48,129,136,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,
      85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,50,48,48,6,3,85,4,3,19,
      41,77,105,99,114,111,115,111,102,116,32,82,111,111,116,32,67,101,114,116,105,102,105,99,97,116,101,32,65,117,116,104,111,114,105,116,121,32,50,48,49,48,48,30,23,13,49,48,48,55,48,49,50,49,51,54,53,53,90,23,13,50,53,48,55,48,49,50,49,52,54,53,
      53,90,48,124,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,
      114,112,111,114,97,116,105,111,110,49,38,48,36,6,3,85,4,3,19,29,77,105,99,114,111,115,111,102,116,32,84,105,109,101,45,83,116,97,109,112,32,80,67,65,32,50,48,49,48,48,130,1,34,48,13,6,9,42,134,72,134,247,13,1,1,1,5,0,3,130,1,15,0,48,130,1,10,
      2,130,1,1,0,169,29,13,188,119,17,138,58,32,236,252,19,151,245,250,127,105,148,107,116,84,16,213,165,10,0,130,133,251,237,124,104,75,44,95,197,195,229,97,194,118,183,62,102,43,91,240,21,83,39,4,49,31,65,27,26,149,29,206,9,19,142,124,97,48,89,
      177,48,68,15,241,96,136,132,84,67,12,215,77,184,56,8,179,66,221,147,172,214,115,48,87,38,130,163,69,13,208,234,245,71,129,205,191,36,96,50,88,96,70,242,88,71,134,50,132,30,116,97,103,145,95,129,84,177,207,147,76,146,193,196,166,93,209,97,19,
      110,40,198,26,249,134,128,187,223,97,252,70,193,39,29,36,103,18,114,26,33,138,175,75,100,137,80,98,177,93,253,119,31,61,240,87,117,172,189,138,66,77,64,81,209,15,156,6,62,103,127,245,102,192,3,150,68,126,239,208,75,253,110,229,154,202,177,168,
      242,122,42,10,49,240,218,78,6,145,182,136,8,53,232,120,28,176,233,153,205,60,231,47,68,186,167,244,220,100,189,164,1,193,32,9,147,120,205,252,188,192,201,68,93,94,22,156,1,5,79,34,77,2,3,1,0,1,163,130,1,230,48,130,1,226,48,16,6,9,43,6,1,4,1,
      130,55,21,1,4,3,2,1,0,48,29,6,3,85,29,14,4,22,4,20,213,99,58,92,138,49,144,243,67,123,124,70,27,197,51,104,90,133,109,85,48,25,6,9,43,6,1,4,1,130,55,20,2,4,12,30,10,0,83,0,117,0,98,0,67,0,65,48,11,6,3,85,29,15,4,4,3,2,1,134,48,15,6,3,85,29,19,
      1,1,255,4,5,48,3,1,1,255,48,31,6,3,85,29,35,4,24,48,22,128,20,213,246,86,203,143,232,162,92,98,104,209,61,148,144,91,215,206,154,24,196,48,86,6,3,85,29,31,4,79,48,77,48,75,160,73,160,71,134,69,104,116,116,112,58,47,47,99,114,108,46,109,105,99,
      114,111,115,111,102,116,46,99,111,109,47,112,107,105,47,99,114,108,47,112,114,111,100,117,99,116,115,47,77,105,99,82,111,111,67,101,114,65,117,116,95,50,48,49,48,45,48,54,45,50,51,46,99,114,108,48,90,6,8,43,6,1,5,5,7,1,1,4,78,48,76,48,74,6,8,
      43,6,1,5,5,7,48,2,134,62,104,116,116,112,58,47,47,119,119,119,46,109,105,99,114,111,115,111,102,116,46,99,111,109,47,112,107,105,47,99,101,114,116,115,47,77,105,99,82,111,111,67,101,114,65,117,116,95,50,48,49,48,45,48,54,45,50,51,46,99,114,116,
      48,129,160,6,3,85,29,32,1,1,255,4,129,149,48,129,146,48,129,143,6,9,43,6,1,4,1,130,55,46,3,48,129,129,48,61,6,8,43,6,1,5,5,7,2,1,22,49,104,116,116,112,58,47,47,119,119,119,46,109,105,99,114,111,115,111,102,116,46,99,111,109,47,80,75,73,47,100,
      111,99,115,47,67,80,83,47,100,101,102,97,117,108,116,46,104,116,109,48,64,6,8,43,6,1,5,5,7,2,2,48,52,30,50,32,29,0,76,0,101,0,103,0,97,0,108,0,95,0,80,0,111,0,108,0,105,0,99,0,121,0,95,0,83,0,116,0,97,0,116,0,101,0,109,0,101,0,110,0,116,0,46,
      32,29,48,13,6,9,42,134,72,134,247,13,1,1,11,5,0,3,130,2,1,0,7,230,136,81,13,226,198,224,152,63,129,113,3,61,157,163,161,33,111,179,235,166,204,245,49,190,207,5,226,169,254,250,87,109,25,48,179,194,197,102,201,106,223,245,231,240,120,189,199,
      168,158,37,227,249,188,237,107,84,87,8,43,81,130,68,18,251,185,83,140,204,244,96,18,138,118,204,64,64,65,155,220,92,23,255,92,249,94,23,53,152,36,86,75,116,239,66,16,200,175,191,127,198,127,242,55,125,90,63,28,242,153,121,74,145,82,0,175,56,
      15,23,245,47,121,129,101,217,169,181,107,228,199,206,246,202,122,0,111,75,48,68,36,34,60,207,237,3,165,150,143,89,41,188,182,253,4,225,112,159,50,74,39,253,85,175,47,254,182,229,142,51,187,98,95,154,219,87,64,233,241,206,153,102,144,140,255,
      106,98,127,221,197,74,11,145,38,226,57,236,25,74,113,99,157,123,33,109,195,156,163,162,60,250,127,125,150,106,144,120,166,109,210,225,156,249,29,252,56,216,148,244,198,165,10,150,134,164,189,158,26,174,4,66,131,184,181,128,155,34,56,32,181,37,
      229,100,236,247,244,191,126,99,89,37,15,122,46,57,87,118,162,113,170,6,138,15,137,22,186,97,167,17,203,154,216,14,71,154,128,197,208,205,167,208,239,125,131,240,225,59,113,9,223,93,116,152,34,8,97,218,176,80,30,111,189,241,225,0,223,231,49,7,
      164,147,58,247,101,71,120,232,248,168,72,171,247,222,114,126,97,107,111,119,169,129,203,167,9,172,57,187,236,198,203,216,130,180,114,205,29,244,184,133,1,30,128,251,27,137,42,84,57,178,91,218,200,13,85,153,122,135,115,59,8,230,152,45,234,141,
      224,51,46,18,41,245,192,47,84,39,33,247,200,172,78,218,40,184,177,169,219,150,178,167,66,162,201,207,25,65,77,224,134,249,42,154,163,17,102,48,211,187,116,50,75,223,99,123,245,153,138,47,27,199,33,175,89,181,174,220,68,60,151,80,113,215,161,
      210,197,85,227,105,222,87,193,209,222,48,192,253,204,230,77,251,13,191,93,79,233,157,30,25,56,47,188,207,88,5,46,239,13,160,80,53,218,239,9,39,28,213,179,126,53,30,8,186,218,54,219,211,95,143,222,116,136,73,18,161,130,2,206,48,130,2,55,2,1,1,
      48,129,248,161,129,208,164,129,205,48,129,202,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,
      114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,37,48,35,6,3,85,4,11,19,28,77,105,99,114,111,115,111,102,116,32,65,109,101,114,105,99,97,32,79,112,101,114,97,116,105,111,110,115,49,38,48,36,6,3,85,4,11,19,29,84,104,97,
      108,101,115,32,84,83,83,32,69,83,78,58,68,54,66,68,45,69,51,69,55,45,49,54,56,53,49,37,48,35,6,3,85,4,3,19,28,77,105,99,114,111,115,111,102,116,32,84,105,109,101,45,83,116,97,109,112,32,83,101,114,118,105,99,101,162,35,10,1,1,48,7,6,5,43,14,
      3,2,26,3,21,0,57,201,6,227,237,233,171,49,19,254,142,55,88,186,202,89,140,202,176,220,160,129,131,48,129,128,164,126,48,124,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,
      4,7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,38,48,36,6,3,85,4,3,19,29,77,105,99,114,111,115,111,102,116,32,84,105,109,101,45,83,116,97,109,
      112,32,80,67,65,32,50,48,49,48,48,13,6,9,42,134,72,134,247,13,1,1,5,5,0,2,5,0,226,186,12,142,48,34,24,15,50,48,50,48,48,55,49,54,48,55,49,53,50,54,90,24,15,50,48,50,48,48,55,49,55,48,55,49,53,50,54,90,48,119,48,61,6,10,43,6,1,4,1,132,89,10,4,
      1,49,47,48,45,48,10,2,5,0,226,186,12,142,2,1,0,48,10,2,1,0,2,2,41,93,2,1,255,48,7,2,1,0,2,2,19,237,48,10,2,5,0,226,187,94,14,2,1,0,48,54,6,10,43,6,1,4,1,132,89,10,4,2,49,40,48,38,48,12,6,10,43,6,1,4,1,132,89,10,3,2,160,10,48,8,2,1,0,2,3,7,161,
      32,161,10,48,8,2,1,0,2,3,1,134,160,48,13,6,9,42,134,72,134,247,13,1,1,5,5,0,3,129,129,0,22,205,149,20,58,138,95,48,118,42,240,78,169,15,110,186,214,146,36,106,29,223,245,68,44,69,40,221,28,166,35,30,4,24,224,95,192,175,98,144,138,50,33,93,181,
      87,178,37,108,154,247,77,103,89,100,210,41,192,22,219,77,90,138,119,78,99,105,205,213,253,74,66,54,237,249,1,8,132,8,228,230,184,181,172,20,40,103,33,149,175,151,144,82,2,195,169,205,76,40,45,160,125,96,245,123,75,168,100,84,164,64,20,173,246,
      80,226,119,122,72,195,131,202,215,105,15,6,196,115,49,130,3,13,48,130,3,9,2,1,1,48,129,147,48,124,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,7,19,7,82,101,100,109,
      111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,38,48,36,6,3,85,4,3,19,29,77,105,99,114,111,115,111,102,116,32,84,105,109,101,45,83,116,97,109,112,32,80,67,65,32,50,
      48,49,48,2,19,51,0,0,1,30,14,188,229,75,22,162,3,27,0,0,0,0,1,30,48,13,6,9,96,134,72,1,101,3,4,2,1,5,0,160,130,1,74,48,26,6,9,42,134,72,134,247,13,1,9,3,49,13,6,11,42,134,72,134,247,13,1,9,16,1,4,48,47,6,9,42,134,72,134,247,13,1,9,4,49,34,4,
      32,62,21,170,24,83,27,186,252,49,103,9,7,107,15,25,70,112,152,158,183,99,112,34,128,158,168,150,20,247,242,141,49,48,129,250,6,11,42,134,72,134,247,13,1,9,16,2,47,49,129,234,48,129,231,48,129,228,48,129,189,4,32,115,59,228,88,195,223,113,58,
      81,229,189,167,159,60,161,55,113,45,205,37,213,148,241,250,34,4,28,15,141,214,95,71,48,129,152,48,129,128,164,126,48,124,49,11,48,9,6,3,85,4,6,19,2,85,83,49,19,48,17,6,3,85,4,8,19,10,87,97,115,104,105,110,103,116,111,110,49,16,48,14,6,3,85,4,
      7,19,7,82,101,100,109,111,110,100,49,30,48,28,6,3,85,4,10,19,21,77,105,99,114,111,115,111,102,116,32,67,111,114,112,111,114,97,116,105,111,110,49,38,48,36,6,3,85,4,3,19,29,77,105,99,114,111,115,111,102,116,32,84,105,109,101,45,83,116,97,109,
      112,32,80,67,65,32,50,48,49,48,2,19,51,0,0,1,30,14,188,229,75,22,162,3,27,0,0,0,0,1,30,48,34,4,32,146,233,11,140,189,205,236,221,167,133,224,158,59,132,173,142,14,113,155,24,123,142,238,136,115,176,28,5,161,169,8,210,48,13,6,9,42,134,72,134,
      247,13,1,1,11,5,0,4,130,1,0,92,17,1,132,69,194,255,78,202,87,178,115,39,106,82,153,206,167,189,144,108,24,33,110,130,234,162,58,28,66,190,179,215,167,160,74,218,94,57,140,245,163,198,242,118,167,108,164,195,250,0,83,44,130,89,5,2,97,103,10,139,
      201,38,17,197,245,109,161,197,163,237,11,217,233,123,131,105,129,60,220,31,62,147,111,97,39,80,127,241,6,103,232,218,131,152,89,60,86,200,250,135,122,39,253,136,25,179,1,228,131,156,208,132,149,56,225,9,131,143,209,48,35,175,209,233,124,123,
      143,184,223,56,9,85,118,35,237,66,33,190,106,165,220,99,105,107,253,80,172,176,242,157,69,146,239,132,157,199,180,139,146,245,155,120,115,171,152,92,66,176,40,65,45,129,29,90,238,162,106,116,139,124,182,148,57,87,86,236,24,152,14,90,47,203,39,
      156,236,214,191,38,166,164,116,213,97,44,165,46,244,151,25,171,85,57,131,239,59,220,197,45,73,34,119,132,58,209,224,170,120,199,134,244,76,107,174,180,45,218,248,68,89,25,207,21,67,86,85,19,134,144,95,142,184,109,230,48,225
    };
   #endif

    return choc::memory::MemoryDLL (dllData, sizeof (dllData));
}

#endif

#endif // CHOC_WEBVIEW_HEADER_INCLUDED
