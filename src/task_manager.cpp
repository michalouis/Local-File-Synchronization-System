#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <queue>
#include <string>
#include "../header/task_manager.h"
#include "../header/message_utils.h"
#include "../header/sync_database.h"

// Task Manager: Functions related to managing the task queue and worker processes

// Global variables
std::queue<task_t> task_queue;
worker_info_t* active_workers = NULL;
int worker_count = 0;
int worker_limit = 5;  // Default value
volatile sig_atomic_t worker_finished_flag = 0;

///// HELPER FUNCTIONS /////

// Signal handler for SIGCHLD
static void signalHandler(int sig) {
    (void)sig; // mark is as unused to avoid compiler warnings
    
    worker_finished_flag = 1;
}

// Set up signal handlers for worker management
static void setupSignalHandler() {
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;   // if interrupted by a signal, restart the system call
    
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
}

// Process output from a worker
static int processWorkerOutput(int pipe_fd, const char* source, const char* target, int fss_out, int log_fd, const char* custom_timestamp) {
    char* buffer = (char*)malloc(4096);
    char* log_buffer = NULL;
    
    if (!buffer) {
        perror("Failed to allocate buffer");
        return 0;
    }

    // Get worker PID and index from active_workers
    pid_t worker_pid = 0;
    int worker_index = -1;
    for (int i = 0; i < worker_count; i++) {
        if (active_workers[i].pipe_fd == pipe_fd) {
            worker_pid = active_workers[i].pid;
            worker_index = i;  // Save the worker index
            break;
        }
    }
    
    // Make pipe non-blocking for reading
    int flags = fcntl(pipe_fd, F_GETFL, 0);
    fcntl(pipe_fd, F_SETFL, flags | O_NONBLOCK);
    
    ///// Process report /////
    char* status = NULL;
    char* details = NULL;
    char* error_list = NULL;
    size_t error_list_length = 0;

    ssize_t bytes_read;
    bool in_report = false;
    bool in_errors = false;
    int error_count = 0;
    while ((bytes_read = read(pipe_fd, buffer, 4095)) > 0) {
        buffer[bytes_read] = '\0';
        
        // Process it line by line
        char *line = buffer;
        char *next_line;
        
        while ((next_line = strchr(line, '\n')) != NULL) {
            *next_line = '\0';
            
            // Check for report markers
            if (strcmp(line, "EXEC_REPORT_START") == 0) {
                in_report = true;
            } else if (strcmp(line, "EXEC_REPORT_END") == 0) {
                in_report = false;
                in_errors = false;
            } else if (in_report) {
                if (strncmp(line, "STATUS: ", 8) == 0) {
                    status = strdup(line + 8);
                } else if (strncmp(line, "DETAILS: ", 9) == 0) {
                    details = strdup(line + 9);
                } else if (strcmp(line, "ERRORS:") == 0) {
                    in_errors = true;
                } else if (in_errors) {
                    // Count each error line
                    error_count++;
                    
                    // Collect error messages
                    char* temp = (char*)malloc(error_list_length + strlen(line) + 2);
                    if (temp) {
                        if (error_list) {
                            memcpy(temp, error_list, error_list_length);
                            free(error_list);
                        }
                        strcpy(temp + error_list_length, line);
                        error_list_length += strlen(line);
                        temp[error_list_length++] = '\n';
                        temp[error_list_length] = '\0';
                        error_list = temp;
                    }
                }
            }
            
            line = next_line + 1;
        }
    }
    
    ///// Generate completion message for sync command operation /////
    const char* operation = active_workers[worker_index].task.operation;
    if (strcmp(operation, "SYNC") == 0) {
        char* sync_msg = (char*)malloc(strlen(source) + strlen(target) + 50);
        if (sync_msg) {
            sprintf(sync_msg, "Sync completed %s -> %s Errors:%d\n", source, target, error_count);
            sync_msg = addTimestampToMessage(sync_msg, custom_timestamp);
            if (sync_msg) {
                // Send console message with new function
                forwardMessage(sync_msg, fss_out, log_fd);
                free(sync_msg);
            }
        }
    }
    
    ///// For log file - structured format /////
    // [TIMESTAMP] [SOURCE_DIR] [TARGET_DIR] [WORKER_PID] [OPERATION] [RESULT] [DETAILS]
    
    // Extract timestamp without brackets
    char* clean_timestamp = NULL;
    if (custom_timestamp) {
        size_t ts_len = strlen(custom_timestamp);
        if (ts_len > 3) {
            clean_timestamp = (char*)malloc(ts_len - 1);
            if (clean_timestamp) {
                strncpy(clean_timestamp, custom_timestamp + 1, ts_len - 3);
                clean_timestamp[ts_len - 3] = '\0';
            }
        }
    }
    if (!clean_timestamp) clean_timestamp = strdup("timestamp-error");
    
    // Choose appropriate details based on operation type
    const char* log_details = "";
    const char* op = active_workers[worker_index].task.operation;
    
    // Safety checks for NULL pointers
    if (!status) status = strdup("");
    if (!details) details = strdup("");
    if (!error_list) error_list = strdup("");
    
    // For FULL or SYNC operations, use the details field from the report
    if (op && (strcmp(op, "FULL") == 0 || strcmp(op, "SYNC") == 0)) {
        if (details && strlen(details) > 0) {
            log_details = details;
        }
    } else {
        if (error_count > 0 && error_list && strlen(error_list) > 0) {
            // Use the first line of error_list for details
            char* newline = strchr(error_list, '\n');
            if (newline) *newline = '\0';
            log_details = error_list;
        } else {
            // No errors, just use the filename if available
            log_details = details ? details : "";
        }
    }
    
    // Format log entry with
    log_buffer = (char*)malloc(strlen(clean_timestamp) + 
                               strlen(source ? source : "") + 
                               strlen(target ? target : "") + 
                               strlen(op ? op : "") + 
                               strlen(status ? status : "") + 
                               strlen(log_details ? log_details : "") + 100);
    if (log_buffer) {
        sprintf(log_buffer, "[%s] [%s] [%s] [%d] [%s] [%s] [%s]\n", 
                clean_timestamp, 
                source ? source : "", 
                target ? target : "", 
                (int)worker_pid,
                op ? op : "", 
                status ? status : "", 
                log_details ? log_details : "");
        
        // Send log message
        forwardMessage(log_buffer, -1, log_fd);
    }
    
    // Cleanup
    free(buffer);
    if (log_buffer) free(log_buffer);
    if (status) free(status);
    if (details) free(details);
    if (error_list) free(error_list);
    if (clean_timestamp) free(clean_timestamp);
    
    return error_count;
}

// Initialize a task
void initTask(task_t* task, const char* source, const char* target, 
            const char* filename, const char* operation) {
    // Allocate and copy source/target paths, filename and operation
    task->source = strdup(source);
    task->target = strdup(target);    
    task->filename = strdup(filename);
    task->operation = strdup(operation);
}

// Free all memory allocated for a task
void freeTaskMemory(task_t* task) {
    if (task->source) free(task->source);
    if (task->target) free(task->target);
    if (task->filename) free(task->filename);
    if (task->operation) free(task->operation);
    
    // Reset pointers to avoid double-free issues
    task->source = NULL;
    task->target = NULL;
    task->filename = NULL;
    task->operation = NULL;
}

// Copy task data from source to destination
void copyTask(task_t* dest, const task_t* src) {
    // First free any existing memory in destination
    freeTaskMemory(dest);
    
    // Then copy all fields with new memory allocation
    dest->source = strdup(src->source);
    dest->target = strdup(src->target);
    dest->filename = strdup(src->filename);
    dest->operation = strdup(src->operation);
}

///// MAIN FUNCTIONS /////

// Initialize worker management system
void initWorkerManager(int max_workers) {
    worker_limit = max_workers;
    worker_count = 0;
    
    // Allocate memory for the active_workers array based on worker_limit
    active_workers = (worker_info_t*)malloc(sizeof(worker_info_t) * worker_limit);
    if (!active_workers) {
        perror("Failed to allocate memory for worker array");
        exit(1);
    }
    
    setupSignalHandler();
}

// Add a new task to the queue
bool addTaskToQueue(const char* source, const char* target, const char* filename,
                    const char* operation, bool checkExistingTask) {
    // For sync command: check if the task is already queued
    if (checkExistingTask)
        if (isTaskQueued(source))
            return false;
    
    // Copy task details to the task structure
    task_t task;
    initTask(&task, source, target, filename ? filename : "", operation);
    
    task_queue.push(task);  // Add task to the queue
    return true; // Task was added successfully
}

// Check if any task is already queued or in progress for this directory
bool isTaskQueued(const char* directory) {
    // Check active workers for any task with this directory
    for (int i = 0; i < worker_count; i++) {
        if (strcmp(active_workers[i].task.source, directory) == 0) {
            return true;  // A worker is already processing this directory
        }
    }
    
    // Check queue for any task with this directory
    std::queue<task_t> temp_queue = task_queue;
    while (!temp_queue.empty()) {
        task_t task = temp_queue.front();
        temp_queue.pop();
        
        if (strcmp(task.source, directory) == 0) {
            return true;  // A task for this directory is already queued
        }
    }
    
    return false;  // No task found for this directory
}

// Start worker processes to handle tasks in the queue
void startWorker() {
    while (!task_queue.empty() && worker_count < worker_limit) {    // As long as there are tasks or workers available
        task_t task = task_queue.front();
        task_queue.pop();
        
        // Create pipe for worker output
        int pipe_fds[2];
        if (pipe(pipe_fds) == -1) {
            perror("pipe");
            task_queue.push(task);
            continue;
        }
        
        // Create child process
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("fork");
            close(pipe_fds[0]); 
            close(pipe_fds[1]);
            freeTaskMemory(&task);
            break;
        } else if (pid == 0) {    // Child process - worker
            close(pipe_fds[0]);  // Close read end
            
            // Redirect stdout to the pipe
            dup2(pipe_fds[1], STDOUT_FILENO);
            close(pipe_fds[1]);
            
            // Prepare arguments for the worker executable
            char *args[6];
            args[0] = (char*)"./bin/worker";  // Worker path
            args[1] = task.source;
            args[2] = task.target;
            args[3] = task.filename;
            args[4] = task.operation;
            args[5] = NULL;

            // Execute the worker
            execv(args[0], args);
            
            perror("execv");    // If execv fails, print error and exit
            exit(1);
        } else {  // Parent process - manager
            close(pipe_fds[1]);  // Close write end
            
            // Only now update the active_workers array
            active_workers[worker_count].pid = pid;
            active_workers[worker_count].pipe_fd = pipe_fds[0];
            active_workers[worker_count].task = task;
            worker_count++;
        }
    }
}

// Process finished workers and handle their output
void processFinishedWorker(int fss_out, int log_fd) {
    if (!worker_finished_flag) return;
    worker_finished_flag = 0;
    
    int status;
    pid_t pid;
    
    // Wait for all terminated children
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Find which worker terminated
        int worker_index = -1;
        for (int i = 0; i < worker_count; i++) {
            if (active_workers[i].pid == pid) {
                worker_index = i;
                break;
            }
        }
        
        if (worker_index >= 0) {
            int pipe_fd = active_workers[worker_index].pipe_fd;
            
            // Access source and target directly - no need for local copies
            const char* source = active_workers[worker_index].task.source;
            const char* target = active_workers[worker_index].task.target;
            
            // Generate timestamp once for both uses
            char* timestamp = getTimestamp();
            if (!timestamp) {
                // Handle allocation failure
                timestamp = strdup("[error] ");
                if (!timestamp) {
                    // Critical failure, continue with next worker
                    continue;
                }
            }
            
            // Process output using our timestamp
            int errors_num = processWorkerOutput(pipe_fd, source, target, fss_out, log_fd, timestamp);
            
            // Update the source directory's last_sync_time with the same timestamp, but without brackets
            sync_info_entry* info = getSyncInfo(source);
            if (info) {
                // Remove the brackets and trailing space: "[2025-01-01 12:30:45] " -> "2025-01-01 12:30:45"
                char* clean_timestamp = NULL;
                size_t ts_len = strlen(timestamp);
                if (ts_len > 3) {
                    clean_timestamp = (char*)malloc(ts_len);
                    if (clean_timestamp) {
                        strncpy(clean_timestamp, timestamp + 1, ts_len - 3);
                        clean_timestamp[ts_len - 3] = '\0';
                        
                        // Free old timestamp and set new one
                        free(info->last_sync_time);
                        info->last_sync_time = clean_timestamp;
                    }
                }
                
                info->error_count += errors_num;
            }
            
            // Free timestamp
            free(timestamp);
            
            // Close the pipe
            close(active_workers[worker_index].pipe_fd);
            
            // Remove this worker by shifting the array
            for (int j = worker_index; j < worker_count - 1; j++) {
                // Copy task data with proper memory allocation
                copyTask(&active_workers[j].task, &active_workers[j+1].task);
                active_workers[j].pid = active_workers[j + 1].pid;
                active_workers[j].pipe_fd = active_workers[j + 1].pipe_fd;
            }

            freeTaskMemory(&active_workers[worker_count - 1].task);      
            worker_count--;
        }
    }
}

// Wait for all active and queued sync tasks to finish
void finishTasks(int fss_out, int log_fd) {
    char* temp_msg = strdup("Waiting for all active workers to finish.\n");
    if (temp_msg) {
        temp_msg = addTimestampToMessage(temp_msg, NULL);
        if (temp_msg) {
            forwardMessage(temp_msg, fss_out, log_fd);
        }
        free(temp_msg);
    }
    
    // Finish active tasks
    while (worker_count > 0) {
        processFinishedWorker(fss_out, log_fd);
        
        // If there are still workers, wait a bit
        if (worker_count > 0) {
            usleep(100000);  // 100ms
        }
    }
    
    temp_msg = strdup("Processing remaining queued tasks.\n");
    if (temp_msg) {
        temp_msg = addTimestampToMessage(temp_msg, NULL);
        if (temp_msg) {
            forwardMessage(temp_msg, fss_out, log_fd);
            free(temp_msg);
        }
    }
    
    // Process remaining tasks in the queue
    while (!task_queue.empty() || worker_count > 0) {
        // Start workers to process queued tasks
        startWorker();
        
        // Wait for workers to finish their tasks
        while (worker_count > 0) {
            processFinishedWorker(fss_out, log_fd);
            
            if (worker_count > 0) {
                usleep(100000);  // 100ms
            }
        }
    }
}

// Free allocated memory for active workers
void shutdownWorkerManager() {
    if (active_workers) {
        // Free all task memory
        for (int i = 0; i < worker_count; i++) {
            freeTaskMemory(&active_workers[i].task);
        }
        free(active_workers);
        active_workers = NULL;
    }
    
    // Clear any tasks left in the queue
    while (!task_queue.empty()) {
        task_t task = task_queue.front();
        freeTaskMemory(&task);
        task_queue.pop();
    }
}