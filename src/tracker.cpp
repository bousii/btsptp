#include <bencode.hpp>
#include <tracker.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <fstream>
#include <iostream>
#include <utils.hpp>

using namespace std;
using boost::asio::ip::tcp;

struct HttpRequest {
    string method;
    string path;
    string query;
    string http_version;
};

#define TRACKER_PORT 8080

void send_response(tcp::socket &socket, const string &data)
{
	string http_header = "HTTP/1.1 200 OK \r\n";
	http_header += "Content-Type: text/plain\r\n";
	http_header += "Content-Length: " + to_string(data.size()) + "\r\n";
	http_header += "\r\n";

	boost::asio::write(socket, boost::asio::buffer(http_header));
	boost::asio::write(socket, boost::asio::buffer(data));
}

HttpRequest parse_http_request_line(const std::string& request_line) {
    std::istringstream line_stream(request_line);
    HttpRequest req;
    line_stream >> req.method >> req.path >> req.http_version;
    auto qmark_pos = req.path.find('?');
    if (qmark_pos != std::string::npos)
        req.query = req.path.substr(qmark_pos + 1);
    return req;
}

string url_decode(const std::string& str)
{
    string result;
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            istringstream iss(str.substr(i + 1, 2));
            if (iss >> hex >> value) {
                result += static_cast<char>(value);
                i += 2;
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

unordered_map<std::string, std::string> parse_query(const std::string& query)
{
    unordered_map<std::string, std::string> params;
    istringstream ss(query);
    string pair;
    while (std::getline(ss, pair, '&')) {
        auto eq_pos = pair.find('=');
        if (eq_pos != string::npos) {
            string key = pair.substr(0, eq_pos);
            string value = url_decode(pair.substr(eq_pos + 1));
            params[key] = value;
        }
    }
    return params;
}

void handle_client(tcp::socket &socket, Tracker &tracker)
{
	try {
	
		boost::asio::streambuf buffer;
		boost::asio::read_until(socket, buffer, "\r\n\r\n"); /* End of HTTP header */
	
		istream request_stream(&buffer);
		string request_line;
		getline(request_stream, request_line);

		if (request_line.back() == '\r') {
			request_line.pop_back();
		}

		HttpRequest request = parse_http_request_line(request_line);

		if (request.method != "GET") {
			std::cerr << "Bad request: not GET" << endl;
			return;
		}

        auto qmark_pos = request.path.find('?');
        std::string query_str;
        if (qmark_pos != std::string::npos) {
            query_str = request.path.substr(qmark_pos + 1);
        }
 		auto params = parse_query(query_str);

        /* Extract required components safely */
        std::string info_hash, peer_id, event = "";
        uint16_t port = 0;
        int64_t uploaded = 0, downloaded = 0, left = 0;

        try {
            if (params.count("info_hash") != 1 || params["info_hash"].size() != 20) throw std::runtime_error("Invalid info_hash");
            info_hash = params["info_hash"];

            if (params.count("peer_id") != 1 || params["peer_id"].size() != 20) throw std::runtime_error("Invalid peer_id");
            peer_id = params["peer_id"];

            if (params.count("port") != 1) throw std::runtime_error("Missing port");
            port = static_cast<uint16_t>(std::stoi(params["port"]));

            if (params.count("uploaded")) uploaded = std::stoll(params["uploaded"]);
            if (params.count("downloaded")) downloaded = std::stoll(params["downloaded"]);
            if (params.count("left")) left = std::stoll(params["left"]);
            if (params.count("event")) event = params["event"];
        } catch (const std::exception& e) {
            std::cerr << "Bad announce request: " << e.what() << "\n";
            return;
        }

        std::cout << "Peer connected: info_hash=" << info_hash
                  << " peer_id=" << peer_id
                  << " port=" << port
                  << " uploaded=" << uploaded
                  << " downloaded=" << downloaded
                  << " left=" << left
                  << " event=" << event
                  << "\n";

		auto ip = socket.remote_endpoint().address().to_string();
		string response = tracker.handle_announce(info_hash, peer_id, ip, port, event);
		send_response(socket, response);

	} catch (const exception &e) {
		std::cerr << "exception handling client: " << e.what() << endl;
	}
}

int main()
{
	try {
		boost::asio::io_context io;
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), TRACKER_PORT));

		cout << "tracker listening on port " << TRACKER_PORT << endl;
		Tracker tracker;
		while(1) {
			tcp::socket socket(io);
			acceptor.accept(socket);
			auto remote_endpoint = socket.remote_endpoint();
			string client_ip = remote_endpoint.address().to_string();
			cout << "client connected: " << remote_endpoint <<  endl;
			handle_client(socket, tracker);
		}
	} catch (std::exception& e) {
	    cerr << "Error: " << e.what() << endl;
	}

	return 0;
}
