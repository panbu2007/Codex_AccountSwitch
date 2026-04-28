#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../Codex_AccountSwitch/history_shadow_import.h"
#include "../Codex_AccountSwitch/proxy_route_state.h"
#include "../Codex_AccountSwitch/request_inspector.h"
#include "../Codex_AccountSwitch/route_settings.h"

namespace fs = std::filesystem;

using cas::GetHistoryShadowManifestPath;
using cas::GetHistoryShadowRoot;
using cas::ImportHistoryShadowCopies;
using cas::InspectionRecord;
using cas::GetRouteStateSnapshot;
using cas::InitializeRouteState;
using cas::MaskHeaderValue;
using cas::NormalizeRouteSettings;
using cas::ParseRouteMode;
using cas::RouteMode;
using cas::RouteModeToConfigValue;
using cas::RouteStateSnapshot;
using cas::RouteSettings;
using cas::SetActiveRouteMode;
using cas::TrimInspectionRecords;
using cas::UndoHistoryShadowCopies;

static void Expect(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

static void TestParseRouteMode()
{
    Expect(ParseRouteMode(L"gpt") == RouteMode::Gpt, "ParseRouteMode should parse gpt");
    Expect(ParseRouteMode(L"GPT") == RouteMode::Gpt, "ParseRouteMode should be case-insensitive for gpt");
    Expect(ParseRouteMode(L"ollama") == RouteMode::Ollama, "ParseRouteMode should parse ollama");
    Expect(ParseRouteMode(L"unexpected") == RouteMode::Gpt, "ParseRouteMode should default to gpt");
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

    Expect(s.routeMode == RouteMode::Ollama, "NormalizeRouteSettings should preserve route mode");
    Expect(s.gptProxyHost == L"127.0.0.1", "NormalizeRouteSettings should default GPT proxy host");
    Expect(s.gptProxyPort == 7890, "NormalizeRouteSettings should clamp GPT proxy port");
    Expect(s.ollamaBaseUrl == L"http://127.0.0.1:11434", "NormalizeRouteSettings should default Ollama base URL");
    Expect(s.requestInspectionRetentionLimit == 50, "NormalizeRouteSettings should enforce minimum retention");
}

static void TestRouteModeToConfigValue()
{
    Expect(RouteModeToConfigValue(RouteMode::Gpt) == L"gpt", "RouteModeToConfigValue should emit gpt");
    Expect(RouteModeToConfigValue(RouteMode::Ollama) == L"ollama", "RouteModeToConfigValue should emit ollama");
}

static void TestMaskHeaderValue()
{
    Expect(MaskHeaderValue(L"Authorization", L"Bearer super-secret-token") == L"present",
        "MaskHeaderValue should hide authorization values");
    Expect(MaskHeaderValue(L"AUTHORIZATION", L"Bearer super-secret-token") == L"present",
        "MaskHeaderValue should match authorization case-insensitively");
    Expect(MaskHeaderValue(L"ChatGPT-Account-Id", L"acc_1234567890") == L"present",
        "MaskHeaderValue should hide account identifiers");
    Expect(MaskHeaderValue(L"x-api-key", L"top-secret") == L"present",
        "MaskHeaderValue should hide api keys");
    Expect(MaskHeaderValue(L"X-API-KEY", L"") == L"",
        "MaskHeaderValue should preserve empty sensitive values as empty");
    Expect(MaskHeaderValue(L"User-Agent", L"codex-cli") == L"codex-cli",
        "MaskHeaderValue should preserve non-sensitive headers");
}

static void TestTrimInspectionRecords()
{
    std::vector<InspectionRecord> items(5);
    for (int i = 0; i < static_cast<int>(items.size()); ++i)
    {
        items[i].path = L"/request/" + std::to_wstring(i);
    }

    TrimInspectionRecords(items, 3);

    Expect(items.size() == 3, "TrimInspectionRecords should keep the newest bounded set");
    Expect(items[0].path == L"/request/2", "TrimInspectionRecords should discard the oldest items first");
    Expect(items[1].path == L"/request/3", "TrimInspectionRecords should preserve middle retained items");
    Expect(items[2].path == L"/request/4", "TrimInspectionRecords should preserve the newest item");
}

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
    Expect(snapshot.routeMode == RouteMode::Ollama, "SetActiveRouteMode should update the active mode");
    Expect(snapshot.gptProxyPort == 7890, "SetActiveRouteMode should preserve other route state values");
}

static void WriteTestFile(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

static std::string ReadTestFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return text;
}

static void TestHistoryShadowImportAndUndo()
{
    const fs::path root = fs::temp_directory_path() / L"cas_history_shadow_import_test";
    fs::remove_all(root);

    const fs::path codexHome = root / L".codex";
    const fs::path openaiSource = codexHome / L"sessions" / L"2026" / L"04" / L"26" / L"source-openai.jsonl";
    const fs::path customSource = codexHome / L"sessions" / L"2026" / L"04" / L"26" / L"source-custom.jsonl";
    const fs::path archivedSource = codexHome / L"archived_sessions" / L"source-archived.jsonl";

    WriteTestFile(openaiSource,
        "{\"timestamp\":\"2026-04-26T00:00:00Z\",\"type\":\"session_meta\",\"payload\":{\"id\":\"019dca37-add6-72c2-8341-32ef8c77e1f5\",\"thread_name\":\"Old session\",\"model_provider\":\"openai\"}}\n"
        "{\"type\":\"message\",\"payload\":{\"text\":\"keep me byte-for-byte\"}}\n");
    WriteTestFile(customSource,
        "{\"timestamp\":\"2026-04-26T00:00:00Z\",\"type\":\"session_meta\",\"payload\":{\"id\":\"custom-session\",\"thread_name\":\"Custom session\",\"model_provider\":\"custom\"}}\n");
    WriteTestFile(archivedSource,
        "{\"timestamp\":\"2026-04-25T00:00:00Z\",\"type\":\"session_meta\",\"payload\":{\"id\":\"019dca00-0000-7000-8000-000000000000\",\"thread_name\":\"Archived session\",\"model_provider\":\"openai\"}}\n");

    const auto first = ImportHistoryShadowCopies(codexHome, L"openai", L"custom");
    Expect(first.ok, "ImportHistoryShadowCopies should succeed for valid temp history");
    Expect(first.scanned == 3, "ImportHistoryShadowCopies should scan sessions and archived_sessions");
    Expect(first.imported == 2, "ImportHistoryShadowCopies should import openai source files");
    Expect(first.skipped == 1, "ImportHistoryShadowCopies should skip non-openai source files");
    Expect(fs::exists(GetHistoryShadowManifestPath(codexHome)), "ImportHistoryShadowCopies should write a manifest");

    const fs::path shadowRoot = GetHistoryShadowRoot(codexHome);
    int shadowCount = 0;
    fs::path firstShadow;
    for (const auto& entry : fs::recursive_directory_iterator(shadowRoot))
    {
        if (entry.is_regular_file() && entry.path().extension() == L".jsonl")
        {
            ++shadowCount;
            if (firstShadow.empty())
            {
                firstShadow = entry.path();
            }
        }
    }
    Expect(shadowCount == 2, "ImportHistoryShadowCopies should create two shadow files");
    const std::string shadowText = ReadTestFile(firstShadow);
    Expect(shadowText.find("\"model_provider\":\"custom\"") != std::string::npos,
        "Shadow first line should target custom provider");
    Expect(shadowText.find("019dca37-add6-72c2-a341-32ef8c77e1f") != std::string::npos,
        "Shadow first line should use deterministic UUID-shaped session id");
    Expect(shadowText.find("[CAS]") != std::string::npos,
        "Shadow thread name should be visibly marked");
    Expect(shadowText.find("keep me byte-for-byte") != std::string::npos,
        "Shadow file should preserve following records");

    const auto second = ImportHistoryShadowCopies(codexHome, L"openai", L"custom");
    Expect(second.imported == 0, "Second import should not duplicate existing shadow files");
    Expect(second.skipped == 3, "Second import should skip all already handled or non-matching files");

    const auto undo = UndoHistoryShadowCopies(codexHome);
    Expect(undo.ok, "UndoHistoryShadowCopies should succeed");
    Expect(fs::exists(openaiSource), "UndoHistoryShadowCopies should not delete source files");
    int remainingShadowCount = 0;
    if (fs::exists(shadowRoot))
    {
        for (const auto& entry : fs::recursive_directory_iterator(shadowRoot))
        {
            if (entry.is_regular_file() && entry.path().extension() == L".jsonl")
            {
                ++remainingShadowCount;
            }
        }
    }
    Expect(remainingShadowCount == 0, "UndoHistoryShadowCopies should delete manifest-listed shadow files");

    fs::remove_all(root);
}

int main()
{
    try
    {
        TestParseRouteMode();
        TestNormalizeRouteSettings();
        TestRouteModeToConfigValue();
        TestMaskHeaderValue();
        TestTrimInspectionRecords();
        TestRouteStateTransitions();
        TestHistoryShadowImportAndUndo();
        std::cout << "route_settings, request_inspector, proxy_route_state, and history_shadow_import tests passed\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "native logic tests failed: " << ex.what() << "\n";
        return 1;
    }
}
