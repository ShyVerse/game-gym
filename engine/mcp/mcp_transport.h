#pragma once
#include <string>

namespace gg {

struct McpRequest {
    std::string session_id;
    std::string body;
};

class McpTransport {
public:
    virtual ~McpTransport() = default;

    virtual void start() = 0;
    virtual void stop() = 0;

    /// Returns the next pending request, or empty McpRequest if none.
    virtual McpRequest poll_request() = 0;

    /// Send a response to a specific client session.
    virtual void send_response(const std::string& session_id,
                               const std::string& response) = 0;
};

} // namespace gg
