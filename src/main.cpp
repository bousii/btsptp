#include <asio.hpp>
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
	ifstream torrent_file(filename, ios::binary);
	if (!torrent_file) {
		throw runtime_error("could not open file");
	}

	vector<unsigned char> file_data(
			(istreambuf_iterator<char>(torrent_file)),
			istreambuf_iterator<char>()
	);

	Bdecoder decoder(file_data);

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
