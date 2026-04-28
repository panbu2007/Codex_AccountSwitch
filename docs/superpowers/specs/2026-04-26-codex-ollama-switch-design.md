# Codex CLI GPT/Ollama Switcher Design

## Summary

Build a Windows-only desktop tool on top of `Codex_AccountSwitch` that lets `Codex CLI` switch quickly between:

- official Codex/GPT traffic routed through the user's existing `Clash` proxy at `127.0.0.1:7890`
- local `Ollama` traffic routed to `127.0.0.1:11434`

The first release will guarantee stable **global switching** only:

- `GPT mode`: `codex -> local proxy -> 7890 -> OpenAI/Codex`
- `Ollama mode`: `codex -> local proxy -> 11434`

The design also includes **request inspection hooks** to verify whether `subagent` traffic can be identified reliably enough for a later split-routing release.

## Goals

- Reuse `Codex_AccountSwitch` where it already matches the problem:
  - Win32 app shell
  - WebView2 UI host
  - tray integration
  - local proxy bootstrap
  - Codex profile rewrite in stealth proxy mode
- Keep the user's `Codex CLI` setup simple:
  - Codex always talks to one local endpoint
  - route changes happen inside the tool, not by repeated manual config edits
- Preserve the existing `Clash 7890` workflow for GPT traffic
- Add enough observability to validate future `main session` vs `subagent` routing

## Non-Goals for MVP

- No guaranteed `main session` and `subagent` split routing in v1
- No cross-platform support
- No multi-provider matrix beyond:
  - `GPT via 7890`
  - `Ollama via 11434`
- No large refactor of unrelated account-management, quota, WebDAV, or cloud-sync features

## Current Codebase Findings

The chosen base repo already contains the critical integration points:

- `Codex_AccountSwitch/Codex_AccountSwitch/webview_host.cpp`
  - `StartLocalProxyService(...)`
  - `StopLocalProxyService(...)`
  - `BuildStealthCodexToml(...)`
  - `ApplyStealthProxyModeToCodexProfile(...)`
  - tray quick-switch logic
- `Codex_AccountSwitch/Codex_AccountSwitch/main_window.cpp`
  - main Win32 window lifecycle
- `webui/`
  - existing WebView-based UI that can expose new mode controls

This means the project does not need a fresh desktop shell or a new Codex profile injection mechanism. The largest required change is the proxy routing layer.

## Approaches Considered

### 1. Keep switching through environment variables

Pros:

- minimal code
- close to current manual workflow

Cons:

- brittle user experience
- poor visibility into active route
- weak foundation for future `subagent` discrimination

### 2. Rewrite Codex config on every switch

Pros:

- explicit routing at the Codex config layer

Cons:

- more invasive to the user's Codex profile
- unnecessary write churn
- weaker runtime observability than a real local proxy

### 3. Single local entry proxy with internal route switching

Pros:

- best match for the current codebase
- stable day-to-day use
- good tray UX
- clean path to future request inspection and split routing

Cons:

- proxy layer needs real changes
- requires careful handling of OpenAI-compatible and Ollama request differences

Recommended approach: **3**

## User Experience

### Primary interaction

The app remains a Windows desktop app with tray support. The tray exposes the switching flow directly.

MVP tray actions:

- show current mode: `GPT` or `Ollama`
- switch to `GPT`
- switch to `Ollama`
- open control panel
- view latest route/log status
- exit

### Default behavior

- when the user selects `GPT`, all new Codex traffic is routed to the GPT path
- when the user selects `Ollama`, all new Codex traffic is routed to the Ollama path
- Codex CLI should keep pointing at the same local proxy endpoint during normal use

### Future behavior

If request inspection proves `subagent` traffic can be identified reliably, the tray/UI can be extended later to support:

- `Main: GPT/Ollama`
- `Subagent: GPT/Ollama`
- default-follow-main behavior with manual override

## Architecture

### High-level flow

In stealth proxy mode, the tool writes Codex config so `Codex CLI` sends traffic to the tool's local proxy.

Route paths:

- `GPT mode`
  - `Codex CLI -> local proxy :11480 -> upstream proxy :7890 -> OpenAI/Codex`
- `Ollama mode`
  - `Codex CLI -> local proxy :11480 -> Ollama :11434`

### Modules

#### 1. Tray/UI layer

Responsibility:

- display current route mode
- toggle route mode
- surface proxy health and last-route information
- expose request inspection logs in a minimal debug view

Likely base:

- existing tray/menu code in `webview_host.cpp`
- existing `webui` settings/proxy pages

#### 2. Route state layer

Responsibility:

- persist the active global route mode
- expose it to tray/UI and proxy handler

Required state for MVP:

- `globalMode = "gpt" | "ollama"`
- optional persisted upstream settings:
  - `gptProxyHost = 127.0.0.1`
  - `gptProxyPort = 7890`
  - `ollamaHost = 127.0.0.1`
  - `ollamaPort = 11434`

#### 3. Local proxy layer

Responsibility:

- receive Codex traffic on one stable local port
- route requests by current mode
- emit structured route/debug logs

Important change:

The current proxy logic is centered on Codex account dispatch. MVP needs a provider-router mode that routes by active provider rather than by account rotation policy.

#### 4. Request inspector layer

Responsibility:

- record request metadata safely
- help compare normal session requests and subagent requests
- support future split-routing feasibility checks

Captured fields:

- timestamp
- target host
- method
- path
- selected headers, with secret masking
- selected body metadata, when parseable
- chosen route
- status code or transport failure

## Configuration

MVP config additions should be minimal and explicit.

Add fields to app config for:

- `routeMode`
- `gptUpstreamProxyHost`
- `gptUpstreamProxyPort`
- `ollamaBaseUrl`
- `requestInspectionEnabled`
- `requestInspectionRetentionLimit`

Keep existing stealth proxy settings if they still fit the startup flow.

## Request Routing Rules

### GPT mode

- forward request through the configured upstream HTTP proxy at `127.0.0.1:7890`
- preserve request semantics expected by Codex/OpenAI
- do not break the user's existing external networking setup

### Ollama mode

- forward request to local Ollama at `http://127.0.0.1:11434`
- adapt only the minimum necessary pieces if Codex request shape differs from Ollama expectations

This compatibility point is the main technical risk. MVP should prefer the simplest path that works with the user's target local model and document unsupported request shapes clearly.

## Error Handling

The tool must fail clearly, not silently.

Required cases:

- local proxy port bind failure
- `7890` unreachable in GPT mode
- `11434` unreachable in Ollama mode
- stealth proxy profile write failure
- malformed upstream response
- route switch requested while proxy is stopped

User-facing behavior:

- tray status reflects current health
- last error is visible in the UI
- logs show whether failure happened on:
  - local listener
  - GPT upstream path
  - Ollama upstream path

## Testing Strategy

### Manual validation for MVP

1. Enable stealth proxy mode and confirm Codex points to local proxy.
2. Start app in `GPT` mode.
3. Confirm a Codex CLI request reaches the GPT route through `7890`.
4. Switch to `Ollama` mode.
5. Confirm a new Codex CLI request reaches `11434`.
6. Stop Ollama and verify the app reports the upstream failure cleanly.
7. Stop Clash or block `7890` and verify the GPT path failure is reported cleanly.
8. Turn on request inspection and collect:
   - one normal session sample
   - one subagent-triggered sample

### Acceptance criteria

- switching between `GPT` and `Ollama` does not require repeated manual Codex reconfiguration during normal use
- route status is visible and trustworthy
- GPT traffic can still use the existing `7890` upstream path
- Ollama traffic can hit `11434`
- the app can capture enough request metadata to judge whether subagent split-routing is viable

## Risks

### 1. Request-shape mismatch for Ollama

Codex may send payloads that Ollama only partially supports. MVP should constrain scope to the specific local setup being tested and avoid pretending broad compatibility before it is proven.

### 2. Subagent detection may be impossible or unstable

If request metadata does not contain a durable marker, `main` vs `subagent` split routing must remain out of scope.

### 3. Existing proxy code is large and multi-purpose

`webview_host.cpp` currently mixes UI, config, proxy, account, and update logic. The implementation plan should contain targeted extraction only where needed for the new route mode to stay maintainable.

## Rollout Plan

### Phase 1

- global GPT/Ollama switching
- tray controls
- route status
- basic request inspection

### Phase 2

- compare normal and subagent request samples
- define detection rules only if they are stable and machine-checkable

### Phase 3

- optional `Main` vs `Subagent` split routing
- default-follow-main with manual subagent override

## Open Decision Resolved

The following items are now fixed for implementation planning:

- base project: `Codex_AccountSwitch`
- platform: Windows only
- client target: `Codex CLI`
- GPT upstream path: via `Clash` on `127.0.0.1:7890`
- Ollama upstream path: via local `127.0.0.1:11434`
- MVP scope: global mode switching first, subagent split only after captured evidence

## Implementation And Validation Update

Status as of 2026-04-28: Phase 1 MVP global switching has been implemented and validated on Windows.

Implemented pieces:

- persistent route settings: `routeMode`, `gptUpstreamProxyHost`, `gptUpstreamProxyPort`, `ollamaBaseUrl`, `requestInspectionEnabled`, and `requestInspectionRetentionLimit`
- runtime route state shared by tray/UI and proxy threads
- tray actions for `Route: GPT` and `Route: Ollama`
- WebView UI controls for route mode, GPT upstream proxy, Ollama base URL, and request inspection
- GPT route forwarding through the explicit named upstream proxy `127.0.0.1:7890`
- Ollama route forwarding to `http://127.0.0.1:11434`
- request inspection summaries and traffic-log fields for `routeMode`, `upstream`, and masked request metadata
- lightweight native tests for route settings, request inspection, and runtime route state

Validated behavior:

- app build succeeded for `Codex_AccountSwitch/Codex_AccountSwitch.vcxproj`
- test executable succeeded with route settings, request inspector, and proxy route state checks
- `webui/js/app.js` passed JavaScript syntax checking
- Ollama route returned local model data through `http://127.0.0.1:11480/v1/models`
- GPT route succeeded after account import and traffic logs showed `routeMode=gpt` with upstream `127.0.0.1:7890`
- a real `codex exec` run triggered `spawn_agent`/`wait` and produced a successful subagent result while requests passed through `11480`

### Codex CLI Connection Mode

The working Codex CLI integration is an OpenAI-compatible base URL/provider path:

```powershell
codex exec ... -c 'model_provider="custom"' -c 'model_providers.custom.base_url="http://127.0.0.1:11480/v1"'
```

The current local service on `11480` is an API endpoint proxy, not a generic HTTP CONNECT proxy. Therefore `HTTP_PROXY=http://127.0.0.1:11480` is not a supported integration mode for Codex CLI traffic that uses HTTPS CONNECT or WebSocket behavior.

For GPT mode, the app itself forwards outbound GPT traffic through the configured upstream proxy, currently `127.0.0.1:7890`. The user does not need to point Codex CLI directly at `7890` when using the custom-provider/base-URL path.

### Subagent Routing Finding

Subagents are confirmed to be created and routed through `11480`, but the captured request fields are not yet enough to safely distinguish main-session requests from subagent requests.

Observed request summaries for main and subagent traffic were very similar and commonly contained:

- `authorization=present`
- `content-type=application/json`
- `user-agent=codex_exec/0.125.0 ...`
- `model=gpt-5.5`
- `stream=true`
- `tools=present`
- `instructions=present`
- `subagent=present`

Because no stable discriminator has been identified, split routing such as `main=gpt, subagent=ollama` remains out of scope for the current MVP. The next required step is deeper sanitized body inspection, especially metadata/thread/agent IDs and tool-call context, without logging secrets or full prompt bodies.
