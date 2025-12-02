#include <bencode.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <fstream>
#include <iostream>
#include <utils.hpp>

using namespace std;

int main(int argc, char *argv[])
{
	/* Should be exactly two args: executable name and file name */
	if (argc < 2) {
		cout << "too few arguments for this program" << endl;
		return 1;
	}
	
	if (argc > 2) {
		cout << "too many arguments for this program" << endl;
		return 1;
	}
	
	string filename(argv[1]);
	if (!boost::algorithm::ends_with(filename, ".torrent")) {
		throw runtime_error("filename must end in .torrent");
	}
	assert(filename.find(".torrent") != std::string::npos);

	ifstream torrent_file(filename, ios::binary);
	if (!torrent_file) {
		throw runtime_error("could not open file");
	}

	std::string file_data(
			(istreambuf_iterator<char>(torrent_file)),
			istreambuf_iterator<char>()
	);

	if (torrent_file.bad()) {
		throw runtime_error("error reading file");
	}

	bencode::data data = bencode::decode(file_data);
	auto root_dict = std::get<bencode::dict>(data);

	string announce_url;
	if (root_dict.count("announce")) {
	    announce_url = std::get<bencode::string>(root_dict["announce"]);
	    cout << "tracker URL: " << announce_url << endl;
	}
	
	if (root_dict.count("info")) {
	    cout << "info dictionary found" << endl;
	    auto info = std::get<bencode::dict>(root_dict["info"]);

	    if (info.count("name")) {
	        auto name = std::get<bencode::string>(info["name"]);
	        cout << "torrent name: " << name << endl;
	    }

	    if (info.count("length")) {
	        auto length = std::get<bencode::integer>(info["length"]);
	        cout << "file size: " << length << " bytes" << endl;
	    }
	}

	/*
	try {
        asio::io_context io;
        asio::ip::tcp::resolver resolver(io);
        asio::ip::tcp::resolver::results_type endpoints =
            resolver.resolve("example.com", "80");
   
        asio::ip::tcp::socket socket(io);
        asio::connect(socket, endpoints);

        cout << "Connected to example.com" << endl;
		socket.close();
	    } catch (std::exception& e) {
	        cerr << "Error: " << e.what() << endl;
	    }
	*/

	return 0;
}
