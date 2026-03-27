#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace gg {

struct McpToolDef {
    std::string name;
    std::string description;
    std::string input_schema;  // JSON Schema string
    std::function<std::string(const std::string& args_json)> handler;
};

class McpServer {
public:
    static std::unique_ptr<McpServer> create(const std::string& server_name,
                                              const std::string& server_version);
    ~McpServer();
    McpServer(const McpServer&) = delete;
    McpServer& operator=(const McpServer&) = delete;

    void register_tool(McpToolDef tool);
    std::string handle_message(const std::string& json_message);
    [[nodiscard]] size_t tool_count() const;

private:
    McpServer(std::string name, std::string version);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gg
