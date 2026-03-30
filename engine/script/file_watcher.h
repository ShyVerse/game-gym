#pragma once

#include <memory>
#include <string>
#include <vector>

namespace gg {

class FileWatcher {
public:
    static std::unique_ptr<FileWatcher> create(const std::string& directory);
    virtual ~FileWatcher() = default;

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    virtual std::vector<std::string> poll_changes() = 0;
    [[nodiscard]] virtual const std::string& directory() const = 0;

protected:
    FileWatcher() = default;
};

} // namespace gg
