#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include "../header/message_utils.h"

#define COMMAND_BUF_S (16 + 2*PATH_MAX) // ((SOURCE & TARGET PATHS) + COMMAND)

volatile sig_atomic_t sigint_received = 0;

// Signal handler for SIGINT (ctrl+c)
void handle_sigint(int) {
    sigint_received = 1;
}

int main(int argc, char* argv[]) {
    int fss_in, fss_out;
    int log_fd = -1;
    char command_buf[COMMAND_BUF_S];
    char log_file[PATH_MAX] = "";
    
    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "l:")) != -1) {
        switch (opt) {
            case 'l':
                strncpy(log_file, optarg, PATH_MAX - 1);
                log_file[PATH_MAX - 1] = '\0';
                break;
            default:
                printf("Usage: %s -l <log_file>\n", argv[0]);
                exit(1);
        }
    }
    if (!strlen(log_file)) {
        printf("Usage: %s -l <log_file>\n", argv[0]);
        exit(1);
    }
    
    // Open or create log file
    log_fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd < 0) {
        perror("Error opening console log file");
        exit(1);
    }

    // Open pipes
    if ((fss_in = open("fss_in", O_WRONLY)) < 0) {  // to send command
        perror("fifo open error: fss_in");
        exit(1);
    }
    if ((fss_out = open("fss_out", O_RDONLY | O_NONBLOCK)) < 0) {   // to receive response
        perror("fifo open error: fss_out");
        exit(1);
    }
    
    // Give manager a moment to start initializing
    usleep(100000);

    // Set up SIGINT handler
    signal(SIGINT, handle_sigint);

    // Initialize pollfd array once
    struct pollfd pfds[2];
    pfds[0].fd = fss_out;      // respone message from manager
    pfds[0].events = POLLIN;
    pfds[1].fd = STDIN_FILENO; // user input
    pfds[1].events = POLLIN;

    int sigint_shutdown_requested = 0;

    for(;;) {
        if (poll(pfds, 2, 100) < 0) {
            if (errno == EINTR) continue; // Interrupted by signal (SIGINT), just continue and shutdown
            perror("poll failed");
            break;
        }

        if (sigint_received && !sigint_shutdown_requested) {
            // Send shutdown command to manager only once
            write(fss_in, "shutdown\n", 9);
            sigint_shutdown_requested = 1;
            printf("\nShutdown requested, waiting for manager...\n");
        }

        // Check for any messages from manager
        if (pfds[0].revents & POLLIN) {
            char temp_buf[128];  // to read data in chunks
            char* output_buf = NULL;
            size_t total_size = 0;
            ssize_t bytes_read;
            
            // Read data in chunks and reallocate as needed
            while ((bytes_read = read(fss_out, temp_buf, sizeof(temp_buf))) > 0) {
                char* new_buf = (char*)realloc(output_buf, total_size + bytes_read + 1);
                if (!new_buf) {
                    perror("Failed to allocate memory for output");
                    free(output_buf);
                    break;
                }
                output_buf = new_buf;
                
                // Copy new data into expanded buffer
                memcpy(output_buf + total_size, temp_buf, bytes_read);
                total_size += bytes_read;
                output_buf[total_size] = '\0';  // Null-terminate
                
                if (bytes_read < (ssize_t)sizeof(temp_buf))
                    break;
            }
            
            if (output_buf && total_size > 0) {
                printf("\n%s\n", output_buf);
                
                // Log manager response
                forwardMessage(output_buf, -1, log_fd);
                
                // Check for shutdown message
                if (strstr(output_buf, "Manager shutdown complete") != NULL) {
                    printf("Shutting down console...\n");
                    free(output_buf);
                    close(fss_out);
                    close(fss_in);
                    close(log_fd);
                    exit(0);
                }

                // Print prompt again after manager message, if not shutting down
                if (!sigint_shutdown_requested) {
                    printf("> ");
                    fflush(stdout);
                }
            
                free(output_buf);
            }
        }

        // If shutdown has been requested, skip user input
        if (sigint_shutdown_requested)
            continue;

        // Check for user input
        if (pfds[1].revents & POLLIN) {
            memset(command_buf, 0, COMMAND_BUF_S);
            ssize_t bytes = read(STDIN_FILENO, command_buf, COMMAND_BUF_S - 1);
            if (bytes <= 0) {
                break;
            }
            // Remove trailing newline if present
            if (command_buf[bytes - 1] == '\n') {
                command_buf[bytes - 1] = '\0';
            } else {
                command_buf[bytes] = '\0';
            }
            if (strlen(command_buf) == 0) {
                // Print prompt again for empty input
                printf("> ");
                fflush(stdout);
                continue;
            }
            
            // Format and log the command
            char* temp_msg = NULL;
            int valid_command = 1;

            if (strncmp(command_buf, "add ", 4) == 0) {
                char source[PATH_MAX] = "";
                char target[PATH_MAX] = "";
                sscanf(command_buf + 4, "%s %s", source, target);
                
                // Allocate memory for formatted message
                temp_msg = (char*)malloc(strlen(source) + strlen(target) + 30);
                if (!temp_msg) {
                    perror("Memory allocation failed");
                    continue;
                }
                
                if (strlen(source) > 0 && strlen(target) > 0) {
                    sprintf(temp_msg, "Command add %s -> %s\n", source, target);
                } else {
                    sprintf(temp_msg, "Invalid command: add %s\n", command_buf + 4);
                    valid_command = 0;
                }
            } else {
                // Extract command name to check if valid
                char cmd[16] = "";
                sscanf(command_buf, "%s", cmd);
                
                // Allocate memory for formatted message
                temp_msg = (char*)malloc(strlen(command_buf) + 30);
                if (!temp_msg) {
                    perror("Memory allocation failed");
                    continue;
                }
                
                // Check if it's one of the known commands
                if (strcmp(cmd, "cancel") == 0 || 
                    strcmp(cmd, "status") == 0 || 
                    strcmp(cmd, "sync") == 0 || 
                    strcmp(cmd, "delete") == 0 || 
                    strcmp(cmd, "shutdown") == 0) {
                    sprintf(temp_msg, "Command %s\n", command_buf);
                } else {
                    sprintf(temp_msg, "Invalid command: %s\n", command_buf);
                    valid_command = 0;
                }
            }

            if (!valid_command) {
                // For invalid commands, add timestamp, print warning, log
                temp_msg = addTimestampToMessage(temp_msg, NULL);
                
                if (temp_msg) {
                    printf("%s", temp_msg);                    
                    forwardMessage(temp_msg, -1, log_fd);
                }
                
                free(temp_msg);
                printf("> ");
                fflush(stdout);
                continue;
            } else {
                // For valid commands, just add timestamp and log
                temp_msg = addTimestampToMessage(temp_msg, NULL);
                
                if (temp_msg) forwardMessage(temp_msg, -1, log_fd);                    
                
                free(temp_msg);
            }

            // Send message to manager
            size_t msg_len = strlen(command_buf);
            size_t bytes_sent = 0;

            while (bytes_sent < msg_len) {
                // Calculate how many bytes to send in this chunk
                size_t chunk_size = (msg_len - bytes_sent) < PIPE_BUF ? (msg_len - bytes_sent) : PIPE_BUF;
                                    
                // Write the chunk
                ssize_t written = write(fss_in, command_buf + bytes_sent, chunk_size);
                
                if (written < 0) {
                    if (errno == EINTR) {
                        // Interrupted by signal, try again
                        continue;
                    }
                    
                    // Real error - use existing error handling
                    perror("write to fss_in failed");
                    // Log the failure
                    char* error_msg = strdup("Failed to send command to manager\n");
                    if (error_msg) {
                        char* formatted_error = addTimestampToMessage(error_msg, NULL);
                        if (formatted_error) {
                            printf("%s", formatted_error);
                            forwardMessage(formatted_error, -1, log_fd);
                            free(formatted_error);
                        }
                        free(error_msg);
                    }
                    break;
                }
                
                // Update the number of bytes sent
                bytes_sent += written;
            }

            usleep(100000);  // Small delay to let manager process command
        }
    }

    close(fss_out);
    close(fss_in);
    close(log_fd);
    exit(0);
}