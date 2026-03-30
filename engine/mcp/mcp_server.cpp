#include "mcp/mcp_server.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace gg {

struct McpServer::Impl {
    std::string name;
    std::string version;
    std::vector<McpToolDef> tools;
};

McpServer::McpServer(std::string name, std::string version) : impl_(std::make_unique<Impl>()) {
    impl_->name = std::move(name);
    impl_->version = std::move(version);
}

McpServer::~McpServer() = default;

std::unique_ptr<McpServer> McpServer::create(const std::string& server_name,
                                             const std::string& server_version) {
    return std::unique_ptr<McpServer>(new McpServer(server_name, server_version));
}

void McpServer::register_tool(McpToolDef tool) {
    impl_->tools.push_back(std::move(tool));
}

size_t McpServer::tool_count() const {
    return impl_->tools.size();
}

std::string McpServer::handle_message(const std::string& json_message) {
    using json = nlohmann::json;

    // --- parse ---
    json req;
    try {
        req = json::parse(json_message);
    } catch (const json::exception&) {
        json err;
        err["jsonrpc"] = "2.0";
        err["id"] = nullptr;
        err["error"] = {{"code", -32700}, {"message", "Parse error"}};
        return err.dump();
    }

    const auto id = req.value("id", json{});
    const auto method = req.value("method", std::string{});

    // Notifications (no id field) — no response needed
    if (method == "notifications/initialized") {
        return "";
    }

    auto make_error = [&](int code, const std::string& msg) {
        json r;
        r["jsonrpc"] = "2.0";
        r["id"] = id;
        r["error"] = {{"code", code}, {"message", msg}};
        return r.dump();
    };

    if (method == "initialize") {
        json result;
        result["protocolVersion"] = "2024-11-05";
        result["serverInfo"] = {{"name", impl_->name}, {"version", impl_->version}};
        result["capabilities"] = {{"tools", json::object()}};

        json resp;
        resp["jsonrpc"] = "2.0";
        resp["id"] = id;
        resp["result"] = result;
        return resp.dump();
    }

    if (method == "tools/list") {
        json tools_array = json::array();
        for (const auto& tool : impl_->tools) {
            json t;
            t["name"] = tool.name;
            t["description"] = tool.description;
            try {
                t["inputSchema"] = json::parse(tool.input_schema);
            } catch (...) {
                t["inputSchema"] = json::object();
            }
            tools_array.push_back(t);
        }

        json resp;
        resp["jsonrpc"] = "2.0";
        resp["id"] = id;
        resp["result"] = {{"tools", tools_array}};
        return resp.dump();
    }

    if (method == "tools/call") {
        const auto params = req.value("params", json::object());
        const auto tool_name = params.value("name", std::string{});
        const auto arguments = params.value("arguments", json::object());

        const McpToolDef* found = nullptr;
        for (const auto& tool : impl_->tools) {
            if (tool.name == tool_name) {
                found = &tool;
                break;
            }
        }

        if (!found) {
            return make_error(-32602, "Unknown tool: " + tool_name);
        }

        std::string result_str;
        try {
            result_str = found->handler(arguments.dump());
        } catch (const std::exception& e) {
            return make_error(-32603, std::string("Tool error: ") + e.what());
        }

        json content = json::array();
        content.push_back({{"type", "text"}, {"text", result_str}});

        json resp;
        resp["jsonrpc"] = "2.0";
        resp["id"] = id;
        resp["result"] = {{"content", content}};
        return resp.dump();
    }

    return make_error(-32601, "Method not found: " + method);
}

} // namespace gg
