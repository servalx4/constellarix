#include "http_client.h"
#include <curl/curl.h>
#include <algorithm>
#include <iostream>

static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
    size_t totalSize = size * nmemb;
    out->append((char*)contents, totalSize);
    return totalSize;
}

HttpClient::HttpClient() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient() {
    for (auto& req : pending) {
        if (req->thread.joinable()) req->thread.join();
    }
    curl_global_cleanup();
}

void HttpClient::fetchAsync(const std::string& url, std::function<void(HttpResponse)> callback) {
    auto req = std::make_unique<PendingRequest>();
    req->callback = std::move(callback);

    auto* reqPtr = req.get();
    req->thread = std::thread([reqPtr, url]() {
        CURL* curl = curl_easy_init();
        if (!curl) {
            reqPtr->response.error = "Failed to init curl";
            reqPtr->done = true;
            return;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &reqPtr->response.body);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "WebGraph3D/1.0");

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            reqPtr->response.error = curl_easy_strerror(res);
        } else {
            long code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
            reqPtr->response.statusCode = (int)code;
        }

        curl_easy_cleanup(curl);
        reqPtr->done = true;
    });

    pending.push_back(std::move(req));
}

void HttpClient::update() {
    for (auto it = pending.begin(); it != pending.end();) {
        if ((*it)->done) {
            if ((*it)->thread.joinable()) (*it)->thread.join();
            (*it)->callback((*it)->response);
            it = pending.erase(it);
        } else {
            ++it;
        }
    }
}
