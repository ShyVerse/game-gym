#pragma once
#include <string>

namespace gg {

class McpTransport {
public:
    virtual ~McpTransport() = default;

    virtual void start() = 0;
    virtual void stop() = 0;

    /// Returns the next pending request, or "" if none.
    virtual std::string poll_request() = 0;

    /// Send a response to the client.
    virtual void send_response(const std::string& response) = 0;
};

} // namespace gg
