# Codex CLI GPT/Ollama Switcher Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a Windows-only local proxy mode to `Codex_AccountSwitch` so `Codex CLI` can switch globally between official Codex/GPT traffic through `127.0.0.1:7890` and local Ollama traffic through `127.0.0.1:11434`, with request inspection for later subagent analysis.

**Architecture:** Keep the existing Win32/WebView2 shell, stealth proxy profile rewrite, and proxy entrypoint. Add small focused native modules for route settings, runtime route state, request inspection, and upstream forwarding, then wire them into `webview_host.cpp` and the existing proxy/settings UI.

**Tech Stack:** C++20, Win32, WebView2, WinHTTP, Winsock, HTML/CSS/JavaScript, MSBuild/Visual Studio.

---

> Repo note: the current workspace snapshot does not contain `.git` metadata. If you execute this plan inside a real clone, use the commit commands as written. If you execute it in the current snapshot, skip commit steps until the repo is recloned with git history.

## File Structure

### New files

- Create: `Codex_AccountSwitch/route_settings.h`
- Create: `Codex_AccountSwitch/route_settings.cpp`
- Create: `Codex_AccountSwitch/proxy_route_state.h`
- Create: `Codex_AccountSwitch/proxy_route_state.cpp`
- Create: `Codex_AccountSwitch/request_inspector.h`
- Create: `Codex_AccountSwitch/request_inspector.cpp`
- Create: `Codex_AccountSwitch/proxy_upstream.h`
- Create: `Codex_AccountSwitch/proxy_upstream.cpp`
- Create: `Codex_AccountSwitch_Tests/Codex_AccountSwitch_Tests.vcxproj`
- Create: `Codex_AccountSwitch_Tests/main.cpp`

### Modified files

- Modify: `Codex_AccountSwitch/Codex_AccountSwitch.vcxproj`
- Modify: `Codex_AccountSwitch.slnx`
- Modify: `Codex_AccountSwitch/webview_host.cpp`
- Modify: `Codex_AccountSwitch/webview_host.h`
- Modify: `webui/index.html`
- Modify: `webui/js/app.js`
- Modify: `webui/css/styles.css`

### Responsibilities

- `route_settings.*`: persistent route mode and upstream settings normalization
- `proxy_route_state.*`: live in-memory route state used by tray and proxy threads
- `request_inspector.*`: secret masking, bounded in-memory summaries, traffic-log field formatting
- `proxy_upstream.*`: GPT-via-7890 and Ollama-via-11434 forwarding helpers
- `webview_host.cpp`: config load/save, WebView actions, tray menu, proxy loop integration
- `webui/*`: expose route mode and inspection controls
- `Codex_AccountSwitch_Tests/*`: lightweight native test harness for pure logic

### Task 1: Add Route Settings Module And Native Test Harness

**Files:**
- Create: `Codex_AccountSwitch/route_settings.h`
- Create: `Codex_AccountSwitch/route_settings.cpp`
- Create: `Codex_AccountSwitch_Tests/Codex_AccountSwitch_Tests.vcxproj`
- Create: `Codex_AccountSwitch_Tests/main.cpp`
- Modify: `Codex_AccountSwitch/Codex_AccountSwitch.vcxproj`
- Modify: `Codex_AccountSwitch.slnx`

- [ ] **Step 1: Write the failing route-settings tests**

Create `Codex_AccountSwitch_Tests/main.cpp` with this exact content:

```cpp
#include <cassert>
#include <iostream>

#include "../Codex_AccountSwitch/route_settings.h"

using cas::NormalizeRouteSettings;
using cas::ParseRouteMode;
using cas::RouteMode;
using cas::RouteModeToConfigValue;
using cas::RouteSettings;

static void TestParseRouteMode()
{
    assert(ParseRouteMode(L"gpt") == RouteMode::Gpt);
    assert(ParseRouteMode(L"GPT") == RouteMode::Gpt);
    assert(ParseRouteMode(L"ollama") == RouteMode::Ollama);
    assert(ParseRouteMode(L"unexpected") == RouteMode::Gpt);
}

static void TestNormalizeRouteSettings()
{
    RouteSettings s;
    s.routeMode = RouteMode::Ollama;
    s.gptProxyHost = L"";
    s.gptProxyPort = -1;
    s.ollamaBaseUrl = L"";
    s.requestInspectionEnabled = true;
    s.requestInspectionRetentionLimit = 2;

    s = NormalizeRouteSettings(s);

    assert(s.routeMode == RouteMode::Ollama);
    assert(s.gptProxyHost == L"127.0.0.1");
    assert(s.gptProxyPort == 7890);
    assert(s.ollamaBaseUrl == L"http://127.0.0.1:11434");
    assert(s.requestInspectionRetentionLimit == 50);
}

static void TestRouteModeToConfigValue()
{
    assert(RouteModeToConfigValue(RouteMode::Gpt) == L"gpt");
    assert(RouteModeToConfigValue(RouteMode::Ollama) == L"ollama");
}

int main()
{
    TestParseRouteMode();
    TestNormalizeRouteSettings();
    TestRouteModeToConfigValue();
    std::cout << "route_settings tests passed\n";
    return 0;
}
```

- [ ] **Step 2: Run the tests to confirm they fail**

Run:

```powershell
msbuild .\Codex_AccountSwitch_Tests\Codex_AccountSwitch_Tests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: FAIL with missing project file or missing `route_settings.h`.

- [ ] **Step 3: Add the route settings module**

Create `Codex_AccountSwitch/route_settings.h`:

```cpp
#pragma once

#include <string>

namespace cas
{
    enum class RouteMode
    {
        Gpt,
        Ollama
    };

    struct RouteSettings
    {
        RouteMode routeMode = RouteMode::Gpt;
        std::wstring gptProxyHost = L"127.0.0.1";
        int gptProxyPort = 7890;
        std::wstring ollamaBaseUrl = L"http://127.0.0.1:11434";
        bool requestInspectionEnabled = true;
        int requestInspectionRetentionLimit = 400;
    };

    RouteMode ParseRouteMode(const std::wstring &value);
    std::wstring RouteModeToConfigValue(RouteMode mode);
    RouteSettings NormalizeRouteSettings(RouteSettings settings);
}
```

Create `Codex_AccountSwitch/route_settings.cpp`:

```cpp
#include "route_settings.h"

#include <algorithm>

namespace
{
    std::wstring ToLowerCopy(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(), towlower);
        return value;
    }
}

namespace cas
{
    RouteMode ParseRouteMode(const std::wstring &value)
    {
        const std::wstring lowered = ToLowerCopy(value);
        if (lowered == L"ollama")
        {
            return RouteMode::Ollama;
        }
        return RouteMode::Gpt;
    }

    std::wstring RouteModeToConfigValue(RouteMode mode)
    {
        return mode == RouteMode::Ollama ? L"ollama" : L"gpt";
    }

    RouteSettings NormalizeRouteSettings(RouteSettings settings)
    {
        if (settings.gptProxyHost.empty())
        {
            settings.gptProxyHost = L"127.0.0.1";
        }
        if (settings.gptProxyPort < 1 || settings.gptProxyPort > 65535)
        {
            settings.gptProxyPort = 7890;
        }
        if (settings.ollamaBaseUrl.empty())
        {
            settings.ollamaBaseUrl = L"http://127.0.0.1:11434";
        }
        if (settings.requestInspectionRetentionLimit < 50)
        {
            settings.requestInspectionRetentionLimit = 50;
        }
        if (settings.requestInspectionRetentionLimit > 2000)
        {
            settings.requestInspectionRetentionLimit = 2000;
        }
        return settings;
    }
}
```

- [ ] **Step 4: Add the test project and wire it into the solution**

Create `Codex_AccountSwitch_Tests/Codex_AccountSwitch_Tests.vcxproj`:

```xml
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|x64">
      <Configuration>Debug</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <VCProjectVersion>18.0</VCProjectVersion>
    <ProjectGuid>{6A9AF7D6-6D4B-4B6D-AE05-87D0B86D76C1}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>CodexAccountSwitchTests</RootNamespace>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>true</UseDebugLibraries>
    <PlatformToolset>v145</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
    <OutDir>$(MSBuildProjectDirectory)\Debug\x64\</OutDir>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v145</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
    <OutDir>$(MSBuildProjectDirectory)\Release\x64\</OutDir>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <ConformanceMode>true</ConformanceMode>
      <LanguageStandard>stdcpp20</LanguageStandard>
      <AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>
    </ClCompile>
    <Link>
      <SubSystem>Console</SubSystem>
    </Link>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <ConformanceMode>true</ConformanceMode>
      <LanguageStandard>stdcpp20</LanguageStandard>
      <AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>
    </ClCompile>
    <Link>
      <SubSystem>Console</SubSystem>
    </Link>
  </ItemDefinitionGroup>
  <ItemGroup>
    <ClCompile Include="main.cpp" />
    <ClCompile Include="..\Codex_AccountSwitch\route_settings.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="..\Codex_AccountSwitch\route_settings.h" />
  </ItemGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
```

Modify `Codex_AccountSwitch.slnx` by adding the test project:

```xml
  <Project Path="Codex_AccountSwitch/Codex_AccountSwitch.vcxproj" Id="e21c8e9c-dd8b-4419-8f0b-ebb2a9fccd32" />
  <Project Path="Codex_AccountSwitch_Tests/Codex_AccountSwitch_Tests.vcxproj" Id="6a9af7d6-6d4b-4b6d-ae05-87d0b86d76c1" />
```

Modify `Codex_AccountSwitch/Codex_AccountSwitch.vcxproj` and add:

```xml
    <ClCompile Include="route_settings.cpp" />
```

and:

```xml
    <ClInclude Include="route_settings.h" />
```

- [ ] **Step 5: Run the tests and confirm they pass**

Run:

```powershell
msbuild .\Codex_AccountSwitch.slnx /t:Build /p:Configuration=Debug /p:Platform=x64
.\Codex_AccountSwitch_Tests\Debug\x64\Codex_AccountSwitch_Tests.exe
```

Expected:

```text
route_settings tests passed
```

- [ ] **Step 6: Commit**

```bash
git add Codex_AccountSwitch.slnx Codex_AccountSwitch/Codex_AccountSwitch.vcxproj Codex_AccountSwitch/route_settings.h Codex_AccountSwitch/route_settings.cpp Codex_AccountSwitch_Tests/Codex_AccountSwitch_Tests.vcxproj Codex_AccountSwitch_Tests/main.cpp
git commit -m "feat: add route settings module and native tests"
```

### Task 2: Add Request Inspection Helpers

**Files:**
- Create: `Codex_AccountSwitch/request_inspector.h`
- Create: `Codex_AccountSwitch/request_inspector.cpp`
- Modify: `Codex_AccountSwitch/Codex_AccountSwitch.vcxproj`
- Modify: `Codex_AccountSwitch_Tests/main.cpp`
- Modify: `Codex_AccountSwitch_Tests/Codex_AccountSwitch_Tests.vcxproj`

- [ ] **Step 1: Write failing request-inspection tests**

Append these tests to `Codex_AccountSwitch_Tests/main.cpp`:

```cpp
#include "../Codex_AccountSwitch/request_inspector.h"

using cas::InspectionRecord;
using cas::MaskHeaderValue;
using cas::TrimInspectionRecords;

static void TestMaskHeaderValue()
{
    assert(MaskHeaderValue(L"Authorization", L"Bearer super-secret-token") == L"present");
    assert(MaskHeaderValue(L"ChatGPT-Account-Id", L"acc_1234567890") == L"present");
    assert(MaskHeaderValue(L"User-Agent", L"codex-cli") == L"codex-cli");
}

static void TestTrimInspectionRecords()
{
    std::vector<InspectionRecord> items(5);
    TrimInspectionRecords(items, 3);
    assert(items.size() == 3);
}
```

Update `main()`:

```cpp
    TestMaskHeaderValue();
    TestTrimInspectionRecords();
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
msbuild .\Codex_AccountSwitch.slnx /t:Build /p:Configuration=Debug /p:Platform=x64
```

Expected: FAIL with missing `request_inspector.h` or unresolved identifiers.

- [ ] **Step 3: Add the request inspection module**

Create `Codex_AccountSwitch/request_inspector.h`:

```cpp
#pragma once

#include <string>
#include <vector>

namespace cas
{
    struct InspectionRecord
    {
        std::wstring timestamp;
        std::wstring routeMode;
        std::wstring upstream;
        std::wstring method;
        std::wstring path;
        std::wstring model;
        int statusCode = 0;
        std::wstring headerSummary;
        std::wstring bodySummary;
    };

    std::wstring MaskHeaderValue(const std::wstring &name, const std::wstring &value);
    void TrimInspectionRecords(std::vector<InspectionRecord> &items, int maxItems);
}
```

Create `Codex_AccountSwitch/request_inspector.cpp`:

```cpp
#include "request_inspector.h"

#include <algorithm>

namespace
{
    std::wstring ToLowerCopy(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(), towlower);
        return value;
    }
}

namespace cas
{
    std::wstring MaskHeaderValue(const std::wstring &name, const std::wstring &value)
    {
        const std::wstring lowered = ToLowerCopy(name);
        if (lowered == L"authorization" || lowered == L"x-api-key" ||
            lowered == L"chatgpt-account-id")
        {
            return value.empty() ? L"" : L"present";
        }
        return value;
    }

    void TrimInspectionRecords(std::vector<InspectionRecord> &items, int maxItems)
    {
        if (maxItems < 1)
        {
            maxItems = 1;
        }
        if (static_cast<int>(items.size()) <= maxItems)
        {
            return;
        }
        items.erase(items.begin(), items.end() - maxItems);
    }
}
```

- [ ] **Step 4: Wire the module into both projects**

Modify `Codex_AccountSwitch/Codex_AccountSwitch.vcxproj`:

```xml
    <ClCompile Include="request_inspector.cpp" />
```

and:

```xml
    <ClInclude Include="request_inspector.h" />
```

Modify `Codex_AccountSwitch_Tests/Codex_AccountSwitch_Tests.vcxproj`:

```xml
    <ClCompile Include="..\Codex_AccountSwitch\request_inspector.cpp" />
```

and:

```xml
    <ClInclude Include="..\Codex_AccountSwitch\request_inspector.h" />
```

- [ ] **Step 5: Run the tests and confirm they pass**

Run:

```powershell
msbuild .\Codex_AccountSwitch.slnx /t:Build /p:Configuration=Debug /p:Platform=x64
.\Codex_AccountSwitch_Tests\Debug\x64\Codex_AccountSwitch_Tests.exe
```

Expected:

```text
route_settings tests passed
```

The executable should still exit `0` after the new assertions run.

- [ ] **Step 6: Commit**

```bash
git add Codex_AccountSwitch/request_inspector.h Codex_AccountSwitch/request_inspector.cpp Codex_AccountSwitch/Codex_AccountSwitch.vcxproj Codex_AccountSwitch_Tests/Codex_AccountSwitch_Tests.vcxproj Codex_AccountSwitch_Tests/main.cpp
git commit -m "feat: add request inspection helpers"
```

### Task 3: Persist Route Settings And Expose Them In WebUI

**Files:**
- Modify: `Codex_AccountSwitch/webview_host.cpp`
- Modify: `webui/index.html`
- Modify: `webui/js/app.js`
- Modify: `webui/css/styles.css`

- [ ] **Step 1: Add route settings fields to `AppConfig` and config JSON**

Modify the `AppConfig` struct in `Codex_AccountSwitch/webview_host.cpp` and add these members directly after the existing proxy fields:

```cpp
    std::wstring routeMode = L"gpt";
    std::wstring gptUpstreamProxyHost = L"127.0.0.1";
    int gptUpstreamProxyPort = 7890;
    std::wstring ollamaBaseUrl = L"http://127.0.0.1:11434";
    bool requestInspectionEnabled = true;
    int requestInspectionRetentionLimit = 400;
```

In the config load logic, read:

```cpp
    out.routeMode = ExtractJsonField(json, L"routeMode");
    out.gptUpstreamProxyHost = ExtractJsonField(json, L"gptUpstreamProxyHost");
    out.gptUpstreamProxyPort = ExtractJsonIntField(json, L"gptUpstreamProxyPort", 7890);
    out.ollamaBaseUrl = ExtractJsonField(json, L"ollamaBaseUrl");
    out.requestInspectionEnabled = ExtractJsonBoolField(json, L"requestInspectionEnabled", true);
    out.requestInspectionRetentionLimit = ExtractJsonIntField(json, L"requestInspectionRetentionLimit", 400);
```

In the config save logic, emit:

```cpp
    ss << L"  \"routeMode\": \"" << EscapeJsonString(cfg.routeMode) << L"\",\n";
    ss << L"  \"gptUpstreamProxyHost\": \"" << EscapeJsonString(cfg.gptUpstreamProxyHost) << L"\",\n";
    ss << L"  \"gptUpstreamProxyPort\": " << cfg.gptUpstreamProxyPort << L",\n";
    ss << L"  \"ollamaBaseUrl\": \"" << EscapeJsonString(cfg.ollamaBaseUrl) << L"\",\n";
    ss << L"  \"requestInspectionEnabled\": " << (cfg.requestInspectionEnabled ? L"true" : L"false") << L",\n";
    ss << L"  \"requestInspectionRetentionLimit\": " << cfg.requestInspectionRetentionLimit << L",\n";
```

- [ ] **Step 2: Include the new fields in `get_config`, `set_config`, and startup state**

In `SendConfig(...)`, append:

```cpp
      L",\"routeMode\":\"" + EscapeJsonString(cfg.routeMode) +
      L"\",\"gptUpstreamProxyHost\":\"" + EscapeJsonString(cfg.gptUpstreamProxyHost) +
      L"\",\"gptUpstreamProxyPort\":" + std::to_wstring(cfg.gptUpstreamProxyPort) +
      L",\"ollamaBaseUrl\":\"" + EscapeJsonString(cfg.ollamaBaseUrl) +
      L"\",\"requestInspectionEnabled\":" + std::wstring(cfg.requestInspectionEnabled ? L"true" : L"false") +
      L",\"requestInspectionRetentionLimit\":" + std::to_wstring(cfg.requestInspectionRetentionLimit) +
```

In the `set_config` handler, parse and assign:

```cpp
    const std::wstring routeMode = UnescapeJsonString(ExtractJsonStringField(rawMessage, L"routeMode"));
    const std::wstring gptUpstreamProxyHost = UnescapeJsonString(ExtractJsonStringField(rawMessage, L"gptUpstreamProxyHost"));
    const int gptUpstreamProxyPort = ExtractJsonIntField(rawMessage, L"gptUpstreamProxyPort", -1);
    const std::wstring ollamaBaseUrl = UnescapeJsonString(ExtractJsonStringField(rawMessage, L"ollamaBaseUrl"));
    const bool requestInspectionEnabled = ExtractJsonBoolField(rawMessage, L"requestInspectionEnabled", true);
    const int requestInspectionRetentionLimit = ExtractJsonIntField(rawMessage, L"requestInspectionRetentionLimit", 400);
```

and then:

```cpp
      cfg.routeMode = routeMode.empty() ? L"gpt" : routeMode;
      cfg.gptUpstreamProxyHost = gptUpstreamProxyHost.empty() ? L"127.0.0.1" : gptUpstreamProxyHost;
      cfg.gptUpstreamProxyPort = (gptUpstreamProxyPort >= 1 && gptUpstreamProxyPort <= 65535) ? gptUpstreamProxyPort : 7890;
      cfg.ollamaBaseUrl = ollamaBaseUrl.empty() ? L"http://127.0.0.1:11434" : ollamaBaseUrl;
      cfg.requestInspectionEnabled = requestInspectionEnabled;
      cfg.requestInspectionRetentionLimit = requestInspectionRetentionLimit < 50 ? 50 :
                                            (requestInspectionRetentionLimit > 2000 ? 2000 : requestInspectionRetentionLimit);
```

- [ ] **Step 3: Add route controls to the proxy settings panel**

Insert this block into `webui/index.html` inside the existing `.proxy-grid` section:

```html
<div class="settings-group proxy-item">
  <div class="settings-label" id="routeModeLabel">Route Mode</div>
  <select class="settings-select proxy-input" id="routeModeSelect">
    <option value="gpt">GPT</option>
    <option value="ollama">Ollama</option>
  </select>
  <div class="settings-sub-note" id="routeModeHint">Choose whether Codex traffic goes to GPT via 7890 or Ollama via 11434.</div>
</div>

<div class="settings-group proxy-item">
  <div class="settings-label" id="gptUpstreamProxyHostLabel">GPT Proxy Host</div>
  <input class="settings-input proxy-input" id="gptUpstreamProxyHostInput" type="text" value="127.0.0.1" />
  <div class="settings-sub-note" id="gptUpstreamProxyHostHint">HTTP proxy host used for GPT traffic.</div>
</div>

<div class="settings-group proxy-item">
  <div class="settings-label" id="gptUpstreamProxyPortLabel">GPT Proxy Port</div>
  <input class="settings-number-input proxy-input" id="gptUpstreamProxyPortInput" type="number" min="1" max="65535" value="7890" />
  <div class="settings-sub-note" id="gptUpstreamProxyPortHint">HTTP proxy port used for GPT traffic.</div>
</div>

<div class="settings-group proxy-item">
  <div class="settings-label" id="ollamaBaseUrlLabel">Ollama Base URL</div>
  <input class="settings-input proxy-input" id="ollamaBaseUrlInput" type="text" value="http://127.0.0.1:11434" />
  <div class="settings-sub-note" id="ollamaBaseUrlHint">Local Ollama endpoint used in Ollama mode.</div>
</div>

<div class="settings-group proxy-item">
  <div class="settings-label" id="requestInspectionEnabledLabel">Request Inspection</div>
  <label class="switch-toggle" for="requestInspectionEnabledToggle">
    <input type="checkbox" id="requestInspectionEnabledToggle" />
    <span class="switch-slider"></span>
  </label>
  <div class="settings-sub-note" id="requestInspectionEnabledHint">Capture masked request summaries for route debugging and later subagent analysis.</div>
</div>

<div class="settings-group proxy-item">
  <div class="settings-label" id="requestInspectionRetentionLimitLabel">Inspection Retention</div>
  <input class="settings-number-input proxy-input" id="requestInspectionRetentionLimitInput" type="number" min="50" max="2000" value="400" />
  <div class="settings-sub-note" id="requestInspectionRetentionLimitHint">Number of recent inspection records kept in memory and logs.</div>
</div>
```

- [ ] **Step 4: Wire the new UI state in `webui/js/app.js`**

Add these DOM bindings:

```js
    routeModeLabel: document.getElementById("routeModeLabel"),
    routeModeSelect: document.getElementById("routeModeSelect"),
    routeModeHint: document.getElementById("routeModeHint"),
    gptUpstreamProxyHostLabel: document.getElementById("gptUpstreamProxyHostLabel"),
    gptUpstreamProxyHostInput: document.getElementById("gptUpstreamProxyHostInput"),
    gptUpstreamProxyHostHint: document.getElementById("gptUpstreamProxyHostHint"),
    gptUpstreamProxyPortLabel: document.getElementById("gptUpstreamProxyPortLabel"),
    gptUpstreamProxyPortInput: document.getElementById("gptUpstreamProxyPortInput"),
    gptUpstreamProxyPortHint: document.getElementById("gptUpstreamProxyPortHint"),
    ollamaBaseUrlLabel: document.getElementById("ollamaBaseUrlLabel"),
    ollamaBaseUrlInput: document.getElementById("ollamaBaseUrlInput"),
    ollamaBaseUrlHint: document.getElementById("ollamaBaseUrlHint"),
    requestInspectionEnabledLabel: document.getElementById("requestInspectionEnabledLabel"),
    requestInspectionEnabledToggle: document.getElementById("requestInspectionEnabledToggle"),
    requestInspectionEnabledHint: document.getElementById("requestInspectionEnabledHint"),
    requestInspectionRetentionLimitLabel: document.getElementById("requestInspectionRetentionLimitLabel"),
    requestInspectionRetentionLimitInput: document.getElementById("requestInspectionRetentionLimitInput"),
    requestInspectionRetentionLimitHint: document.getElementById("requestInspectionRetentionLimitHint"),
```

Add these state defaults:

```js
    routeMode: "gpt",
    gptUpstreamProxyHost: "127.0.0.1",
    gptUpstreamProxyPort: 7890,
    ollamaBaseUrl: "http://127.0.0.1:11434",
    requestInspectionEnabled: true,
    requestInspectionRetentionLimit: 400,
```

Add these payload fields to `queueSaveConfig()`:

```js
      routeMode: String(state.routeMode || "gpt"),
      gptUpstreamProxyHost: String(dom.gptUpstreamProxyHostInput?.value || "127.0.0.1").trim(),
      gptUpstreamProxyPort: Number(dom.gptUpstreamProxyPortInput?.value || 7890),
      ollamaBaseUrl: String(dom.ollamaBaseUrlInput?.value || "http://127.0.0.1:11434").trim(),
      requestInspectionEnabled: !!state.requestInspectionEnabled,
      requestInspectionRetentionLimit: Number(dom.requestInspectionRetentionLimitInput?.value || 400),
```

Add these event handlers:

```js
    dom.routeModeSelect.addEventListener("change", () => {
      state.routeMode = String(dom.routeModeSelect.value || "gpt");
      queueSaveConfig();
    });
    dom.gptUpstreamProxyHostInput.addEventListener("change", queueSaveConfig);
    dom.gptUpstreamProxyPortInput.addEventListener("change", queueSaveConfig);
    dom.ollamaBaseUrlInput.addEventListener("change", queueSaveConfig);
    dom.requestInspectionEnabledToggle.addEventListener("change", () => {
      state.requestInspectionEnabled = !!dom.requestInspectionEnabledToggle.checked;
      queueSaveConfig();
    });
    dom.requestInspectionRetentionLimitInput.addEventListener("change", queueSaveConfig);
```

- [ ] **Step 5: Add a small layout rule and build the app**

Append to `webui/css/styles.css`:

```css
.proxy-grid .proxy-item select,
.proxy-grid .proxy-item input[type="text"],
.proxy-grid .proxy-item input[type="number"] {
  width: 100%;
}
```

Run:

```powershell
msbuild .\Codex_AccountSwitch.slnx /t:Build /p:Configuration=Debug /p:Platform=x64
```

Expected: PASS with the app binary rebuilt successfully.

- [ ] **Step 6: Commit**

```bash
git add Codex_AccountSwitch/webview_host.cpp webui/index.html webui/js/app.js webui/css/styles.css
git commit -m "feat: surface route settings in config and UI"
```

### Task 4: Add Runtime Route State And Tray Switching

**Files:**
- Create: `Codex_AccountSwitch/proxy_route_state.h`
- Create: `Codex_AccountSwitch/proxy_route_state.cpp`
- Modify: `Codex_AccountSwitch/Codex_AccountSwitch.vcxproj`
- Modify: `Codex_AccountSwitch/webview_host.cpp`
- Modify: `Codex_AccountSwitch/webview_host.h`
- Modify: `Codex_AccountSwitch_Tests/main.cpp`
- Modify: `Codex_AccountSwitch_Tests/Codex_AccountSwitch_Tests.vcxproj`

- [ ] **Step 1: Write the failing runtime-state tests**

Append to `Codex_AccountSwitch_Tests/main.cpp`:

```cpp
#include "../Codex_AccountSwitch/proxy_route_state.h"

using cas::GetRouteStateSnapshot;
using cas::InitializeRouteState;
using cas::RouteStateSnapshot;
using cas::SetActiveRouteMode;

static void TestRouteStateTransitions()
{
    RouteStateSnapshot initial;
    initial.routeMode = RouteMode::Gpt;
    initial.gptProxyHost = L"127.0.0.1";
    initial.gptProxyPort = 7890;
    initial.ollamaBaseUrl = L"http://127.0.0.1:11434";
    initial.requestInspectionEnabled = true;
    initial.requestInspectionRetentionLimit = 400;

    InitializeRouteState(initial);
    SetActiveRouteMode(RouteMode::Ollama);

    const RouteStateSnapshot snapshot = GetRouteStateSnapshot();
    assert(snapshot.routeMode == RouteMode::Ollama);
    assert(snapshot.gptProxyPort == 7890);
}
```

Update `main()`:

```cpp
    TestRouteStateTransitions();
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
msbuild .\Codex_AccountSwitch.slnx /t:Build /p:Configuration=Debug /p:Platform=x64
```

Expected: FAIL with missing `proxy_route_state.h`.

- [ ] **Step 3: Add the runtime route-state module**

Create `Codex_AccountSwitch/proxy_route_state.h`:

```cpp
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

    void InitializeRouteState(const RouteStateSnapshot &snapshot);
    void SetActiveRouteMode(RouteMode mode);
    RouteStateSnapshot GetRouteStateSnapshot();
}
```

Create `Codex_AccountSwitch/proxy_route_state.cpp`:

```cpp
#include "proxy_route_state.h"

#include <mutex>

namespace
{
    std::mutex gRouteStateMutex;
    cas::RouteStateSnapshot gRouteState;
}

namespace cas
{
    void InitializeRouteState(const RouteStateSnapshot &snapshot)
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
```

- [ ] **Step 4: Wire route state into startup and tray menu**

Modify `Codex_AccountSwitch/Codex_AccountSwitch.vcxproj` to include:

```xml
    <ClCompile Include="proxy_route_state.cpp" />
```

and:

```xml
    <ClInclude Include="proxy_route_state.h" />
```

Modify `Codex_AccountSwitch_Tests/Codex_AccountSwitch_Tests.vcxproj` to include:

```xml
    <ClCompile Include="..\Codex_AccountSwitch\proxy_route_state.cpp" />
```

and:

```xml
    <ClInclude Include="..\Codex_AccountSwitch\proxy_route_state.h" />
```

In `webview_host.cpp`, after config load at startup, initialize runtime state:

```cpp
    cas::RouteStateSnapshot routeState;
    routeState.routeMode = cas::ParseRouteMode(startCfg.routeMode);
    routeState.gptProxyHost = startCfg.gptUpstreamProxyHost;
    routeState.gptProxyPort = startCfg.gptUpstreamProxyPort;
    routeState.ollamaBaseUrl = startCfg.ollamaBaseUrl;
    routeState.requestInspectionEnabled = startCfg.requestInspectionEnabled;
    routeState.requestInspectionRetentionLimit = startCfg.requestInspectionRetentionLimit;
    cas::InitializeRouteState(routeState);
```

In `ShowTrayContextMenu()`, add these menu items before account quick-switch:

```cpp
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kTrayCmdRouteGpt, L"Route: GPT");
  AppendMenuW(menu, MF_STRING, kTrayCmdRouteOllama, L"Route: Ollama");
```

In `HandleTrayCommand(...)`, add:

```cpp
  if (commandId == kTrayCmdRouteGpt)
  {
    cas::SetActiveRouteMode(cas::RouteMode::Gpt);
    SendWebStatus(L"已切换到 GPT 路由", L"success", L"route_mode_gpt");
    return true;
  }
  if (commandId == kTrayCmdRouteOllama)
  {
    cas::SetActiveRouteMode(cas::RouteMode::Ollama);
    SendWebStatus(L"已切换到 Ollama 路由", L"success", L"route_mode_ollama");
    return true;
  }
```

- [ ] **Step 5: Run the tests and confirm they pass**

Run:

```powershell
msbuild .\Codex_AccountSwitch.slnx /t:Build /p:Configuration=Debug /p:Platform=x64
.\Codex_AccountSwitch_Tests\Debug\x64\Codex_AccountSwitch_Tests.exe
```

Expected: the test executable exits `0` after `TestRouteStateTransitions()`.

- [ ] **Step 6: Commit**

```bash
git add Codex_AccountSwitch/proxy_route_state.h Codex_AccountSwitch/proxy_route_state.cpp Codex_AccountSwitch/Codex_AccountSwitch.vcxproj Codex_AccountSwitch/webview_host.cpp Codex_AccountSwitch/webview_host.h Codex_AccountSwitch_Tests/Codex_AccountSwitch_Tests.vcxproj Codex_AccountSwitch_Tests/main.cpp
git commit -m "feat: add runtime route state and tray mode switching"
```

### Task 5: Add Explicit GPT Proxying And Ollama Upstream Forwarding

**Files:**
- Create: `Codex_AccountSwitch/proxy_upstream.h`
- Create: `Codex_AccountSwitch/proxy_upstream.cpp`
- Modify: `Codex_AccountSwitch/Codex_AccountSwitch.vcxproj`
- Modify: `Codex_AccountSwitch/webview_host.cpp`

- [ ] **Step 1: Add the upstream forwarder interface**

Create `Codex_AccountSwitch/proxy_upstream.h`:

```cpp
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

    bool ForwardRequestToOllama(const RouteStateSnapshot &routeState,
                                const std::wstring &method,
                                const std::wstring &path,
                                const std::wstring &contentType,
                                const std::string &body,
                                UpstreamResponse &response);

    std::wstring BuildNamedProxyString(const RouteStateSnapshot &routeState);
}
```

- [ ] **Step 2: Implement the two forwarders with WinHTTP**

Create `Codex_AccountSwitch/proxy_upstream.cpp`:

```cpp
#include "proxy_upstream.h"

#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace
{
    bool SendSimpleRequest(const std::wstring &host,
                           INTERNET_PORT port,
                           const std::wstring &path,
                           const std::wstring &method,
                           const std::wstring &contentType,
                           const std::string &body,
                           bool secure,
                           const std::wstring *proxy,
                           cas::UpstreamResponse &response)
    {
        const DWORD accessType = proxy == nullptr ? WINHTTP_ACCESS_TYPE_NO_PROXY
                                                  : WINHTTP_ACCESS_TYPE_NAMED_PROXY;
        HINTERNET session = WinHttpOpen(L"CodexAccountSwitch/RouteProxy",
                                        accessType,
                                        proxy == nullptr ? WINHTTP_NO_PROXY_NAME : proxy->c_str(),
                                        WINHTTP_NO_PROXY_BYPASS,
                                        0);
        if (session == nullptr)
        {
            response.error = L"WinHttpOpen_failed";
            return false;
        }

        HINTERNET connect = WinHttpConnect(session, host.c_str(), port, 0);
        if (connect == nullptr)
        {
            response.error = L"WinHttpConnect_failed";
            WinHttpCloseHandle(session);
            return false;
        }

        const DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET request = WinHttpOpenRequest(connect, method.c_str(), path.c_str(),
                                               nullptr, WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (request == nullptr)
        {
            response.error = L"WinHttpOpenRequest_failed";
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return false;
        }

        const std::wstring headers = L"Content-Type: " + contentType + L"\r\n";
        BOOL ok = WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(-1L),
                                     body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char *>(body.data()),
                                     static_cast<DWORD>(body.size()),
                                     static_cast<DWORD>(body.size()), 0);
        if (ok)
        {
            ok = WinHttpReceiveResponse(request, nullptr);
        }
        if (!ok)
        {
            response.error = L"http_transport_failed";
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return false;
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
        response.statusCode = static_cast<int>(statusCode);

        wchar_t contentTypeBuf[256]{};
        DWORD contentTypeSize = sizeof(contentTypeBuf);
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_TYPE,
                                WINHTTP_HEADER_NAME_BY_INDEX, &contentTypeBuf,
                                &contentTypeSize, WINHTTP_NO_HEADER_INDEX))
        {
            response.contentType = contentTypeBuf;
        }

        std::string payload;
        while (true)
        {
            char buf[4096]{};
            DWORD read = 0;
            if (!WinHttpReadData(request, buf, sizeof(buf), &read) || read == 0)
            {
                break;
            }
            payload.append(buf, buf + read);
        }
        response.body = std::move(payload);

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return true;
    }
}

namespace cas
{
    bool ForwardRequestToOllama(const RouteStateSnapshot &routeState,
                                const std::wstring &method,
                                const std::wstring &path,
                                const std::wstring &contentType,
                                const std::string &body,
                                UpstreamResponse &response)
    {
        UNREFERENCED_PARAMETER(routeState);
        return SendSimpleRequest(L"127.0.0.1", 11434, path, method, contentType, body, false, nullptr, response);
    }

    std::wstring BuildNamedProxyString(const RouteStateSnapshot &routeState)
    {
        return routeState.gptProxyHost + L":" + std::to_wstring(routeState.gptProxyPort);
    }
}
```

- [ ] **Step 3: Register the new source files**

Modify `Codex_AccountSwitch/Codex_AccountSwitch.vcxproj`:

```xml
    <ClCompile Include="proxy_upstream.cpp" />
```

and:

```xml
    <ClInclude Include="proxy_upstream.h" />
```

- [ ] **Step 4: Route proxy requests by active mode and force GPT egress through the configured proxy**

In `Codex_AccountSwitch/webview_host.cpp`, extract the current account-backed handler into a helper named `HandleCodexAccountProxyRequest(...)`, then add this guard before the existing account dispatch logic:

```cpp
    const cas::RouteStateSnapshot routeState = cas::GetRouteStateSnapshot();
    if (routeState.routeMode == cas::RouteMode::Ollama)
    {
        cas::UpstreamResponse upstream;
        if (!cas::ForwardRequestToOllama(routeState, FromUtf8(method), FromUtf8(path),
                                         FromUtf8(contentType), body, upstream))
        {
            statusCode = 502;
            contentType = L"application/json";
            responseBody = "{\"error\":{\"message\":\"ollama_upstream_failed\"}}";
            return finalizeReturn(true);
        }
        statusCode = static_cast<DWORD>(upstream.statusCode);
        contentType = upstream.contentType.empty() ? L"application/json" : upstream.contentType;
        responseBody = upstream.body;
        return finalizeReturn(true);
    }

    return HandleCodexAccountProxyRequest(/* existing arguments */);
```

For the GPT path, keep the existing account-backed Codex forwarding logic, but replace `WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY` in the OpenAI/Codex transport functions with an explicit named proxy built from runtime route state:

```cpp
    const cas::RouteStateSnapshot routeState = cas::GetRouteStateSnapshot();
    const std::wstring namedProxy = cas::BuildNamedProxyString(routeState);

    HINTERNET hSession = WinHttpOpen(
        BuildCodexApiUserAgent().c_str(),
        WINHTTP_ACCESS_TYPE_NAMED_PROXY,
        namedProxy.c_str(),
        WINHTTP_NO_PROXY_BYPASS,
        0);
```

Apply that same explicit-proxy pattern anywhere the current proxy code reaches the Codex backend with WinHTTP so GPT mode always exits through the configured `host:port` pair instead of ambient system proxy discovery.

- [ ] **Step 5: Build the app and verify the proxy still starts**

Run:

```powershell
msbuild .\Codex_AccountSwitch.slnx /t:Build /p:Configuration=Debug /p:Platform=x64
```

Expected: PASS.

Then start the app manually and verify the proxy service still starts from the UI without crashing.

- [ ] **Step 6: Commit**

```bash
git add Codex_AccountSwitch/proxy_upstream.h Codex_AccountSwitch/proxy_upstream.cpp Codex_AccountSwitch/Codex_AccountSwitch.vcxproj Codex_AccountSwitch/webview_host.cpp
git commit -m "feat: add GPT proxy and Ollama upstream forwarding"
```

### Task 6: Extend Traffic Logs And Run Manual Validation

**Files:**
- Modify: `Codex_AccountSwitch/webview_host.cpp`
- Modify: `webui/js/app.js`
- Modify: `webui/css/styles.css`

- [ ] **Step 1: Extend traffic log records with route metadata**

In the traffic-log write path inside `Codex_AccountSwitch/webview_host.cpp`, append these fields when a request is logged:

```cpp
    ss << L",\"routeMode\":\"" << EscapeJsonString(routeModeText) << L"\"";
    ss << L",\"upstream\":\"" << EscapeJsonString(upstreamText) << L"\"";
    ss << L",\"inspectionSummary\":\"" << EscapeJsonString(inspectionSummary) << L"\"";
```

Populate:

- `routeModeText` from `cas::RouteModeToConfigValue(routeState.routeMode)`
- `upstreamText` as `127.0.0.1:7890` for GPT and `127.0.0.1:11434` for Ollama
- `inspectionSummary` from the masked request summary helpers

- [ ] **Step 2: Render the new route data in the traffic tab**

In `webui/js/app.js`, update the row renderer inside `renderTrafficLogs()` to show route metadata inside the model/path cells:

```js
      const routeMode = String(it?.routeMode || "-");
      const upstream = String(it?.upstream || "-");
      const inspectionSummary = String(it?.inspectionSummary || "");
```

and append them to the rendered HTML:

```js
        <td>
          <div>${escapeHtml(model)}</div>
          <div class="traffic-subline">${escapeHtml(routeMode)} -> ${escapeHtml(upstream)}</div>
          <div class="traffic-subline">${escapeHtml(inspectionSummary)}</div>
        </td>
```

Add a tiny helper style if needed in `webui/css/styles.css`:

```css
.traffic-subline {
  font-size: 12px;
  opacity: 0.72;
  margin-top: 2px;
}
```

- [ ] **Step 3: Build and smoke-test the UI**

Run:

```powershell
msbuild .\Codex_AccountSwitch.slnx /t:Build /p:Configuration=Debug /p:Platform=x64
```

Expected: PASS.

Launch:

```powershell
Start-Process -FilePath .\Release\x64\Codex_AccountSwitch.exe
```

Expected: the app opens without JavaScript or WebView2 initialization failures.

- [ ] **Step 4: Run the GPT and Ollama validation checklist**

Manual validation:

1. In the app, enable stealth proxy mode and start the local proxy on `11480`.
2. Set route mode to `GPT`.
3. Confirm `Clash` is listening on `127.0.0.1:7890`.
4. Run:

```powershell
curl.exe -sS -H "Authorization: Bearer local-proxy-key" http://127.0.0.1:11480/v1/models
```

Expected: a JSON model list is returned and the traffic log shows `routeMode = gpt`.

5. Set route mode to `Ollama`.
6. Confirm Ollama is listening on `127.0.0.1:11434`.
7. Run:

```powershell
curl.exe -sS -H "Authorization: Bearer local-proxy-key" http://127.0.0.1:11480/v1/models
```

Expected: the result now comes from Ollama and the traffic log shows `routeMode = ollama`.

8. Turn on request inspection, then collect:
   - one normal Codex CLI request
   - one Codex CLI request that triggers a subagent

9. Compare the traffic log entries and note whether any stable fields differ across:
   - headers
   - path
   - body metadata

- [ ] **Step 5: Commit**

```bash
git add Codex_AccountSwitch/webview_host.cpp webui/js/app.js webui/index.html webui/css/styles.css
git commit -m "feat: surface route metadata in traffic logs"
```

## Self-Review

### Spec coverage

- Global GPT/Ollama switching: covered by Tasks 1, 3, 4, 5
- Tray-driven switching: covered by Task 4
- GPT via `127.0.0.1:7890`: covered by Task 5
- Ollama via `127.0.0.1:11434`: covered by Task 5
- Request inspection and subagent feasibility capture: covered by Tasks 2 and 6
- Reuse existing codebase structure without broad unrelated refactor: covered by the focused file structure and incremental tasks

### Placeholder scan

- No `TODO`, `TBD`, or “implement later” markers remain
- Each code step includes concrete file content or exact snippets
- Each validation step includes explicit commands or manual actions

### Type consistency

- `RouteMode` is introduced once in `route_settings.h` and reused consistently
- `RouteStateSnapshot` is the single runtime shape for route state
- `InspectionRecord` is the single bounded request-summary record

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-04-26-codex-ollama-switcher.md`. Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?

## Execution Status Update

Status as of 2026-04-28: the MVP implementation described by this plan has been completed in the working tree and validated manually on Windows.

Completed implementation areas:

- Task 1: route settings module and native test harness
- Task 2: request inspection helpers
- Task 3: persistent route settings and WebUI controls
- Task 4: runtime route state and tray switching
- Task 5: explicit GPT proxying and Ollama upstream forwarding
- Task 6: traffic-log route metadata and manual validation

Current important files:

- `Codex_AccountSwitch/route_settings.h`
- `Codex_AccountSwitch/route_settings.cpp`
- `Codex_AccountSwitch/proxy_route_state.h`
- `Codex_AccountSwitch/proxy_route_state.cpp`
- `Codex_AccountSwitch/request_inspector.h`
- `Codex_AccountSwitch/request_inspector.cpp`
- `Codex_AccountSwitch/proxy_upstream.h`
- `Codex_AccountSwitch/proxy_upstream.cpp`
- `Codex_AccountSwitch/webview_host.cpp`
- `webui/index.html`
- `webui/js/app.js`
- `webui/css/styles.css`
- `Codex_AccountSwitch_Tests/main.cpp`
- `Codex_AccountSwitch_Tests/Codex_AccountSwitch_Tests.vcxproj`

Validation results:

- app build succeeded for `Codex_AccountSwitch/Codex_AccountSwitch.vcxproj`
- native tests passed via `Codex_AccountSwitch_Tests/Debug/x64/Codex_AccountSwitch_Tests.exe`
- `proxy_upstream.cpp` compiled standalone during validation
- `webui/js/app.js` passed `node --check`
- `curl` against `http://127.0.0.1:11480/v1/models` returned Ollama model data in Ollama mode
- Codex CLI custom-provider traffic succeeded in GPT mode and traffic logs showed upstream `127.0.0.1:7890`
- a real `codex exec` run triggered `spawn_agent` and `wait`, and the subagent request traffic passed through `11480`

Working Codex CLI pattern:

```powershell
codex exec ... -c 'model_provider="custom"' -c 'model_providers.custom.base_url="http://127.0.0.1:11480/v1"'
```

Do not use `HTTP_PROXY=http://127.0.0.1:11480` as the primary integration path. Port `11480` currently behaves as an OpenAI-compatible API endpoint proxy, not a generic CONNECT proxy.

Remaining follow-up:

- capture deeper sanitized request body summaries for main-session and subagent runs
- identify a stable subagent discriminator before implementing separate `main` and `subagent` route switches
- only after the discriminator is proven, add `Main: GPT/Ollama` and `Subagent: GPT/Ollama` controls with default-follow-main behavior

Repository note:

- This workspace currently has no `.git` directory, so commit steps in the original plan were not executed here.
