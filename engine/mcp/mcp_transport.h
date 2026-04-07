#pragma once
#include <condition_variable>
#include <mutex>
#include <string>

namespace gg {

struct McpRequest {
    std::string session_id;
    std::string body;

    // Synchronous response delivery: main loop writes response here
    std::mutex* response_mutex = nullptr;
    std::condition_variable* response_cv = nullptr;
    std::string* response_out = nullptr;
};

class McpTransport {
public:
    virtual ~McpTransport() = default;

    virtual void start() = 0;
    virtual void stop() = 0;

    /// Returns the next pending request, or empty McpRequest if none.
    virtual McpRequest poll_request() = 0;

    /// Send a response to a specific client session.
    virtual void send_response(const std::string& session_id, const std::string& response) = 0;
};

} // namespace gg
