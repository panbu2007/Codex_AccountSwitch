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

    std::wstring MaskHeaderValue(const std::wstring& name, const std::wstring& value);
    void TrimInspectionRecords(std::vector<InspectionRecord>& items, int maxItems);
}
