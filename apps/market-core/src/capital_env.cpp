#include "mr/market_core/capital_env.hpp"
#include <cstdlib>

namespace mr {

std::optional<std::string> resolve_capital_api_password(
    const char* api_password_value, const char* /*legacy_password_value*/) {
    // Only CAPITAL_API_PASSWORD (matches .env.example). Do not accept CAPITAL_PASSWORD.
    if (api_password_value != nullptr && api_password_value[0] != '\0') {
        return std::string(api_password_value);
    }
    return std::nullopt;
}

CapitalEnvCredentials load_capital_credentials_from_env() {
    CapitalEnvCredentials out;
    if (const char* v = std::getenv(kCapitalApiKeyEnv)) out.api_key = v;
    if (const char* v = std::getenv(kCapitalIdentifierEnv)) out.identifier = v;
    if (const char* v = std::getenv(kCapitalEpicEnv)) out.epic = v;
    if (const char* v = std::getenv(kCapitalBaseUrlEnv)) out.base_url = v;

    auto pw = resolve_capital_api_password(
        std::getenv(kCapitalApiPasswordEnv), std::getenv("CAPITAL_PASSWORD"));
    if (pw) out.api_password = *pw;
    return out;
}

}  // namespace mr
