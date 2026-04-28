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
