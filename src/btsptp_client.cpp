#include <bencode.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <torrent_metadata.hpp>
#include <torrent_state.hpp>
#include <iostream>
#include <utils.hpp>
#include <peer_info.hpp>
#include <peer_connection.hpp>
#include <chrono>

using namespace std;

struct tracker_resp {
	vector<PeerInfo> peer_list;
	long long interval;
};

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

tracker_resp announce_to_tracker(boost::asio::io_context &io, string announce_url,
								 string announce_request)
{
	vector<PeerInfo> peer_list;

	boost::asio::ip::tcp::socket tracker_socket(io);
	boost::asio::ip::tcp::resolver resolver(io);
	URL url = parse_url(announce_url);
	
	auto endpoints = resolver.resolve(url.host, url.port);
	boost::asio::connect(tracker_socket, endpoints);

	string get_request = build_http_get_request(announce_request, url.host, url.port);

	/* Send request */
	boost::asio::write(tracker_socket, boost::asio::buffer(get_request));

	/* Read until end of headers */
	boost::asio::streambuf response_buf;
	boost::system::error_code ec;
	boost::asio::read_until(tracker_socket, response_buf, "\r\n\r\n", ec);
	if (ec) {
		throw boost::system::system_error(ec);
	}

	std::istream response_stream(&response_buf);
	std::string http_version;
	unsigned int status_code;
	std::string status_message;
	
	response_stream >> http_version >> status_code;
	std::getline(response_stream, status_message);
	
	/* Read headers and find Content-Length */
	size_t content_length = 0;
	std::string header_line;
	while (std::getline(response_stream, header_line) && header_line != "\r") {
		if (header_line.find("Content-Length: ") == 0) {
			content_length = std::stoul(header_line.substr(16));
		}
	}

	/*
	 * At this point, response_buf still contains any body data that came with headers
	 * Calculate how much body we already have
	 */
	size_t body_in_buffer = response_buf.size();
	std::cout << "Body already in buffer: " << body_in_buffer << std::endl;
	
	/* Read remaining body if needed */
	if (body_in_buffer < content_length) {
		size_t remaining = content_length - body_in_buffer;
		std::cout << "Reading remaining: " << remaining << " bytes" << std::endl;
		
		boost::asio::read(tracker_socket, response_buf,
						  boost::asio::transfer_exactly(remaining), ec);
		if (ec) {
			throw boost::system::system_error(ec);
		}
	}
	
    string body {
		istreambuf_iterator<char>(&response_buf),
		istreambuf_iterator<char>()
	};
	
	std::cout << "Final body size: " << body.size() << std::endl;

	/* Parse bencoded response */
	auto data = bencode::decode(body);
	auto root_dict = get<bencode::dict>(data);

	auto interval = 30;
	if (root_dict.count("interval")) {
		interval = get<bencode::integer>(root_dict["interval"]);
	} else {
		throw std::runtime_error("No interval provided from tracker");
	}

	if (!root_dict.count("peers")) {
		throw std::runtime_error("No peers list provided from tracker");
	}
	
	vector<bencode::data> peer_data = get<bencode::list>(root_dict["peers"]);
	for (auto &one_data : peer_data) {
		auto peer_dict = get<bencode::dict>(one_data);
		auto peer_id = get<bencode::string>(peer_dict["peer id"]);
		auto peer_ip = get<bencode::string>(peer_dict["ip"]);
		auto peer_port = get<bencode::integer>(peer_dict["port"]);
		PeerInfo peer(peer_id, peer_ip, peer_port);
		peer_list.push_back(peer);
	}

	tracker_resp resp;
	resp.interval = interval;
	resp.peer_list = peer_list;

	return resp;
}


int main(int argc, char *argv[])
{
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

		TorrentState state(torrent, torrent.file_name);
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
	            state.bytes_left(),    /* left */
	            "started"               /* event */
	        );
	
	        cout << "=== announce request ===" << endl;
	        cout << announce_request << endl;
	
			tracker_resp resp = announce_to_tracker(io, torrent.announce_url, announce_request);
			cout << "INTERVAL " << resp.interval << endl;
			for (PeerInfo peer : resp.peer_list) {
				cout << "peer id: " << peer.peer_id << endl;
			}
		vector<thread> peer_threads;

		thread acceptor_thread([&]() {
			cout << "acceptor thread started" << endl;
			while(1) {
				try {
					boost::asio::ip::tcp::socket sock(io);
					acceptor.accept(sock);
	
					peer_threads.push_back(thread([&, sock = std::move(sock)]() mutable {
	                    try {
	                        PeerConnection conn(io, PeerInfo("", "", 0), state, peer_id, torrent.info_hash);
	                        conn.start_with_socket(std::move(sock));
	                        conn.receive_handshake();  /* response */
	                        conn.run();
	                    } catch (const exception& e) {
	                        cerr << "Incoming peer error: " << e.what() << endl;
	                    }
	                }));
	            } catch (const exception& e) {
	                cerr << "Acceptor error: " << e.what() << endl;
	            }
			}
		});
		acceptor_thread.detach();

		if (!state.is_file_complete()) {
			cout << "=== connecting to peers ===" << endl;

        	for (const PeerInfo& peer : resp.peer_list) {
	            peer_threads.push_back(thread([&, peer]() {
	                try {
	                    cout << "connecting to peer: " << peer.ip << ":" << peer.port << endl;
	
	                    PeerConnection conn(io, peer, state, peer_id, torrent.info_hash);
	                    conn.connect();
	                    conn.send_handshake(); /* initate */
	                    conn.run();
	                } catch (const exception& e) {
	                    cerr << "outgoing peer error (" << peer.ip << "): " << e.what() << endl;
	                }
	            }));
        	}
		} else {
			cout << "file already complete, seeding only now" << endl;
		}

		if (!state.is_file_complete()) {
			cout << "=== Downloading ===" << endl;
			
			while (!state.is_file_complete()) {
				this_thread::sleep_for(chrono::seconds(5));
				
				size_t left = state.bytes_left();
				cout << "progress: " << left << " bytes remaining" << endl;
			}
			
			cout << "\n=== download complete! ===\n" << endl;
			
			/* Announce completion */
			string complete_request = build_announce_request(
				torrent.announce_url,
				torrent.info_hash,
				peer_id,
				our_port,
				0,
				0,
				0,
				"completed"
			);
			
			announce_to_tracker(io, torrent.announce_url, complete_request);
		}

		cout << "=== Seeding ===" << endl;
		cout << "Press Ctrl+C to exit" << endl;
		
		while (true) {
			this_thread::sleep_for(chrono::seconds(resp.interval));
			
			/* reannounce */
			string seeding_request = build_announce_request(
				torrent.announce_url,
				torrent.info_hash,
				peer_id,
				our_port,
				0,
				0,
				0,
				""
			);
			
			try {
				announce_to_tracker(io, torrent.announce_url, seeding_request);
				cout << "Re-announced to tracker" << endl;
			} catch (const exception& e) {
				cerr << "Re-announce failed: " << e.what() << endl;
			}
		}
    } catch (const exception& e) {
        cerr << "error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
