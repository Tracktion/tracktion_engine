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

#include "choc_tests.h"
#include "../containers/choc_ArgumentList.h"
#include "choc_webview_example.h"
#include "choc_webserver_example.h"

//==============================================================================
int main (int argc, const char** argv)
{
    choc::ArgumentList args (argc, argv);

    if (args.contains ("webview"))
        return openDemoWebViewWindow();

    if (args.contains ("webserver"))
        return runDemoWebServer();

    choc::test::TestProgress progress;
    return choc_unit_tests::runAllTests (progress, args.contains ("--multithread")) ? 0 : 1;
}

//==============================================================================
// include this last to make sure the tests don't rely on any of the garbage
// that gets dragged in..
#include "../javascript/choc_javascript_Duktape.h"
#include "../javascript/choc_javascript_QuickJS.h"

#if CHOC_V8_AVAILABLE
 #include "../javascript/choc_javascript_V8.h"
#endif
