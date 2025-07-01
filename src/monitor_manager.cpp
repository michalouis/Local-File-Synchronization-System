#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sys/inotify.h>
#include <limits.h>
#include "../header/sync_database.h"
#include "../header/message_utils.h"
#include "../header/monitor_manager.h"
#include "../header/task_manager.h"

// Monitor Manager: Using inotify, the following functions manage directory monitoring

// Initialize monitor manager (inotify), returns file descriptor
int initMonitorManager() {
    int inotify_fd = inotify_init1(IN_NONBLOCK);
    if (inotify_fd < 0) {
        perror("inotify_init1");
        return -1;
    }

    return inotify_fd;
}

// Add directory to monitor by creating an inotify watch
int addDirToMonitor(int inotify_fd, const char* dir_path) {
    int wd = inotify_add_watch(inotify_fd, dir_path, IN_CREATE | IN_MODIFY | IN_DELETE);
    if (wd < 0) {
        perror("inotify_add_watch");
        return -1;
    }
    return wd;
}

// Remove directory from monitor
int rmvDirFromMonitor(int inotify_fd, int wd) {
    if (wd != -1 && inotify_fd != -1) {
        return inotify_rm_watch(inotify_fd, wd);
    }
    return 0;
}

// Handle changes in one of the monitoring directories (process inotify events)
void handleDirChange(int inotify_fd, int fss_out, int log_fd) {
    const int EVENT_SIZE = sizeof(struct inotify_event);
    const int BUF_LEN = 128 * (EVENT_SIZE + 16);
    char buffer[BUF_LEN];
    char* output_buf = NULL;
    
    int length = read(inotify_fd, buffer, BUF_LEN);
    
    if (length < 0) {
        if (errno != EAGAIN) {
            perror("read from inotify fd");
        }
        return;
    }
    
    int i = 0;
    while (i < length) {    // Read all events
        struct inotify_event *event = (struct inotify_event*)&buffer[i];
        
        // Skip directory events
        if (!(event->mask & IN_ISDIR)) {
            // Find which directory this event belongs to
            for (auto& pair : sync_info) {
                if (pair.second.wd == event->wd) {
                    const char* operation = "";
                    bool valid_event = true;
                    
                    // Determine the type of event
                    if (event->mask & IN_CREATE) {
                        operation = "ADDED";
                    } else if (event->mask & IN_MODIFY) {
                        operation = "MODIFIED";
                    } else if (event->mask & IN_DELETE) {
                        operation = "DELETED";
                    } else {
                        valid_event = false;
                    }
                    
                    if (valid_event) {  // Add task to queue
                        addTaskToQueue(pair.second.source_dir, pair.second.target_dir,
                                        event->name, operation, false);
                        
                        // // Format the change notification message
                        // const char* base_msg = "File change detected: ";
                        // size_t needed_size = strlen(base_msg) + strlen(event->name) + strlen(operation) + 10;

                        // char* event_msg = (char*)malloc(needed_size);
                        // if (event_msg) {
                        //     sprintf(event_msg, "File change detected: %s (%s)\n", 
                        //             event->name, operation);
                            
                        //     // Add timestamp
                        //     event_msg = addTimestampToMessage(event_msg, NULL);
                        //     if (event_msg) {
                        //         output_buf = appendToBuffer(output_buf, event_msg);
                        //         free(event_msg);
                        //     }
                        // }
                    }
                    break;
                }
            }
        }
        
        i += EVENT_SIZE + event->len;
    }
    
    // Send notification to console and log if we have any messages
    if (output_buf && strlen(output_buf) > 0) {
        forwardMessage(output_buf, fss_out, log_fd);
        free(output_buf);  // Clean up dynamic memory
    }
}

// Shutdown and clean up resources used by the monitor manager
void shutdownMonitorManager(int inotify_fd) {
    // Clean up inotify resources
    for (auto& pair : sync_info) {
        if (pair.second.wd >= 0) {
            rmvDirFromMonitor(inotify_fd, pair.second.wd);
            pair.second.wd = -1;
        }
    }
    close(inotify_fd);
}