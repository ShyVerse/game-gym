#include "mcp/mcp_stdio_transport.h"
#include <iostream>
#include <string>

namespace gg {

McpStdioTransport::~McpStdioTransport() {
    stop();
}

std::unique_ptr<McpStdioTransport> McpStdioTransport::create() {
    return std::unique_ptr<McpStdioTransport>(new McpStdioTransport());
}

void McpStdioTransport::start() {
    running_.store(true);
    reader_thread_ = std::thread([this]() { reader_loop(); });
    reader_thread_.detach();
}

void McpStdioTransport::stop() {
    running_.store(false);
    // Cannot interrupt a blocking getline — thread was detached; it will exit
    // when the next line arrives (or stdin closes).
}

std::string McpStdioTransport::poll_request() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (request_queue_.empty()) {
        return "";
    }
    std::string front = request_queue_.front();
    request_queue_.pop();
    return front;
}

void McpStdioTransport::send_response(const std::string& response) {
    std::cout << response << "\n";
    std::cout.flush();
}

void McpStdioTransport::reader_loop() {
    std::string line;
    while (running_.load() && std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        std::lock_guard<std::mutex> lock(queue_mutex_);
        request_queue_.push(line);
    }
}

} // namespace gg
