#include "script/script_engine.h"

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// Lifecycle tests
// ---------------------------------------------------------------------------

TEST(ScriptEngineTest, CreatesSuccessfully) {
    auto engine = gg::ScriptEngine::create();
    ASSERT_NE(engine, nullptr);
    EXPECT_TRUE(engine->is_alive());
}

TEST(ScriptEngineTest, ShutdownIsIdempotent) {
    auto engine = gg::ScriptEngine::create();
    engine->shutdown();
    EXPECT_FALSE(engine->is_alive());
    engine->shutdown(); // second call must not crash
    EXPECT_FALSE(engine->is_alive());
}

// ---------------------------------------------------------------------------
// execute() tests
// ---------------------------------------------------------------------------

TEST(ScriptEngineTest, ExecuteReturnsResult) {
    auto engine = gg::ScriptEngine::create();
    auto result = engine->execute("1 + 2");
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.value, "3");
    EXPECT_TRUE(result.error.empty());
}

TEST(ScriptEngineTest, ExecuteReportsCompileError) {
    auto engine = gg::ScriptEngine::create();
    auto result = engine->execute("function {{{ bad syntax");
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.error.empty());
}

TEST(ScriptEngineTest, ExecuteReportsRuntimeError) {
    auto engine = gg::ScriptEngine::create();
    auto result = engine->execute("undeclaredVariable.property");
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.error.empty());
}

TEST(ScriptEngineTest, ExecuteAfterShutdownReturnsError) {
    auto engine = gg::ScriptEngine::create();
    engine->shutdown();
    auto result = engine->execute("1 + 1");
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.error.empty());
}

// ---------------------------------------------------------------------------
// call_function() tests
// ---------------------------------------------------------------------------

TEST(ScriptEngineTest, CallDefinedFunction) {
    auto engine = gg::ScriptEngine::create();
    engine->execute("function add(a, b) { return a + b; }");
    auto result = engine->call_function("add", "[3, 4]");
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.value, "7");
}

TEST(ScriptEngineTest, CallUndefinedFunctionReturnsUndefined) {
    auto engine = gg::ScriptEngine::create();
    auto result = engine->call_function("nonExistentFunction");
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.value, "undefined");
}

TEST(ScriptEngineTest, CallFunctionWithNoArgs) {
    auto engine = gg::ScriptEngine::create();
    engine->execute("function greet() { return 'hello'; }");
    auto result = engine->call_function("greet");
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.value, "hello");
}

// ---------------------------------------------------------------------------
// register_function() tests
// ---------------------------------------------------------------------------

TEST(ScriptEngineTest, RegisterAndCallNativeFunction) {
    auto engine = gg::ScriptEngine::create();
    engine->register_function("nativeAdd", [](const std::string& args_json) -> std::string {
        // Simple test: parse "[a, b]" and return sum.
        // For testing we just parse two integers from the JSON array.
        int a = 0, b = 0;
        std::sscanf(args_json.c_str(), "[%d,%d]", &a, &b);
        return std::to_string(a + b);
    });
    auto result = engine->execute("nativeAdd(10, 20)");
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.value, "30");
}

TEST(ScriptEngineTest, NativeFunctionErrorDoesNotCrashEngine) {
    auto engine = gg::ScriptEngine::create();
    engine->register_function("throwingNative", [](const std::string&) -> std::string {
        throw std::runtime_error("native error");
    });
    auto result = engine->execute("throwingNative()");
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.error.empty());
    // Engine should still be usable after native error.
    auto result2 = engine->execute("1 + 1");
    EXPECT_TRUE(result2.ok);
    EXPECT_EQ(result2.value, "2");
}

// ---------------------------------------------------------------------------
// idle_gc() test
// ---------------------------------------------------------------------------

TEST(ScriptEngineTest, IdleGcDoesNotCrash) {
    auto engine = gg::ScriptEngine::create();
    engine->idle_gc(1.0);
    // Engine should still be usable.
    auto result = engine->execute("42");
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.value, "42");
}
