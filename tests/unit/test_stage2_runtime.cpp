#include <gtest/gtest.h>
#include "mr/market_core/capital_env.hpp"
#include "mr/market_core/runtime.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>

using namespace mr;

TEST(CapitalEnv, PasswordEnvNameMatchesDotEnvExample) {
    EXPECT_STREQ(kCapitalApiPasswordEnv, "CAPITAL_API_PASSWORD");
}

TEST(CapitalEnv, ResolvesCapitalApiPassword) {
    auto ok = resolve_capital_api_password("secret-from-api-password", "legacy");
    ASSERT_TRUE(ok.has_value());
    EXPECT_EQ(*ok, "secret-from-api-password");
}

TEST(CapitalEnv, IgnoresLegacyCapitalPassword) {
    EXPECT_FALSE(resolve_capital_api_password(nullptr, "legacy-only").has_value());
    EXPECT_FALSE(resolve_capital_api_password("", "legacy-only").has_value());
}

TEST(CapitalEnv, LoadFromEnvUsesApiPasswordNotLegacy) {
    ASSERT_EQ(::setenv(kCapitalApiKeyEnv, "key", 1), 0);
    ASSERT_EQ(::setenv(kCapitalApiPasswordEnv, "pw-api", 1), 0);
    ASSERT_EQ(::setenv("CAPITAL_PASSWORD", "pw-legacy", 1), 0);
    ASSERT_EQ(::setenv(kCapitalIdentifierEnv, "user@example.com", 1), 0);
    ASSERT_EQ(::setenv(kCapitalEpicEnv, "GOLD", 1), 0);
    ::unsetenv(kCapitalBaseUrlEnv);

    auto creds = load_capital_credentials_from_env();
    EXPECT_TRUE(creds.complete());
    EXPECT_EQ(creds.api_password, "pw-api");
    EXPECT_NE(creds.api_password, "pw-legacy");

    ::unsetenv(kCapitalApiKeyEnv);
    ::unsetenv(kCapitalApiPasswordEnv);
    ::unsetenv("CAPITAL_PASSWORD");
    ::unsetenv(kCapitalIdentifierEnv);
    ::unsetenv(kCapitalEpicEnv);
}

TEST(PaperRuntime, StaysAliveUntilStopped) {
    MarketCoreRuntime runtime;
    ConfigRegistry config;
    runtime.configure(config, RuntimeMode::Paper);
    EXPECT_TRUE(runtime.running());

    std::atomic<bool> flag{true};
    std::thread worker([&] { runtime.run_paper(flag); });

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_TRUE(runtime.running());

    flag = false;
    worker.join();
    EXPECT_FALSE(runtime.running());
}

TEST(PaperRuntime, RequestShutdownStopsIdleLoop) {
    MarketCoreRuntime runtime;
    ConfigRegistry config;
    runtime.configure(config, RuntimeMode::Paper);

    std::atomic<bool> flag{true};
    std::thread worker([&] { runtime.run_paper(flag); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    runtime.request_shutdown();
    worker.join();
    EXPECT_FALSE(runtime.running());
}
