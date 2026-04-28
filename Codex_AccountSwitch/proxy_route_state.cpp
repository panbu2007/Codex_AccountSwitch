#include "proxy_route_state.h"

#include <mutex>

namespace
{
    std::mutex gRouteStateMutex;
    cas::RouteStateSnapshot gRouteState;
}

namespace cas
{
    void InitializeRouteState(const RouteStateSnapshot& snapshot)
    {
        std::lock_guard<std::mutex> lock(gRouteStateMutex);
        gRouteState = snapshot;
    }

    void SetActiveRouteMode(RouteMode mode)
    {
        std::lock_guard<std::mutex> lock(gRouteStateMutex);
        gRouteState.routeMode = mode;
    }

    RouteStateSnapshot GetRouteStateSnapshot()
    {
        std::lock_guard<std::mutex> lock(gRouteStateMutex);
        return gRouteState;
    }
}
