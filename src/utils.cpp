#include <utils.hpp>
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <random>
#include <ctime>

HttpRequest parse_http_request_line(const std::string& request_line) {
    std::istringstream line_stream(request_line);
    HttpRequest req;
    line_stream >> req.method >> req.path >> req.http_version;
    auto qmark_pos = req.path.find('?');
    if (qmark_pos != std::string::npos)
        req.query = req.path.substr(qmark_pos + 1);
    return req;
}

std::string sha1_hash(const std::string &data) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)data.c_str(), data.length(), hash);
    return std::string((char*)hash, SHA_DIGEST_LENGTH);
}

std::string hash_to_hex(const std::string &hash) {
    std::stringstream ss;
    for (unsigned char c : hash) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }
    return ss.str();
}

std::string url_encode(const std::string &str) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : str) {
        /* keep alphanumeric and safe characters */
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            /* percent-encode everything else */
            escaped << '%' << std::setw(2) << int((unsigned char)c);
        }
    }

    return escaped.str();
}

std::string url_decode(const std::string &str) {
    std::string result;
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::istringstream is(str.substr(i + 1, 2));
            if (is >> std::hex >> value) {
                result += static_cast<char>(value);
                i += 2;
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }

    return result;
}

/* Grab host and port for resolution */
URL parse_url(const std::string &url)
{
	URL out;

    size_t scheme_end = url.find("://");
    size_t host_start = (scheme_end == std::string::npos) ? 0 : scheme_end + 3;

    size_t path_start = url.find('/', host_start);
    if (path_start == std::string::npos) {
        path_start = url.size();
    }

    std::string hostport = url.substr(host_start, path_start - host_start);

    size_t colon = hostport.find(':');
    if (colon != std::string::npos) {
        out.host = hostport.substr(0, colon);
        out.port = hostport.substr(colon + 1);
    } else {
        out.host = hostport;
    }

    return out;
}

std::unordered_map<std::string, std::string> parse_query_params(const std::string &query) {
    std::unordered_map<std::string, std::string> params;
    std::istringstream iss(query);
    std::string pair;

    while (std::getline(iss, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = pair.substr(0, eq_pos);
            std::string value = url_decode(pair.substr(eq_pos + 1));
            params[key] = value;
        }
    }

    return params;
}

/*
 * Generate random peer_id (20 bytes)
 * Format: -PC0001-XXXXXXXXXXXX (per convention)
 */
std::string generate_peer_id() {
    std::string peer_id = "-PC0001-";  /* Client ID: PC version 0.0.1 */
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::mt19937 rng(std::time(nullptr));
    std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);

    for (int i = 0; i < 12; ++i) {
        peer_id += charset[dist(rng)];

    }

    return peer_id;
}

std::string build_announce_request(const std::string &announce_url,
                                   const std::string &info_hash,
                                   const std::string &peer_id,
                                   uint16_t port,
                                   int64_t uploaded,
                                   int64_t downloaded,
                                   int64_t left,
                                   const std::string &event) {
    std::ostringstream url;
    url << announce_url;

    if (announce_url.find('?') == std::string::npos) {
        url << "?";
    } else {
        url << "&";
    }

    url << "info_hash=" << url_encode(info_hash);
    url << "&peer_id=" << url_encode(peer_id);
    url << "&port=" << port;
    url << "&uploaded=" << uploaded;
    url << "&downloaded=" << downloaded;
    url << "&left=" << left;

    if (!event.empty()) {
        url << "&event=" << event;
    }

    return url.str();
}
