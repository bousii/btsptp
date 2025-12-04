#ifndef TORRENT_STATE_HPP
#define TORRENT_STATE_HPP

#include <torrent_metadata.hpp>
#include <mutex>

class TorrentState {
private:
	std::vector<bool> done_bmap;
	std::vector<bool> in_progress_bmap;
	std::mutex state_mutex;
	TorrentMetadata metadata;
	std::string file_path;
	std::mutex file_mutex;

public:
    TorrentState(const TorrentMetadata &meta, const std::string &path);
	bool verify_piece(int index, const std::string &piece_string);
	bool have_piece(int index);
	int get_next_piece_to_download();
	void set_in_progress(int index);
	void set_complete(int index);
	std::vector<bool> get_bitfield();
	void write_piece(int index, const std::string &data);
	std::string read_piece(int index);
	bool is_file_complete();
	int bytes_left();
	int get_total_pieces();
	int get_piece_length();
	TorrentMetadata get_metadata();
};

#endif /* torrent_state.hpp */
