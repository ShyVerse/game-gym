#include "script/file_watcher.h"

#ifdef __linux__

#include <filesystem>
#include <map>
#include <sys/inotify.h>
#include <unistd.h>
#include <vector>

namespace gg {

class FileWatcherLinux final : public FileWatcher {
public:
    explicit FileWatcherLinux(std::string directory) : directory_(std::move(directory)) {
        fd_ = inotify_init1(IN_NONBLOCK);
        if (fd_ < 0) {
            return;
        }
        add_watch_recursive(directory_);
    }

    ~FileWatcherLinux() override {
        if (fd_ >= 0) {
            for (auto& [wd, path] : wd_to_path_) {
                inotify_rm_watch(fd_, wd);
            }
            close(fd_);
        }
    }

    std::vector<std::string> poll_changes() override {
        std::vector<std::string> result;
        if (fd_ < 0) {
            return result;
        }

        constexpr size_t BUF_SIZE = 4096;
        alignas(struct inotify_event) char buf[BUF_SIZE];

        while (true) {
            ssize_t len = read(fd_, buf, BUF_SIZE);
            if (len <= 0) {
                break;
            }

            for (char* ptr = buf; ptr < buf + len;) {
                auto* event = reinterpret_cast<struct inotify_event*>(ptr);
                if (event->len > 0) {
                    auto it = wd_to_path_.find(event->wd);
                    if (it != wd_to_path_.end()) {
                        std::string full = it->second + "/" + event->name;
                        result.push_back(full);
                    }
                }
                ptr += sizeof(struct inotify_event) + event->len;
            }
        }
        return result;
    }

    const std::string& directory() const override { return directory_; }

private:
    void add_watch_recursive(const std::string& path) {
        namespace fs = std::filesystem;
        int wd = inotify_add_watch(fd_, path.c_str(), IN_MODIFY | IN_CREATE | IN_MOVED_TO);
        if (wd >= 0) {
            wd_to_path_[wd] = path;
        }
        if (fs::is_directory(path)) {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.is_directory()) {
                    add_watch_recursive(entry.path().string());
                }
            }
        }
    }

    std::string directory_;
    int fd_ = -1;
    std::map<int, std::string> wd_to_path_;
};

std::unique_ptr<FileWatcher> FileWatcher::create(const std::string& directory) {
    return std::make_unique<FileWatcherLinux>(directory);
}

} // namespace gg

#endif // __linux__
