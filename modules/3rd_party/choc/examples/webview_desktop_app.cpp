#include <iostream>
#include <thread>
#include <chrono>
#include "../choc/gui/choc_WebView.h"
#include "../choc/gui/choc_DesktopWindow.h"
#include "../choc/gui/choc_MessageLoop.h"
#include "../choc/text/choc_JSON.h"
#include "../choc/platform/choc_Platform.h"

class SimpleWebApplication
{
public:
    SimpleWebApplication()
    {
        setupWindow();
        setupWebView();
    }

    void run()
    {
        std::cout << "Starting WebView desktop application...\n";
        std::cout << "The application window should appear shortly.\n";
        std::cout << "Close the window or press Ctrl+C to exit.\n";

        // Initialize message loop and DPI awareness
        choc::ui::setWindowsDPIAwareness(); // No-op on non-Windows platforms
        choc::messageloop::initialise();

        // Make window visible and bring to front
        window.setVisible (true);
        window.toFront();

        // Run the message loop
        choc::messageloop::run();

        std::cout << "Application closed.\n";
    }

private:
    choc::ui::DesktopWindow window { {100, 100, 800, 600} };

    std::unique_ptr<choc::ui::WebView> webView;

    void setupWindow()
    {
        window.setWindowTitle ("CHOC WebView Desktop App Example");
        window.centreWithSize (800, 600);
        window.setResizable (true);
        window.setMinimumSize (400, 300);

        // Set up close callback
        window.windowClosed = []()
        {
            std::cout << "Window closed, stopping message loop...\n";
            choc::messageloop::stop();
        };
    }

    void setupWebView()
    {
        choc::ui::WebView::Options options;
        options.enableDebugMode = true;
        options.enableDebugInspector = false; // Set to true to open dev tools

        options.webviewIsReady = [this](choc::ui::WebView& view)
        {
            std::cout << "WebView is ready, setting up bindings and loading content...\n";
            this->setupJavaScriptBindings (view);
            view.navigate ("choc://app/");
        };

        // Custom resource handler for serving local content
        options.fetchResource = [this](const std::string& path) -> choc::ui::WebView::Options::Resource
        {
            return this->handleResourceRequest (path);
        };

        webView = std::make_unique<choc::ui::WebView>(options);
        window.setContent (webView->getViewHandle());
    }

    void setupJavaScriptBindings (choc::ui::WebView& view)
    {
        // Bind C++ functions to be callable from JavaScript
        view.bind ("cpp_getCurrentTime", [](const choc::value::ValueView& args) -> choc::value::Value
        {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t (now);
            return choc::value::Value (std::to_string (time_t));
        });

        view.bind ("cpp_showAlert", [](const choc::value::ValueView& args) -> choc::value::Value
        {
            if (args.isArray() && args.size() > 0)
            {
                std::string message = args[0].getWithDefault<std::string>("No message");
                std::cout << "Alert from JavaScript: " << message << "\n";
            }
            return choc::value::Value();
        });

        view.bind ("cpp_calculate", [](const choc::value::ValueView& args) -> choc::value::Value
        {
            if (args.isArray() && args.size() >= 3)
            {
                double a = args[0].getWithDefault<double>(0.0);
                std::string op = args[1].getWithDefault<std::string>("+");
                double b = args[2].getWithDefault<double>(0.0);

                double result = 0.0;
                if (op == "+") result = a + b;
                else if (op == "-") result = a - b;
                else if (op == "*") result = a * b;
                else if (op == "/") result = b != 0.0 ? a / b : 0.0;

                auto response = choc::value::createObject ("result");
                response.setMember ("value", choc::value::Value (result));
                response.setMember ("expression", choc::value::Value (std::to_string (a) + " " + op + " " + std::to_string (b)));
                return response;
            }
            return choc::value::Value();
        });

        view.bind ("cpp_getSystemInfo", [](const choc::value::ValueView& args) -> choc::value::Value
        {
            auto info = choc::value::createObject ("systemInfo");
            info.setMember ("platform", choc::value::Value (std::string (CHOC_OPERATING_SYSTEM_NAME)));
            info.setMember ("timestamp", choc::value::Value (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()));

            auto features = choc::value::createEmptyArray();
            features.addArrayElement (choc::value::Value ("WebView"));
            features.addArrayElement (choc::value::Value ("Desktop Window"));
            features.addArrayElement (choc::value::Value ("JavaScript Binding"));
            info.setMember ("features", features);

            return info;
        });
    }

    choc::ui::WebView::Options::Resource handleResourceRequest (const std::string& path)
    {
        choc::ui::WebView::Options::Resource resource;

        if (path == "/" || path == "/index.html")
        {
            auto html = getMainHTML();
            resource.data = std::vector<uint8_t>(html.begin(), html.end());
            resource.mimeType = "text/html";
        }
        else if (path == "/style.css")
        {
            auto css = getCSS();
            resource.data = std::vector<uint8_t>(css.begin(), css.end());
            resource.mimeType = "text/css";
        }
        else if (path == "/script.js")
        {
            auto js = getJavaScript();
            resource.data = std::vector<uint8_t>(js.begin(), js.end());
            resource.mimeType = "application/javascript";
        }

        return resource;
    }

    void loadInitialContent()
    {
        // Navigate to our local content
        webView->navigate ("choc://app/");
    }

    std::string getMainHTML()
    {
        return R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>CHOC WebView App</title>
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <div class="container">
        <header>
            <h1>🎯 CHOC WebView Desktop Application</h1>
            <p>Demonstrating C++ ↔ JavaScript integration</p>
        </header>

        <section class="demo-section">
            <h2>System Information</h2>
            <button onclick="getSystemInfo()">Get System Info</button>
            <div id="systemInfo" class="info-box"></div>
        </section>

        <section class="demo-section">
            <h2>Current Time</h2>
            <button onclick="getCurrentTime()">Get Current Time</button>
            <div id="timeDisplay" class="info-box"></div>
        </section>

        <section class="demo-section">
            <h2>Calculator (C++ Backend)</h2>
            <div class="calculator">
                <input type="number" id="num1" placeholder="First number" value="10">
                <select id="operation">
                    <option value="+">+</option>
                    <option value="-">-</option>
                    <option value="*">×</option>
                    <option value="/">/</option>
                </select>
                <input type="number" id="num2" placeholder="Second number" value="5">
                <button onclick="calculate()">Calculate</button>
            </div>
            <div id="calcResult" class="info-box"></div>
        </section>

        <section class="demo-section">
            <h2>JavaScript → C++ Alerts</h2>
            <input type="text" id="alertMessage" placeholder="Enter message" value="Hello from JavaScript!">
            <button onclick="sendAlert()">Send Alert to C++</button>
        </section>

        <section class="demo-section">
            <h2>Live Data</h2>
            <button onclick="toggleAutoUpdate()" id="autoUpdateBtn">Start Auto-Update</button>
            <div id="liveData" class="info-box"></div>
        </section>
    </div>

    <script src="script.js"></script>
</body>
</html>
)HTML";
    }

    std::string getCSS()
    {
        return R"CSS(
body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    margin: 0;
    padding: 20px;
    background: linear-gradient (135deg, #667eea 0%, #764ba2 100%);
    color: #333;
    min-height: 100vh;
}

.container {
    max-width: 800px;
    margin: 0 auto;
    background: white;
    border-radius: 12px;
    box-shadow: 0 10px 30px rgba (0,0,0,0.2);
    overflow: hidden;
}

header {
    background: linear-gradient (135deg, #4facfe 0%, #00f2fe 100%);
    color: white;
    padding: 30px;
    text-align: center;
}

header h1 {
    margin: 0 0 10px 0;
    font-size: 2.5em;
    font-weight: 300;
}

header p {
    margin: 0;
    opacity: 0.9;
    font-size: 1.1em;
}

.demo-section {
    padding: 30px;
    border-bottom: 1px solid #eee;
}

.demo-section:last-child {
    border-bottom: none;
}

.demo-section h2 {
    margin: 0 0 20px 0;
    color: #2c3e50;
    font-size: 1.4em;
}

button {
    background: linear-gradient (135deg, #667eea 0%, #764ba2 100%);
    color: white;
    border: none;
    padding: 12px 24px;
    border-radius: 25px;
    cursor: pointer;
    font-size: 14px;
    font-weight: 500;
    transition: all 0.3s ease;
    margin: 5px;
}

button:hover {
    transform: translateY (-2px);
    box-shadow: 0 5px 15px rgba (102, 126, 234, 0.4);
}

button:active {
    transform: translateY (0);
}

input, select {
    padding: 10px 15px;
    border: 2px solid #e1e8ed;
    border-radius: 8px;
    font-size: 14px;
    margin: 5px;
    transition: border-color 0.3s ease;
}

input:focus, select:focus {
    outline: none;
    border-color: #667eea;
}

.calculator {
    display: flex;
    align-items: center;
    flex-wrap: wrap;
    gap: 10px;
    margin-bottom: 15px;
}

.info-box {
    background: #f8f9fa;
    border: 1px solid #e9ecef;
    border-radius: 8px;
    padding: 15px;
    margin-top: 15px;
    font-family: 'Courier New', monospace;
    font-size: 14px;
    min-height: 20px;
    white-space: pre-wrap;
}

.info-box:empty {
    display: none;
}

.success {
    background: #d4edda;
    border-color: #c3e6cb;
    color: #155724;
}

.error {
    background: #f8d7da;
    border-color: #f5c6cb;
    color: #721c24;
}

@media (max-width: 600px) {
    .calculator {
        flex-direction: column;
        align-items: stretch;
    }

    .calculator input,
    .calculator select,
    .calculator button {
        margin: 5px 0;
    }
}
)CSS";
    }

    std::string getJavaScript()
    {
        return R"JS(
let autoUpdateInterval = null;

async function getSystemInfo() {
    try {
        const info = await cpp_getSystemInfo();
        const infoDiv = document.getElementById ('systemInfo');
        infoDiv.className = 'info-box success';
        infoDiv.textContent = `Platform: ${info.platform}
Timestamp: ${info.timestamp}
Features: ${info.features.join (', ')}`;
    } catch(error) {
        showError ('systemInfo', 'Failed to get system info: ' + error.message);
    }
}

async function getCurrentTime() {
    try {
        const time = await cpp_getCurrentTime();
        const timeDiv = document.getElementById ('timeDisplay');
        timeDiv.className = 'info-box success';
        const date = new Date (parseInt (time) * 1000);
        timeDiv.textContent = `Current time: ${date.toLocaleString()}
Unix timestamp: ${time}`;
    } catch(error) {
        showError ('timeDisplay', 'Failed to get current time: ' + error.message);
    }
}

async function calculate() {
    try {
        const num1 = parseFloat (document.getElementById ('num1').value) || 0;
        const operation = document.getElementById ('operation').value;
        const num2 = parseFloat (document.getElementById ('num2').value) || 0;

        const result = await cpp_calculate (num1, operation, num2);
        const resultDiv = document.getElementById ('calcResult');
        resultDiv.className = 'info-box success';
        resultDiv.textContent = `${result.expression} = ${result.value}`;
    } catch(error) {
        showError ('calcResult', 'Calculation failed: ' + error.message);
    }
}

async function sendAlert() {
    try {
        const message = document.getElementById ('alertMessage').value || 'Hello from JavaScript!';
        await cpp_showAlert ([message]);

        // Show confirmation in the UI
        const alertSection = document.querySelector ('input#alertMessage').parentElement;
        const feedback = document.createElement ('div');
        feedback.className = 'info-box success';
        feedback.textContent = `Alert sent to C++: "${message}"`;
        feedback.style.marginTop = '10px';

        // Remove any existing feedback
        const existing = alertSection.querySelector ('.info-box');
        if(existing) existing.remove();

        alertSection.appendChild (feedback);

        // Auto-remove after 3 seconds
        setTimeout (() => feedback.remove(), 3000);
    } catch(error) {
        console.error ('Failed to send alert:', error);
    }
}

function toggleAutoUpdate() {
    const btn = document.getElementById ('autoUpdateBtn');
    const dataDiv = document.getElementById ('liveData');

    if(autoUpdateInterval) {
        clearInterval (autoUpdateInterval);
        autoUpdateInterval = null;
        btn.textContent = 'Start Auto-Update';
        dataDiv.className = 'info-box';
        dataDiv.textContent = 'Auto-update stopped';
    } else {
        btn.textContent = 'Stop Auto-Update';
        autoUpdateInterval = setInterval (async() => {
            try {
                const time = await cpp_getCurrentTime();
                const info = await cpp_getSystemInfo();
                dataDiv.className = 'info-box success';
                dataDiv.textContent = `Live Update - ${new Date().toLocaleTimeString()}
Platform: ${info.platform}
Server Time: ${new Date (parseInt (time) * 1000).toLocaleString()}`;
            } catch(error) {
                dataDiv.className = 'info-box error';
                dataDiv.textContent = 'Update failed: ' + error.message;
            }
        }, 1000);

        dataDiv.className = 'info-box success';
        dataDiv.textContent = 'Auto-update started...';
    }
}

function showError (elementId, message) {
    const element = document.getElementById (elementId);
    element.className = 'info-box error';
    element.textContent = message;
}

// Initialize the app
document.addEventListener ('DOMContentLoaded', function() {
    console.log ('CHOC WebView Desktop App loaded');
    getSystemInfo();
});

// Handle window beforeunload
window.addEventListener ('beforeunload', function() {
    if(autoUpdateInterval) {
        clearInterval (autoUpdateInterval);
    }
});
)JS";
    }
};

int main()
{
    std::cout << "CHOC WebView Desktop Application Example\n";
    std::cout << "========================================\n";

    try
    {
        SimpleWebApplication app;
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cout << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}