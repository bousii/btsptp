#include <peer_connection.hpp>
#include <boost/asio.hpp>
#include <boost/endian/conversion.hpp>
#include <iostream>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <utils.hpp>

#define HANDSHAKE_SIZE 68
#define PROTOCOL_VERSION 19
#define BTSPTP_PROTOCOL "BitTorrent protocol"
PeerConnection::PeerConnection(boost::asio::io_context& io,
        					   const PeerInfo& peer,
							   TorrentState& state,
							   const std::string& our_id,
							   const std::string& hash)
	: socket(io),
	  peer_info(peer),
	  torrent_state(state),
	  our_peer_id(our_id),
	  info_hash(hash),
	  am_choking(true),
	  am_interested(false),
	  peer_choking(true),
	  peer_interested(false),
	  current_piece_index(-1)
{
	peer_bitfield.resize(torrent_state.get_total_pieces(), false);
}

std::vector<uint8_t> PeerConnection::build_handshake()
{
	std::vector<uint8_t> handshake;
	handshake.reserve(HANDSHAKE_SIZE); /* handshake is 68 bytes long */
	handshake.push_back(PROTOCOL_VERSION);
	
	std::string protocol = BTSPTP_PROTOCOL;
	handshake.insert(handshake.end(), protocol.begin(), protocol.end());

	for (int i = 0; i < 8; i++) {
		handshake.push_back(0);
	}
	handshake.insert(handshake.end(), info_hash.begin(), info_hash.end());
	handshake.insert(handshake.end(), our_peer_id.begin(), our_peer_id.end());
	return handshake;
}

void PeerConnection::validate_handshake(const std::vector<uint8_t> &response)
{
    if (response[0] != PROTOCOL_VERSION) {
        throw std::runtime_error("Invalid protocol version");
    }

    std::string their_protocol(response.begin() + 1, response.begin() + 20);
    if (their_protocol != BTSPTP_PROTOCOL) {
        throw std::runtime_error("Invalid protocol string");
    }

    std::string their_info_hash(response.begin() + 28, response.begin() + 48);
    if (their_info_hash != info_hash) {
        throw std::runtime_error("Info hash mismatch - different torrent");
    }
}

void PeerConnection::connect() {
    try {
        boost::asio::ip::tcp::endpoint endpoint(
            boost::asio::ip::make_address(peer_info.ip),
            peer_info.port
        );

        socket.connect(endpoint);

        std::cout << "Connected to peer " << peer_info.ip << ":" 
                  << peer_info.port << std::endl;

    } catch (const boost::system::system_error& e) {
        std::cerr << "Failed to connect to " << peer_info.ip << ":" 
                  << peer_info.port << " - " << e.what() << std::endl;
        throw;
    }
}

void PeerConnection::send_handshake()
{
	auto handshake = build_handshake();
    boost::asio::write(socket, boost::asio::buffer(handshake));
    std::cout << "Sent handshake to peer" << std::endl;

    std::vector<uint8_t> response(HANDSHAKE_SIZE);
    boost::asio::read(socket, boost::asio::buffer(response));
    std::cout << "Received handshake from peer" << std::endl;
	validate_handshake(response);
    std::cout << "Handshake successful with peer" << std::endl;
}

void PeerConnection::receive_handshake()
{
    std::vector<uint8_t> response(HANDSHAKE_SIZE);
    boost::asio::read(socket, boost::asio::buffer(response));
	validate_handshake(response);
    std::cout << "Received handshake from peer" << std::endl;
	std::vector<uint8_t> handshake = build_handshake();

    boost::asio::write(socket, boost::asio::buffer(handshake));
    std::cout << "Sent handshake to peer" << std::endl;
    std::cout << "Handshake successful with peer" << std::endl;
}

void PeerConnection::start_with_socket(boost::asio::ip::tcp::socket sock)
{
    socket = std::move(sock);
    std::cout << "Accepted connection from peer" << std::endl;
}

void PeerConnection::handle_message(uint8_t msg_id, const std::string& payload)
{
	switch (msg_id) {
		case MSG_CHOKE:
			handle_choke();
			break;
		case MSG_UNCHOKE:
			handle_unchoke();
			break;
		case MSG_INTERESTED:
			handle_interested();
			break;
		case MSG_NOT_INTERESTED:
			handle_not_interested();
			break;
		case MSG_HAVE:
			handle_have(payload);
			break;
		case MSG_BITFIELD:
			handle_bitfield(payload);
			break;
		case MSG_REQUEST:
			handle_request(payload);
			break;
		case MSG_PIECE:
			handle_piece(payload);
			break;
		case MSG_CANCEL:
			std::cout << "received cancel, ignoring..." << std::endl;
			break;
		default:
			std::cerr << "Unknown message ID: " << (int)msg_id << std::endl;
			break;
	}
}

void PeerConnection::send_message(uint8_t msg_id, const std::string& payload)
{
	uint32_t len = payload.size() + 1;

	uint32_t lenbe = boost::endian::native_to_big(len); /* Big endian so fun... :( */

	std::vector<uint8_t> message;
	message.reserve(sizeof(len) + len);

	uint8_t *lenbe_bytes = reinterpret_cast<uint8_t*>(&lenbe);
	message.insert(message.end(), lenbe_bytes, lenbe_bytes + sizeof(lenbe));

	message.push_back(msg_id);
	message.insert(message.end(), payload.begin(), payload.end());

	boost::asio::write(socket, boost::asio::buffer(message));
}

void PeerConnection::handle_choke()
{
	peer_choking = true;
}

void PeerConnection::handle_unchoke()
{
	peer_choking = false;
	download_next_piece();
}

void PeerConnection::handle_interested()
{
	peer_interested = true;
	send_unchoke();
}
void PeerConnection::handle_not_interested()
{
	peer_interested = false;
}

void PeerConnection::handle_have(const std::string& payload)
{
	if (payload.size() != sizeof(uint32_t)) {
		std::cerr << "invalid HAVE payload size" << std::endl;
		return;
	}

	uint32_t index_be;
	std::memcpy(&index_be, payload.data(), sizeof(uint32_t));
	uint32_t piece_index = boost::endian::big_to_native(index_be); /* Big endian so fun... :( */
	if (piece_index < peer_bitfield.size()) {
		peer_bitfield[piece_index] = true;
		if (!torrent_state.have_piece(piece_index) && !am_interested) {
			send_interested();
		}
	}
}

void PeerConnection::handle_bitfield(const std::string& payload)
{
	std::vector<bool> potential_bitfield = unpack_bitfield(payload, peer_bitfield.size());
	peer_bitfield = potential_bitfield;

	for (size_t i = 0; i < peer_bitfield.size(); i++) {
		if (peer_bitfield[i] && !torrent_state.have_piece(i)) {
			send_interested();
			break;
		}
	}
}

void PeerConnection::handle_request(const std::string& payload)
{
	if (payload.size() != 12) {
		std::cerr << "invalid Request payload size" << std::endl;
		return;
	}

	uint32_t index_be, begin_be, length_be;
	std::memcpy(&index_be, payload.data(), sizeof(uint32_t));
	std::memcpy(&begin_be, payload.data() + sizeof(uint32_t), sizeof(uint32_t));
	std::memcpy(&length_be, payload.data() + sizeof(uint32_t) * 2, sizeof(uint32_t));

	uint32_t index = boost::endian::big_to_native(index_be); /* Big endian so fun... :( */
	uint32_t begin = boost::endian::big_to_native(begin_be);
	uint32_t length = boost::endian::big_to_native(length_be);

	if (am_choking || !torrent_state.have_piece(index)) return;
	std::string full_piece = torrent_state.read_piece(index);
	if (begin + length > full_piece.size()) {
		std::cerr << "Request out of bounds" << std::endl;
		return;
	}

	std::string piece = full_piece.substr(begin, length);
	send_piece(index, begin, piece);

}
void PeerConnection::handle_piece(const std::string& payload)
{
	if (payload.size() < 8) {
        std::cerr << "Invalid PIECE payload size" << std::endl;
        return;
    }

    uint32_t index_be, begin_be;
    std::memcpy(&index_be, payload.data(), sizeof(uint32_t));
    std::memcpy(&begin_be, payload.data() + sizeof(uint32_t), sizeof(uint32_t));

	uint32_t index = boost::endian::big_to_native(index_be); /* Big endian so fun... :( */
	uint32_t begin = boost::endian::big_to_native(begin_be);

    std::string block_data = payload.substr(sizeof(uint32_t) * 2);

    std::cout << "Received piece " << index
              << " offset " << begin
              << " size " << block_data.size() << std::endl;

    /*
	 * For simplicity: assume we request full pieces, not blocks.
     * So begin should be 0 and block_data should be full piece.
	 */

    if (begin != 0) {
        std::cerr << "Warning: received partial piece (not implemented)" << std::endl;
        return;
    }

    if (!torrent_state.verify_piece(index, block_data)) {
        std::cerr << "Piece " << index << " failed verification!" << std::endl;
		/* Could re-request */
        return;
    }

    torrent_state.write_piece(index, block_data);
    torrent_state.set_complete(index);

    std::cout << "Piece " << index << " complete and verified!" << std::endl;

    send_have(index);

    current_piece_index = -1;
    download_next_piece();
}

void PeerConnection::send_interested()
{
	send_message(MSG_INTERESTED, "");
	am_interested = true;
}
void PeerConnection::send_choke()
{
	send_message(MSG_CHOKE, "");
	am_choking = true;
}

void PeerConnection::send_unchoke()
{
	send_message(MSG_UNCHOKE, "");
	am_choking = false;

}
void PeerConnection::send_not_interested()
{
	send_message(MSG_NOT_INTERESTED, "");
	am_interested = false;

}
void PeerConnection::send_bitfield()
{
	std::vector<bool> our_bitfield = torrent_state.get_bitfield();
	std::string packed = pack_bitfield(our_bitfield);
	send_message(MSG_BITFIELD, packed);
}
void PeerConnection::send_request(int index, int begin, int length)
{
	uint32_t index_be = boost::endian::native_to_big(static_cast<uint32_t>(index));
    uint32_t begin_be = boost::endian::native_to_big(static_cast<uint32_t>(begin));
    uint32_t length_be = boost::endian::native_to_big(static_cast<uint32_t>(length));

    std::string payload;
    payload.append(reinterpret_cast<char*>(&index_be), sizeof(index_be));
    payload.append(reinterpret_cast<char*>(&begin_be), sizeof(begin_be));
    payload.append(reinterpret_cast<char*>(&length_be), sizeof(length_be));

    send_message(MSG_REQUEST, payload);
}

void PeerConnection::send_piece(int index, int begin, const std::string& data)
{
	uint32_t index_be = boost::endian::native_to_big(static_cast<uint32_t>(index));
    uint32_t begin_be = boost::endian::native_to_big(static_cast<uint32_t>(begin));

    std::string payload;
    payload.append(reinterpret_cast<char*>(&index_be), sizeof(index_be));
    payload.append(reinterpret_cast<char*>(&begin_be), sizeof(begin_be));
    payload.append(data);

    send_message(MSG_PIECE, payload);
}

void PeerConnection::send_have(int index)
{
	uint32_t index_be = boost::endian::native_to_big(static_cast<uint32_t>(index));
    std::string payload(reinterpret_cast<char*>(&index_be), sizeof(index_be));

    send_message(MSG_HAVE, payload);
}

bool PeerConnection::peer_has_piece(int index) const
{
	if (index < 0 || index >= static_cast<int>(peer_bitfield.size())) {
		return false;
	}

	return peer_bitfield[index];
}

void PeerConnection::download_next_piece() {
    if (peer_choking || current_piece_index != -1) {
        return;
    }

    if (torrent_state.is_file_complete()) {
        std::cout << "Download complete!" << std::endl;
        return;
    }

    int piece_index = torrent_state.get_next_piece_to_download();

    if (piece_index == -1) {
        return;
    }

    if (!peer_has_piece(piece_index)) {
        return;
    }

    torrent_state.set_in_progress(piece_index);
    current_piece_index = piece_index;

    size_t piece_length = torrent_state.get_piece_length();
    if (piece_index == static_cast<int>(torrent_state.get_total_pieces() - 1)) {
        size_t total_size = torrent_state.get_metadata().file_length;
        size_t last_piece_size = total_size - (piece_index * piece_length);
        piece_length = last_piece_size;
    }

    std::cout << "Requesting piece " << piece_index 
              << " length " << piece_length << std::endl;
    send_request(piece_index, 0, piece_length);
}

void PeerConnection::run() {
    try {
        /* After handshake, exchange bitfields */
        send_bitfield();

        while (true) {
            if (torrent_state.is_file_complete()) {
                std::cout << "File complete, continuing to seed..." << std::endl;
                /* break here if you don't want to seed */
			}
            uint32_t length_be;
            boost::asio::read(socket, boost::asio::buffer(&length_be, sizeof(uint32_t)));
            uint32_t length = boost::endian::big_to_native(length_be);

            /* Handle keep-alive (length = 0) */
            if (length == 0) {
                std::cout << "Received keep-alive" << std::endl;
                continue;
            }

            uint8_t msg_id;
            boost::asio::read(socket, boost::asio::buffer(&msg_id, 1));

            std::string payload;
            if (length > 1) {
                payload.resize(length - 1);
                boost::asio::read(socket, boost::asio::buffer(&payload[0], length - 1));
            }

            handle_message(msg_id, payload);
        }

    } catch (const boost::system::system_error& e) {
        std::cerr << "Connection error with peer: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error in peer connection: " << e.what() << std::endl;
    }
    std::cout << "Peer connection ended" << std::endl;
}

