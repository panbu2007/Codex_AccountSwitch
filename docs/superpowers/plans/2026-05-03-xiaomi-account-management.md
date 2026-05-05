# Xiaomi Account Management Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Manage the Xiaomi MiMo API key from Add Account and Account Management instead of the API Proxy page.

**Architecture:** Reuse existing Xiaomi config fields. Render a synthetic external account row in the WebUI when `xiaomiApiKey` exists, and route its actions to config updates instead of Codex account actions.

**Tech Stack:** HTML, JavaScript, existing WebView2 config messaging.

---

### Task 1: Move Xiaomi Inputs

**Files:**
- Modify: `webui/index.html`
- Modify: `webui/js/app.js`

- [ ] Add a Xiaomi tab to the Add Account modal.
- [ ] Move `xiaomiBaseUrlInput` and `xiaomiApiKeyInput` into the Xiaomi pane.
- [ ] Add save button and i18n strings.

### Task 2: Render Xiaomi Account Row

**Files:**
- Modify: `webui/js/app.js`

- [ ] Add helper that appends a synthetic Xiaomi record when `state.xiaomiApiKey` is present.
- [ ] Render Xiaomi row with external provider label and masked key status.
- [ ] Disable quota refresh for Xiaomi row.
- [ ] Use row actions for switch, overwrite key, and delete.

### Task 3: Wire Actions

**Files:**
- Modify: `webui/js/app.js`

- [ ] Save Xiaomi tab by requiring a non-empty new API key.
- [ ] Overwrite action opens Xiaomi tab with empty key field.
- [ ] Delete action clears `state.xiaomiApiKey` and saves config.
- [ ] Switch action sets `state.routeMode = "xiaomi"` and saves config.

### Task 4: Verify

**Files:**
- No new files.

- [ ] Run `node --check webui/js/app.js`.
- [ ] Build Debug.
