#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/inotify.h>
#include <sys/poll.h>
#include <limits.h>
#include <signal.h>
#include "../header/sync_database.h"
#include "../header/message_utils.h"
#include "../header/commands.h"
#include "../header/monitor_manager.h"
#include "../header/task_manager.h"

volatile sig_atomic_t sigint_received = 0;

// Signal handler for SIGINT (ctrl+c)
void handle_sigint(int) {
    sigint_received = 1;
}

int main(int argc, char *argv[]) {
    int fss_in, fss_out;
    
    // Default values
    char log_file[PATH_MAX] = "";
    char config_file[PATH_MAX] = "";
    int worker_limit = 0;
    
    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "l:c:n:")) != -1) {
        switch (opt) {
            case 'l':
                strncpy(log_file, optarg, PATH_MAX - 1);
                log_file[PATH_MAX - 1] = '\0';
                break;
            case 'c':
                strncpy(config_file, optarg, PATH_MAX - 1);
                config_file[PATH_MAX - 1] = '\0';
                break;
            case 'n':
                worker_limit = atoi(optarg);
                if (worker_limit <= 0) {
                    printf("Worker limit must be a positive number\n");
                    exit(1);
                }
                break;
            default:
                printf("Usage: %s -l <log_file> -c <config_file> -n <worker_limit>\n", argv[0]);
                exit(1);
        }
    }
    if (!strlen(log_file) || !strlen(config_file) || worker_limit <= 0) {
        printf("Usage: %s -l <log_file> -c <config_file> -n <worker_limit>\n", argv[0]);
        exit(1);
    }
    
    // Open or create log file
    int log_fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd < 0) {
        perror("Error opening log file");
        exit(1);
    }
    
    // Initialize worker manager
    initWorkerManager(worker_limit);
    
    // Read config file and store data to sync_info
    int num_dirs = 0;
    if (access(config_file, F_OK) == 0) {
        num_dirs = readConfig(config_file);
        if (num_dirs == 0) printf("No directories found in config file. Starting with empty configuration.\n");
    } else {
        printf("Config file not found or not accessible. Starting with empty configuration.\n");
    }

    // Check if pipes exist and unlink them
    if (access("fss_in", F_OK) == 0) {
        if (unlink("fss_in") == -1) { perror("unlink: fss_in"); exit(1); }
    }
    if (access("fss_out", F_OK) == 0) {
        if (unlink("fss_out") == -1) { perror("unlink: fss_out"); exit(1); }
    }

    // Create named pipes
    if (mkfifo("fss_in", 0666) == -1){  // Receives commands from console
        if (errno != EEXIST){ perror("mkfifo: fss_in"); exit(1); }
    }
    if (mkfifo("fss_out", 0666) == -1){ // Sends responses to console
        if (errno != EEXIST){ perror("mkfifo: fss_out"); exit(1); }
    }

    // Open pipes
    if ((fss_in = open("fss_in", O_RDONLY | O_NONBLOCK)) < 0) {
        perror("fifo open error: fss_in"); exit(1);
    }
    if ((fss_out = open("fss_out", O_WRONLY)) < 0) {
        perror("fifo open error: fss_out"); exit(1);
    }

    // Initialize monitor manager (inotify)
    int monitor_fd = initMonitorManager();
    if (monitor_fd < 0) {
        perror("Failed to initialize inotify");
        exit(1);
    }

    // Set up polling for both inotify and command input
    struct pollfd fds[2];
    fds[0].fd = fss_in;
    fds[0].events = POLLIN;
    fds[1].fd = monitor_fd;
    fds[1].events = POLLIN;

    // Initial syncronization
    for (auto& pair : sync_info) {
        commandAdd(pair.second.source_dir, pair.second.target_dir, fss_out, log_fd, monitor_fd);
    }

    // Register signal handler
    signal(SIGINT, handle_sigint);

    // Polling loop
    for (;;) {
        if (sigint_received) {
            commandShutdown(fss_out, log_fd);
            break;
        }

        // Check if we need to process completed workers
        if (worker_finished_flag) {
            processFinishedWorker(fss_out, log_fd);
        }
        
        // Start worker processes for queued tasks
        startWorker();
        
        int poll_result = poll(fds, 2, 100);
        
        if (poll_result < 0) {
            // Error in poll
            if (errno == EINTR) {
                // Interrupted by signal, just continue
                continue;
            }
            perror("poll");
            break;
        } else if (poll_result == 0) {
            // Timeout, no events
            continue;
        }
        
        // Check for inotify events
        if (fds[1].revents & POLLIN) {
            handleDirChange(monitor_fd, fss_out, log_fd);
        }
        
        // Check for command input
        if (fds[0].revents & POLLIN) {
            // Read data in chunks
            char temp_buf[128];  // to read data in chunks
            char* buffer_in = NULL;
            size_t total_size = 0;
            ssize_t bytes_read;
            
            // Read data in chunks and reallocate as needed
            while ((bytes_read = read(fss_in, temp_buf, sizeof(temp_buf))) > 0) {
                char* new_buf = (char*)realloc(buffer_in, total_size + bytes_read + 1);
                if (!new_buf) {
                    perror("Failed to allocate memory for command");
                    free(buffer_in);
                    break;
                }
                buffer_in = new_buf;
                
                // Copy new data into expanded buffer
                memcpy(buffer_in + total_size, temp_buf, bytes_read);
                total_size += bytes_read;
                buffer_in[total_size] = '\0';  // Null-terminate
                
                if (bytes_read < (ssize_t)sizeof(temp_buf))
                    break;
            }
            
            if (bytes_read < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {  // No data available right now
                    free(buffer_in);
                    continue;
                } else {
                    perror("read from fss_in");
                    free(buffer_in);
                    break;
                }
            } else if (buffer_in && total_size > 0) {
                // Parse the command string
                char cmd[16] = "";
                char src_dir[PATH_MAX] = "";
                char trg_dir[PATH_MAX] = "";
                
                // Parse command and arguments
                int parsed_num = sscanf(buffer_in, "%s %s %s", cmd, src_dir, trg_dir);
                
                if (parsed_num >= 1) {
                    // Check which command was given
                    if (strcmp(cmd, "add") == 0 && parsed_num == 3) {
                        commandAdd(src_dir, trg_dir, fss_out, log_fd, monitor_fd);
                    } else if (strcmp(cmd, "cancel") == 0 && parsed_num == 2) {
                        commandCancel(src_dir, fss_out, log_fd, monitor_fd);
                    } else if (strcmp(cmd, "status") == 0 && parsed_num == 2) {
                        commandStatus(src_dir, fss_out);
                    } else if (strcmp(cmd, "sync") == 0 && parsed_num == 2) {
                        commandSync(src_dir, fss_out, log_fd, monitor_fd);
                    } else if (strcmp(cmd, "delete") == 0 && parsed_num == 2) {
                        commandDelete(src_dir, fss_out, log_fd);
                    } else if (strcmp(cmd, "shutdown") == 0) {
                        free(buffer_in);
                        commandShutdown(fss_out, log_fd);
                        break;
                    } else {
                        // Unknown or invalid command format
                        forwardMessage("Invalid command!\n", fss_out, -1);
                    }
                } else {
                    forwardMessage("Invalid command!\n", fss_out, -1);
                }
                free(buffer_in);
            }
        }
    }

    // Close file descriptors and cleanup
    close(fss_in);
    close(fss_out);
    close(log_fd);
    
    // Remove named pipes
    unlink("fss_in");
    unlink("fss_out");
    
    // Clean up resources from sync info, task manager and inotify
    shutdownWorkerManager();
    shutdownMonitorManager(monitor_fd);
    cleanupAllSyncInfo();
    exit(0);
}