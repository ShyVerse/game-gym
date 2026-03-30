#include "script/file_watcher.h"

#ifdef __APPLE__

namespace gg {

class FileWatcherMacOS final : public FileWatcher {
public:
    explicit FileWatcherMacOS(std::string dir)
        : dir_(std::move(dir)) {}

    std::vector<std::string> poll_changes() override { return {}; }
    const std::string& directory() const override { return dir_; }

private:
    std::string dir_;
};

std::unique_ptr<FileWatcher> FileWatcher::create(
        const std::string& directory) {
    return std::make_unique<FileWatcherMacOS>(directory);
}

} // namespace gg

#endif
