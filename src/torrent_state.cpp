#include <torrent_state.hpp>
#include <fstream>
#include <iostream>
#include <utils.hpp>
#include <cassert>

TorrentState::TorrentState(const TorrentMetadata &meta, const std::string &path)
	: metadata(meta), file_path(path)
{
	done_bmap.resize(metadata.piece_hashes.size(), false);
	in_progress_bmap.resize(metadata.piece_hashes.size(), false);

	std::ifstream file(file_path, std::ios::binary);
	if (!file.good()) {
		std::cout << "file unavailable locally. starting as leecher..." << std::endl;
		return;
	}
	
	std::cout << "file available locally. verifying pieces..." << std::endl;
	for (size_t i = 0; i < metadata.piece_hashes.size(); i++) {
		file.seekg(i * metadata.piece_length);

		size_t piece_size = metadata.piece_length;
		if (i == metadata.piece_hashes.size() - 1) {
			piece_size = metadata.file_length - (i * metadata.piece_length);
		}
		std::vector<char> piece_data(piece_size);
		file.read(piece_data.data(), piece_size);

		std::string piece_string(piece_data.begin(), piece_data.end());
		done_bmap[i] = verify_piece(i, piece_string);
	}

	int pieces_cnt = 0;
	for (bool b : done_bmap) {
		if (b) pieces_cnt++;
	}

	std::cout << "have " << pieces_cnt << " / "  << metadata.piece_hashes.size()
		<< std::endl;

}

bool TorrentState::verify_piece(int index, const std::string &piece_string)
{
	return sha1_hash(piece_string) == metadata.piece_hashes[index];
}

bool TorrentState::have_piece(int index)
{
	std::lock_guard<std::mutex> lock(state_mutex);
	return done_bmap[index];
}

int TorrentState::get_next_piece_to_download()
{
	std::lock_guard<std::mutex> lock(state_mutex);
	for (size_t i = 0; i < done_bmap.size(); i++) {
		if (!done_bmap[i] && !in_progress_bmap[i]) {
			return i;
		}
	}
	return -1;
}

void TorrentState::set_in_progress(int index)
{
	std::lock_guard<std::mutex> lock(state_mutex);
	in_progress_bmap[index] = true;
}

void TorrentState::set_complete(int index)
{
	std::lock_guard<std::mutex> lock(state_mutex);
	done_bmap[index] = true;
	in_progress_bmap[index] = false;
}

std::vector<bool> TorrentState::get_bitfield() 
{
	std::lock_guard<std::mutex> lock(state_mutex);
	return done_bmap;
}

void TorrentState::write_piece(int index, const std::string &data)
{
	size_t indecks = static_cast<size_t>(index);
	assert(index >= 0 && indecks < metadata.piece_hashes.size());

	std::lock_guard<std::mutex> lock(file_mutex);
	std::fstream file(file_path, std::ios::out | std::ios::in | std::ios::binary);
	if (!file.is_open()) {
		file.open(file_path, std::ios::out | std::ios::binary); /* Create file */
		file.close();
		file.open(file_path, std::ios::out | std::ios::in | std::ios::binary);
	}

	if (!file.is_open()) {
		throw std::runtime_error("unable to access file when should have");
	}

	file.seekp(index * metadata.piece_length);
	file.write(data.data(), data.size());
	file.flush();
}

std::string TorrentState::read_piece(int index)
{
	size_t indecks = static_cast<size_t>(index);
	assert(index >= 0 && indecks < metadata.piece_hashes.size());

	std::lock_guard<std::mutex> lock(file_mutex);
	std::ifstream file(file_path, std::ios::binary);
	if (!file.good()) {
		throw std::runtime_error("unable to access file when should have");
	}

	file.seekg(index * metadata.piece_length);
	size_t piece_size = metadata.piece_length;
	if (indecks == metadata.piece_hashes.size() - 1) {
		piece_size = metadata.file_length - (indecks * metadata.piece_length);
	}
	std::vector<char> piece_data(piece_size);
	file.read(piece_data.data(), piece_size);
	std::string piece_string(piece_data.begin(), piece_data.end());
	return piece_string;
}

bool TorrentState::is_file_complete()
{
	std::lock_guard<std::mutex> lock(state_mutex);
	for (bool b : done_bmap) {
		if(!b) return false;
	}
	return true;
}

int TorrentState::bytes_left()
{
	std::lock_guard<std::mutex> lock(state_mutex);
	int missing_cnt = 0;
	int bytes_left;
	for (bool b : done_bmap) {
		if(!b) missing_cnt++;
	}
	if (!done_bmap[done_bmap.size()-1]) {
		int last_piece_size = metadata.file_length -
			((done_bmap.size()-1) * metadata.piece_length);
		bytes_left = (missing_cnt - 1) * metadata.piece_length + last_piece_size;
	} else {
		bytes_left = missing_cnt * metadata.piece_length;
	}
	return bytes_left;
}

int TorrentState::get_total_pieces()
{
	return metadata.piece_hashes.size();
}

int TorrentState::get_piece_length()
{
	return metadata.piece_length;
}

TorrentMetadata TorrentState::get_metadata()
{
	return metadata;
}
