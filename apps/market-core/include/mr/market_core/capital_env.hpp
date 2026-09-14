#pragma once
#include <optional>
#include <string>

namespace mr {

/** Canonical Capital env names — must match `.env.example`. */
inline constexpr const char* kCapitalApiKeyEnv = "CAPITAL_API_KEY";
inline constexpr const char* kCapitalApiPasswordEnv = "CAPITAL_API_PASSWORD";
inline constexpr const char* kCapitalIdentifierEnv = "CAPITAL_IDENTIFIER";
inline constexpr const char* kCapitalEpicEnv = "CAPITAL_EPIC";
inline constexpr const char* kCapitalBaseUrlEnv = "CAPITAL_BASE_URL";

struct CapitalEnvCredentials {
    std::string api_key;
    std::string api_password;
    std::string identifier;
    std::string epic;
    std::string base_url;

    [[nodiscard]] bool complete() const {
        return !api_key.empty() && !api_password.empty() && !identifier.empty() && !epic.empty();
    }
};

/**
 * Resolve password from explicit values (for unit tests).
 * Accepts only CAPITAL_API_PASSWORD; ignores legacy CAPITAL_PASSWORD.
 */
[[nodiscard]] std::optional<std::string> resolve_capital_api_password(
    const char* api_password_value, const char* legacy_password_value);

/** Load Capital credentials from process environment. */
[[nodiscard]] CapitalEnvCredentials load_capital_credentials_from_env();

}  // namespace mr
