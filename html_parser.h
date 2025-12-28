#pragma once
#include <string>
#include <vector>

std::vector<std::string> extractLinks(const std::string& html, const std::string& baseUrl);
std::string normalizeUrl(const std::string& url, const std::string& baseUrl);
