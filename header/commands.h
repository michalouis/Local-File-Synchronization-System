#ifndef COMMANDS_H
#define COMMANDS_H

// Commands: These functions handle the commands sent to the manager (+ custom delete command to remove directory data from memory)

// Add pair to sync_info and start monitoring it
void commandAdd(const char* source, const char* target, int fss_out, int log_fd, int inotify_fd);

// Stop monitoring directory 
void commandCancel(const char* source, int fss_out, int log_fd, int inotify_fd);

// Show status of directory (use "all" to print all directories)
void commandStatus(const char* source, int fss_out);

// Sync directory
void commandSync(const char* source, int fss_out, int log_fd, int inotify_fd);

// Delete directory from sync_info (only if inactive)
void commandDelete(const char* source, int fss_out, int log_fd);

// Shutdown the manager and clean up resources
void commandShutdown(int fss_out, int log_fd);

#endif // COMMANDS_H