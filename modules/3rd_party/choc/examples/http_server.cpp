#include <iostream>
#include <string_view>
#include <thread>
#include <chrono>

#include "../choc/network/choc_HTTPServer.h"
#include "../choc/text/choc_StringUtilities.h"
#include "../choc/text/choc_JSON.h"

static constexpr std::string_view demoPageHTML = R"xx (
<!DOCTYPE html> <html>

<head><title>CHOC webserver demo</title></head>

<body>
    <h1>CHOC webserver demo</h1>

    <p>This page is being served by an instance of a <code>choc::network::HTTPServer</code></p>
    <p>It connects a websocket to the C++ back-end and shows how to send messages either way...</p>
    <p><button onclick="sendEvent()">Click to send a message via the websocket</button></p>
    <p id="reply"></p>
</body>

<script>

function printMessage (message)
{
    document.querySelector ("#reply").innerText += message + "\n";
}

const socket = new WebSocket (SOCKET_URL);

socket.onopen = () =>
{
    printMessage ("Websocket connected");
}

socket.onmessage = (message) =>
{
    printMessage (`Received websocket message: "${message.data}"`);
};

function sendEvent()
{
    socket.send ("hello from javascript");
}

</script>

</html>
)xx";

choc::network::HTTPServer server;

//==============================================================================
// This is the object that we'll use to communicate with each instance of a connected client.
struct ExampleClientInstance  : public choc::network::HTTPServer::ClientInstance
{
    ExampleClientInstance()
    {
        static int clientCount = 0;
        clientID = ++clientCount;

        std::cout << "New client connected, ID: " << clientID << std::endl;
    }

    ~ExampleClientInstance()
    {
        std::cout << "Client ID " << clientID << " disconnected" << std::endl;
    }

    choc::network::HTTPContent getHTTPContent (std::string_view path) override
    {
        // This path is asking for the default page content
        if (path == "/")
        {
            // When we return the content for the page, we'll embed the URL into it that
            // it'll need to use to open the websocket back to this address..
            auto pageContent = choc::text::replace (demoPageHTML,
                                                    "SOCKET_URL", choc::json::getEscapedQuotedString (server.getWebSocketAddress()));

            return choc::network::HTTPContent::forHTML (pageContent);
        }

        // If you want to serve content for other paths, you would do that here...

        return {};
    }

    void upgradedToWebSocket (std::string_view path) override
    {
        std::cout << "Client ID " << clientID << " opened websocket for path: " << path << std::endl;
    }

    void handleWebSocketMessage (std::string_view message) override
    {
        std::cout << "Client ID " << clientID << " received websocket message: " << message << std::endl;

        // for this demo, we'll just bounce back the same message we received, but
        // obviously this can be anything..
        sendWebSocketMessage (std::string (message));
    }

    int clientID = 0;
};

//==============================================================================
int main()
{
    auto address = "127.0.0.1";
    uint16_t preferredPortNum = 3000;

    bool openedOK = server.open (address, preferredPortNum, 0,
                                 []
                                 {
                                     // Create a new object for each client..
                                     return std::make_unique<ExampleClientInstance>();
                                 },
                                 [] (const std::string& error)
                                 {
                                     // Handle some kind of server error..
                                     std::cout << "Error from webserver: " << error << std::endl;
                                 });

    if (! openedOK)
        return 1;

    std::cout << "HTTP server is running!" << std::endl
              << std::endl
              << "Use a browser to view it at: " << server.getHTTPAddress() << std::endl;

    // While the server is running, this thread no longer needs to be involved.
    // For this command-line demo we'll just sleep, but you could also run the
    // message loop or get on with other tasks.

    for (;;)
        std::this_thread::sleep_for (std::chrono::seconds (1));

    return 0;
}