#ifndef PEER_INFO_H
#define PEER_INFO_H

#include <string>
#include <cstdint>
#include <ctime>

class PeerInfo {
public:
	std::string peer_id;
	std::string ip;
	uint16_t port;
	std::string status;
	time_t last_announce;

	PeerInfo(std::string id, std::string ip, uint16_t port);
};

#endif /* peer_info.hpp */
