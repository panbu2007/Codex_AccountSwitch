# Xiaomi MiMo Route Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Xiaomi MiMo as a third local proxy route using an OpenAI-compatible base URL and user-provided API key.

**Architecture:** Extend the existing route state/config path used by GPT and Ollama. Generalize direct upstream forwarding so Ollama remains unauthenticated while Xiaomi adds API key headers.

**Tech Stack:** C++17 Win32/WebView2, WinHTTP, HTML/JS WebUI, native console tests.

---

### Task 1: Route Model And Tests

**Files:**
- Modify: `Codex_AccountSwitch/route_settings.h`
- Modify: `Codex_AccountSwitch/route_settings.cpp`
- Modify: `Codex_AccountSwitch/proxy_route_state.h`
- Modify: `Codex_AccountSwitch_Tests/main.cpp`

- [ ] Add `RouteMode::Xiaomi`.
- [ ] Parse `xiaomi` case-insensitively.
- [ ] Serialize Xiaomi as `xiaomi`.
- [ ] Add `xiaomiBaseUrl` and `xiaomiApiKey` to route settings and snapshots.
- [ ] Normalize empty Xiaomi base URL to `https://api.mimo-v2.com/v1`.
- [ ] Extend tests for parsing, serialization, normalization, and route state preservation.

### Task 2: Direct Xiaomi Upstream

**Files:**
- Modify: `Codex_AccountSwitch/proxy_upstream.h`
- Modify: `Codex_AccountSwitch/proxy_upstream.cpp`
- Modify: `Codex_AccountSwitch/webview_host.cpp`

- [ ] Generalize the direct WinHTTP request helper to accept extra headers.
- [ ] Keep `ForwardRequestToOllama` behavior unchanged.
- [ ] Add `ForwardRequestToXiaomi` that validates base URL and API key.
- [ ] In proxy forwarding, branch Xiaomi before GPT dispatch.
- [ ] Return JSON errors for missing key, invalid URL, and transport failure.
- [ ] Set traffic metadata account to `xiaomi`.

### Task 3: Config And UI

**Files:**
- Modify: `Codex_AccountSwitch/webview_host.cpp`
- Modify: `webui/index.html`
- Modify: `webui/js/app.js`

- [ ] Add Xiaomi fields to `AppConfig`, load/save JSON, config emission, and `set_config`.
- [ ] Add route option and inputs in the Proxy tab.
- [ ] Add English and Chinese i18n strings.
- [ ] Include Xiaomi settings in pending-config matching.
- [ ] Preserve existing GPT and Ollama defaults.

### Task 4: Verification

**Files:**
- No new files.

- [ ] Run native tests with the existing test project.
- [ ] Build Debug only.
- [ ] Do not rebuild or replace Release unless following the AGENTS.md proxy continuity handoff.
