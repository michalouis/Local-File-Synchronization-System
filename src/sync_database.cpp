#include "../header/sync_database.h"
#include "../header/message_utils.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// sync database: Manages synchronization information for directories using unordered map (stl)

std::unordered_map<std::string, sync_info_entry> sync_info;

///// HELPER FUNCTION /////

// Function to free memory for a single sync_info_entry
void freeSyncInfoEntry(sync_info_entry* entry) {
    if (!entry) return;
    
    if (entry->source_dir) free(entry->source_dir);
    if (entry->target_dir) free(entry->target_dir);
    if (entry->last_sync_time) free(entry->last_sync_time);
    
    entry->source_dir = NULL;
    entry->target_dir = NULL;
    entry->last_sync_time = NULL;
}

///// MAIN FUNCTIONS /////

// Insert directories from config file into the map
int readConfig(const char* config_path) {
    FILE* file = fopen(config_path, "r");
    if (file == NULL) {
        perror("Error opening config file");
        return -1;
    }

    char line[CONFIG_BUF_S];
    char source_dir[PATH_MAX], target_dir[PATH_MAX];
    int count = 0;
    int line_num = 0;

    // Read file line by line
    while (fgets(line, CONFIG_BUF_S, file) != NULL) {
        line_num++;
        
        // Remove trailing newline if present
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        // Skip empty lines
        if (strlen(line) == 0) {
            continue;
        }
        
        // Parse line to get source and target directories
        if (sscanf(line, "%s %s", source_dir, target_dir) == 2) {
            // Check if directories exist and are accessible
            if (access(source_dir, F_OK) != 0) {
                fprintf(stderr, "Line %d: ", line_num);
                perror(source_dir);
                fclose(file);
                return -1;  // Abort on directory access error
            }
            
            if (access(target_dir, F_OK) != 0) {
                fprintf(stderr, "Line %d: ", line_num);
                perror(target_dir);
                fclose(file);
                return -1;  // Abort on directory access error
            }

            // Check if source already exists in sync_info
            if (sync_info.find(source_dir) != sync_info.end()) {
                printf("Duplicate source directory in config (line %d): %s\n", line_num, source_dir);
                continue;
            }

            addSyncInfo(source_dir, target_dir);  // add to map            
            count++;
        } else {
            printf("\nWARNING! Invalid format in line: %d\n", line_num);
        }
    }

    fclose(file);
    return count;
}

// Add <source, target> info to database
void addSyncInfo(const char* source, const char* target) {
    sync_info_entry info;
    info.source_dir = strdup(source);
    info.target_dir = strdup(target);
    info.last_sync_time = strdup("Never");
    info.wd = -1;
    info.error_count = 0;
    
    // Check if memory allocation succeeded
    if (!info.source_dir || !info.target_dir || !info.last_sync_time) {
        freeSyncInfoEntry(&info);
        perror("Memory allocation failed in addSyncInfo");
        return;
    }
    
    // Add to map
    sync_info[std::string(source)] = info;
}

// Get directory info
sync_info_entry* getSyncInfo(const char* directory) {
    if (sync_info.find(directory) != sync_info.end()) {
        return &(sync_info[directory]);
    }
    return NULL;
}

// Remove directory from map
void rmvSyncInfo(const char* directory) {
    auto entry = sync_info.find(directory);
    if (entry != sync_info.end()) {
        freeSyncInfoEntry(&(entry->second));
        sync_info.erase(entry);
    }
}

// Pass information of a single entry into dynamically allocated buffer
char* copySyncInfo(const char* directory) {
    sync_info_entry* info = getSyncInfo(directory);
    
    if (info == NULL) {
        return NULL;
    }
    
    size_t buffer_size = strlen(info->source_dir) + strlen(info->target_dir) + 
                         strlen(info->last_sync_time) + 100;
    
    // Allocate the buffer dynamically
    char* buffer = (char*)malloc(buffer_size);
    if (!buffer) {
        perror("Failed to allocate memory for sync info");
        return NULL;
    }
    
    // Format entry information into buffer
    sprintf(buffer,
        "Source: %s\n"
        "Target: %s\n"
        "Last Sync: %s\n"
        "Error Count: %d\n"
        "Status: %s\n",
        info->source_dir, 
        info->target_dir,
        info->last_sync_time,
        info->error_count,
        info->wd >= 0 ? "Active" : "Inactive");
    
    return buffer;
}

// Print content of all entries in sync_info
void printAllSyncInfo() {
    if (sync_info.empty()) {
        printf("\nNo directories configured for monitoring.\n");
    } else {
        printf("\nCurrent sync_info data:\n\n");
        
        // Iterate through all entries and print each one
        for (const auto& pair : sync_info) {
            char* buffer = copySyncInfo(pair.first.c_str());
            if (buffer) {
                printf("%s", buffer);
                free(buffer);
            }
            printf("----------------------------------------\n");
        }
        printf("\n");
    }
}

// Clean up all memory used by sync_info
void cleanupAllSyncInfo() {
    // Free memory for all entries
    for (auto& pair : sync_info) {
        freeSyncInfoEntry(&pair.second);
    }
    
    // Clear the map
    sync_info.clear();
}