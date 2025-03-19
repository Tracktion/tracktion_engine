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

#include "../gui/choc_DesktopWindow.h"

inline int openDemoWebViewWindow()
{
    choc::messageloop::initialise(); // Tells the message loop that this is the message thread
    choc::ui::setWindowsDPIAwareness(); // For Windows, we need to tell the OS we're high-DPI-aware

    choc::ui::DesktopWindow window ({ 100, 100, 800, 600 });

    window.setWindowTitle ("Hello");
    window.setResizable (true);
    window.setMinimumSize (300, 300);
    window.setMaximumSize (1500, 1200);
    window.windowClosed = [] { choc::messageloop::stop(); };

    choc::ui::WebView::Options options;
    options.enableDebugMode = true;

    options.webviewIsReady = [&] (choc::ui::WebView& webview)
    {
        CHOC_ASSERT (webview.loadedOK());

        window.setContent (webview.getViewHandle());

        CHOC_ASSERT (webview.bind ("eventCallbackFn", [] (const choc::value::ValueView& args) -> choc::value::Value
        {
            auto message = "eventCallbackFn() called with args: " + choc::json::toString (args);

            // This just shows how to invoke an async callback
            choc::messageloop::postMessage ([message]
            {
                std::cout << "WebView callback message: " << message << std::endl;
            });

            return choc::value::createString (message);
        }));

        CHOC_ASSERT (webview.bind ("loadCHOCWebsite", [&webview] (const choc::value::ValueView&) -> choc::value::Value
        {
            webview.navigate ("https://github.com/Tracktion/choc");
            return {};
        }));

        CHOC_ASSERT (webview.setHTML (R"xxx(
          <!DOCTYPE html> <html>
            <head> <title>Page Title</title> </head>
            <script>
              var eventCounter = 0;

              // invokes a call to eventCallbackFn() and displays the return value
              function sendEvent()
              {
                // When you invoke a function, it returns a Promise object
                eventCallbackFn({ counter: ++eventCounter }, "Hello World")
                  .then ((result) => { document.getElementById ("eventResultDisplay").innerText = result; });
              }
            </script>

            <body>
              <h1>CHOC WebView Demo</h1>
              <p>This is a demo of a choc::ui::WebView window</p>
              <p><button onclick="sendEvent()">Click to invoke an event callback</button></p>
              <p><button onclick="loadCHOCWebsite()">Click to visit the CHOC github repo</button></p>
              <p><input type="file" /></p>
              <p id="eventResultDisplay"></p>
            </body>
          </html>
        )xxx"));

        window.toFront();
    };

    choc::ui::WebView webview (options);

    choc::messageloop::run();
    return 0;
}