#ifndef TRACKER_HPP
#define TRACKER_HPP

#include <unordered_map>
#include <vector>
#include <peer_info.hpp>

#define ANNOUNCE_INTERVAL 30
#define PEER_TIMEOUT 120

class Tracker {
private:
	std::unordered_map<std::string, std::vector<PeerInfo>> torrents;
	int announce_interval;
	int peer_timeout;

public:
	Tracker(int interval = ANNOUNCE_INTERVAL, int timeout = PEER_TIMEOUT);

	std::string handle_announce(const std::string &info_hash,
								const std::string &peer_id,
								const std::string &ip,
								uint16_t port,
								const std::string &event);
	std::string generate_response(const std::string &info_hash, const std::string &caller_id);
	void cleanup_inactive_peers();
	void update_peer(const std::string &info_hash, const PeerInfo &peer);
	int get_peer_count(const std::string &info_hash) const;
};

#endif /* tracker.hpp */
