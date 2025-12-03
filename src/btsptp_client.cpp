#include <bencode.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <torrent_metadata.hpp>
#include <iostream>
#include <utils.hpp>
#include <peer.hpp>

using namespace std;


std::string parse_http_response_body(const std::string &http_response)
{
	boost::asio::streambuf buffer;
	boost::asio::read_until(socket, buffer, "\r\n\r\n"); /* End of HTTP header */

	istream request_stream(&buffer);
	string request_line;
	getline(request_stream, request_line);

	if (request_line.back() == '\r') {
		request_line.pop_back();
	}

}

std::string build_http_get_request(const std::string &announce_request,
								   const std::string &host,
								   const std::string &port)
{
	std::ostringstream req;
    req << "GET " << announce_request << " HTTP/1.1\r\n";
    req << "Host: " << host;
    if (port != "80") req << ":" << port;
    req << "\r\n";
    req << "User-Agent: MyClient/1.0\r\n";
    req << "Connection: close\r\n";
    req << "\r\n";

	return req.str();
}
vector<Peer> announce_to_tracker(boost::asio::io_context &io, string announce_url,
								 string announce_request)
{
	vector<Peer> peer_list;

	boost::asio::ip::tcp::socket tracker_socket(io);
	boost::asio::ip::tcp::resolver resolver(io);
	URL url = parse_url(announce_url);
	
	auto endpoints = resolver.resolve(url.host, url.port);
	boost::asio::connect(tracker_socket, endpoints);

	string get_request = build_http_get_request(announce_request, url.host, url.port);


	boost::asio::write(socket, boost::asio::buffer(get_request));

    boost::asio::streambuf response_buf;
    boost::system::error_code ec;
    boost::asio::read(socket, response_buf, boost::asio::transfer_all(), ec);
    if (ec && ec != boost::asio::error::eof) {
        throw boost::system::system_error(ec);
    }

    std::string response(
        boost::asio::buffers_begin(response_buf.data()),
        boost::asio::buffers_end(response_buf.data())
    );

    // Strip headers
    auto header_end = response.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        throw std::runtime_error("Invalid HTTP response: missing header-body separator");
    }
    std::string body = response.substr(header_end + 4);
	auto data = bencode::decode(body);

	auto root_dict = get<bencode::dict>(data);

	if (root_dict.count("interval")) {
		
	}

	return peer_list;
}
int main(int argc, char *argv[]) {
    if (argc < 2) {
        cout << "too few arguments for this program" << endl;
        return 1;
    }

    if (argc > 2) {
        cout << "too many arguments for this program" << endl;
        return 1;
    }

    string filename(argv[1]);
    if (!boost::algorithm::ends_with(filename, ".torrent")) {
        throw runtime_error("filename must end in .torrent");
    }

    try {

		boost::asio::io_context io;
		boost::asio::ip::tcp::acceptor acceptor(io,
    	boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));

        TorrentMetadata torrent(filename);
        torrent.print_info();

        string peer_id = generate_peer_id();
        cout << "our peer ID: " << peer_id << endl;

        uint16_t our_port = acceptor.local_endpoint().port();
        string announce_request = build_announce_request(
            torrent.announce_url,
            torrent.info_hash,
            peer_id,
            our_port,
            0,                      /* uploaded */
            0,                      /* downloaded */
            torrent.file_length,    /* left */
            "started"               /* event */
        );

        cout << "=== announce request ===" << endl;
        cout << announce_request << endl;

		/* Let OS assign random listening port */

		vector<Peer> peer_list = announce_to_tracker(io, torrent.announce_url, announce_request);


    } catch (const exception& e) {
        cerr << "error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
