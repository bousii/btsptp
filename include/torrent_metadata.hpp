#ifndef TORRENT_METADATA_HPP
#define TORRENT_METADATA_HPP

#include <string>
#include <vector>
#include <cstdint>

class TorrentMetadata {
public:
    std::string announce_url;
    std::string info_hash;
    std::string file_name;
    int64_t file_length;
    int64_t piece_length;
    std::vector<std::string> piece_hashes;
    int num_pieces;

    TorrentMetadata(const std::string& torrent_filename);
    void print_info() const;
private:
    void parse(const std::string& torrent_filename);
    void calculate_info_hash(const std::string& bencoded_info);
};

#endif /* torrent_metadata.hpp */
