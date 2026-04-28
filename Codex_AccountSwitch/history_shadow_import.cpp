#include "history_shadow_import.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace
{
    std::wstring ToLowerCopy(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(), towlower);
        return value;
    }

    std::string WideToUtf8(const std::wstring &value)
    {
        std::string out;
        out.reserve(value.size());
        for (wchar_t ch : value)
        {
            if (ch >= 0 && ch <= 0x7f)
            {
                out.push_back(static_cast<char>(ch));
            }
            else
            {
                out.push_back('?');
            }
        }
        return out;
    }

    std::wstring Utf8ToWideAscii(const std::string &value)
    {
        std::wstring out;
        out.reserve(value.size());
        for (unsigned char ch : value)
        {
            out.push_back(static_cast<wchar_t>(ch));
        }
        return out;
    }

    std::wstring ExtractJsonStringAfter(const std::string &line,
                                        const std::string &anchor,
                                        const std::string &key)
    {
        const size_t anchorPos = anchor.empty() ? 0 : line.find(anchor);
        if (anchorPos == std::string::npos)
        {
            return L"";
        }
        const std::string needle = "\"" + key + "\"";
        const size_t keyPos = line.find(needle, anchorPos);
        if (keyPos == std::string::npos)
        {
            return L"";
        }
        const size_t colonPos = line.find(':', keyPos + needle.size());
        if (colonPos == std::string::npos)
        {
            return L"";
        }
        const size_t quotePos = line.find('"', colonPos + 1);
        if (quotePos == std::string::npos)
        {
            return L"";
        }
        std::string value;
        bool escaped = false;
        for (size_t i = quotePos + 1; i < line.size(); ++i)
        {
            const char ch = line[i];
            if (escaped)
            {
                value.push_back(ch);
                escaped = false;
                continue;
            }
            if (ch == '\\')
            {
                escaped = true;
                continue;
            }
            if (ch == '"')
            {
                return Utf8ToWideAscii(value);
            }
            value.push_back(ch);
        }
        return L"";
    }

    bool ReplaceJsonStringAfter(std::string &line,
                                const std::string &anchor,
                                const std::string &key,
                                const std::wstring &newValue)
    {
        const size_t anchorPos = anchor.empty() ? 0 : line.find(anchor);
        if (anchorPos == std::string::npos)
        {
            return false;
        }
        const std::string needle = "\"" + key + "\"";
        const size_t keyPos = line.find(needle, anchorPos);
        if (keyPos == std::string::npos)
        {
            return false;
        }
        const size_t colonPos = line.find(':', keyPos + needle.size());
        if (colonPos == std::string::npos)
        {
            return false;
        }
        const size_t quotePos = line.find('"', colonPos + 1);
        if (quotePos == std::string::npos)
        {
            return false;
        }
        bool escaped = false;
        for (size_t i = quotePos + 1; i < line.size(); ++i)
        {
            const char ch = line[i];
            if (escaped)
            {
                escaped = false;
                continue;
            }
            if (ch == '\\')
            {
                escaped = true;
                continue;
            }
            if (ch == '"')
            {
                line.replace(quotePos + 1, i - quotePos - 1, WideToUtf8(newValue));
                return true;
            }
        }
        return false;
    }

    bool ReadFirstLine(const fs::path &path, std::string &line)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            return false;
        }
        return static_cast<bool>(std::getline(in, line));
    }

    bool ReadWholeFileAfterFirstLine(const fs::path &path, std::string &rest)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            return false;
        }
        std::string first;
        std::getline(in, first);
        std::ostringstream ss;
        ss << in.rdbuf();
        rest = ss.str();
        return true;
    }

    bool WriteTextFile(const fs::path &path, const std::string &text)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
        {
            return false;
        }
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            return false;
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(out);
    }

    bool IsWithinPath(const fs::path &child, const fs::path &parent)
    {
        std::error_code ec;
        const fs::path childAbs = fs::weakly_canonical(child, ec);
        if (ec)
        {
            return false;
        }
        ec.clear();
        const fs::path parentAbs = fs::weakly_canonical(parent, ec);
        if (ec)
        {
            return false;
        }
        auto childIt = childAbs.begin();
        auto parentIt = parentAbs.begin();
        for (; parentIt != parentAbs.end(); ++parentIt, ++childIt)
        {
            if (childIt == childAbs.end())
            {
                return false;
            }
            if (ToLowerCopy(childIt->wstring()) != ToLowerCopy(parentIt->wstring()))
            {
                return false;
            }
        }
        return true;
    }

    std::wstring PathToManifestString(const fs::path &path)
    {
        return path.wstring();
    }

    std::string EscapeJsonAscii(const std::wstring &value)
    {
        std::string out;
        for (wchar_t ch : value)
        {
            if (ch == L'\\')
            {
                out += "\\\\";
            }
            else if (ch == L'"')
            {
                out += "\\\"";
            }
            else if (ch >= 0 && ch <= 0x7f)
            {
                out.push_back(static_cast<char>(ch));
            }
            else
            {
                out.push_back('?');
            }
        }
        return out;
    }

    std::string BuildManifest(const std::vector<cas::HistoryShadowItem> &items,
                              const std::wstring &targetProvider)
    {
        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"version\": 1,\n";
        ss << "  \"target_provider\": \"" << EscapeJsonAscii(targetProvider) << "\",\n";
        ss << "  \"items\": [\n";
        for (size_t i = 0; i < items.size(); ++i)
        {
            const auto &item = items[i];
            ss << "    {\n";
            ss << "      \"source_path\": \"" << EscapeJsonAscii(PathToManifestString(item.sourcePath)) << "\",\n";
            ss << "      \"source_id\": \"" << EscapeJsonAscii(item.sourceId) << "\",\n";
            ss << "      \"shadow_path\": \"" << EscapeJsonAscii(PathToManifestString(item.shadowPath)) << "\",\n";
            ss << "      \"shadow_id\": \"" << EscapeJsonAscii(item.shadowId) << "\",\n";
            ss << "      \"source_provider\": \"" << EscapeJsonAscii(item.sourceProvider) << "\",\n";
            ss << "      \"target_provider\": \"" << EscapeJsonAscii(item.targetProvider) << "\"\n";
            ss << "    }" << (i + 1 == items.size() ? "\n" : ",\n");
        }
        ss << "  ]\n";
        ss << "}\n";
        return ss.str();
    }

    std::string ReadWholeText(const fs::path &path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            return {};
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    std::vector<fs::path> ExtractManifestShadowPaths(const fs::path &manifestPath)
    {
        const std::string text = ReadWholeText(manifestPath);
        std::vector<fs::path> paths;
        size_t pos = 0;
        const std::string needle = "\"shadow_path\"";
        while ((pos = text.find(needle, pos)) != std::string::npos)
        {
            const size_t colonPos = text.find(':', pos + needle.size());
            const size_t quotePos = colonPos == std::string::npos ? std::string::npos : text.find('"', colonPos + 1);
            if (quotePos == std::string::npos)
            {
                break;
            }
            std::string value;
            bool escaped = false;
            for (size_t i = quotePos + 1; i < text.size(); ++i)
            {
                const char ch = text[i];
                if (escaped)
                {
                    value.push_back(ch);
                    escaped = false;
                    continue;
                }
                if (ch == '\\')
                {
                    escaped = true;
                    continue;
                }
                if (ch == '"')
                {
                    paths.emplace_back(Utf8ToWideAscii(value));
                    pos = i + 1;
                    break;
                }
                value.push_back(ch);
            }
        }
        return paths;
    }

    fs::path BuildShadowPath(const fs::path &codexHome, const fs::path &sourcePath)
    {
        fs::path year = L"unknown";
        fs::path month = L"unknown";
        fs::path day = L"unknown";
        const fs::path parent = sourcePath.parent_path();
        if (!parent.empty() && !parent.parent_path().empty() && !parent.parent_path().parent_path().empty())
        {
            day = parent.filename();
            month = parent.parent_path().filename();
            year = parent.parent_path().parent_path().filename();
        }
        return cas::GetHistoryShadowRoot(codexHome) / year / month / day /
               (sourcePath.stem().wstring() + L".cas-shadow.jsonl");
    }

    bool IsImportableSessionMeta(const std::string &firstLine,
                                 const std::wstring &sourceProvider,
                                 std::wstring &sessionId,
                                 std::wstring &threadName)
    {
        if (ExtractJsonStringAfter(firstLine, "", "type") != L"session_meta")
        {
            return false;
        }
        const std::wstring provider =
            ExtractJsonStringAfter(firstLine, "\"payload\"", "model_provider");
        if (ToLowerCopy(provider) != ToLowerCopy(sourceProvider))
        {
            return false;
        }
        sessionId = ExtractJsonStringAfter(firstLine, "\"payload\"", "id");
        if (sessionId.empty())
        {
            return false;
        }
        threadName = ExtractJsonStringAfter(firstLine, "\"payload\"", "thread_name");
        return true;
    }

    bool IsHexDigit(wchar_t ch)
    {
        return (ch >= L'0' && ch <= L'9') ||
               (ch >= L'a' && ch <= L'f') ||
               (ch >= L'A' && ch <= L'F');
    }

    bool LooksLikeUuid(const std::wstring &value)
    {
        if (value.size() != 36)
        {
            return false;
        }
        for (size_t i = 0; i < value.size(); ++i)
        {
            if (i == 8 || i == 13 || i == 18 || i == 23)
            {
                if (value[i] != L'-')
                {
                    return false;
                }
            }
            else if (!IsHexDigit(value[i]))
            {
                return false;
            }
        }
        return true;
    }

    uint64_t HashWide(const std::wstring &value)
    {
        uint64_t hash = 1469598103934665603ull;
        for (wchar_t ch : value)
        {
            hash ^= static_cast<uint64_t>(ch);
            hash *= 1099511628211ull;
        }
        return hash;
    }

    std::wstring Hex12(uint64_t value)
    {
        const wchar_t *digits = L"0123456789abcdef";
        std::wstring out(12, L'0');
        for (int i = 11; i >= 0; --i)
        {
            out[static_cast<size_t>(i)] = digits[value & 0xf];
            value >>= 4;
        }
        return out;
    }

    std::wstring MakeShadowSessionId(const std::wstring &sourceId)
    {
        if (LooksLikeUuid(sourceId))
        {
            std::wstring shadow = ToLowerCopy(sourceId);
            const uint64_t hash = HashWide(sourceId);
            shadow[19] = L'a';
            shadow[35] = L"0123456789abcdef"[hash & 0xf];
            if (shadow == ToLowerCopy(sourceId))
            {
                shadow[35] = shadow[35] == L'f' ? L'e' : L'f';
            }
            return shadow;
        }
        return L"00000000-0000-4000-8000-" + Hex12(HashWide(sourceId));
    }
}

namespace cas
{
    fs::path GetHistoryShadowRoot(const fs::path &codexHome)
    {
        return codexHome / L"sessions" / L"cas_shadow_import";
    }

    fs::path GetHistoryShadowManifestPath(const fs::path &codexHome)
    {
        return codexHome / L"bak" / L"cas_history_shadow_import" / L"manifest.json";
    }

    HistoryShadowResult ImportHistoryShadowCopies(const fs::path &codexHome,
                                                  const std::wstring &sourceProvider,
                                                  const std::wstring &targetProvider)
    {
        HistoryShadowResult result;
        result.message = L"history_shadow_import_complete";
        if (codexHome.empty())
        {
            result.ok = false;
            result.message = L"codex_home_missing";
            return result;
        }

        const fs::path sessionsDir = codexHome / L"sessions";
        const fs::path archivedDir = codexHome / L"archived_sessions";
        const fs::path shadowRoot = GetHistoryShadowRoot(codexHome);
        std::vector<fs::path> candidates;
        std::error_code ec;
        if (fs::exists(sessionsDir, ec) && !ec)
        {
            for (const auto &entry : fs::recursive_directory_iterator(sessionsDir, ec))
            {
                if (ec)
                {
                    break;
                }
                if (!entry.is_regular_file())
                {
                    continue;
                }
                if (entry.path().extension() != L".jsonl")
                {
                    continue;
                }
                if (IsWithinPath(entry.path(), shadowRoot))
                {
                    continue;
                }
                candidates.push_back(entry.path());
            }
        }
        ec.clear();
        if (fs::exists(archivedDir, ec) && !ec)
        {
            for (const auto &entry : fs::directory_iterator(archivedDir, ec))
            {
                if (ec)
                {
                    break;
                }
                if (entry.is_regular_file() && entry.path().extension() == L".jsonl")
                {
                    candidates.push_back(entry.path());
                }
            }
        }

        const fs::path manifestPath = GetHistoryShadowManifestPath(codexHome);
        std::vector<HistoryShadowItem> manifestItems;
        for (const fs::path &sourcePath : candidates)
        {
            ++result.scanned;
            std::string firstLine;
            if (!ReadFirstLine(sourcePath, firstLine))
            {
                ++result.skipped;
                continue;
            }
            std::wstring sourceId;
            std::wstring threadName;
            if (!IsImportableSessionMeta(firstLine, sourceProvider, sourceId, threadName))
            {
                ++result.skipped;
                continue;
            }

            const std::wstring shadowId = MakeShadowSessionId(sourceId);
            const fs::path shadowPath = BuildShadowPath(codexHome, sourcePath);
            if (fs::exists(shadowPath, ec) && !ec)
            {
                ++result.skipped;
                HistoryShadowItem item;
                item.sourcePath = sourcePath;
                item.sourceId = sourceId;
                item.shadowPath = shadowPath;
                item.shadowId = shadowId;
                item.sourceProvider = sourceProvider;
                item.targetProvider = targetProvider;
                manifestItems.push_back(item);
                continue;
            }

            std::string rest;
            if (!ReadWholeFileAfterFirstLine(sourcePath, rest))
            {
                ++result.failed;
                result.ok = false;
                continue;
            }

            std::string shadowFirstLine = firstLine;
            ReplaceJsonStringAfter(shadowFirstLine, "\"payload\"", "id", shadowId);
            ReplaceJsonStringAfter(shadowFirstLine, "\"payload\"", "model_provider", targetProvider);
            if (!threadName.empty() && threadName.rfind(L"[CAS] ", 0) != 0)
            {
                ReplaceJsonStringAfter(shadowFirstLine, "\"payload\"", "thread_name", L"[CAS] " + threadName);
            }

            std::string shadowText = shadowFirstLine;
            shadowText += "\n";
            shadowText += rest;
            if (!WriteTextFile(shadowPath, shadowText))
            {
                ++result.failed;
                result.ok = false;
                continue;
            }

            HistoryShadowItem item;
            item.sourcePath = sourcePath;
            item.sourceId = sourceId;
            item.shadowPath = shadowPath;
            item.shadowId = shadowId;
            item.sourceProvider = sourceProvider;
            item.targetProvider = targetProvider;
            result.items.push_back(item);
            manifestItems.push_back(item);
            ++result.imported;
        }

        if (!WriteTextFile(manifestPath, BuildManifest(manifestItems, targetProvider)))
        {
            result.ok = false;
            result.message = L"history_shadow_manifest_write_failed";
            ++result.failed;
        }
        return result;
    }

    HistoryShadowResult UndoHistoryShadowCopies(const fs::path &codexHome)
    {
        HistoryShadowResult result;
        result.message = L"history_shadow_undo_complete";
        const fs::path manifestPath = GetHistoryShadowManifestPath(codexHome);
        const fs::path shadowRoot = GetHistoryShadowRoot(codexHome);
        std::error_code ec;
        if (!fs::exists(manifestPath, ec) || ec)
        {
            result.message = L"history_shadow_manifest_missing";
            return result;
        }

        const std::vector<fs::path> paths = ExtractManifestShadowPaths(manifestPath);
        for (const fs::path &path : paths)
        {
            ++result.scanned;
            if (!IsWithinPath(path, shadowRoot))
            {
                ++result.failed;
                result.ok = false;
                continue;
            }
            ec.clear();
            if (fs::exists(path, ec) && !ec)
            {
                fs::remove(path, ec);
                if (ec)
                {
                    ++result.failed;
                    result.ok = false;
                }
                else
                {
                    ++result.imported;
                }
            }
            else
            {
                ++result.skipped;
            }
        }

        const fs::path backupDir = codexHome / L"bak" / L"cas_history_shadow_import";
        fs::create_directories(backupDir, ec);
        const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
        const fs::path backupManifest = backupDir / (L"manifest.undo-" + std::to_wstring(stamp) + L".json");
        ec.clear();
        fs::copy_file(manifestPath, backupManifest, fs::copy_options::overwrite_existing, ec);
        WriteTextFile(manifestPath, "{\n  \"version\": 1,\n  \"target_provider\": \"custom\",\n  \"items\": []\n}\n");
        return result;
    }
}
