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
#include <atomic>
#include <csignal>

using namespace std;

atomic<bool> should_exit(false);

void signal_handler(int signal) {
    cout << "\nReceived signal " << signal << ", shutting down gracefully..." << endl;
    should_exit = true;
}

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
	
	/* Read remaining body if needed */
	if (body_in_buffer < content_length) {
		size_t remaining = content_length - body_in_buffer;
		
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

void run_acceptor(boost::asio::io_context& io,
                  boost::asio::ip::tcp::acceptor& acceptor,
                  TorrentState& state,
                  const string& peer_id,
                  const string& info_hash)
{
    cout << "acceptor thread started" << endl;

    while (!should_exit) {
        try {
            boost::asio::ip::tcp::socket sock(io);

            acceptor.non_blocking(true);
            boost::system::error_code ec;
            acceptor.accept(sock, ec);
            if (ec == boost::asio::error::would_block) {
                this_thread::sleep_for(chrono::milliseconds(100));
                continue;
            }

            if (ec) {
                throw boost::system::system_error(ec);
            }

            cout << "accepted incoming connection from "
                 << sock.remote_endpoint() << endl;

            // Detach incoming peer threads immediately
            thread([&, sock = std::move(sock)]() mutable {
                try {
                    PeerConnection conn(io, PeerInfo("", "", 0), state, peer_id, info_hash);
                    conn.start_with_socket(std::move(sock));
                    conn.receive_handshake();
                    conn.run();
                } catch (const exception& e) {
                    cerr << "incoming peer error: " << e.what() << endl;
                }
            }).detach();

        } catch (const exception& e) {
            if (!should_exit) {
                cerr << "acceptor error: " << e.what() << endl;
            }
        }
    }

    cout << "acceptor thread exiting" << endl;
}

void connect_to_peer(boost::asio::io_context& io,
                     const PeerInfo& peer,
                     TorrentState& state,
                     const string& peer_id,
                     const string& info_hash)
{
    try {
        cout << "connecting to peer: " << peer.ip << ":" << peer.port << endl;

        PeerConnection conn(io, peer, state, peer_id, info_hash);
        conn.connect();
        conn.send_handshake();
        conn.run();

    } catch (const exception& e) {
        cerr << "peer error (" << peer.ip << ":" << peer.port << "): "
             << e.what() << endl;
    }
}

void monitor_download_progress(TorrentState& state)
{
    cout << "\n=== downloading ===" << endl;

    while (!state.is_file_complete() && !should_exit) {
        this_thread::sleep_for(chrono::seconds(5));

        size_t left = state.bytes_left();
        float progress = 100.0f * (1.0f - (float)left / state.get_metadata().file_length);

        cout << "progress: " << (int)progress << "% ("
             << left << " bytes remaining)" << endl;
    }

    if (state.is_file_complete()) {
        cout << "\n=== download complete! ===\n" << endl;
    }
}

void seed_and_reannounce(boost::asio::io_context& io,
                        const TorrentMetadata& torrent,
                        const string& peer_id,
                        uint16_t our_port,
                        int interval)
{
    cout << "=== seeding ===" << endl;
    cout << "press Ctrl+C to exit\n" << endl;

    while (!should_exit) {
        this_thread::sleep_for(chrono::seconds(interval));

        if (should_exit) break;

        string seeding_request = build_announce_request(
            torrent.announce_url,
            torrent.info_hash,
            peer_id,
            our_port,
            0, 0, 0, ""
        );

        try {
            announce_to_tracker(io, torrent.announce_url, seeding_request);
            cout << "re-announced to tracker" << endl;
        } catch (const exception& e) {
            cerr << "re-announce failed: " << e.what() << endl;
        }
    }

    cout << "seeding thread exiting" << endl;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (argc > 3) {
        cout << "usage: " << argv[0] << " <torrent_file>" << " <optional_listening_port> " << endl;
        return 1;
    }

	int acceptor_port = 0;
	if (argc == 3) {
		acceptor_port = atoi(argv[2]);
	}

    string filename(argv[1]);
    if (!boost::algorithm::ends_with(filename, ".torrent")) {
        cerr << "error: filename must end in .torrent" << endl;
        return 1;
    }

    try {
        boost::asio::io_context io;

        TorrentMetadata torrent(filename);
        torrent.print_info();

        TorrentState state(torrent, torrent.file_name);

        string peer_id = generate_peer_id();
        cout << "Our peer ID: " << peer_id << endl;

        boost::asio::ip::tcp::acceptor acceptor(
            io,
            boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), acceptor_port)
        );
        uint16_t our_port = acceptor.local_endpoint().port();
        cout << "listening on port: " << our_port << endl;

        string announce_request = build_announce_request(
            torrent.announce_url,
            torrent.info_hash,
            peer_id,
            our_port,
            0,
            0,
            state.bytes_left(),
            "started"
        );

        cout << "=== announcing to tracker ===" << endl;
        tracker_resp resp = announce_to_tracker(io, torrent.announce_url, announce_request);

        cout << "tracker responded with " << resp.peer_list.size() << " peers" << endl;
        cout << "interval: " << resp.interval << " seconds\n" << endl;

        thread acceptor_thread(run_acceptor,
                              ref(io),
                              ref(acceptor),
                              ref(state),
                              ref(peer_id),
                              ref(torrent.info_hash));

        if (!state.is_file_complete()) {
            cout << "=== connecting to peers ===" << endl;

            for (const PeerInfo& peer : resp.peer_list) {
                // Detach outgoing peer threads immediately
                thread(connect_to_peer,
                       ref(io),
                       peer,
                       ref(state),
                       ref(peer_id),
                       ref(torrent.info_hash)).detach();
            }
        } else {
            cout << "file already complete - seeding only\n" << endl;
        }

        if (!state.is_file_complete()) {
            monitor_download_progress(state);

            if (state.is_file_complete()) {
                string complete_request = build_announce_request(
                    torrent.announce_url,
                    torrent.info_hash,
                    peer_id,
                    our_port,
                    0, 0, 0,
                    "completed"
                );
                announce_to_tracker(io, torrent.announce_url, complete_request);
            }
        }

        seed_and_reannounce(io, torrent, peer_id, our_port, resp.interval);

        cout << "\nShutting down..." << endl;

        try {
            string stopped_request = build_announce_request(
                torrent.announce_url,
                torrent.info_hash,
                peer_id,
                our_port,
                0, 0, 0,
                "stopped"
            );
            announce_to_tracker(io, torrent.announce_url, stopped_request);
            cout << "notified tracker of shutdown" << endl;
        } catch (const exception& e) {
            cerr << "failed to notify tracker: " << e.what() << endl;
        }

        if (acceptor_thread.joinable()) {
            acceptor_thread.join();
        }

        cout << "shutdown complete. exiting." << endl;

    } catch (const exception& e) {
        cerr << "error: " << e.what() << endl;
        should_exit = true;
        return 1;
    }

    return 0;
}
