#include "script/file_watcher.h"

#ifdef __APPLE__

#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#include <mutex>
#include <set>

namespace gg {

class FileWatcherMacOS final : public FileWatcher {
public:
    explicit FileWatcherMacOS(std::string directory)
        : directory_(std::move(directory)) {
        start();
    }

    ~FileWatcherMacOS() override { stop(); }

    std::vector<std::string> poll_changes() override {
        // Events arrive asynchronously on the dispatch queue, so we just
        // drain the accumulated set under the lock.
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> result(changed_.begin(), changed_.end());
        changed_.clear();
        return result;
    }

    const std::string& directory() const override { return directory_; }

private:
    void start() {
        CFStringRef path_ref = CFStringCreateWithCString(
            kCFAllocatorDefault, directory_.c_str(), kCFStringEncodingUTF8);
        CFArrayRef paths = CFArrayCreate(
            kCFAllocatorDefault,
            reinterpret_cast<const void**>(&path_ref), 1,
            &kCFTypeArrayCallBacks);

        FSEventStreamContext ctx{};
        ctx.info = this;

        stream_ = FSEventStreamCreate(
            kCFAllocatorDefault, &FileWatcherMacOS::callback, &ctx, paths,
            kFSEventStreamEventIdSinceNow, 0.1,
            kFSEventStreamCreateFlagFileEvents |
                kFSEventStreamCreateFlagNoDefer);

        if (!stream_) {
            CFRelease(paths);
            CFRelease(path_ref);
            return;
        }

        // Use a serial dispatch queue instead of the deprecated RunLoop API.
        queue_ = dispatch_queue_create("gg.file_watcher", DISPATCH_QUEUE_SERIAL);
        FSEventStreamSetDispatchQueue(stream_, queue_);
        FSEventStreamStart(stream_);

        CFRelease(paths);
        CFRelease(path_ref);
    }

    void stop() {
        if (stream_) {
            FSEventStreamStop(stream_);
            FSEventStreamInvalidate(stream_);
            FSEventStreamRelease(stream_);
            stream_ = nullptr;
        }
        if (queue_) {
            // Drain any in-flight callbacks before releasing the queue.
            dispatch_sync(queue_, ^{});
            dispatch_release(queue_);
            queue_ = nullptr;
        }
    }

    static void callback(ConstFSEventStreamRef /*stream_ref*/,
                          void* client_info, size_t num_events,
                          void* event_paths,
                          const FSEventStreamEventFlags* /*flags*/,
                          const FSEventStreamEventId* /*ids*/) {
        auto* self = static_cast<FileWatcherMacOS*>(client_info);
        auto** paths = static_cast<char**>(event_paths);

        std::lock_guard<std::mutex> lock(self->mutex_);
        for (size_t i = 0; i < num_events; ++i) {
            self->changed_.insert(paths[i]);
        }
    }

    std::string directory_;
    FSEventStreamRef stream_ = nullptr;
    dispatch_queue_t queue_ = nullptr;
    std::mutex mutex_;
    std::set<std::string> changed_;
};

std::unique_ptr<FileWatcher> FileWatcher::create(
        const std::string& directory) {
    return std::make_unique<FileWatcherMacOS>(directory);
}

} // namespace gg

#endif // __APPLE__
