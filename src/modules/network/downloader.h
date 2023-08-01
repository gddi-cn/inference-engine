#ifndef __DOWNLOADER_H__
#define __DOWNLOADER_H__

#include <functional>
#include <hv/http_client.h>
#include <hv/requests.h>
#include <string>
#include <vector>

namespace gddi {
namespace network {

class Downloader {
public:
    void async_http_get(const std::string &url,
                        const std::function<void(const bool, const std::vector<unsigned char> &)> &);
    bool sync_http_get(const std::string &url, std::vector<unsigned char> &body);

private:
    hv::HttpClient client_;
};

}// namespace network
}// namespace gddi

#endif