#include "mcp/mcp_sse_transport.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <sstream>

namespace gg {

McpSseTransport::McpSseTransport(uint16_t port) : port_(port) {}

McpSseTransport::~McpSseTransport() {
    stop();
}

std::unique_ptr<McpSseTransport> McpSseTransport::create(uint16_t port) {
    return std::unique_ptr<McpSseTransport>(new McpSseTransport(port));
}

uint16_t McpSseTransport::port() const {
    return port_;
}

size_t McpSseTransport::client_count() const {
    std::lock_guard<std::mutex> lock(
        const_cast<std::mutex&>(clients_mutex_));
    size_t count = 0;
    for (const auto& [id, client] : clients_) {
        if (client->connected.load()) {
            ++count;
        }
    }
    return count;
}

void McpSseTransport::start() {
    running_.store(true);
    server_thread_ = std::thread([this]() { server_thread_func(); });
}

void McpSseTransport::stop() {
    running_.store(false);

    // Notify all SSE clients to unblock
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& [id, client] : clients_) {
            client->connected.store(false);
            client->cv.notify_all();
        }
    }

    // Make a dummy request to unblock the server's listen()
    httplib::Client cli("localhost", port_);
    cli.set_connection_timeout(1);
    cli.Get("/health");

    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

McpRequest McpSseTransport::poll_request() {
    std::lock_guard<std::mutex> lock(request_mutex_);
    if (request_queue_.empty()) {
        return {};
    }
    auto req = std::move(request_queue_.front());
    request_queue_.pop();
    return req;
}

void McpSseTransport::send_response(const std::string& session_id,
                                     const std::string& response) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    auto it = clients_.find(session_id);
    if (it == clients_.end()) {
        return;
    }
    auto& client = it->second;
    {
        std::lock_guard<std::mutex> client_lock(client->mutex);
        client->responses.push(response);
    }
    client->cv.notify_one();
}

void McpSseTransport::server_thread_func() {
    httplib::Server svr;

    // Health check
    svr.Get("/health", [this](const httplib::Request&, httplib::Response& res) {
        nlohmann::json j;
        j["status"] = "ok";
        j["clients"] = client_count();
        res.set_content(j.dump(), "application/json");
    });

    // SSE endpoint
    svr.Get("/sse", [this](const httplib::Request&, httplib::Response& res) {
        std::string session_id;
        auto client = std::make_shared<SseClient>();

        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            session_id = std::to_string(next_session_id_++);
            clients_[session_id] = client;
        }

        res.set_header("Cache-Control", "no-cache");
        res.set_header("X-Accel-Buffering", "no");

        res.set_chunked_content_provider(
            "text/event-stream",
            [this, session_id, client](size_t /*offset*/,
                                       httplib::DataSink& sink) -> bool {
                // Send endpoint event with session info
                std::string endpoint_event =
                    "event: endpoint\ndata: " +
                    nlohmann::json({{"sessionId", session_id},
                                    {"url", "/message"}})
                        .dump() +
                    "\n\n";
                sink.write(endpoint_event.c_str(), endpoint_event.size());

                while (client->connected.load() && running_.load()) {
                    std::unique_lock<std::mutex> lock(client->mutex);
                    client->cv.wait_for(lock, std::chrono::seconds(15),
                                        [&] {
                                            return !client->responses.empty() ||
                                                   !client->connected.load() ||
                                                   !running_.load();
                                        });

                    while (!client->responses.empty()) {
                        const auto& msg = client->responses.front();
                        std::string sse = "event: message\ndata: " + msg + "\n\n";
                        if (!sink.write(sse.c_str(), sse.size())) {
                            client->connected.store(false);
                            break;
                        }
                        client->responses.pop();
                    }

                    // Send keepalive comment
                    if (client->connected.load() && running_.load()) {
                        std::string keepalive = ": keepalive\n\n";
                        if (!sink.write(keepalive.c_str(), keepalive.size())) {
                            client->connected.store(false);
                        }
                    }
                }

                // Cleanup
                {
                    std::lock_guard<std::mutex> lock2(clients_mutex_);
                    clients_.erase(session_id);
                }
                sink.done();
                return false;
            },
            [](bool) {} // resource releaser
        );
    });

    // Message endpoint
    svr.Post("/message",
             [this](const httplib::Request& req, httplib::Response& res) {
                 auto session_id = req.get_header_value("Mcp-Session-Id");
                 if (session_id.empty()) {
                     res.status = 400;
                     res.set_content(
                         R"({"error":"Missing Mcp-Session-Id header"})",
                         "application/json");
                     return;
                 }

                 {
                     std::lock_guard<std::mutex> lock(clients_mutex_);
                     if (clients_.find(session_id) == clients_.end()) {
                         res.status = 404;
                         res.set_content(
                             R"({"error":"Unknown session"})",
                             "application/json");
                         return;
                     }
                 }

                 {
                     std::lock_guard<std::mutex> lock(request_mutex_);
                     request_queue_.push({session_id, req.body});
                 }

                 res.status = 202;
                 res.set_content(R"({"accepted":true})",
                                "application/json");
             });

    svr.listen("0.0.0.0", port_);
}

} // namespace gg
