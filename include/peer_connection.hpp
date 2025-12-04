#ifndef PEER_CONNECTION_HPP
#define PEER_CONNECTION_HPP

#include <boost/asio.hpp>
#include <string>
#include <vector>
#include <cstdint>
#include "peer_info.hpp"
#include "torrent_state.hpp"

class PeerConnection {
private:

	/* Connection */
    boost::asio::ip::tcp::socket socket;
    PeerInfo peer_info;
    TorrentState& torrent_state;

	/* Torrent Protocol info */
    std::string our_peer_id;
    std::string info_hash;

	/* Peer state information */
    std::vector<bool> peer_bitfield;
    bool am_choking;
    bool am_interested;
    bool peer_choking;
    bool peer_interested;

    /* Current download state */
    int current_piece_index;

    /* Private helper methods */
    void send_message(uint8_t msg_id, const std::string& payload);
    void send_bitfield();
    void send_interested();
    void send_not_interested();
    void send_choke();
    void send_unchoke();
    void send_request(int index, int begin, int length);
    void send_piece(int index, int begin, const std::string& data);
    void send_have(int index);

    void handle_message(uint8_t msg_id, const std::string& payload);
    void handle_choke();
    void handle_unchoke();
    void handle_interested();
    void handle_not_interested();
    void handle_have(const std::string& payload);
    void handle_bitfield(const std::string& payload);
    void handle_request(const std::string& payload);
    void handle_piece(const std::string& payload);

    void download_next_piece();
    bool peer_has_piece(int index) const;

public:
    PeerConnection(
        boost::asio::io_context& io,
        const PeerInfo& peer,
        TorrentState& state,
        const std::string& our_id,
        const std::string& hash
    );

    /* For outgoing connections (we initiate) */
    void connect();

    /* For incoming connections (they initiated, we have socket) */
    void start_with_socket(boost::asio::ip::tcp::socket socket);

    /* Protocol */
    void perform_handshake();

    /* Main message loop (run in thread) */
    void run();
};

#endif
