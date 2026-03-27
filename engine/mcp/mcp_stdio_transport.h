#pragma once
#include "mcp/mcp_transport.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace gg {

class McpStdioTransport : public McpTransport {
public:
    static std::unique_ptr<McpStdioTransport> create();
    ~McpStdioTransport() override;

    McpStdioTransport(const McpStdioTransport&) = delete;
    McpStdioTransport& operator=(const McpStdioTransport&) = delete;

    void start() override;
    void stop() override;
    std::string poll_request() override;
    void send_response(const std::string& response) override;

private:
    McpStdioTransport() = default;
    void reader_loop();

    std::thread          reader_thread_;
    std::mutex           queue_mutex_;
    std::queue<std::string> request_queue_;
    std::atomic<bool>    running_{false};
};

} // namespace gg
