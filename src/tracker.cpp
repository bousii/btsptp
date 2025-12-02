#include <tracker.hpp>
#include <algorithm>

Tracker::Tracker(int interval, int timeout)
	: announce_interval(interval), peer_timeout(timeout) {}

std::string handle_announce(const std::string &info_hash,
							const std::string &peer_id,
							const std::string &ip,
							uint16_t port,
							const std::string &event)
{
	/* TODO: handle announce */
	return "";
}

std::string generate_response(const std::string &info_hash)
{
	/* TODO: generate response */

	return "";
}

void Tracker::cleanup_inactive_peers()
{
	time_t current_time = time(nullptr);
	for (auto it = torrents.begin(); it != torrents.end(); it++) {
		auto peer_list = it->second;
		peer_list.erase(std::remove_if(peer_list.begin(), peer_list.end(),
			[this, current_time](const Peer &peer) {
				return (current_time - peer.last_announce) > peer_timeout;
			}),
			peer_list.end()
		);
	}
}

void Tracker::update_peer(const std::string &info_hash, const Peer &peer)
{
	auto it = torrents.find(info_hash);
	if (it == torrents.end()) {
		return;
	}
	std::vector<Peer> &peer_list = it->second;
	bool exists = false;
	for (auto &existing_peer : peer_list) {
		existing_peer.ip = peer.ip;
		existing_peer.port = peer.port;
		existing_peer.status = peer.status;
		existing_peer.last_announce = time(nullptr);
		exists = true;
		break;
	}
	if (!exists) {
		peer_list.push_back(peer);
	}
}

int Tracker::get_peer_count(const std::string &info_hash) const
{
	auto it = torrents.find(info_hash);
	if (it == torrents.end()) {
		return -1;
	}
	return it->second.size();
}



int main() {
	return 0;
}
