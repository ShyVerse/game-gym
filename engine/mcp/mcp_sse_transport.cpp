#include "mcp/mcp_sse_transport.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

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
    std::scoped_lock<std::mutex> lock(clients_mutex_);
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
    server_ = std::make_unique<httplib::Server>();
    server_thread_ = std::thread([this]() { server_thread_func(); });
}

void McpSseTransport::stop() {
    running_.store(false);

    // Notify all SSE clients to unblock
    {
        std::scoped_lock<std::mutex> lock(clients_mutex_);
        for (auto& [id, client] : clients_) {
            client->connected.store(false);
            client->cv.notify_all();
        }
    }

    // Wake up any blocked POST handlers
    {
        std::scoped_lock<std::mutex> lock(request_mutex_);
        while (!request_queue_.empty()) {
            auto& req = request_queue_.front();
            if (req.response_cv) {
                req.response_cv->notify_all();
            }
            request_queue_.pop();
        }
    }

    if (server_) {
        server_->stop();
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

McpRequest McpSseTransport::poll_request() {
    std::scoped_lock<std::mutex> lock(request_mutex_);
    if (request_queue_.empty()) {
        return {};
    }
    auto req = std::move(request_queue_.front());
    request_queue_.pop();
    return req;
}

void McpSseTransport::send_response(const std::string& session_id, const std::string& response) {
    // For Streamable HTTP: responses are delivered synchronously via McpRequest fields.
    // This method is kept for SSE push notifications to connected clients.
    std::scoped_lock<std::mutex> lock(clients_mutex_);
    auto it = clients_.find(session_id);
    if (it == clients_.end()) {
        return;
    }
    auto& client = it->second;
    {
        std::scoped_lock<std::mutex> client_lock(client->mutex);
        client->responses.push(response);
    }
    client->cv.notify_one();
}

void McpSseTransport::server_thread_func() {
    auto& svr = *server_;

    // Health check
    svr.Get("/health", [this](const httplib::Request&, httplib::Response& res) {
        nlohmann::json j;
        j["status"] = "ok";
        j["clients"] = client_count();
        res.set_content(j.dump(), "application/json");
    });

    // MCP Streamable HTTP: GET /mcp — optional server-to-client SSE stream
    svr.Get("/", [this](const httplib::Request&, httplib::Response& res) {
        std::string session_id;
        auto client = std::make_shared<SseClient>();

        {
            std::scoped_lock<std::mutex> lock(clients_mutex_);
            session_id = std::to_string(next_session_id_++);
            clients_[session_id] = client;
        }

        res.set_header("Cache-Control", "no-cache");
        res.set_header("X-Accel-Buffering", "no");
        res.set_header("Mcp-Session-Id", session_id);

        res.set_chunked_content_provider(
            "text/event-stream",
            [this, session_id, client](size_t /*offset*/, httplib::DataSink& sink) -> bool {
                while (client->connected.load() && running_.load()) {
                    std::unique_lock<std::mutex> lock(client->mutex);
                    client->cv.wait_for(lock, std::chrono::seconds(15), [&] {
                        return !client->responses.empty() || !client->connected.load() ||
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

                {
                    std::scoped_lock<std::mutex> lock2(clients_mutex_);
                    clients_.erase(session_id);
                }
                sink.done();
                return false;
            },
            [](bool) {});
    });

    // Legacy endpoints — redirect to /
    svr.Get("/sse", [](const httplib::Request&, httplib::Response& res) {
        res.status = 301;
        res.set_header("Location", "/");
    });

    // MCP Streamable HTTP: POST /mcp — JSON-RPC request handling
    svr.Post("/", [this](const httplib::Request& req, httplib::Response& res) {
        // Parse to check if it's a notification (no id) or a request (has id)
        nlohmann::json parsed;
        try {
            parsed = nlohmann::json::parse(req.body);
        } catch (...) {
            res.status = 400;
            nlohmann::json err;
            err["jsonrpc"] = "2.0";
            err["id"] = nullptr;
            err["error"] = {{"code", -32700}, {"message", "Parse error"}};
            res.set_content(err.dump(), "application/json");
            return;
        }

        const bool is_notification = !parsed.contains("id");

        if (is_notification) {
            // Notifications: queue for main loop, return 202
            auto session_id = req.get_header_value("Mcp-Session-Id");
            {
                std::scoped_lock<std::mutex> lock(request_mutex_);
                request_queue_.push({session_id, req.body, nullptr, nullptr, nullptr});
            }
            res.status = 202;
            return;
        }

        // Requests: queue and wait for main loop to process
        std::mutex response_mutex;
        std::condition_variable response_cv;
        std::string response_body;

        auto session_id = req.get_header_value("Mcp-Session-Id");

        {
            std::scoped_lock<std::mutex> lock(request_mutex_);
            request_queue_.push(
                {session_id, req.body, &response_mutex, &response_cv, &response_body});
        }

        // Wait for main loop to fill response_body
        {
            std::unique_lock<std::mutex> lock(response_mutex);
            response_cv.wait_for(lock, std::chrono::seconds(30), [&] {
                return !response_body.empty() || !running_.load();
            });
        }

        if (response_body.empty()) {
            res.status = 504;
            nlohmann::json err;
            err["jsonrpc"] = "2.0";
            err["id"] = parsed.value("id", nlohmann::json{});
            err["error"] = {{"code", -32603}, {"message", "Request timeout"}};
            res.set_content(err.dump(), "application/json");
            return;
        }

        // For initialize responses, assign a session ID
        const auto method = parsed.value("method", std::string{});
        if (method == "initialize") {
            std::string new_session_id;
            {
                std::scoped_lock<std::mutex> lock(clients_mutex_);
                new_session_id = std::to_string(next_session_id_++);
                // Register a client entry for future SSE connections
                clients_[new_session_id] = std::make_shared<SseClient>();
            }
            res.set_header("Mcp-Session-Id", new_session_id);
        }

        res.status = 200;
        res.set_content(response_body, "application/json");
    });

    svr.Get("/mcp", [](const httplib::Request&, httplib::Response& res) {
        res.status = 301;
        res.set_header("Location", "/");
    });
    svr.Post("/message", [](const httplib::Request&, httplib::Response& res) {
        res.status = 301;
        res.set_header("Location", "/");
    });
    svr.Post("/mcp", [](const httplib::Request&, httplib::Response& res) {
        res.status = 301;
        res.set_header("Location", "/");
    });

    svr.listen("127.0.0.1", port_);
}

} // namespace gg
