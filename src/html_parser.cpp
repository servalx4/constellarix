#include "html_parser.h"
#include <regex>
#include <set>

std::string normalizeUrl(const std::string& url, const std::string& baseUrl) {
    if (url.empty() || url[0] == '#') return "";
    if (url.find("javascript:") == 0 || url.find("mailto:") == 0) return "";

    // Already absolute
    if (url.find("http://") == 0 || url.find("https://") == 0) {
        return url;
    }

    // Extract base from baseUrl
    std::string base = baseUrl;
    size_t protoEnd = base.find("://");
    if (protoEnd == std::string::npos) return "";

    size_t pathStart = base.find('/', protoEnd + 3);
    std::string domain = (pathStart != std::string::npos) ? base.substr(0, pathStart) : base;

    if (url[0] == '/') {
        // Absolute path
        return domain + url;
    } else {
        // Relative path
        if (pathStart != std::string::npos) {
            size_t lastSlash = base.rfind('/');
            return base.substr(0, lastSlash + 1) + url;
        }
        return domain + "/" + url;
    }
}

std::vector<std::string> extractLinks(const std::string& html, const std::string& baseUrl) {
    std::vector<std::string> links;
    std::set<std::string> seen;

    // Limit input size to prevent regex stack overflow
    std::string safeHtml = html.length() > 500000 ? html.substr(0, 500000) : html;

    try {
        // Simple regex for href="..." or href='...'
        std::regex hrefRegex(R"(href\s*=\s*["']([^"']+)["'])", std::regex::icase);

        auto begin = std::sregex_iterator(safeHtml.begin(), safeHtml.end(), hrefRegex);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            std::string url = (*it)[1].str();
            std::string normalized = normalizeUrl(url, baseUrl);

            if (!normalized.empty() && seen.find(normalized) == seen.end()) {
                seen.insert(normalized);
                links.push_back(normalized);
            }
        }
    } catch (...) {
        // Regex failed, return empty
    }

    return links;
}
