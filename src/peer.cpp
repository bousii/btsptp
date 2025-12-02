#include <peer.hpp>

Peer::Peer(std::string id, std::string ip, uint16_t port)
	: peer_id(id), ip(ip), port(port) {}
