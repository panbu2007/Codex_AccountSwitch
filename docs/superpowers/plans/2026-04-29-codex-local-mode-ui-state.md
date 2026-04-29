# Codex Local Mode UI State Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the proxy page clearly distinguish local proxy service state from Codex local proxy mode and keep the UI switch synced to actual Codex config.

**Architecture:** Add a native detector for CAS-managed local proxy config in `~/.codex/config.toml`, report it in config payloads, and post an async status update after apply/restore. Update WebUI labels, default port text, and message handling.

**Tech Stack:** C++20, Win32/WebView2, HTML/CSS/JavaScript.

---

## Tasks

- [x] Add `IsCodexProfileUsingLocalProxyMode()` in `webview_host.cpp`.
- [x] Send actual `codexLocalProxyMode` and use it for `proxyStealthMode` UI compatibility.
- [x] Use actual mode state as the restore baseline when saving config.
- [x] Post `codex_local_proxy_mode` after async apply/restore completes.
- [x] Rename UI text to `Codex uses local proxy mode`.
- [x] Change displayed default proxy port from `8045` to `11480`.
- [ ] Run JS syntax check and native build/tests.
