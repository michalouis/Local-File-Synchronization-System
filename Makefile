OBJS = fss_manager.o fss_console.o worker.o sync_database.o message_utils.o commands.o monitor_manager.o task_manager.o
SOURCE = fss_manager.c fss_console.c worker.c sync_database.cpp message_utils.cpp commands.cpp monitor_manager.cpp task_manager.cpp
HEADER = sync_database.h message_utils.h commands.h monitor_manager.h
OUT = fss_manager fss_console worker
CC = g++
FLAGS = -g -Wall -Wextra

# Directories
SRC_DIR = src
HEADER_DIR = header
BIN_DIR = bin

# Default
all: $(BIN_DIR) $(addprefix $(BIN_DIR)/,$(OUT))

# Create bin directory if it doesn't exist
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# To create executables individually
.PHONY: fss_manager fss_console worker

fss_manager: $(BIN_DIR)/fss_manager
fss_console: $(BIN_DIR)/fss_console
worker: $(BIN_DIR)/worker

# Create executables from source files
$(BIN_DIR)/fss_manager: $(SRC_DIR)/fss_manager.cpp $(SRC_DIR)/sync_database.cpp $(SRC_DIR)/message_utils.cpp $(SRC_DIR)/commands.cpp $(SRC_DIR)/monitor_manager.cpp $(SRC_DIR)/task_manager.cpp | $(BIN_DIR)
	$(CC) $(FLAGS) $(SRC_DIR)/fss_manager.cpp $(SRC_DIR)/sync_database.cpp $(SRC_DIR)/message_utils.cpp $(SRC_DIR)/commands.cpp $(SRC_DIR)/monitor_manager.cpp $(SRC_DIR)/task_manager.cpp -o $@

$(BIN_DIR)/fss_console: $(SRC_DIR)/fss_console.cpp $(SRC_DIR)/message_utils.cpp | $(BIN_DIR)
	$(CC) $(FLAGS) $(SRC_DIR)/fss_console.cpp $(SRC_DIR)/message_utils.cpp -o $@

$(BIN_DIR)/worker: $(SRC_DIR)/worker.cpp | $(BIN_DIR)
	$(CC) $(FLAGS) $< -o $@

clean:
	rm -rf $(BIN_DIR)

# Clean specific executables
clean-fss_manager:
	rm -f $(BIN_DIR)/fss_manager

clean-fss_console:
	rm -f $(BIN_DIR)/fss_console

clean-worker:
	rm -f $(BIN_DIR)/worker