# Compiler settings
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -Iinclude -I/usr/include/asio
LDFLAGS = -lcrypto -pthread

# Directories
SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include

# Target binaries
CLIENT_TARGET = torrent_client
TRACKER_TARGET = tracker

# Source files
CLIENT_SRCS = $(SRC_DIR)/client.cpp $(SRC_DIR)/peer.cpp $(SRC_DIR)/utils.cpp
TRACKER_SRCS = $(SRC_DIR)/tracker.cpp $(SRC_DIR)/utils.cpp

# Object files
CLIENT_OBJS = $(CLIENT_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
TRACKER_OBJS = $(TRACKER_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

all: client tracker

# Build client only
client: $(CLIENT_TARGET)

# Build tracker only
tracker: $(TRACKER_TARGET)

# Link client
$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) $(CLIENT_OBJS) -o $@ $(LDFLAGS)
	@echo "Built $(CLIENT_TARGET)"

# Link tracker
$(TRACKER_TARGET): $(TRACKER_OBJS)
	$(CXX) $(CXXFLAGS) $(TRACKER_OBJS) -o $@ $(LDFLAGS)
	@echo "Built $(TRACKER_TARGET)"

# Compile source files from src/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean rule
clean:
	rm -rf $(BUILD_DIR) $(CLIENT_TARGET) $(TRACKER_TARGET)
	@echo "Cleaned build artifacts"

# Run targets (optional)
run-client: $(CLIENT_TARGET)
	./$(CLIENT_TARGET)

run-tracker: $(TRACKER_TARGET)
	./$(TRACKER_TARGET)

.PHONY: all client tracker clean run-client run-tracker
