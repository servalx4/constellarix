#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>

struct HttpResponse {
    int statusCode = 0;
    std::string body;
    std::string error;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    void fetchAsync(const std::string& url, std::function<void(HttpResponse)> callback);
    void update(); // Process completed requests on main thread

private:
    struct PendingRequest {
        std::thread thread;
        std::atomic<bool> done{false};
        HttpResponse response;
        std::function<void(HttpResponse)> callback;
    };
    std::vector<std::unique_ptr<PendingRequest>> pending;
};
