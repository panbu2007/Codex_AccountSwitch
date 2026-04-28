#pragma once

#include <string>

#include "route_settings.h"

namespace cas
{
    struct RouteStateSnapshot
    {
        RouteMode routeMode = RouteMode::Gpt;
        std::wstring gptProxyHost = L"127.0.0.1";
        int gptProxyPort = 7890;
        std::wstring ollamaBaseUrl = L"http://127.0.0.1:11434";
        bool requestInspectionEnabled = true;
        int requestInspectionRetentionLimit = 400;
    };

    void InitializeRouteState(const RouteStateSnapshot& snapshot);
    void SetActiveRouteMode(RouteMode mode);
    RouteStateSnapshot GetRouteStateSnapshot();
}
