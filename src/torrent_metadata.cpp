#include <torrent_metadata.hpp>
#include <utils.hpp>
#include <bencode.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>

/* SHA1 hashes are 20 bytes */
#define PIECE_HASH_SIZE 20

TorrentMetadata::TorrentMetadata(const std::string& torrent_filename) {
    parse(torrent_filename);
}

void TorrentMetadata::parse(const std::string& torrent_filename) {
    std::ifstream file(torrent_filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open torrent file: " + torrent_filename);
    }

    std::string file_data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    if (file.bad()) {
		throw std::runtime_error("Error reading torrent file");
    }

    bencode::data data = bencode::decode(file_data);
    auto root_dict = std::get<bencode::dict>(data);

    if (root_dict.count("announce")) {
        announce_url = std::get<bencode::string>(root_dict["announce"]);
    } else {
        throw std::runtime_error("No announce URL in torrent file");
    }
    if (!root_dict.count("info")) {
        throw std::runtime_error("No info dictionary in torrent file");
    }

    auto info = std::get<bencode::dict>(root_dict["info"]);

	/* Name of file to be downloaded */
    if (info.count("name")) {
        file_name = std::get<bencode::string>(info["name"]);
    } else {
        throw std::runtime_error("No file name in torrent");
    }

	/* Total length of file to be downloaded */
    if (info.count("length")) {
        file_length = std::get<bencode::integer>(info["length"]);
    } else {
        throw std::runtime_error("No file length in torrent");
    }

	/* Size of each piece in bytes */
    if (info.count("piece length")) {
        piece_length = std::get<bencode::integer>(info["piece length"]);
    } else {
        throw std::runtime_error("No piece length in torrent");
    }

	/* SHA1 hashes of pieces for verification */
    if (info.count("pieces")) {
        std::string pieces_str = std::get<bencode::string>(info["pieces"]);

        for (size_t i = 0; i < pieces_str.length(); i += PIECE_HASH_SIZE) {
            if (i + PIECE_HASH_SIZE <= pieces_str.length()) {
                piece_hashes.push_back(pieces_str.substr(i, PIECE_HASH_SIZE));
            }
        }

        num_pieces = piece_hashes.size();
    } else {
        throw std::runtime_error("No pieces in torrent");
    }

    std::string bencoded_info = bencode::encode(info);
    calculate_info_hash(bencoded_info);
}

void TorrentMetadata::calculate_info_hash(const std::string& bencoded_info) {
    info_hash = sha1_hash(bencoded_info);
}

void TorrentMetadata::print_info() const {
    std::cout << "=== torrent metadata ===" << std::endl;
    std::cout << "File name: " << file_name << std::endl;
    std::cout << "File size: " << file_length << " bytes" << std::endl;
    std::cout << "Piece length: " << piece_length << " bytes" << std::endl;
    std::cout << "Number of pieces: " << num_pieces << std::endl;
    std::cout << "Tracker URL: " << announce_url << std::endl;
    std::cout << "Info hash (hex): " << hash_to_hex(info_hash) << std::endl;
}
