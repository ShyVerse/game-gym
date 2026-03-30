#include "script/script_engine.h"

#include <fstream>
#include <libplatform/libplatform.h>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <v8.h>

namespace gg {

// ---------------------------------------------------------------------------
// V8 platform -- initialised exactly once across all ScriptEngine instances.
// ---------------------------------------------------------------------------

namespace {

std::once_flag g_v8_init_flag;
std::unique_ptr<v8::Platform> g_platform;

void ensure_v8_initialized() {
    std::call_once(g_v8_init_flag, [] {
        g_platform = v8::platform::NewDefaultPlatform();
        v8::V8::InitializePlatform(g_platform.get());
        v8::V8::Initialize();
    });
}

// Helper: convert a v8::TryCatch into an error string.
std::string
format_exception(v8::Isolate* isolate, v8::TryCatch& try_catch, v8::Local<v8::Context> context) {
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Message> message = try_catch.Message();
    std::ostringstream oss;

    if (!message.IsEmpty()) {
        v8::String::Utf8Value filename(isolate, message->GetScriptOrigin().ResourceName());
        const char* filename_str = *filename ? *filename : "<unknown>";
        int line = message->GetLineNumber(context).FromMaybe(-1);
        oss << filename_str << ":" << line << ": ";
    }

    v8::String::Utf8Value exception_str(isolate, try_catch.Exception());
    const char* exc = *exception_str ? *exception_str : "<exception>";
    oss << exc;
    return oss.str();
}

// Tag for external pointers stored via v8::External.
constexpr v8::ExternalPointerTypeTag kCallbackPtrTag = 1;

} // anonymous namespace

// ---------------------------------------------------------------------------
// ScriptEngine::Impl
// ---------------------------------------------------------------------------

struct ScriptEngine::Impl {
    v8::Isolate* isolate = nullptr;
    v8::Global<v8::Context> context;
    std::unique_ptr<v8::ArrayBuffer::Allocator> allocator;
    bool alive = false;

    // Registered native callbacks, keyed by function name.
    // Stored as unique_ptr so pointers remain stable across rehash.
    std::unordered_map<std::string, std::unique_ptr<NativeCallback>> callbacks;

    // V8 FunctionCallback trampoline: reads the NativeCallback from External
    // data, serialises arguments to JSON, calls the callback, and sets the
    // return value.
    static void native_trampoline(const v8::FunctionCallbackInfo<v8::Value>& info) {
        v8::Isolate* iso = info.GetIsolate();
        v8::HandleScope scope(iso);
        v8::Local<v8::Context> ctx = iso->GetCurrentContext();

        // Retrieve the NativeCallback pointer from External data.
        v8::Local<v8::External> external = v8::Local<v8::External>::Cast(info.Data());
        auto* cb = static_cast<NativeCallback*>(external->Value(kCallbackPtrTag));

        // Serialise arguments to a JSON array string.
        std::ostringstream args_oss;
        args_oss << "[";
        for (int i = 0; i < info.Length(); ++i) {
            if (i > 0)
                args_oss << ",";
            v8::Local<v8::Value> arg = info[i];
            v8::Local<v8::String> json_str;
            if (v8::JSON::Stringify(ctx, arg).ToLocal(&json_str)) {
                v8::String::Utf8Value utf8(iso, json_str);
                args_oss << (*utf8 ? *utf8 : "null");
            } else {
                args_oss << "null";
            }
        }
        args_oss << "]";

        // Call the native callback.
        try {
            std::string result_str = (*cb)(args_oss.str());
            // Parse the result string back into a JS value via JSON.parse,
            // or set it as a plain string if it's not valid JSON.
            v8::Local<v8::String> v8_result =
                v8::String::NewFromUtf8(iso, result_str.c_str()).ToLocalChecked();
            v8::Local<v8::Value> parsed;
            if (v8::JSON::Parse(ctx, v8_result).ToLocal(&parsed)) {
                info.GetReturnValue().Set(parsed);
            } else {
                info.GetReturnValue().Set(v8_result);
            }
        } catch (const std::exception& e) {
            iso->ThrowException(v8::String::NewFromUtf8(iso, e.what()).ToLocalChecked());
        } catch (...) {
            iso->ThrowException(
                v8::String::NewFromUtf8(iso, "unknown native error").ToLocalChecked());
        }
    }
};

// ---------------------------------------------------------------------------
// ScriptEngine construction / destruction
// ---------------------------------------------------------------------------

ScriptEngine::ScriptEngine() : impl_(std::make_unique<Impl>()) {}

ScriptEngine::~ScriptEngine() {
    shutdown();
}

std::unique_ptr<ScriptEngine> ScriptEngine::create() {
    ensure_v8_initialized();

    auto engine = std::unique_ptr<ScriptEngine>(new ScriptEngine());
    auto& impl = *engine->impl_;

    // Create the isolate.
    impl.allocator.reset(v8::ArrayBuffer::Allocator::NewDefaultAllocator());
    v8::Isolate::CreateParams params;
    params.array_buffer_allocator = impl.allocator.get();
    impl.isolate = v8::Isolate::New(params);

    // Create the context.
    {
        v8::Isolate::Scope isolate_scope(impl.isolate);
        v8::HandleScope handle_scope(impl.isolate);
        v8::Local<v8::Context> ctx = v8::Context::New(impl.isolate);
        impl.context.Reset(impl.isolate, ctx);
    }

    impl.alive = true;
    return engine;
}

// ---------------------------------------------------------------------------
// execute()
// ---------------------------------------------------------------------------

ScriptResult ScriptEngine::execute(const std::string& source, const std::string& filename) {
    if (!impl_->alive) {
        return {false, "", "engine is shut down"};
    }

    v8::Isolate::Scope isolate_scope(impl_->isolate);
    v8::HandleScope handle_scope(impl_->isolate);
    v8::Local<v8::Context> ctx = impl_->context.Get(impl_->isolate);
    v8::Context::Scope context_scope(ctx);

    v8::TryCatch try_catch(impl_->isolate);

    // Build source string and origin.
    v8::Local<v8::String> v8_source =
        v8::String::NewFromUtf8(impl_->isolate, source.c_str()).ToLocalChecked();
    v8::Local<v8::String> v8_filename =
        v8::String::NewFromUtf8(impl_->isolate, filename.c_str()).ToLocalChecked();
    v8::ScriptOrigin origin(v8_filename);

    // Compile.
    v8::Local<v8::Script> script;
    if (!v8::Script::Compile(ctx, v8_source, &origin).ToLocal(&script)) {
        return {false, "", format_exception(impl_->isolate, try_catch, ctx)};
    }

    // Run.
    v8::Local<v8::Value> result;
    if (!script->Run(ctx).ToLocal(&result)) {
        return {false, "", format_exception(impl_->isolate, try_catch, ctx)};
    }

    // Stringify the result.
    v8::String::Utf8Value utf8(impl_->isolate, result);
    std::string result_str = *utf8 ? *utf8 : "";
    return {true, result_str, ""};
}

// ---------------------------------------------------------------------------
// execute_module()
// ---------------------------------------------------------------------------

ScriptResult ScriptEngine::execute_module(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {false, "", "cannot open file: " + path};
    }

    std::ostringstream oss;
    oss << "(function() { " << file.rdbuf() << "\n})();";
    if (file.bad()) {
        return {false, "", "read error: " + path};
    }
    return execute(oss.str(), path);
}

// ---------------------------------------------------------------------------
// call_function()
// ---------------------------------------------------------------------------

ScriptResult ScriptEngine::call_function(const std::string& name, const std::string& args_json) {
    // Build JS expression that gracefully handles undefined functions.
    // If the function does not exist, return undefined instead of throwing.
    std::ostringstream oss;
    oss << "(typeof " << name << " === 'function' ? " << name << ".apply(null, " << args_json
        << ") : undefined)";
    return execute(oss.str(), "<call:" + name + ">");
}

// ---------------------------------------------------------------------------
// register_function()
// ---------------------------------------------------------------------------

void ScriptEngine::register_function(const std::string& name, NativeCallback callback) {
    if (!impl_->alive)
        return;

    // Store the callback on the heap so the pointer remains stable
    // even when the unordered_map rehashes.
    auto& cb = impl_->callbacks[name];
    cb = std::make_unique<NativeCallback>(std::move(callback));
    NativeCallback* cb_ptr = cb.get();

    v8::Isolate::Scope isolate_scope(impl_->isolate);
    v8::HandleScope handle_scope(impl_->isolate);
    v8::Local<v8::Context> ctx = impl_->context.Get(impl_->isolate);
    v8::Context::Scope context_scope(ctx);
    v8::Local<v8::External> external_data =
        v8::External::New(impl_->isolate, cb_ptr, kCallbackPtrTag);

    // Create a FunctionTemplate and register in the global scope.
    v8::Local<v8::FunctionTemplate> ft =
        v8::FunctionTemplate::New(impl_->isolate, Impl::native_trampoline, external_data);
    v8::Local<v8::Function> fn = ft->GetFunction(ctx).ToLocalChecked();

    v8::Local<v8::String> v8_name =
        v8::String::NewFromUtf8(impl_->isolate, name.c_str()).ToLocalChecked();
    ctx->Global()->Set(ctx, v8_name, fn).Check();
}

// ---------------------------------------------------------------------------
// idle_gc()
// ---------------------------------------------------------------------------

void ScriptEngine::idle_gc(double deadline_seconds) {
    if (!impl_->alive || deadline_seconds <= 0.0) {
        return;
    }

    v8::Isolate::Scope isolate_scope(impl_->isolate);
    v8::platform::RunIdleTasks(g_platform.get(), impl_->isolate, deadline_seconds);
}

// ---------------------------------------------------------------------------
// shutdown()
// ---------------------------------------------------------------------------

void ScriptEngine::shutdown() {
    if (!impl_->alive)
        return;
    impl_->alive = false;

    // Clear stored callbacks before disposing the isolate.
    impl_->callbacks.clear();

    {
        v8::Isolate::Scope isolate_scope(impl_->isolate);
        impl_->context.Reset();
    }

    v8::platform::NotifyIsolateShutdown(g_platform.get(), impl_->isolate);
    impl_->isolate->Dispose();
    impl_->isolate = nullptr;
}

// ---------------------------------------------------------------------------
// is_alive()
// ---------------------------------------------------------------------------

bool ScriptEngine::is_alive() const {
    return impl_->alive;
}

} // namespace gg
