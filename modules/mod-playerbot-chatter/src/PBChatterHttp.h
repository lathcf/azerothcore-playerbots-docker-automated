#ifndef MOD_PB_CHATTER_HTTP_H
#define MOD_PB_CHATTER_HTTP_H
#include <string>
namespace PBChatterHttp
{
    // POST jsonBody to url; returns response body, or "" on failure.
    // readTimeoutSec bounds how long to wait for the response body.
    std::string Post(std::string const& url, std::string const& jsonBody, int readTimeoutSec = 60);
}
#endif
