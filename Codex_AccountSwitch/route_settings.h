#pragma once

#include <string>

namespace cas
{
    enum class RouteMode
    {
        Gpt,
        Ollama,
        Xiaomi
    };

    struct RouteSettings
    {
        RouteMode routeMode = RouteMode::Gpt;
        std::wstring gptProxyHost = L"127.0.0.1";
        int gptProxyPort = 7890;
        std::wstring ollamaBaseUrl = L"http://127.0.0.1:11434";
        std::wstring xiaomiBaseUrl = L"https://token-plan-cn.xiaomimimo.com/v1";
        std::wstring xiaomiApiKey;
        std::wstring xiaomiModel = L"mimo-v2.5-pro";
        bool requestInspectionEnabled = true;
        int requestInspectionRetentionLimit = 400;
    };

    RouteMode ParseRouteMode(const std::wstring& value);
    std::wstring RouteModeToConfigValue(RouteMode mode);
    RouteSettings NormalizeRouteSettings(RouteSettings settings);
}
