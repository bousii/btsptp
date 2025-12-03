#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <unordered_map>
#include <cstdint>

struct HttpRequest {
	std::string method;
	std::string path;
	std::string query;
	std::string http_version;
};

struct URL {
	std::string host;
	std::string port = "80";
};

std::string sha1_hash(const std::string& data);
std::string hash_to_hex(const std::string& hash);
std::string url_encode(const std::string& str);
std::string url_decode(const std::string& str);
std::unordered_map<std::string, std::string> parse_query_params(const std::string& query);
std::string generate_peer_id();
std::string build_announce_request(const std::string& announce_url,
                                   const std::string& info_hash,
                                   const std::string& peer_id,
                                   uint16_t port,
                                   int64_t uploaded,
                                   int64_t downloaded,
                                   int64_t left,
                                   const std::string& event);
URL parse_url(const std::string &url);
HttpRequest parse_http_request_line(const std::string& request_line);
#endif /* utils.hpp */
