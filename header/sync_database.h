#ifndef SYNC_INFO_H
#define SYNC_INFO_H

#include <limits.h>
#include <stdbool.h>
#include <unordered_map>
#include <string>
#include "../header/message_utils.h"    // for TIMESTAMP_SIZE

// sync database: Manages synchronization information for directories using unordered map (stl)

#define CONFIG_BUF_S (2*PATH_MAX+8)

struct sync_info_entry {
    char* source_dir;
    char* target_dir;
    char* last_sync_time;
    int error_count;
    int wd;
};

extern std::unordered_map<std::string, sync_info_entry> sync_info;

// Insert directories from config file into the map
int readConfig(const char* config_path);

// Add source directory to map
void addSyncInfo(const char* source, const char* target);

// Get directory info
sync_info_entry* getSyncInfo(const char* directory);

// Remove directory from map
void rmvSyncInfo(const char* directory);

// Pass information of a single entry into dynamically allocated buffer
// Returns NULL if entry not found
char* copySyncInfo(const char* directory);

// Print content of all entries in sync_info
void printAllSyncInfo();

// Clean up all memory used by sync_info
void cleanupAllSyncInfo();

#endif