

#include <server/http/https_client.h>

namespace CppServer {
namespace HTTP {

void HTTPClient::onReceived(const void* buffer, size_t size)
{
    // Receive HTTP response header
    if (_response.IsPendingHeader())
    {
        if (_response.ReceiveHeader(buffer, size))
            onReceivedResponseHeader(_response);

        size = 0;
    }

    // Check for HTTP response error
    if (_response.error())
    {
        onReceivedResponseError(_response, "Invalid HTTP response!");
        _response.Clear();
        DisconnectAsync();
        return;
    }

    // Receive HTTP response body
    if (_response.ReceiveBody(buffer, size))
    {
        onReceivedResponse(_response);
        _response.Clear();
        return;
    }

    // Check for HTTP response error
    if (_response.error())
    {
        onReceivedResponseError(_response, "Invalid HTTP response!");
        _response.Clear();
        DisconnectAsync();
        return;
    }
}

void HTTPClient::onDisconnected()
{
    // Receive HTTP response body
    if (_response.IsPendingBody())
    {
        onReceivedResponse(_response);
        _response.Clear();
        return;
    }
}

std::future<HTTPResponse> HTTPClientEx::SendRequest(const HTTPRequest& request, const CppCommon::Timespan& timeout)
{
    // Create TCP resolver if the current one is empty
    if (!_resolver)
        _resolver = std::make_shared<Asio::TCPResolver>(service());
    // Create timeout check timer if the current one is empty
    if (!_timeout)
        _timeout = std::make_shared<Asio::Timer>(service());

    _promise = std::promise<HTTPResponse>();
    _request = request;

    // Check if the HTTP request is valid
    if (_request.empty() || _request.error())
    {
        SetPromiseError("Invalid HTTP request!");
        return _promise.get_future();
    }

    if (!IsConnected())
    {
        // Connect to the Web server
        if (!ConnectAsync(_resolver))
        {
            SetPromiseError("Connection failed!");
            return _promise.get_future();
        }
    }
    else
    {
        // Send prepared HTTP request
        if (!SendRequestAsync())
        {
            SetPromiseError("Failed to send HTTP request!");
            return _promise.get_future();
        }
    }

    // Setup timeout check timer
    auto self(this->shared_from_this());
    auto timeout_handler = [this, self](bool canceled)
    {
        if (canceled)
            return;

        // Disconnect on timeout
        onReceivedResponseError(_response, "Timeout!");
        _response.Clear();
        DisconnectAsync();
    };
    if (!_timeout->Setup(timeout_handler, timeout) || !_timeout->WaitAsync())
    {
        SetPromiseError("Failed to setup timeout timer!");
        return _promise.get_future();
    }

    return _promise.get_future();
}

void HTTPClientEx::onConnected()
{
    HTTPClient::onConnected();

    // Send prepared HTTP request on connect
    if (!_request.empty() && !_request.error())
        if (!SendRequestAsync())
            SetPromiseError("Failed to send HTTP request!");
}

void HTTPClientEx::onDisconnected()
{
    // Cancel timeout check timer
    if (_timeout)
        _timeout->Cancel();

    HTTPClient::onDisconnected();
}

void HTTPClientEx::onReceivedResponse(const HTTPResponse& response)
{
    // Cancel timeout check timer
    if (_timeout)
        _timeout->Cancel();

    SetPromiseValue(response);
}

void HTTPClientEx::onReceivedResponseError(const HTTPResponse& response, const std::string& error)
{
    // Cancel timeout check timer
    if (_timeout)
        _timeout->Cancel();

    SetPromiseError(error);
}

void HTTPClientEx::SetPromiseValue(const HTTPResponse& response)
{
    _promise.set_value(response);
    _request.Clear();
}

void HTTPClientEx::SetPromiseError(const std::string& error)
{
    _promise.set_exception(std::make_exception_ptr(std::runtime_error(error)));
    _request.Clear();
}

} // namespace HTTP
} // namespace CppServer
