#include "request_inspector.h"

#include <algorithm>
#include <cwctype>

namespace
{
    std::wstring ToLowerCopy(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
        return value;
    }
}

namespace cas
{
    std::wstring MaskHeaderValue(const std::wstring& name, const std::wstring& value)
    {
        const std::wstring lowered = ToLowerCopy(name);
        if (lowered == L"authorization" || lowered == L"x-api-key" ||
            lowered == L"chatgpt-account-id")
        {
            return value.empty() ? L"" : L"present";
        }

        return value;
    }

    void TrimInspectionRecords(std::vector<InspectionRecord>& items, int maxItems)
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
