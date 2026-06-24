#include "PBChatterHttp.h"
#include "PBChatterConfig.h"
#include "Log.h"
#include "httplib.h"
#include <regex>

std::string PBChatterHttp::Post(std::string const& url, std::string const& jsonBody, int readTimeoutSec)
{
    std::smatch m;
    std::regex re(R"(^(https?)://([^:/]+)(?::(\d+))?(/.*)?$)");
    if (!std::regex_match(url, m, re))
    {
        LOG_ERROR("server.loading", "[PlayerbotChatter] Bad URL: {}", url);
        return "";
    }
    if (m[1] == "https")
    {
        // httplib is built HTTP-only here (no CPPHTTPLIB_OPENSSL_SUPPORT). Refuse rather
        // than silently send cleartext to the https host/port.
        LOG_ERROR("server.loading", "[PlayerbotChatter] HTTPS not supported; use an http:// URL.");
        return "";
    }
    std::string host = m[2];
    int port = m[3].matched ? std::stoi(m[3]) : 80;
    std::string path = m[4].matched ? std::string(m[4]) : "/";

    httplib::Client cli(host, port);
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(readTimeoutSec, 0);
    cli.set_write_timeout(10, 0);

    // Use the std::string body overload: Post(path, body, content_type)
    auto res = cli.Post(path, jsonBody, "application/json");
    if (!res)
    {
        LOG_WARN("server.loading", "[PlayerbotChatter] Ollama POST failed (no response).");
        return "";
    }
    if (res->status != 200)
    {
        LOG_WARN("server.loading", "[PlayerbotChatter] Ollama HTTP {}.", res->status);
        return "";
    }
    return res->body;
}
