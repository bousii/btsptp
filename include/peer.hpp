#ifndef PEER_H
#define PEER_H

#include <string>
#include <cstdint>
#include <ctime>

class Peer {
public:
	std::string peer_id;
	std::string ip;
	uint16_t port;
	std::string status;
	time_t last_announce;

	Peer(std::string id, std::string ip, uint16_t port);
};

#endif /* peer.hpp */
