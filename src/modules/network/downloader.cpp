#include "downloader.h"

#include <chrono>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <vector>

namespace gddi {
namespace network {

void Downloader::async_http_get(const std::string &url,
                                const std::function<void(const bool, const std::vector<unsigned char> &)> &callback) {
    HttpRequestPtr req(new HttpRequest);
    req->method = HTTP_GET;
    req->url = url;
    req->timeout = 3;
    client_.sendAsync(req, [callback](const HttpResponsePtr &resp) {
        try {
            if (resp == nullptr) { throw std::runtime_error("request failed!"); }
            if (resp->status_code != 200) { throw std::runtime_error(resp->status_message()); }
            callback(true, std::vector<unsigned char>(resp->body.begin(), resp->body.end()));
        } catch (std::exception &exception) {
            std::string what = exception.what();
            callback(false, std::vector<unsigned char>(what.begin(), what.end()));
        }
    });
}

bool Downloader::sync_http_get(const std::string &url, std::vector<unsigned char> &body) {
    HttpRequest req;
    req.method = HTTP_GET;
    req.url = url;
    req.timeout = 3;

    HttpResponse resp;
    client_.send(&req, &resp);
    if (resp.status_code == 200) {
        body = std::vector<unsigned char>(resp.body.begin(), resp.body.end());
        return true;
    }

    return false;
}

}// namespace network
}// namespace gddi