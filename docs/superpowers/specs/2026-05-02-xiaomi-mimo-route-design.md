# Xiaomi MiMo Route Design

## Goal

Add Xiaomi MiMo as a third local proxy route alongside GPT and Ollama. When selected, Codex traffic continues to enter the local CAS proxy, and CAS forwards the OpenAI-compatible request to Xiaomi MiMo using a user-provided API key.

## Requirements

- Add a `xiaomi` route mode beside existing `gpt` and `ollama`.
- Store Xiaomi settings in app config:
  - `xiaomiBaseUrl`, defaulting to `https://api.mimo-v2.com/v1`.
  - `xiaomiApiKey`, supplied by the user.
- Expose Xiaomi route and settings in the WebUI proxy configuration.
- Forward Xiaomi route requests to the configured base URL with OpenAI-compatible paths preserved.
- Authenticate Xiaomi requests with the configured API key. MiMo docs show `api-key`; CAS should also send `Authorization: Bearer <key>` for compatibility.
- Keep GPT account scheduling, quota failover, and Codex account switching unchanged.
- Keep Ollama local forwarding unchanged.
- Record Xiaomi traffic as route `xiaomi`, upstream Xiaomi base URL, account `xiaomi`.

## Architecture

The change extends the existing route abstraction instead of adding a separate proxy service.

- `route_settings.*` owns parse, serialization, defaults, and validation for `RouteMode`.
- `proxy_route_state.*` stores the active route snapshot used by request threads.
- `proxy_upstream.*` already contains generic base URL parsing and direct upstream forwarding for Ollama. This will be generalized for OpenAI-compatible upstreams that need optional API key headers.
- `webview_host.cpp` loads, saves, normalizes, emits, and consumes route config.
- `webui/index.html` and `webui/js/app.js` render and persist the new Xiaomi fields.

## Data Flow

1. The user selects Xiaomi in Route Mode and fills Base URL/API Key.
2. WebUI posts config through `set_config`.
3. Native config normalizes and saves Xiaomi fields.
4. Runtime route snapshot is updated.
5. Proxy request handling checks the active route:
   - GPT uses the existing Codex account path.
   - Ollama uses existing direct local forwarding.
   - Xiaomi forwards directly to the configured MiMo base URL with API key headers.
6. Response status, content type, body, and token usage parsing follow the existing upstream response path.

## Error Handling

- Empty Xiaomi base URL falls back to `https://api.mimo-v2.com/v1`.
- Invalid Xiaomi base URL returns a `502` JSON error with code-like message `xiaomi_base_url_invalid`.
- Empty Xiaomi API key returns a `401` JSON error with message `xiaomi_api_key_empty`.
- Transport failures return a `502` JSON error with the upstream failure message.
- Xiaomi failures do not trigger GPT account quota failover, because they are not tied to local Codex accounts.

## UI

The Proxy tab adds:

- A `Xiaomi` option in Route Mode.
- `Xiaomi Base URL` input.
- `Xiaomi API Key` input.

Fields may remain visible with the other route fields to keep the implementation small and consistent with the current layout.

## Testing

- Extend native tests for:
  - `ParseRouteMode("xiaomi")`.
  - `RouteModeToConfigValue(RouteMode::Xiaomi)`.
  - Xiaomi default normalization.
  - Route snapshot preserves Xiaomi base URL/API key.
- Run the existing native test target.
- Build Debug. Avoid Release replacement unless following the proxy continuity rule in `AGENTS.md`.

## Scope Boundaries

- No automatic Xiaomi model discovery.
- No Xiaomi-specific model picker presets beyond existing custom model support.
- No Anthropic-compatible MiMo route in this change.
- No Release executable replacement unless explicitly requested.
