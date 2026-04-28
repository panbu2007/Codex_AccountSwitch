#include "proxy_upstream.h"

#include <algorithm>
#include <vector>
#include <windows.h>
#include <winhttp.h>

namespace
{
    struct ParsedBaseUrl
    {
        std::wstring host;
        INTERNET_PORT port = INTERNET_DEFAULT_HTTP_PORT;
        bool secure = false;
        std::wstring basePath;
    };

    bool ParseBaseUrl(const std::wstring& url, ParsedBaseUrl& parsed)
    {
        URL_COMPONENTS components{};
        components.dwStructSize = sizeof(components);

        wchar_t hostName[256]{};
        wchar_t urlPath[1024]{};
        components.lpszHostName = hostName;
        components.dwHostNameLength =
            static_cast<DWORD>(sizeof(hostName) / sizeof(hostName[0]));
        components.lpszUrlPath = urlPath;
        components.dwUrlPathLength =
            static_cast<DWORD>(sizeof(urlPath) / sizeof(urlPath[0]));

        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &components))
        {
            return false;
        }

        parsed.host.assign(components.lpszHostName, components.dwHostNameLength);
        parsed.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
        parsed.port = components.nPort != 0
                          ? components.nPort
                          : (parsed.secure ? INTERNET_DEFAULT_HTTPS_PORT
                                           : INTERNET_DEFAULT_HTTP_PORT);
        parsed.basePath.assign(components.lpszUrlPath, components.dwUrlPathLength);
        if (parsed.basePath == L"/")
        {
            parsed.basePath.clear();
        }

        return !parsed.host.empty();
    }

    std::wstring JoinPath(const std::wstring& basePath, std::wstring requestPath)
    {
        if (requestPath.empty())
        {
            requestPath = L"/";
        }
        else if (requestPath.front() != L'/')
        {
            requestPath.insert(requestPath.begin(), L'/');
        }

        if (basePath.empty())
        {
            return requestPath;
        }

        if (requestPath == basePath)
        {
            return requestPath;
        }

        const std::wstring basePrefix = basePath + L"/";
        if (requestPath.rfind(basePrefix, 0) == 0)
        {
            return requestPath;
        }

        if (basePath.back() == L'/' && requestPath.front() == L'/')
        {
            return basePath.substr(0, basePath.size() - 1) + requestPath;
        }
        if (basePath.back() != L'/' && requestPath.front() != L'/')
        {
            return basePath + L"/" + requestPath;
        }

        return basePath + requestPath;
    }

    bool SendSimpleRequest(const ParsedBaseUrl& baseUrl,
                           const std::wstring& method,
                           const std::wstring& path,
                           const std::wstring& contentType,
                           const std::string& body,
                           cas::UpstreamResponse& response)
    {
        response = {};

        HINTERNET session = WinHttpOpen(L"CodexAccountSwitch/OllamaProxy",
                                        WINHTTP_ACCESS_TYPE_NO_PROXY,
                                        WINHTTP_NO_PROXY_NAME,
                                        WINHTTP_NO_PROXY_BYPASS, 0);
        if (session == nullptr)
        {
            response.error = L"WinHttpOpen_failed";
            return false;
        }

        HINTERNET connect =
            WinHttpConnect(session, baseUrl.host.c_str(), baseUrl.port, 0);
        if (connect == nullptr)
        {
            response.error = L"WinHttpConnect_failed";
            WinHttpCloseHandle(session);
            return false;
        }

        const std::wstring targetPath = JoinPath(baseUrl.basePath, path);
        HINTERNET request = WinHttpOpenRequest(
            connect, method.c_str(), targetPath.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            baseUrl.secure ? WINHTTP_FLAG_SECURE : 0);
        if (request == nullptr)
        {
            response.error = L"WinHttpOpenRequest_failed";
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return false;
        }

        const std::wstring headers =
            L"Content-Type: " +
            (contentType.empty() ? std::wstring(L"application/json") : contentType) +
            L"\r\nAccept: */*\r\nConnection: close\r\n";

        BOOL ok = WinHttpSendRequest(
            request, headers.c_str(), static_cast<DWORD>(-1L),
            body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
            static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0);
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
        WinHttpQueryHeaders(request,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
                            WINHTTP_NO_HEADER_INDEX);
        response.statusCode = static_cast<int>(statusCode);

        DWORD contentTypeBytes = 0;
        if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_TYPE,
                                 WINHTTP_HEADER_NAME_BY_INDEX,
                                 WINHTTP_NO_OUTPUT_BUFFER, &contentTypeBytes,
                                 WINHTTP_NO_HEADER_INDEX))
        {
            contentTypeBytes = 0;
        }
        if (contentTypeBytes > sizeof(wchar_t))
        {
            std::vector<wchar_t> contentTypeBuffer(
                contentTypeBytes / sizeof(wchar_t), L'\0');
            if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_TYPE,
                                    WINHTTP_HEADER_NAME_BY_INDEX,
                                    contentTypeBuffer.data(), &contentTypeBytes,
                                    WINHTTP_NO_HEADER_INDEX))
            {
                response.contentType.assign(contentTypeBuffer.data());
            }
        }

        while (true)
        {
            char chunk[4096]{};
            DWORD read = 0;
            if (!WinHttpReadData(request, chunk, sizeof(chunk), &read) || read == 0)
            {
                break;
            }
            response.body.append(chunk, chunk + read);
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return true;
    }
}

namespace cas
{
    bool ForwardRequestToOllama(const RouteStateSnapshot& routeState,
                                const std::wstring& method,
                                const std::wstring& path,
                                const std::wstring& contentType,
                                const std::string& body,
                                UpstreamResponse& response)
    {
        ParsedBaseUrl baseUrl;
        if (!ParseBaseUrl(routeState.ollamaBaseUrl, baseUrl))
        {
            response = {};
            response.error = L"ollama_base_url_invalid";
            return false;
        }

        return SendSimpleRequest(baseUrl, method, path, contentType, body, response);
    }

    std::wstring BuildNamedProxyString(const RouteStateSnapshot& routeState)
    {
        return routeState.gptProxyHost + L":" +
               std::to_wstring(routeState.gptProxyPort);
    }
}
