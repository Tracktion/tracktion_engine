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

#pragma once

#include <string>
#include <memory>
#include <functional>
#include <filesystem>
#include <variant>
#include <optional>
#include "../text/choc_StringUtilities.h"
#include "../platform/choc_Assert.h"

namespace choc::network
{

/// This is used for the data returned by HTTPServer::ClientInstance::getHTTPContent()
struct HTTPContent
{
    /// If set, this provides the content that should be returned directly.
    std::optional<std::string> content;
    /// If this is set, then the given file will be read and its content returned.
    std::optional<std::filesystem::path> file;
    /// If this is set, then it speficies a custom MIME type, but if left
    /// blank, a standard type will be determined from the file name.
    std::optional<std::string> mimeType;

    /// Returns true if the content contains either a file or data.
    operator bool() const;

    /// Quick way to create a HTTPContent object for some data, with a default MIME type.
    static HTTPContent forContent (std::string content);
    /// Quick way to create a HTTPContent object for some HTML, with the correct MIME type.
    static HTTPContent forHTML (std::string html);
    /// Quick way to create a HTTPContent object for a file, with a default MIME type.
    static HTTPContent forFile (std::filesystem::path);
};

/**
    A Server object runs a HTTP server on a specific port, and creates a client handler for
    each incoming connection. It also allows a connection to be upgraded to a websocket, and
    allows a client to send and receive messages on the socket.

    The point of this code is to wrap the complexity of the boost::beast and boost::asio
    utilities into a package that doesn't require writing a thousand lines of esoteric
    boilerplate to get the basic operations that are enough for most simple webserver apps.

    Unfortunately, this class does have dependencies: you need to have boost beast and
    its countless boost dependencies available for this file to include. But as long as
    you have them on your include path, everything is header-only so you don't need to
    worry about including anything else or adding anything to your project.
*/
class HTTPServer
{
public:
    struct ClientInstance
    {
        ClientInstance() = default;
        virtual ~ClientInstance() = default;

        /// This must return either a string with the file content or the location
        /// of a file to serve back to the client in response to the given path request.
        virtual HTTPContent getHTTPContent (std::string_view requestedPath) = 0;

        // This callback indicates that the client has upgraded to a websocket with
        /// the given path.
        virtual void upgradedToWebSocket (std::string_view path) = 0;

        /// Implement this function to handle incoming messages from the client via
        /// its websocket.
        virtual void handleWebSocketMessage (std::string_view message) = 0;

        /// Sends a message to the client over its websocket. This can only be called
        /// after upgradedToWebSocket() has indicated that a websocket is available.
        /// Returns false if the message failed to send
        bool sendWebSocketMessage (std::string message);

    private:
        friend class HTTPServer;
        std::function<bool(std::string)> sendFn;
    };

    /// This function creates new instances of your ClientInstance object.
    using CreateClientInstanceFn = std::function<std::shared_ptr<ClientInstance>()>;

    /// This function will be called for any errors that the server wants to report.
    using HandleErrorFn = std::function<void(const std::string&)>;

    //==============================================================================
    HTTPServer();
    ~HTTPServer();

    /// Launches a server which will listen on the given address and port number.
    /// If you pass 0 for the port number, a free one will be automatically chosen.
    /// When a client connects, it will call the CreateClientInstanceFn
    /// function provided to create a suitable ClientInstance which will live for
    /// as long as that client remains connected.
    /// If the numThreads is zero, a default thread pool size will be used.
    /// Returns false if it fails to initialise.
    bool open (std::string_view IPAddress,
               uint16_t portNumber,
               uint32_t numThreads,
               CreateClientInstanceFn,
               HandleErrorFn);

    /// Stops accepting any connections and resets this server.
    void close();

    /// Returns true if the server initialised OK and is now running.
    bool isOpen() const;

    /// Returns the current host address that the server is using.
    std::string getHost() const;

    /// Returns the port number that the server is currently using, or 0 if
    /// the server isn't running.
    uint16_t getPort() const;

    /// Returns the full address (host + port) that this server is curently using.
    std::string getAddress() const;

    /// Returns the full address, with HTTP scheme prefix, that the server is using.
    std::string getHTTPAddress() const;

    /// Returns the full address, with WebSocket scheme prefix, that the server is using.
    std::string getWebSocketAddress() const;

private:
    struct Pimpl;
    std::shared_ptr<Pimpl> pimpl;
};




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

} // namespace choc::network

#include <mutex>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <thread>
#include "choc_MIMETypes.h"

#define BOOST_STATIC_STRING_STANDALONE 1
#include "../platform/choc_DisableAllWarnings.h"

#if __has_include(<boost/beast.hpp>) && __has_include(<boost/asio.hpp>)
 #include <boost/beast.hpp>
 #include <boost/asio.hpp>
#else
 #error "The choc HTTPServer class requires boost::beast, boost::asio and their dependencies to be available on your include path"
#endif

#include "../platform/choc_ReenableAllWarnings.h"

namespace choc::network
{

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;

static constexpr uint32_t defaultNumThreads = 4;
static constexpr uint64_t messageBodySizeLimit = 10000;
static constexpr int timeoutSeconds = 30;

//==============================================================================
struct HTTPServer::Pimpl  : public std::enable_shared_from_this<HTTPServer::Pimpl>
{
    Pimpl (CreateClientInstanceFn&& cc, HandleErrorFn&& he)
        : createClient (std::move (cc)), handleError (std::move (he)) {}

    ~Pimpl() { stop(); }

    bool openFirstAvailablePort (asio::ip::address addr, uint16_t portToTry)
    {
        for (;;)
        {
            if (auto errorCode = openEndpoint (asio::ip::tcp::endpoint { addr, portToTry }))
            {
                if (errorCode == asio::error::address_in_use)
                {
                    if (portToTry < 65535)
                    {
                        ++portToTry;
                        acceptor.reset();
                        continue;
                    }
                }

                return false;
            }

            asyncAccept();
            return true;
        }
    }

    beast::error_code openEndpoint (asio::ip::tcp::endpoint endpoint)
    {
        beast::error_code errorCode;

        acceptor = std::make_unique<asio::ip::tcp::acceptor> (ioContext);

        acceptor->open (endpoint.protocol(), errorCode);
        if (errorCode) { reportErrorIfFatal (errorCode, "acceptor::open"); return errorCode; }

        acceptor->set_option (asio::socket_base::reuse_address(true), errorCode);
        if (errorCode) { reportErrorIfFatal (errorCode, "acceptor::set_option"); return errorCode; }

        acceptor->bind (endpoint, errorCode);
        if (errorCode) { reportErrorIfFatal (errorCode, "acceptor::bind"); return errorCode; }

        acceptor->listen (asio::socket_base::max_listen_connections, errorCode);
        if (errorCode) { reportErrorIfFatal (errorCode, "acceptor::listen"); return errorCode; }

        auto ep = acceptor->local_endpoint();
        std::ostringstream oss;
        oss << ep;
        address = oss.str();

        port = ep.port();
        host = choc::text::splitString (address, ':', false).front();

        return {};
    }

    void asyncAccept()
    {
        acceptor->async_accept (asio::make_strand (ioContext),
                                beast::bind_front_handler (&Pimpl::handleAccept, shared_from_this()));
    }

    void handleAccept (beast::error_code errorCode, asio::ip::tcp::socket socket)
    {
        if (errorCode)
            return reportErrorIfFatal (errorCode, "IOContextListener accept");

        if (auto client = createClient())
        {
            std::make_shared<ClientSession> (*this, std::move (socket), std::move (client))->run();
            asyncAccept();
        }
    }

    void start (uint32_t numThreads)
    {
        if (numThreads == 0 || numThreads > 512)
            numThreads = defaultNumThreads;

        CHOC_ASSERT (threadPool.empty());
        threadPool.reserve (numThreads);

        for (uint32_t i = 0; i < numThreads; ++i)
        {
            threadPool.emplace_back ([this]
            {
                try
                {
                    ioContext.run();
                }
                catch (std::exception& e)
                {
                    reportError (e.what());
                }
            });
        }
    }

    void stop()
    {
        if (! ioContext.stopped())
            ioContext.stop();

        for (auto& t : threadPool)
            t.join();
    }

    void reportError (const std::string& message)
    {
        if (handleError)
            handleError (message);
    }

    void reportErrorIfFatal (beast::error_code errorCode, const std::string& context)
    {
        if (errorCode != asio::error::operation_aborted
             && errorCode != beast::websocket::error::closed)
            reportError (context + ": " + errorCode.message());
    }

    CreateClientInstanceFn createClient;
    HandleErrorFn handleError;

    std::string host, address;
    uint16_t port = 0;

    asio::io_context ioContext;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
    std::vector<std::thread> threadPool;

    using ParserType = http::request_parser<http::string_body>;
    using MessageType = ParserType::value_type;

    //==============================================================================
    struct WebsocketSession   : public std::enable_shared_from_this<WebsocketSession>
    {
        WebsocketSession (Pimpl& p, std::shared_ptr<HTTPServer::ClientInstance> c, asio::ip::tcp::socket&& socket)
            : owner (p), clientInstance (std::move (c)), websocketStream (std::move (socket))
        {
        }

        ~WebsocketSession()
        {
            clientInstance->sendFn = {};
            clientInstance.reset();
        }

        void run (const MessageType& req)
        {
            websocketStream.set_option (beast::websocket::stream_base::timeout::suggested (beast::role_type::server));

            websocketStream.set_option (beast::websocket::stream_base::decorator ([] (beast::websocket::response_type& res)
            {
                res.set (http::field::server, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-choc-multi");
            }));

            websocketStream.async_accept (req, beast::bind_front_handler (&WebsocketSession::handleAccept, shared_from_this()));
        }

    private:
        Pimpl& owner;
        std::shared_ptr<HTTPServer::ClientInstance> clientInstance;
        beast::flat_buffer buffer;
        beast::websocket::stream<beast::tcp_stream> websocketStream;
        std::vector<std::shared_ptr<const std::string>> queue;

        bool send (const std::shared_ptr<const std::string>& message)
        {
            asio::post (websocketStream.get_executor(),
                        beast::bind_front_handler (&WebsocketSession::handleSend, shared_from_this(), message));

            return true;
        }

        void handleAccept (beast::error_code errorCode)
        {
            if (errorCode)
                return owner.reportErrorIfFatal (errorCode, "WebSocket accept");

            websocketStream.async_read (buffer, beast::bind_front_handler (&WebsocketSession::handleRead, shared_from_this()));

            clientInstance->sendFn = [this] (std::string m)
            {
                return this->send (std::make_shared<const std::string> (std::move (m)));
            };
        }

        void handleRead (beast::error_code errorCode, std::size_t)
        {
            if (errorCode)
                return owner.reportErrorIfFatal (errorCode, "WebSocket read");

            clientInstance->handleWebSocketMessage (beast::buffers_to_string (buffer.data()));
            buffer.consume (buffer.size());
            websocketStream.async_read (buffer, beast::bind_front_handler (&WebsocketSession::handleRead, shared_from_this()));
        }

        void handleSend (std::shared_ptr<const std::string>&& ss)
        {
            queue.push_back (std::move (ss));

            if (queue.size() <= 1)
                websocketStream.async_write (asio::buffer (*queue.front()),
                                             beast::bind_front_handler (&WebsocketSession::handleWrite, shared_from_this()));
        }

        void handleWrite (beast::error_code errorCode, std::size_t)
        {
            if (errorCode)
                return owner.reportErrorIfFatal (errorCode, "WebSocket write");

            queue.erase (queue.begin());

            if (! queue.empty())
                websocketStream.async_write (asio::buffer (*queue.front()),
                                             beast::bind_front_handler (&WebsocketSession::handleWrite, shared_from_this()));
        }
    };

    //==============================================================================
    struct ClientSession  : public std::enable_shared_from_this<ClientSession>
    {
        ClientSession (Pimpl& p, asio::ip::tcp::socket&& socket, std::shared_ptr<HTTPServer::ClientInstance> client)
            : owner (p), tcpStream (std::move (socket)), clientInstance (std::move (client))
        {
        }

        ~ClientSession()
        {
            clientInstance.reset();
        }

        void run()  { performRead(); }

    private:
        //==============================================================================
        Pimpl& owner;
        beast::tcp_stream tcpStream;
        beast::flat_buffer flatBuffer;
        std::shared_ptr<HTTPServer::ClientInstance> clientInstance;
        std::optional<ParserType> parser;

        void performRead()
        {
            parser.emplace(); // Construct a new parser for each message
            parser->body_limit (messageBodySizeLimit);
            tcpStream.expires_after (std::chrono::seconds (timeoutSeconds));

            http::async_read (tcpStream, flatBuffer, parser->get(),
                              beast::bind_front_handler (&ClientSession::handleRead, shared_from_this()));
        }

        void upgradeToWebsocket (asio::ip::tcp::socket&& socket, MessageType&& req)
        {
            auto target = std::string (req.target());

            std::make_shared<WebsocketSession> (owner, clientInstance, std::move (socket))
               ->run (std::move (req));

            clientInstance->upgradedToWebSocket (target);
        }

        void handleRead (beast::error_code errorCode, std::size_t)
        {
            if (errorCode == http::error::end_of_stream) // connection closed
            {
                tcpStream.socket().shutdown (asio::ip::tcp::socket::shutdown_send, errorCode);
                return;
            }

            if (errorCode)
                return owner.reportErrorIfFatal (errorCode, "HTTP read");

            if (beast::websocket::is_upgrade (parser->get()))
                return upgradeToWebsocket (tcpStream.release_socket(), parser->release());

            handleRequest (parser->release());
        }

        template <typename ResponseType>
        void sendRequestResponse (ResponseType&& response, bool keepAlive)
        {
            response.set (http::field::server, BOOST_BEAST_VERSION_STRING);
            response.keep_alive (keepAlive);

            auto sp = std::make_shared<ResponseType> (std::move (response));

            http::async_write (tcpStream, *sp,
                               [self = shared_from_this(), sp] (beast::error_code e, std::size_t bytes)
                               {
                                   self->handleWrite (e, bytes, sp->need_eof());
                               },
                               nullptr);
        }

        template <typename RequestType>
        void sendFailureResponse (const RequestType& req, http::status status, std::string text)
        {
            http::response<http::string_body> res { status, req.version() };
            res.set (http::field::content_type, "text/html");
            res.body() = std::move (text);
            res.prepare_payload();

            sendRequestResponse (std::move (res), req.keep_alive());
        }

        void handleWrite (beast::error_code errorCode, std::size_t, bool close)
        {
            if (errorCode)
                return owner.reportErrorIfFatal (errorCode, "HTTP write");

            if (close)
            {
                tcpStream.socket().shutdown (asio::ip::tcp::socket::shutdown_send, errorCode);
                return;
            }

            performRead();
        }

        void handleRequest (const MessageType& req)
        {
            if (req.method() != http::verb::get
                 && req.method() != http::verb::head)
                return sendFailureResponse (req, http::status::bad_request, "Unknown HTTP-method");

            auto requestPath = std::string (req.target());

            if (requestPath.empty()
                 || requestPath[0] != '/'
                 || requestPath.find("..") != std::string_view::npos)
                return sendFailureResponse (req, http::status::bad_request, "Illegal request-target");

            auto result = clientInstance->getHTTPContent (requestPath);

            auto getMIMEType = [&] (const std::string& path) -> std::string
            {
                if (result.mimeType)
                    return *result.mimeType;

                return choc::network::getMIMETypeFromFilename (path);
            };

            if (result.content)
            {
                if (req.method() == http::verb::get)
                {
                    auto size = http::string_body::size (*result.content);

                    http::response<http::string_body> res (std::piecewise_construct,
                                                           std::make_tuple (std::move (*result.content)),
                                                           std::make_tuple (http::status::ok, req.version()));

                    res.set (http::field::content_type, getMIMEType (requestPath));
                    res.content_length (size);

                    return sendRequestResponse (std::move (res), req.keep_alive());
                }

                return sendFailureResponse (req, http::status::bad_request, "POST not supported");
            }

            if (result.file)
            {
                beast::error_code errorCode;
                http::file_body::value_type body;
                auto path = result.file->string();
                body.open (path.c_str(), beast::file_mode::scan, errorCode);

                if (errorCode != boost::system::errc::no_such_file_or_directory)
                {
                    if (errorCode)
                        return sendFailureResponse (req, http::status::internal_server_error,
                                                    "An error occurred: '" + errorCode.message() + "'");

                    auto size = body.size();

                    if (req.method() == http::verb::head)
                    {
                        http::response<http::empty_body> res { http::status::ok, req.version() };
                        res.set (http::field::content_type, getMIMEType (path));
                        res.content_length (size);
                        return sendRequestResponse (std::move (res), req.keep_alive());
                    }

                    http::response<http::file_body> res (std::piecewise_construct,
                                                         std::make_tuple (std::move (body)),
                                                         std::make_tuple (http::status::ok, req.version()));

                    res.set (http::field::content_type, getMIMEType (path));
                    res.content_length (size);
                    return sendRequestResponse (std::move (res), req.keep_alive());
                }
            }

            return sendFailureResponse (req, http::status::not_found,
                                        "The resource '" + std::string (req.target()) + "' was not found.");
        }
    };
};

//==============================================================================
inline HTTPServer::HTTPServer() = default;
inline HTTPServer::~HTTPServer() { close(); }

inline bool HTTPServer::open (std::string_view address, uint16_t port, uint32_t numThreads,
                              CreateClientInstanceFn createClient, HandleErrorFn handleError)
{
    CHOC_ASSERT (createClient != nullptr);
    close();
    pimpl = std::make_shared<Pimpl> (std::move (createClient), std::move (handleError));

    if (pimpl->openFirstAvailablePort (asio::ip::make_address (address), port))
    {
        pimpl->start (numThreads);
        return true;
    }

    pimpl.reset();
    return false;
}

inline void HTTPServer::close()
{
    if (pimpl)
    {
        pimpl->stop();
        pimpl.reset();
    }
}

inline bool HTTPServer::isOpen() const                      { return pimpl != nullptr; }
inline std::string HTTPServer::getHost() const              { return pimpl != nullptr ? pimpl->host : std::string(); }
inline uint16_t HTTPServer::getPort() const                 { return pimpl != nullptr ? pimpl->port : 0; }
inline std::string HTTPServer::getAddress() const           { return pimpl != nullptr ? pimpl->address : std::string(); }
inline std::string HTTPServer::getHTTPAddress() const       { return "http://" + getAddress(); }
inline std::string HTTPServer::getWebSocketAddress() const  { return "ws://" + getAddress(); }

inline bool HTTPServer::ClientInstance::sendWebSocketMessage (std::string m)
{
    return sendFn != nullptr && sendFn (std::move (m));
}

inline HTTPContent::operator bool() const   { return content || file; }

inline HTTPContent HTTPContent::forContent (std::string content)    { HTTPContent c; c.content = std::move (content); return c; }
inline HTTPContent HTTPContent::forHTML (std::string html)          { HTTPContent c; c.content = std::move (html); c.mimeType = "text/html"; return c; }
inline HTTPContent HTTPContent::forFile (std::filesystem::path f)   { HTTPContent c; c.file = std::move (f); return c; }

} // namespace choc::network
