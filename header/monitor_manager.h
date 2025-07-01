#ifndef DIRECTORY_MONITOR_H
#define DIRECTORY_MONITOR_H

// Monitor Manager: Using inotify, the following functions manage directory monitoring

// Initialize monitor manager (inotify), returns file descriptor
int initMonitorManager();

// Add directory to monitor by creating an inotify watch
int addDirToMonitor(int inotify_fd, const char* dir_path);

// Remove directory from monitor
int rmvDirFromMonitor(int inotify_fd, int wd);

// Handle changes in one of the monitoring directories (process inotify events)
void handleDirChange(int inotify_fd, int fss_out, int log_fd);

// Shutdown and clean up resources used by the monitor manager
void shutdownMonitorManager(int inotify_fd);

#endif // DIRECTORY_MONITOR_H