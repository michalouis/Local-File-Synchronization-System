#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <queue>
#include <string>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/types.h>

// Task Manager: Functions related to managing the task queue and worker processes

// Sync operation structure
typedef struct {
    char* source;       // Source directory path
    char* target;       // Target directory path
    char* filename;     // File to synchronize (empty for full sync)
    char* operation;    // Operation type: ADDED, MODIFIED, DELETED, FULL or SYNC
} task_t;

// Worker process structure
typedef struct {
    pid_t pid;           // Process ID of worker
    int pipe_fd;         // File descriptor for reading worker output
    task_t task;         // The task this worker is processing
} worker_info_t;

extern volatile sig_atomic_t worker_finished_flag;

// Initialize worker management system
void initWorkerManager(int max_workers);

// Add a new task to the queue
bool addTaskToQueue(const char* source, const char* target, const char* filename,
                    const char* operation, bool checkExistingTask);

// Check if any task is already queued or in progress for this directory
bool isTaskQueued(const char* directory);

// Start worker processes to handle tasks in the queue
void startWorker();

// Process finished workers and handle their output
void processFinishedWorker(int fss_out, int log_fd);
    
// Wait for all active workers to terminate
void finishTasks(int fss_out, int log_fd);

// Free allocated memory for active workers
void shutdownWorkerManager();

#endif // TASK_MANAGER_H