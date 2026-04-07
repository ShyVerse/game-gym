#include "ecs/components.h"
#include "ecs/world.h"
#include "mcp/mcp_server.h"
#include "mcp/mcp_sse_transport.h"
#include "mcp/mcp_tools.h"
#include "physics/physics_world.h"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

namespace {

constexpr uint16_t TEST_PORT = 19315;

class McpSseTest : public ::testing::Test {
protected:
    void SetUp() override {
        transport_ = gg::McpSseTransport::create(TEST_PORT);
        transport_->start();
        // Give server time to bind
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override { transport_->stop(); }

    std::unique_ptr<gg::McpSseTransport> transport_;
};

} // namespace

// ---------------------------------------------------------------------------
// Server lifecycle
// ---------------------------------------------------------------------------

TEST_F(McpSseTest, HealthCheck) {
    httplib::Client cli("localhost", TEST_PORT);
    auto res = cli.Get("/health");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto j = nlohmann::json::parse(res->body);
    EXPECT_EQ(j["status"].get<std::string>(), "ok");
    EXPECT_EQ(j["clients"].get<int>(), 0);
}

TEST_F(McpSseTest, PostWithoutSessionReturns400) {
    httplib::Client cli("localhost", TEST_PORT);
    auto res = cli.Post(
        "/message", R"({"jsonrpc":"2.0","id":1,"method":"initialize"})", "application/json");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 400);
}

TEST_F(McpSseTest, PostWithUnknownSessionReturns404) {
    httplib::Client cli("localhost", TEST_PORT);
    httplib::Headers headers = {{"Mcp-Session-Id", "nonexistent"}};
    auto res = cli.Post("/message",
                        headers,
                        R"({"jsonrpc":"2.0","id":1,"method":"initialize"})",
                        "application/json");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);
}

TEST_F(McpSseTest, SseConnectionAndMessageRoundtrip) {
    // Connect SSE in a background thread
    std::mutex data_mutex;
    std::string session_id;
    std::string received_response;
    std::atomic<bool> endpoint_received{false};
    std::atomic<bool> message_received{false};

    std::thread sse_thread([&]() {
        httplib::Client cli("localhost", TEST_PORT);
        cli.set_read_timeout(10);

        cli.Get("/sse", [&](const char* data, size_t len) -> bool {
            std::string chunk(data, len);

            // Parse SSE events from chunk
            if (chunk.find("event: endpoint") != std::string::npos) {
                auto data_pos = chunk.find("data: ");
                if (data_pos != std::string::npos) {
                    auto line_end = chunk.find('\n', data_pos);
                    auto json_str = chunk.substr(data_pos + 6, line_end - data_pos - 6);
                    auto j = nlohmann::json::parse(json_str);
                    {
                        std::lock_guard<std::mutex> lock(data_mutex);
                        session_id = j["sessionId"].get<std::string>();
                    }
                    endpoint_received.store(true);
                }
            }

            if (chunk.find("event: message") != std::string::npos) {
                auto data_pos = chunk.find("data: ", chunk.find("event: message"));
                if (data_pos != std::string::npos) {
                    auto line_end = chunk.find('\n', data_pos);
                    {
                        std::lock_guard<std::mutex> lock(data_mutex);
                        received_response = chunk.substr(data_pos + 6, line_end - data_pos - 6);
                    }
                    message_received.store(true);
                    return false; // Stop reading
                }
            }
            return !message_received.load();
        });
    });

    // Wait for SSE connection and endpoint event
    for (int i = 0; i < 100 && !endpoint_received.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ASSERT_TRUE(endpoint_received.load()) << "SSE endpoint event not received";

    std::string sid;
    {
        std::lock_guard<std::mutex> lock(data_mutex);
        sid = session_id;
    }
    ASSERT_FALSE(sid.empty());

    // Wait a bit for server to register the client
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify client count
    EXPECT_EQ(transport_->client_count(), 1u);

    // Send a JSON-RPC request via POST
    httplib::Client post_cli("localhost", TEST_PORT);
    httplib::Headers headers = {{"Mcp-Session-Id", sid}};
    auto res = post_cli.Post("/message",
                             headers,
                             R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})",
                             "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 202);

    // Poll and handle the request (simulate engine main loop)
    gg::McpRequest mcp_req;
    for (int i = 0; i < 50; ++i) {
        mcp_req = transport_->poll_request();
        if (!mcp_req.body.empty())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ASSERT_FALSE(mcp_req.body.empty());
    EXPECT_EQ(mcp_req.session_id, sid);

    // Create a server to handle the message
    auto server = gg::McpServer::create("test-engine", "1.0.0");
    std::string response = server->handle_message(mcp_req.body);
    transport_->send_response(mcp_req.session_id, response);

    // Wait for SSE response
    for (int i = 0; i < 50 && !message_received.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Stop transport first to unblock SSE thread
    transport_->stop();

    sse_thread.join();

    ASSERT_TRUE(message_received.load()) << "SSE message event not received";
    std::string resp_str;
    {
        std::lock_guard<std::mutex> lock(data_mutex);
        resp_str = received_response;
    }
    auto resp_json = nlohmann::json::parse(resp_str);
    EXPECT_TRUE(resp_json.contains("result"));
    EXPECT_EQ(resp_json["result"]["serverInfo"]["name"].get<std::string>(), "test-engine");
}

TEST_F(McpSseTest, PortAccessor) {
    EXPECT_EQ(transport_->port(), TEST_PORT);
}

TEST_F(McpSseTest, PollRequestEmptyWhenNoRequests) {
    auto req = transport_->poll_request();
    EXPECT_TRUE(req.body.empty());
    EXPECT_TRUE(req.session_id.empty());
}

TEST_F(McpSseTest, SendResponseToUnknownSessionIsNoop) {
    // Should not crash or throw
    transport_->send_response("unknown_session", R"({"test":true})");
}
