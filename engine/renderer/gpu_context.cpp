#include "renderer/gpu_context.h"

#include "core/window.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstring>
#include <glfw3webgpu.h>
#include <stdexcept>

namespace gg {

// ---------------------------------------------------------------------------
// Callback helpers
// ---------------------------------------------------------------------------

struct AdapterResult {
    WGPUAdapter adapter = nullptr;
    bool done = false;
};

static void on_adapter(WGPURequestAdapterStatus status,
                       WGPUAdapter adapter,
                       WGPUStringView message,
                       void* userdata1,
                       void* /*userdata2*/) {
    auto* result = static_cast<AdapterResult*>(userdata1);
    if (status == WGPURequestAdapterStatus_Success) {
        result->adapter = adapter;
    } else {
        const char* msg = (message.data && message.length > 0) ? message.data : "(unknown)";
        std::fprintf(stderr, "[GpuContext] Adapter request failed: %s\n", msg);
    }
    result->done = true;
}

struct DeviceResult {
    WGPUDevice device = nullptr;
    bool done = false;
};

static void on_device(WGPURequestDeviceStatus status,
                      WGPUDevice device,
                      WGPUStringView message,
                      void* userdata1,
                      void* /*userdata2*/) {
    auto* result = static_cast<DeviceResult*>(userdata1);
    if (status == WGPURequestDeviceStatus_Success) {
        result->device = device;
    } else {
        const char* msg = (message.data && message.length > 0) ? message.data : "(unknown)";
        std::fprintf(stderr, "[GpuContext] Device request failed: %s\n", msg);
    }
    result->done = true;
}

static void on_uncaptured_error(WGPUDevice const* /*device*/,
                                WGPUErrorType type,
                                WGPUStringView message,
                                void* /*userdata1*/,
                                void* /*userdata2*/) {
    const char* msg = (message.data && message.length > 0) ? message.data : "(no message)";
    std::fprintf(
        stderr, "[GpuContext] Uncaptured error (type=%d): %s\n", static_cast<int>(type), msg);
}

static void on_device_lost(WGPUDevice const* /*device*/,
                           WGPUDeviceLostReason reason,
                           WGPUStringView message,
                           void* /*userdata1*/,
                           void* /*userdata2*/) {
    const char* msg = (message.data && message.length > 0) ? message.data : "(no message)";
    std::fprintf(
        stderr, "[GpuContext] Device lost (reason=%d): %s\n", static_cast<int>(reason), msg);
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<GpuContext> GpuContext::create(const Window& window) {
    auto ctx = std::unique_ptr<GpuContext>(new GpuContext());

    // 1. Instance
    WGPUInstanceDescriptor inst_desc{};
    inst_desc.nextInChain = nullptr;
    ctx->instance_ = wgpuCreateInstance(&inst_desc);
    if (!ctx->instance_) {
        throw std::runtime_error("GpuContext: failed to create WGPUInstance");
    }

    // 2. Surface (via glfw3webgpu helper)
    ctx->surface_ = glfwCreateWGPUSurface(ctx->instance_, window.native_handle());
    if (!ctx->surface_) {
        throw std::runtime_error("GpuContext: failed to create WGPUSurface");
    }

    // 3. Adapter
    WGPURequestAdapterOptions adapter_opts{};
    adapter_opts.nextInChain = nullptr;
    adapter_opts.featureLevel = WGPUFeatureLevel_Core;
    adapter_opts.powerPreference = WGPUPowerPreference_HighPerformance;
    adapter_opts.forceFallbackAdapter = false;
    adapter_opts.backendType = WGPUBackendType_Undefined;
    adapter_opts.compatibleSurface = ctx->surface_;

    AdapterResult adapter_result{};
    WGPURequestAdapterCallbackInfo adapter_cb{};
    adapter_cb.nextInChain = nullptr;
    adapter_cb.mode = WGPUCallbackMode_AllowSpontaneous;
    adapter_cb.callback = on_adapter;
    adapter_cb.userdata1 = &adapter_result;
    adapter_cb.userdata2 = nullptr;

    wgpuInstanceRequestAdapter(ctx->instance_, &adapter_opts, adapter_cb);

    // With AllowSpontaneous the callback fires synchronously on wgpu-native
    if (!adapter_result.adapter) {
        throw std::runtime_error("GpuContext: failed to obtain WGPUAdapter");
    }
    ctx->adapter_ = adapter_result.adapter;

    // 4. Device
    WGPUDeviceDescriptor dev_desc{};
    dev_desc.nextInChain = nullptr;
    dev_desc.label = {.data = "gg-device", .length = WGPU_STRLEN};
    dev_desc.requiredFeatureCount = 0;
    dev_desc.requiredFeatures = nullptr;
    dev_desc.requiredLimits = nullptr;
    dev_desc.defaultQueue.label = {.data = "gg-queue", .length = WGPU_STRLEN};

    dev_desc.deviceLostCallbackInfo.nextInChain = nullptr;
    dev_desc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    dev_desc.deviceLostCallbackInfo.callback = on_device_lost;
    dev_desc.deviceLostCallbackInfo.userdata1 = nullptr;
    dev_desc.deviceLostCallbackInfo.userdata2 = nullptr;

    dev_desc.uncapturedErrorCallbackInfo.nextInChain = nullptr;
    dev_desc.uncapturedErrorCallbackInfo.callback = on_uncaptured_error;
    dev_desc.uncapturedErrorCallbackInfo.userdata1 = nullptr;
    dev_desc.uncapturedErrorCallbackInfo.userdata2 = nullptr;

    DeviceResult device_result{};
    WGPURequestDeviceCallbackInfo device_cb{};
    device_cb.nextInChain = nullptr;
    device_cb.mode = WGPUCallbackMode_AllowSpontaneous;
    device_cb.callback = on_device;
    device_cb.userdata1 = &device_result;
    device_cb.userdata2 = nullptr;

    wgpuAdapterRequestDevice(ctx->adapter_, &dev_desc, device_cb);

    if (!device_result.device) {
        throw std::runtime_error("GpuContext: failed to obtain WGPUDevice");
    }
    ctx->device_ = device_result.device;

    // 5. Queue
    ctx->queue_ = wgpuDeviceGetQueue(ctx->device_);
    if (!ctx->queue_) {
        throw std::runtime_error("GpuContext: failed to obtain WGPUQueue");
    }

    // 6. Surface format & configure
    ctx->surface_width_ = window.width();
    ctx->surface_height_ = window.height();

    WGPUSurfaceCapabilities caps{};
    caps.nextInChain = nullptr;
    wgpuSurfaceGetCapabilities(ctx->surface_, ctx->adapter_, &caps);
    if (caps.formatCount > 0) {
        ctx->surface_format_ = caps.formats[0];
    } else {
        ctx->surface_format_ = WGPUTextureFormat_BGRA8Unorm;
    }
    wgpuSurfaceCapabilitiesFreeMembers(caps);

    ctx->configure_surface();

    return ctx;
}

// ---------------------------------------------------------------------------
// Surface configuration
// ---------------------------------------------------------------------------

void GpuContext::configure_surface() {
    WGPUSurfaceConfiguration config{};
    config.nextInChain = nullptr;
    config.device = device_;
    config.format = surface_format_;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.width = surface_width_;
    config.height = surface_height_;
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;
    config.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(surface_, &config);
}

// ---------------------------------------------------------------------------
// Resize
// ---------------------------------------------------------------------------

void GpuContext::resize(uint32_t w, uint32_t h) {
    surface_width_ = w;
    surface_height_ = h;
    configure_surface();
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

GpuContext::~GpuContext() {
    if (surface_ && device_) {
        wgpuSurfaceUnconfigure(surface_);
    }
    if (queue_) {
        wgpuQueueRelease(queue_);
        queue_ = nullptr;
    }
    if (device_) {
        wgpuDeviceRelease(device_);
        device_ = nullptr;
    }
    if (adapter_) {
        wgpuAdapterRelease(adapter_);
        adapter_ = nullptr;
    }
    if (surface_) {
        wgpuSurfaceRelease(surface_);
        surface_ = nullptr;
    }
    if (instance_) {
        wgpuInstanceRelease(instance_);
        instance_ = nullptr;
    }
}

} // namespace gg
