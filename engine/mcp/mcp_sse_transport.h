#pragma once
#include "mcp/mcp_transport.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <httplib.h>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

namespace gg {

class McpSseTransport : public McpTransport {
public:
    static std::unique_ptr<McpSseTransport> create(uint16_t port = 9315);
    ~McpSseTransport() override;

    McpSseTransport(const McpSseTransport&) = delete;
    McpSseTransport& operator=(const McpSseTransport&) = delete;

    void start() override;
    void stop() override;
    McpRequest poll_request() override;
    void send_response(const std::string& session_id, const std::string& response) override;

    [[nodiscard]] uint16_t port() const;
    [[nodiscard]] size_t client_count() const;

private:
    explicit McpSseTransport(uint16_t port);
    void server_thread_func();

    uint16_t port_;
    std::thread server_thread_;
    std::atomic<bool> running_{false};

    // Request queue: HTTP thread pushes, main loop pops
    std::mutex request_mutex_;
    std::queue<McpRequest> request_queue_;

    // Response delivery: main loop pushes, SSE threads pop
    struct SseClient {
        std::mutex mutex;
        std::condition_variable cv;
        std::queue<std::string> responses;
        std::atomic<bool> connected{true};
    };
    mutable std::mutex clients_mutex_;
    std::unordered_map<std::string, std::shared_ptr<SseClient>> clients_;

    uint64_t next_session_id_{1};

    std::unique_ptr<httplib::Server> server_;
};

} // namespace gg
