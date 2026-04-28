# Codex History Shadow Import Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a reversible history shadow-import feature so old `openai` Codex sessions can be copied into a CAS-managed `custom` provider view without modifying original history files.

**Architecture:** Add a focused native module for scanning JSONL session metadata, creating deterministic shadow copies, writing a manifest, and undoing only CAS-owned shadow files. Wire the module into the existing native test harness, WebView host action dispatcher, and proxy/settings UI.

**Tech Stack:** C++20, Win32, `<filesystem>`, JSONL text processing, WebView2 host actions, HTML/CSS/JavaScript, MSBuild.

---

## File Structure

### New files

- Create: `Codex_AccountSwitch/history_shadow_import.h`
- Create: `Codex_AccountSwitch/history_shadow_import.cpp`

### Modified files

- Modify: `Codex_AccountSwitch/Codex_AccountSwitch.vcxproj`
- Modify: `Codex_AccountSwitch/webview_host.cpp`
- Modify: `Codex_AccountSwitch_Tests/Codex_AccountSwitch_Tests.vcxproj`
- Modify: `Codex_AccountSwitch_Tests/main.cpp`
- Modify: `webui/index.html`
- Modify: `webui/js/app.js`
- Modify: `webui/css/styles.css`

## Task 1: Add History Shadow Import Module And Tests

**Files:**
- Create: `Codex_AccountSwitch/history_shadow_import.h`
- Create: `Codex_AccountSwitch/history_shadow_import.cpp`
- Modify: `Codex_AccountSwitch_Tests/main.cpp`
- Modify: `Codex_AccountSwitch_Tests/Codex_AccountSwitch_Tests.vcxproj`

- [ ] **Step 1: Add tests for importability, rewrite, dedupe, and undo safety**

Add tests to `Codex_AccountSwitch_Tests/main.cpp` that create a temp Codex home with:

```text
sessions/2026/04/26/source-openai.jsonl
sessions/2026/04/26/source-custom.jsonl
archived_sessions/source-archived.jsonl
```

The openai file first line should contain:

```json
{"timestamp":"2026-04-26T00:00:00Z","type":"session_meta","payload":{"id":"019dca37-add6-72c2-8341-32ef8c77e1f5","thread_name":"Old session","model_provider":"openai"}}
```

Assert:

- import scans all three JSONL files
- only `openai` files are imported
- shadow files are under `sessions/cas_shadow_import`
- shadow first line contains `model_provider":"custom"`
- shadow first line contains a deterministic UUID-shaped shadow id derived from `019dca37-add6-72c2-8341-32ef8c77e1f5`
- second import skips already imported items
- undo deletes shadow files and does not delete source files

- [ ] **Step 2: Add module interface**

Create `Codex_AccountSwitch/history_shadow_import.h` with:

```cpp
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace cas
{
    struct HistoryShadowItem
    {
        std::filesystem::path sourcePath;
        std::wstring sourceId;
        std::filesystem::path shadowPath;
        std::wstring shadowId;
        std::wstring sourceProvider;
        std::wstring targetProvider;
    };

    struct HistoryShadowResult
    {
        bool ok = true;
        int scanned = 0;
        int imported = 0;
        int skipped = 0;
        int failed = 0;
        std::wstring message;
        std::vector<HistoryShadowItem> items;
    };

    HistoryShadowResult ImportHistoryShadowCopies(const std::filesystem::path &codexHome,
                                                  const std::wstring &sourceProvider,
                                                  const std::wstring &targetProvider);

    HistoryShadowResult UndoHistoryShadowCopies(const std::filesystem::path &codexHome);

    std::filesystem::path GetHistoryShadowManifestPath(const std::filesystem::path &codexHome);
    std::filesystem::path GetHistoryShadowRoot(const std::filesystem::path &codexHome);
}
```

- [ ] **Step 3: Implement the module**

Implement `history_shadow_import.cpp` with these behaviors:

- recursively scan `codexHome / "sessions"` for `.jsonl`
- skip anything under `sessions/cas_shadow_import`
- scan `codexHome / "archived_sessions"` for top-level `.jsonl`
- parse first line with conservative string extraction helpers for `type`, `payload.id`, `payload.model_provider`, and `payload.thread_name`
- create deterministic shadow id as `cas-<source_id>`
- write shadow file to `sessions/cas_shadow_import/YYYY/MM/DD/<source-stem>.cas-shadow.jsonl`
- rewrite only first line in the copied shadow file
- write manifest JSON to `bak/cas_history_shadow_import/manifest.json`
- undo deletes only manifest-listed files whose canonical path is under `sessions/cas_shadow_import`

- [ ] **Step 4: Run native tests**

Run:

```powershell
msbuild .\Codex_AccountSwitch_Tests\Codex_AccountSwitch_Tests.vcxproj /p:Configuration=Debug /p:Platform=x64
.\Codex_AccountSwitch_Tests\Debug\x64\Codex_AccountSwitch_Tests.exe
```

Expected: PASS and existing route/proxy tests still pass.

## Task 2: Wire Module Into Projects

**Files:**
- Modify: `Codex_AccountSwitch/Codex_AccountSwitch.vcxproj`
- Modify: `Codex_AccountSwitch_Tests/Codex_AccountSwitch_Tests.vcxproj`

- [ ] **Step 1: Register source and header files**

Add to both project files:

```xml
<ClCompile Include="history_shadow_import.cpp" />
<ClInclude Include="history_shadow_import.h" />
```

For the test project, use the existing relative path convention:

```xml
<ClCompile Include="..\Codex_AccountSwitch\history_shadow_import.cpp" />
<ClInclude Include="..\Codex_AccountSwitch\history_shadow_import.h" />
```

- [ ] **Step 2: Build app and tests**

Run:

```powershell
msbuild .\Codex_AccountSwitch.slnx /t:Build /p:Configuration=Debug /p:Platform=x64
```

Expected: PASS.

## Task 3: Add WebView Host Actions

**Files:**
- Modify: `Codex_AccountSwitch/webview_host.cpp`

- [ ] **Step 1: Include the module**

Add:

```cpp
#include "history_shadow_import.h"
```

- [ ] **Step 2: Add result sender helper**

Add a helper near existing WebView response helpers that sends:

```json
{
  "type": "history_shadow_result",
  "ok": true,
  "scanned": 10,
  "imported": 2,
  "skipped": 8,
  "failed": 0,
  "message": "history_shadow_import_complete"
}
```

- [ ] **Step 3: Add actions**

In `WebViewHost::HandleWebAction`, add:

- `history_shadow_import`: calls `cas::ImportHistoryShadowCopies(GetCodexHomeDir(), L"openai", L"custom")`
- `history_shadow_undo`: calls `cas::UndoHistoryShadowCopies(GetCodexHomeDir())`

Each action should send `history_shadow_result` and a user-facing status message.

- [ ] **Step 4: Build**

Run the app build. Expected: PASS.

## Task 4: Add UI Buttons And Status

**Files:**
- Modify: `webui/index.html`
- Modify: `webui/js/app.js`
- Modify: `webui/css/styles.css`

- [ ] **Step 1: Add proxy/settings UI section**

Add a section below route/proxy settings:

```html
<div class="history-shadow-card">
  <div class="settings-label" id="historyShadowTitle">History Visibility</div>
  <div class="settings-sub-note" id="historyShadowHint">Create reversible shadow copies so old OpenAI sessions can appear under the custom provider. Original history files are not modified.</div>
  <div class="history-shadow-actions">
    <button class="tool-btn" id="historyShadowImportBtn">Import OpenAI History</button>
    <button class="tool-btn danger" id="historyShadowUndoBtn">Undo CAS History Import</button>
  </div>
  <div class="settings-sub-note" id="historyShadowStatus"></div>
</div>
```

- [ ] **Step 2: Add i18n keys and DOM bindings**

Add English and Chinese fallback strings in `webui/js/app.js`:

- `history_shadow.title`
- `history_shadow.hint`
- `history_shadow.import`
- `history_shadow.undo`
- `history_shadow.complete`
- `history_shadow.failed`

Add DOM refs and set text in the existing `applyTranslations()` flow.

- [ ] **Step 3: Bind buttons and handle result messages**

Bind:

```js
post("history_shadow_import")
post("history_shadow_undo")
```

Handle `type === "history_shadow_result"` by updating `historyShadowStatus` with scanned/imported/skipped/failed counts.

- [ ] **Step 4: Add minimal CSS**

Add a compact card style consistent with existing settings panels.

- [ ] **Step 5: Syntax check**

Run:

```powershell
node --check .\webui\js\app.js
```

Expected: PASS.

## Self-Review

### Spec coverage

- No original mutation: covered by Task 1 module behavior and tests.
- Manifest dedupe: covered by Task 1 tests.
- One-click undo: covered by Task 3 host action and Task 4 UI.
- UI safety messaging: covered by Task 4.
- SQLite non-goal: preserved by not touching SQLite files.

### Placeholder scan

No `TODO`, `TBD`, or unspecified implementation steps remain.

### Type consistency

The plan uses `HistoryShadowResult` and `HistoryShadowItem` consistently across module, tests, and host action serialization.

## Execution Handoff

Plan saved to `docs/superpowers/plans/2026-04-28-history-shadow-import.md`.

Execution mode for this session: inline execution, because the user already approved continuing and no subagent delegation was requested.
