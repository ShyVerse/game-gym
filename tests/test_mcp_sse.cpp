#include "mcp/mcp_server.h"
#include "mcp/mcp_sse_transport.h"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

namespace {

constexpr uint16_t TEST_PORT = 19315;

/// Test fixture that runs transport + a simulated engine main loop.
class McpStreamableTest : public ::testing::Test {
protected:
    void SetUp() override {
        server_ = gg::McpServer::create("test-engine", "1.0.0");
        transport_ = gg::McpSseTransport::create(TEST_PORT);
        transport_->start();

        // Simulate engine main loop in a background thread
        loop_running_.store(true);
        main_loop_thread_ = std::thread([this]() {
            while (loop_running_.load()) {
                gg::McpRequest req = transport_->poll_request();
                if (!req.body.empty()) {
                    std::string response = server_->handle_message(req.body);
                    if (req.response_promise) {
                        req.response_promise->set_value(response);
                    } else if (!response.empty()) {
                        transport_->send_response(req.session_id, response);
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });

        // Give server time to bind
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        loop_running_.store(false);
        if (main_loop_thread_.joinable()) {
            main_loop_thread_.join();
        }
        transport_->stop();
    }

    std::unique_ptr<gg::McpServer> server_;
    std::unique_ptr<gg::McpSseTransport> transport_;
    std::thread main_loop_thread_;
    std::atomic<bool> loop_running_{false};
};

} // namespace

// ---------------------------------------------------------------------------
// Health check
// ---------------------------------------------------------------------------

TEST_F(McpStreamableTest, HealthCheck) {
    httplib::Client cli("localhost", TEST_PORT);
    auto res = cli.Get("/health");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto j = nlohmann::json::parse(res->body);
    EXPECT_EQ(j["status"].get<std::string>(), "ok");
}

// ---------------------------------------------------------------------------
// Streamable HTTP: POST /mcp returns JSON-RPC response inline
// ---------------------------------------------------------------------------

TEST_F(McpStreamableTest, InitializeReturnsInlineJson) {
    httplib::Client cli("localhost", TEST_PORT);
    auto res = cli.Post(
        "/", R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})", "application/json");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto j = nlohmann::json::parse(res->body);
    ASSERT_TRUE(j.contains("result"));
    EXPECT_EQ(j["result"]["serverInfo"]["name"].get<std::string>(), "test-engine");

    // Should have Mcp-Session-Id header
    auto session_id = res->get_header_value("Mcp-Session-Id");
    EXPECT_FALSE(session_id.empty());
}

TEST_F(McpStreamableTest, ToolsListReturnsInline) {
    httplib::Client cli("localhost", TEST_PORT);
    auto res = cli.Post(
        "/", R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})", "application/json");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto j = nlohmann::json::parse(res->body);
    ASSERT_TRUE(j.contains("result"));
    ASSERT_TRUE(j["result"].contains("tools"));
}

TEST_F(McpStreamableTest, UnknownMethodReturnsError) {
    httplib::Client cli("localhost", TEST_PORT);
    auto res = cli.Post(
        "/", R"({"jsonrpc":"2.0","id":3,"method":"nonexistent","params":{}})", "application/json");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto j = nlohmann::json::parse(res->body);
    ASSERT_TRUE(j.contains("error"));
    EXPECT_EQ(j["error"]["code"].get<int>(), -32601);
}

TEST_F(McpStreamableTest, NotificationReturns202) {
    httplib::Client cli("localhost", TEST_PORT);
    // notifications/initialized has no "id" field
    auto res = cli.Post(
        "/", R"({"jsonrpc":"2.0","method":"notifications/initialized"})", "application/json");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 202);
}

TEST_F(McpStreamableTest, InvalidJsonReturnsParseError) {
    httplib::Client cli("localhost", TEST_PORT);
    auto res = cli.Post("/", "not valid json {{{", "application/json");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 400);

    auto j = nlohmann::json::parse(res->body);
    ASSERT_TRUE(j.contains("error"));
    EXPECT_EQ(j["error"]["code"].get<int>(), -32700);
}

// ---------------------------------------------------------------------------
// Accessors and edge cases
// ---------------------------------------------------------------------------

TEST_F(McpStreamableTest, PortAccessor) {
    EXPECT_EQ(transport_->port(), TEST_PORT);
}

TEST_F(McpStreamableTest, PollRequestEmptyWhenNoRequests) {
    // Main loop is consuming requests, but we can still check the method exists
    auto transport2 = gg::McpSseTransport::create(TEST_PORT + 1);
    auto req = transport2->poll_request();
    EXPECT_TRUE(req.body.empty());
    EXPECT_TRUE(req.session_id.empty());
}

TEST_F(McpStreamableTest, SendResponseToUnknownSessionIsNoop) {
    transport_->send_response("unknown_session", R"({"test":true})");
}

// ---------------------------------------------------------------------------
// Legacy endpoints redirect
// ---------------------------------------------------------------------------

TEST_F(McpStreamableTest, LegacySseRedirects) {
    httplib::Client cli("localhost", TEST_PORT);
    cli.set_follow_location(false);
    auto res = cli.Get("/sse");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 301);
    EXPECT_EQ(res->get_header_value("Location"), "/");
}

TEST_F(McpStreamableTest, LegacyMessageRedirects) {
    httplib::Client cli("localhost", TEST_PORT);
    cli.set_follow_location(false);
    auto res = cli.Post("/message", "{}", "application/json");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 301);
    EXPECT_EQ(res->get_header_value("Location"), "/");
}
