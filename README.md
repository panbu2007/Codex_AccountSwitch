<h1 align="center"><b>Codex Account Switch</b></h1>

<p align="center">
  <b>Local-first, multi-account, visual manager for Codex accounts</b><br/>
  Built with <code>C++ / Win32 / WebView2</code> for stability and speed.
</p>

<p align="center">
  <a href="./README_CN.md"><b>简体中文 README</b></a>
</p>

## Core Features

- Unified workflow for backup / switch / delete / rename accounts
- Batch actions (batch refresh, batch delete)
- Import/export backup bundles (ZIP)
- Import current login, manual token paste, and quick OAuth file import
- Built-in OAuth login flow (callback listener + manual callback URL submit)
- Quota dashboard, auto-refresh (5H / 7D), low-quota alerts and switch prompts
- API proxy service (port, timeout, LAN access, API key, dispatch strategy)
- Traffic logs and token statistics pages
- Multi-language UI and themes (Auto / Light / Dark)

## Codex Client Local API Proxy for Seamless Number Switching
- API Reverse Proxy - Start Service
- API Reverse Proxy - Codex Client Uses Local Reverse Proxy Mode
- Enables seamless account switching without restarting
- Settings - Account - Automatic Account Switching Prompt for Low Credit Limit. Enabling this feature allows for automatic number switching to continue development work when credit limits are insufficient.

## GPT / Ollama Route Switching

This Windows build adds a local route switcher for Codex CLI traffic.

- `GPT` route: Codex CLI talks to the local API proxy, then the app forwards GPT traffic through the configured upstream proxy. The default upstream is Clash at `127.0.0.1:7890`.
- `Ollama` route: Codex CLI talks to the same local API proxy, then the app forwards traffic to local Ollama. The default Ollama endpoint is `http://127.0.0.1:11434`.
- Route mode can be changed from the tray menu or the WebView settings page.
- Traffic logs now include route metadata such as `routeMode`, selected upstream, and masked request inspection summaries.

### Codex CLI Usage

Use the local API proxy as an OpenAI-compatible base URL:

```powershell
codex exec ... -c 'model_provider="custom"' -c 'model_providers.custom.base_url="http://127.0.0.1:11480/v1"'
```

Port `11480` is an API endpoint proxy, not a generic HTTP CONNECT proxy. Do not use `HTTP_PROXY=http://127.0.0.1:11480` as the primary Codex CLI integration mode.

### Subagent Status

Codex subagent requests have been confirmed to pass through the local proxy. Current request inspection does not yet expose a stable discriminator for main-session versus subagent requests, so separate main/subagent routing is intentionally not enabled yet. The current supported behavior is global `GPT` or global `Ollama` switching.

## UI Preview

### 1. Dashboard (Light)
<p align="center">
  <img src="./image/en/1.png" alt="Dashboard" width="75%" />
</p>

### 1B. Dashboard (Dark)
<p align="center">
  <img src="./image/en/1_b.png" alt="Dashboard_b" width="75%" />
</p>

### 2. Accounts
<p align="center">
  <img src="./image/en/2.png" alt="Accounts" width="75%" />
</p>

### 3. API Proxy
<p align="center">
  <img src="./image/en/3.png" alt="API Proxy" width="75%" />
</p>

### 4. Traffic Logs
<p align="center">
  <img src="./image/en/4.png" alt="Traffic Logs" width="75%" />
</p>

### 5. Token Stats
<p align="center">
  <img src="./image/en/5.png" alt="Token Stats" width="75%" />
</p>

### 6. About
<p align="center">
  <img src="./image/en/6.png" alt="About" width="75%" />
</p>

### 7. Settings
<p align="center">
  <img src="./image/en/7.png" alt="Settings" width="75%" />
</p>

## Technical Architecture

- Native layer: `C++ / Win32 / WebView2`
- Frontend layer: `HTML + CSS + JavaScript`
- Bridge: WebView `postMessage` + host action routing
- Storage: local JSON files under user profile path

Main folders:

- `Codex_AccountSwitch/`: core C++ source
- `webui/`: frontend assets
- `installer/`: installer scripts
- `image/`: README screenshots

## Data Directory

Runtime data is stored in:

- `%LOCALAPPDATA%\Codex Account Switch\config.json`
- `%LOCALAPPDATA%\Codex Account Switch\backups\index.json`
- `%LOCALAPPDATA%\Codex Account Switch\backups\...`

## Installation Guide

### Requirements

- Windows 10/11 (x64/x86/ARM64 target build supported)
- WebView2 Runtime

### Build

1. Open solution: `Codex_AccountSwitch.slnx`
2. Select one of: `Release | x64`, `Release | x86`, `Release | ARM64`
3. Build outputs:
   - `Release/x64/Codex_AccountSwitch.exe`
   - `Release/x86/Codex_AccountSwitch.exe`
   - `Release/ARM/Codex_AccountSwitch.exe`

### Build Installer

- `installer/build_installer.bat` (recommended)
- `installer/build_installer.ps1`

Output folder: `dist/`

## Acknowledgements

- Thanks to the `Microsoft Edge WebView2` team for a stable, high-performance embedded web runtime.
- Thanks to all users and contributors for testing, bug reports, and feedback.
- Thanks to [router-for-me/CLIProxyAPI](https://github.com/router-for-me/CLIProxyAPI) for shared implementation ideas around Codex requests and OAuth retrieval.
- Thanks to [lbjlaq/Antigravity-Manager](https://github.com/lbjlaq/Antigravity-Manager) for shared UI and interaction design ideas.

## Contributors

- [isxlan0](https://github.com/isxlan0)

## License

Licensed under the `MIT License`. See `LICENSE`.

## Security Notice

All account data is stored locally by default. Data never leaves your device unless you explicitly export or share it.
