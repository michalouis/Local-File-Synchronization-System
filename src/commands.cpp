#include "../header/commands.h"
#include "../header/sync_database.h"
#include "../header/message_utils.h"
#include "../header/monitor_manager.h"
#include "../header/task_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

// Add pair to sync_info and start monitoring it
void commandAdd(const char* source, const char* target, int fss_out, int log_fd, int inotify_fd) {
    char* message_buffer = NULL;
    sync_info_entry* info = getSyncInfo(source);

    // Special case for syncCommand():
    // User wants to sync directory but it's inactive -> we reactivate it and do a full
    // sync using "SYNC" operation, to get sync completion message
    if (target == NULL) {
        info->wd = addDirToMonitor(inotify_fd, source);
        if (info->wd >= 0) {
            // Create message about activating/reactivating directory
            message_buffer = (char*)malloc(strlen(source) + strlen(info->target_dir) + 50);
            if (message_buffer) {
                sprintf(message_buffer, "Added directory: %s -> %s\n", source, info->target_dir);
                message_buffer = addTimestampToMessage(message_buffer, NULL);
                printf("%s", message_buffer);
                
                char* monitor_msg = (char*)malloc(strlen(source) + 40);
                if (monitor_msg) {
                    sprintf(monitor_msg, "Monitoring started for %s\n", source);
                    monitor_msg = addTimestampToMessage(monitor_msg, NULL);
                    printf("%s", monitor_msg);
                    if (monitor_msg && message_buffer) {
                        message_buffer = appendToBuffer(message_buffer, monitor_msg);
                    }
                    free(monitor_msg);
                }
                
                // Send messages to console and log
                forwardMessage(message_buffer, fss_out, log_fd);
                free(message_buffer);
            }

            addTaskToQueue(source, info->target_dir, "ALL", "SYNC", false);
        } else {
            // Failed to set up monitoring
            message_buffer = (char*)malloc(strlen(source) + 50);
            if (message_buffer) {
                sprintf(message_buffer, "Failed to set up monitoring for %s\n", source);
                message_buffer = addTimestampToMessage(message_buffer, NULL);
                printf("%s", message_buffer);
                if (message_buffer) {
                    forwardMessage(message_buffer, fss_out, -1);
                    free(message_buffer);
                }
            }
        }
        return;
    }

    // Check if directory data exists
    if (info != NULL) {
        // Check if it's already active OR if target directory is different
        if (info->wd >= 0 || strcmp(info->target_dir, target) != 0) {
            message_buffer = (char*)malloc(strlen(source) + 40);
            if (message_buffer) {
                sprintf(message_buffer, "Already in queue: %s\n", source);
                message_buffer = addTimestampToMessage(message_buffer, NULL);
                printf("%s", message_buffer);
                if (message_buffer) {
                    forwardMessage(message_buffer, fss_out, -1);
                    free(message_buffer);
                }
            }
            return;
        }
    } else {
        // New directory - add to map
        addSyncInfo(source, target);
        info = getSyncInfo(source);
        if (!info) return;  // Should never happen, but just in case
    }
    
    // Set up inotify watch
    info->wd = addDirToMonitor(inotify_fd, source);
    if (info->wd >= 0) {
        message_buffer = (char*)malloc(strlen(source) + strlen(target) + 100);
        if (message_buffer) {
            sprintf(message_buffer, "Added directory: %s -> %s\n", source, target);
            message_buffer = addTimestampToMessage(message_buffer, NULL);
            printf("%s", message_buffer);
            
            char* monitor_msg = (char*)malloc(strlen(source) + 40);
            if (monitor_msg) {
                sprintf(monitor_msg, "Monitoring started for %s\n", source);
                monitor_msg = addTimestampToMessage(monitor_msg, NULL);
                printf("%s", monitor_msg);
                
                if (monitor_msg && message_buffer) {
                    message_buffer = appendToBuffer(message_buffer, monitor_msg);
                }
                free(monitor_msg);
            }
            
            if (message_buffer) {
                forwardMessage(message_buffer, fss_out, log_fd);
                free(message_buffer);
            }
        }
        
        // Queue a full sync task for the newly added directory
        addTaskToQueue(source, target, "ALL", "FULL", false);
    } else {
        // Failed to set up monitoring
        message_buffer = (char*)malloc(strlen(source) + 50);
        if (message_buffer) {
            sprintf(message_buffer, "Failed to set up monitoring for %s\n", source);
            message_buffer = addTimestampToMessage(message_buffer, NULL);
            printf("%s", message_buffer);
            if (message_buffer) {
                forwardMessage(message_buffer, fss_out, -1);
                free(message_buffer);
            }
        }
    }
}

// Stop monitoring directory 
void commandCancel(const char* source, int fss_out, int log_fd, int inotify_fd) {
    char* message_buffer = NULL;
    
    sync_info_entry* info = getSyncInfo(source);
    if (info == NULL || info->wd < 0) {  // If NOT found in map or inactive
        message_buffer = (char*)malloc(strlen(source) + 40);
        if (message_buffer) {
            sprintf(message_buffer, "Directory not monitored: %s\n", source);
            message_buffer = addTimestampToMessage(message_buffer, NULL);
            printf("%s", message_buffer);
            if (message_buffer) {
                forwardMessage(message_buffer, fss_out, -1);
                free(message_buffer);
            }
        }
    } else if (isTaskQueued(source)) {
        message_buffer = (char*)malloc(strlen(source) + 50);
        if (message_buffer) {
            sprintf(message_buffer, "Directory is currently being synced: %s\n", source);
            message_buffer = addTimestampToMessage(message_buffer, NULL);
            printf("%s", message_buffer);
            if (message_buffer) {
                forwardMessage(message_buffer, fss_out, -1);
                free(message_buffer);
            }
        }
    } else {  // Directory exists in map
        if (rmvDirFromMonitor(inotify_fd, info->wd) >= 0) {
            info->wd = -1;  // Mark as inactive
            message_buffer = (char*)malloc(strlen(source) + 40);
            if (message_buffer) {
                sprintf(message_buffer, "Monitoring stopped for %s\n", source);
                message_buffer = addTimestampToMessage(message_buffer, NULL);
                printf("%s", message_buffer);
                if (message_buffer) {
                    forwardMessage(message_buffer, fss_out, log_fd);
                    free(message_buffer);
                }
            }
        } else {
            message_buffer = (char*)malloc(strlen(source) + 50);
            if (message_buffer) {
                sprintf(message_buffer, "Failed to stop monitoring for %s\n", source);
                message_buffer = addTimestampToMessage(message_buffer, NULL);
                printf("%s", message_buffer);
                if (message_buffer) {
                    forwardMessage(message_buffer, fss_out, -1);
                    free(message_buffer);
                }
            }
        }
    }
}

// Show status of directory (use "all" to print all directories)
void commandStatus(const char* source, int fss_out) {
    char* message_buffer = NULL;
    char* entry_buffer = NULL;

    if (strcmp(source, "all") == 0) {   // Print all directories (testing purpose only)
        printAllSyncInfo();
        message_buffer = strdup("All directories printed to manager console\n");
        if (message_buffer) {
            message_buffer = addTimestampToMessage(message_buffer, NULL);
            printf("%s", message_buffer);
            if (message_buffer) {
                forwardMessage(message_buffer, fss_out, -1);
                free(message_buffer);
            }
        }
        return;
    }
    
    sync_info_entry* info = getSyncInfo(source);
    if (info == NULL) {    // Directory not found in map
        message_buffer = (char*)malloc(strlen(source) + 40);
        if (message_buffer) {
            sprintf(message_buffer, "Directory not monitored: %s\n", source);
            message_buffer = addTimestampToMessage(message_buffer, NULL);
            printf("%s", message_buffer);
            if (message_buffer) {
                forwardMessage(message_buffer, fss_out, -1);
                free(message_buffer);
            }
        }
    } else {    // Directory exists
        entry_buffer = copySyncInfo(source);
        if (entry_buffer) { // Check if buffer was returned successfully
            message_buffer = (char*)malloc(strlen(source) + 40);
            if (message_buffer) {
                sprintf(message_buffer, "Status requested for %s\n", source);
                message_buffer = addTimestampToMessage(message_buffer, NULL);
                if (message_buffer) {
                    printf("%s", message_buffer);
                    printf("%s", entry_buffer);
                    
                    // Append entry details to message
                    message_buffer = appendToBuffer(message_buffer, entry_buffer);
                    if (message_buffer) {
                        forwardMessage(message_buffer, fss_out, -1);
                        free(message_buffer);
                    }
                }
            }
            // Free the entry buffer since copySyncInfo now allocates it dynamically
            free(entry_buffer);
        } else {
            message_buffer = (char*)malloc(strlen(source) + 50);
            if (message_buffer) {
                sprintf(message_buffer, "Error retrieving status for %s\n", source);
                message_buffer = addTimestampToMessage(message_buffer, NULL);
                if (message_buffer) {
                    forwardMessage(message_buffer, fss_out, -1);
                    free(message_buffer);
                }
            }
        }
    }
}

// Sync directory
void commandSync(const char* source, int fss_out, int log_fd, int inotify_fd) {
    char* message_buffer = NULL;
    
    sync_info_entry* info = getSyncInfo(source);
    if (info == NULL) {
        message_buffer = (char*)malloc(strlen(source) + 40);
        if (message_buffer) {
            sprintf(message_buffer, "Directory not monitored: %s\n", source);
            message_buffer = addTimestampToMessage(message_buffer, NULL);
            printf("%s", message_buffer);
            if (message_buffer) {
                forwardMessage(message_buffer, fss_out, -1);
                free(message_buffer);
            }
        }
    } else if (info->wd < 0) {
        commandAdd(source, NULL, fss_out, log_fd, inotify_fd);  // Special case, reactivate the directory
        message_buffer = (char*)malloc(strlen(source) + strlen(info->target_dir) + 40);
        if (message_buffer) {
            sprintf(message_buffer, "Syncing directory: %s -> %s\n", source, info->target_dir);
            message_buffer = addTimestampToMessage(message_buffer, NULL);
            printf("%s", message_buffer);
            if (message_buffer) {
                forwardMessage(message_buffer, fss_out, log_fd);
                free(message_buffer);
            }
        }
    } else {
        int log_message = log_fd;
        if (addTaskToQueue(source, info->target_dir, "ALL", "SYNC", true)) {
            message_buffer = (char*)malloc(strlen(source) + strlen(info->target_dir) + 40);
            if (message_buffer) {
                sprintf(message_buffer, "Syncing directory: %s -> %s\n", source, info->target_dir);
            }
        } else {
            message_buffer = (char*)malloc(strlen(source) + 40);
            if (message_buffer) {
                sprintf(message_buffer, "Sync already in progress %s\n", source);
            }
            log_message = -1;
        }
        
        if (message_buffer) {
            message_buffer = addTimestampToMessage(message_buffer, NULL);
            printf("%s", message_buffer);
            if (message_buffer) {
                forwardMessage(message_buffer, fss_out, log_message);
                free(message_buffer);
            }
        }
    }
}

// Delete directory from sync_info (only if inactive)
void commandDelete(const char* source, int fss_out, int log_fd) {
    char* message_buffer = NULL;

    sync_info_entry* info = getSyncInfo(source);
    if (info == NULL) {
        message_buffer = (char*)malloc(strlen(source) + 40);
        if (message_buffer) {
            sprintf(message_buffer, "Directory not monitored: %s\n", source);
            message_buffer = addTimestampToMessage(message_buffer, NULL);
            printf("%s", message_buffer);
            if (message_buffer) {
                forwardMessage(message_buffer, fss_out, -1);
                free(message_buffer);
            }
        }
    } else {
        // Check if it's active or inactive before deleting
        if (info->wd >= 0) {
            message_buffer = (char*)malloc(strlen(source) + 50);
            if (message_buffer) {
                sprintf(message_buffer, "Active directory cannot be deleted: %s\n", source);
                message_buffer = addTimestampToMessage(message_buffer, NULL);
                printf("%s", message_buffer);
                if (message_buffer) {
                    forwardMessage(message_buffer, fss_out, -1);
                    free(message_buffer);
                }
            }
        } else {
            rmvSyncInfo(source);            
            message_buffer = (char*)malloc(strlen(source) + 30);
            if (message_buffer) {
                sprintf(message_buffer, "Directory deleted: %s\n", source);
                message_buffer = addTimestampToMessage(message_buffer, NULL);
                printf("%s", message_buffer);
                if (message_buffer) {
                    forwardMessage(message_buffer, fss_out, log_fd);
                    free(message_buffer);
                }
            }
        }
    }
}

// Shutdown the manager and clean up resources
void commandShutdown(int fss_out, int log_fd) {
    char* message_buffer = strdup("Shutting down manager...\n");
    if (message_buffer) {
        message_buffer = addTimestampToMessage(message_buffer, NULL);
        printf("%s", message_buffer);
        if (message_buffer) {
            // No need to forward to console (just log)
            forwardMessage(message_buffer, -1, log_fd);
            free(message_buffer);
        }
    }

    // Finish tasks
    finishTasks(fss_out, log_fd);

    message_buffer = strdup("Manager shutdown complete.\n");
    if (message_buffer) {
        message_buffer = addTimestampToMessage(message_buffer, NULL);
        printf("%s", message_buffer);
        if (message_buffer) {
            forwardMessage(message_buffer, fss_out, -1);
            free(message_buffer);
        }
    }
    
    usleep(100000);  // Sleep for a short time to allow messages to be sent
}