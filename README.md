# BTSPTP (BitTorrent Style Peer to Peer) Client/Tracker Implementation

## Overview

This is a functional BitTorrent client implementation that supports downloading and uploading files using the BitTorrent protocol. The client can connect to trackers, communicate with peers, download pieces from multiple peers simultaneously, and seed files to other clients.

## Language and Libraries

### Language

**C++20** - This project is implemented in C++

### External Libraries

**Boost Libraries (required):**
- **Boost.Asio** - Provides asynchronous I/O including:
  - TCP socket wrappers
  - DNS resolution
  - Acceptor for incoming connections
  - Non-blocking I/O operations
  
- **Boost.Algorithm** - String manipulation utilities:
  - String predicates (ends_with, starts_with)
  - Case conversion
  
- **Boost.Endian** - Byte order conversion:
  - Converting between host and network byte order

**Custom Components:**
- [Bencode parser/encoder](https://github.com/jimporter/bencode.hpp) for bencoding library

### Platform Compatibility

**Primary Platform:** Linux/Unix (POSIX-compliant systems)

**Windows Compatibility:** This project was initially created to be cross platform, using Boost libraries to enable this. However, some later editions of signal handling makes this unlikely to work on raw Windows. If you're on windows, using WSL or Cygwin would be much more likely to work.

⚠️ **This implementation has NOT been tested on Windows and may require modifications:**
- Signal handling (SIGINT, SIGTERM) uses POSIX signals - Windows uses different mechanisms
- Path handling may need adjustments (forward vs backslash)
- Thread detachment behavior may differ
- Socket timeout handling could vary

**WARNING** Use Linux or macOS for guaranteed compatibility. Windows users should consider WSL (Windows Subsystem for Linux).

## Dependencies

### For Building and Running the Client

1. **C++ Compiler with C++20 support:**
   - `sudo apt-get install build-essential` (Ubuntu/Debian)

2. **Boost Libraries:**
   - Boost 1.66 or later
   - Install on Ubuntu/Debian:
```bash
     sudo apt-get install libboost-all-dev
```
   - Install on macOS:
```bash
     brew install boost
```

3. **Make**
```bash
   sudo apt-get install make
```
  - Install on macOS:
```bash
     brew install make
```

4. **OpenSSL**
```bash
  sudo apt-get install libssl-dev
```
  - Install on macOS:
```bash
    brew install openssl
```

### For Creating Torrents

**mktorrent** - Command-line utility for creating .torrent files:
```bash
# Ubuntu/Debian
sudo apt-get install mktorrent

# macOS
brew install mktorrent
```

## Building the Project

### Using Makefile
```bash
make clean
make
make tracker
make client
```

This will produce two:
- `torrent_client` - The BitTorrent peer client
- `tracker` - The Tracker server

## Creating Torrent Files

Before you can download files, you need to create a .torrent file using `mktorrent`.

### Basic Torrent Creation
```bash
mktorrent -a http://<tracker_ip>:8080/announce -o myfile.torrent myfile.mov
```

**Parameters:**
- `-a <announce_url>` - Tracker announce URL (required), sample uses localhost but you can use wherever the tracker will be located
- `-o <output_file>` - Output .torrent filename
- `myfile.mov` - The file you want to share
- '-l <piece_size>' - Size of each piece

### Example Workflow
```bash
# 1. Create a test file
echo "Hello BitTorrent!" > test.txt

# 2. Create torrent file pointing to your tracker
mktorrent -a http://<tracker_ip>:8080/announce -o test.torrent test.txt

# 3. The .torrent file is now ready to use
ls -lh test.torrent
```

## Running the Tracker

The tracker coordinates peers and maintains the swarm for each torrent.

### Start the Tracker
```bash
./tracker
```

This starts the tracker listening on `http://<tracker_ip>:8080/announce`

### Tracker Functionality

The tracker will:
- Accept announce requests from peers
- Maintain a list of active peers for each torrent
- Return peer lists to requesting clients
- Remove stale peers that haven't announced recently
- Log activity to console

## Running the Client

### Basic Usage
```bash
./torrent_client <torrent_file>
```

**Arguments:**
- `<torrent_file>` - Path to .torrent file (must end with .torrent extension)

### Complete Example Workflow

#### Scenario: One Seeder, Two Leechers

**Terminal 1 - Start Tracker:**
```bash
./tracker
```

**Terminal 2 - Start First Client (Seeder):**
```bash
# This client already has the complete file
./torrent_client video.mov.torrent
```

**Terminal 3 - Start Second Client (Leecher):**
```bash
# This client doesn't have the file yet
# Make sure video.mov is NOT in this directory
./torrent_client video.mov.torrent
```

**Terminal 4 - Start Third Client (Leecher):**
```bash
# Another leecher
./torrent_client video.mov.torrent
```

## Command Line Options

**Current Options:**
- `<torrent_file>` - Path to .torrent file (required)

**File Requirements:**
- Must exist
- Must have `.torrent` extension

## Implementation Architecture

### Core Components

1. **TorrentMetadata** - Parses .torrent files, extracts metadata (announce URL, piece hashes, file info)

2. **TorrentState** - Manages download/upload state, tracks piece availability, handles file I/O

3. **PeerConnection** - Handles individual peer communication, implements BitTorrent wire protocol

4. **Main Client** - Orchestrates tracker communication, spawns peer threads, monitors progress

### Concurrency Model

- **Multi-threaded architecture** using `std::thread`
- **Acceptor thread** for incoming connections
- **Peer threads** for each connection (detached)
- **Synchronization primitives** for shared resources among threads (TorrentState)

### Network Protocol

- **Tracker:** HTTP GET requests with bencoded responses
- **Peers:** Binary wire protocol over TCP
- **Byte order:** Network byte order (big-endian) using Boost.Endian
- **Messages:** Length-prefixed with message type identifiers

## Features Implemented

**Sockets / TCP Setup** - Boost.Asio-based networking

**Tracker Communication** - HTTP announces with started/completed/stopped events

**Peer Communication** - Handshakes, keep-alives, state management

**Download from ≥2 Peers Simultaneously** - Multi-threaded concurrent downloads

**Upload to ≥2 Peers Simultaneously** - Accepts and serves multiple peers

**File Assembly & Verification** - SHA1 hash verification, correct piece ordering

## File Structure

```
├── include
│   ├── bencode.hpp -- Imported bencoding library
│   ├── peer_connection.hpp -- Peer connection logic header (handshake, sending messages, pieces, etc)
│   ├── peer_info.hpp -- Peer info the tracker uses
│   ├── torrent_metadata.hpp -- Read only information extracted from .torrent file
│   ├── torrent_state.hpp -- State of client/downloaded file. Shared amongst all threads to prevent race conditions
│   ├── tracker.hpp -- Core tracker logic (excluding HTTP server)
│   └── utils.hpp -- Miscellaneous helper functions
├── Makefile
├── README.md
├── src
│   ├── btsptp_client.cpp -- Main client implementation
│   ├── peer_connection.cpp -- Implementation of main BitTorrent messaging scheme
│   ├── peer_info.cpp -- Constructor for peer information
│   ├── torrent_metadata.cpp -- Main torrent file parsing logic
│   ├── torrent_state.cpp -- File IO, synchronization logic of shared state
│   ├── tracker.cpp -- Tracker logic (handle announcing, removing peers, encoding responses)
│   ├── tracker_server.cpp -- HTTP server main method for tracker. Uses tracker.cpp
│   └── utils.cpp - Implementation of helper functions
├── torrent_client
└── tracker
```

## Testing Scenarios

The implementation has been tested with:

1. Single seeder → single leecher
2. Multiple simultaneous leechers (2+ clients downloading at once)
3. Leecher becoming seeder after completion

## Known Limitations

- Windows compatibility untested/unlikely to work fully due to POSIX signals and thread weirdness
- Shutdown of clients is not very graceful, just detaching peer threads due to time's sake

## References

- Supplemental articles provided in project description
- [Boost.Asio Documentation](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html)
- [mktorrent man page](https://github.com/Rudde/mktorrent)

## Author

**Owen Giles**

