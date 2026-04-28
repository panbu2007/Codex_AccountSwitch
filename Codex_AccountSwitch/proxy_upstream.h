#pragma once

#include <string>

#include "proxy_route_state.h"

namespace cas
{
    struct UpstreamResponse
    {
        int statusCode = 0;
        std::wstring contentType;
        std::string body;
        std::wstring error;
    };

    bool ForwardRequestToOllama(const RouteStateSnapshot& routeState,
                                const std::wstring& method,
                                const std::wstring& path,
                                const std::wstring& contentType,
                                const std::string& body,
                                UpstreamResponse& response);

    std::wstring BuildNamedProxyString(const RouteStateSnapshot& routeState);
}
