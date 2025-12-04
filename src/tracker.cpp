#include <bencode.hpp>
#include <peer_info.hpp>
#include <tracker.hpp>
#include <algorithm>

Tracker::Tracker(int interval, int timeout)
	: announce_interval(interval), peer_timeout(timeout) {}

std::string Tracker::handle_announce(const std::string &info_hash,
							const std::string &peer_id,
							const std::string &ip,
							uint16_t port,
							const std::string &event)
{
	PeerInfo peer(peer_id, ip, port);
	peer.last_announce = time(nullptr);
	peer.status = event;

	update_peer(info_hash, peer);
	return generate_response(info_hash, peer.peer_id);
}

std::string Tracker::generate_response(const std::string &info_hash, const std::string &caller_id)
{
    auto it = torrents.find(info_hash);
    if (it == torrents.end()) {
        bencode::dict response;
        response["interval"] = (long long)announce_interval;
        response["peers"] = bencode::list();
        return bencode::encode(response);
    }

    const std::vector<PeerInfo> &peer_list = it->second;

    bencode::list peers;
    for (const auto &peer : peer_list) {
		if (peer.peer_id == caller_id) {
			continue;
		}
        bencode::dict peer_dict;
        peer_dict["peer id"] = peer.peer_id;
        peer_dict["ip"] = peer.ip;
        peer_dict["port"] = (long long)peer.port;
        peers.push_back(peer_dict);
    }

    bencode::dict response;
    response["interval"] = (long long)announce_interval;
    response["peers"] = peers;

    return bencode::encode(response);
}

void Tracker::cleanup_inactive_peers()
{
    time_t current_time = time(nullptr);

    for (auto &[info_hash, peer_list] : torrents) {
        peer_list.erase(
            std::remove_if(peer_list.begin(), peer_list.end(),
                [this, current_time](const PeerInfo &peer) {
                    return (current_time - peer.last_announce) > peer_timeout;
                }),
            peer_list.end()
        );
    }
}

void Tracker::update_peer(const std::string &info_hash, const PeerInfo &peer)
{
	auto &peer_list = torrents[info_hash];

	bool exists = false;
	for (auto &existing_peer : peer_list) {
		if (peer.peer_id != existing_peer.peer_id) {
			continue;
		}
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
