#include "route_settings.h"

#include <algorithm>
#include <cwctype>

namespace
{
    std::wstring ToLowerCopy(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(), towlower);
        return value;
    }
}

namespace cas
{
    RouteMode ParseRouteMode(const std::wstring& value)
    {
        const std::wstring lowered = ToLowerCopy(value);
        if (lowered == L"ollama")
        {
            return RouteMode::Ollama;
        }

        return RouteMode::Gpt;
    }

    std::wstring RouteModeToConfigValue(RouteMode mode)
    {
        return mode == RouteMode::Ollama ? L"ollama" : L"gpt";
    }

    RouteSettings NormalizeRouteSettings(RouteSettings settings)
    {
        if (settings.gptProxyHost.empty())
        {
            settings.gptProxyHost = L"127.0.0.1";
        }

        if (settings.gptProxyPort < 1 || settings.gptProxyPort > 65535)
        {
            settings.gptProxyPort = 7890;
        }

        if (settings.ollamaBaseUrl.empty())
        {
            settings.ollamaBaseUrl = L"http://127.0.0.1:11434";
        }

        if (settings.requestInspectionRetentionLimit < 50)
        {
            settings.requestInspectionRetentionLimit = 50;
        }

        if (settings.requestInspectionRetentionLimit > 2000)
        {
            settings.requestInspectionRetentionLimit = 2000;
        }

        return settings;
    }
}
