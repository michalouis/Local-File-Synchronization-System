#ifndef MESSAGE_UTILS_H
#define MESSAGE_UTILS_H

#include <limits.h>  // For PATH_MAX
#include <sys/types.h>

// message_utils: Functions to handle timestamp generation, message formatting and buffering

// Buffer sizes
#define TIMESTAMP_SIZE 23
#define COMMAND_BUF_S (16 + 2*PATH_MAX) // ((SOURCE & TARGET PATHS) + COMMAND)

// Returns timestamp string in [YYYY/MM/DD HH:MM:SS] format
char* getTimestamp();

// Add timestamp to the beginning of a message, returns the new exapnded message
char* addTimestampToMessage(char* msg, const char* custom_timestamp);

// Concatenate a message to an existing dynamically allocated buffer
// If the buffer is NULL, a new buffer is created
// Returns the new or expanded buffer
char* appendToBuffer(char* existing_buffer, const char* msg);

// Sends buffered messages to the log file (if log_fd > 0) and to the console's terminal (if fss_out > 0)
void forwardMessage(const char* msg, int fss_out, int log_fd);

#endif