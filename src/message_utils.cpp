#include "../header/message_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

// message_utils: Functions to handle timestamp generation, message formatting and buffering

// Returns dynamically allocated timestamp string in [YYYY/MM/DD HH:MM:SS] format
// Caller is responsible for freeing the returned string
char* getTimestamp() {
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    
    // Allocate memory for timestamp
    char* timestamp_str = (char*)malloc(TIMESTAMP_SIZE);
    if (!timestamp_str) {
        perror("Memory allocation failed for timestamp");
        return NULL;
    }
    
    strftime(timestamp_str, TIMESTAMP_SIZE, "[%Y-%m-%d %H:%M:%S] ", tm_now);
    return timestamp_str;
}

// Add timestamp to the beginning of a message
// Returns reallocated message buffer with timestamp
char* addTimestampToMessage(char* msg, const char* custom_timestamp) {
    if (!msg) return NULL;
    
    // Get the timestamp
    char* timestamp;
    if (custom_timestamp != NULL) {
        timestamp = strdup(custom_timestamp);
    } else {
        timestamp = getTimestamp();
    }
    if (!timestamp) return msg;  // Return original if timestamp fails
    
    // Reallocate the original msg buffer to fit timestamp + original content
    size_t timestamp_len = strlen(timestamp);
    size_t msg_len = strlen(msg);
    size_t total_len = timestamp_len + msg_len + 1;
    
    char* new_msg = (char*)realloc(msg, total_len);
    if (!new_msg) {
        free(timestamp);
        return msg;  // Return original if realloc fails
    }
    
    memmove(new_msg + timestamp_len, new_msg, msg_len + 1); // Move original message content to make room for timestamp
    memcpy(new_msg, timestamp, timestamp_len);  // Copy timestamp to beginning of buffer
    
    free(timestamp);
    return new_msg;  // Return the possibly reallocated pointer
}

// Concatenate a message to an existing dynamically allocated buffer
// If the buffer is NULL, a new buffer is created
// Returns the new or expanded buffer; caller is responsible for freeing it
char* appendToBuffer(char* existing_buffer, const char* msg) {
    if (!msg || strlen(msg) == 0) {
        return existing_buffer;  // Nothing to append
    }
    
    size_t msg_len = strlen(msg);
    
    if (!existing_buffer) {
        // No existing buffer, just duplicate the message
        return strdup(msg);
    }
    
    size_t buffer_len = strlen(existing_buffer);
    size_t new_len = buffer_len + msg_len + 1;  // +1 for null terminator
    
    // Reallocate buffer to fit the additional message
    char* new_buffer = (char*)realloc(existing_buffer, new_len);
    if (!new_buffer) {
        // If reallocation fails, don't lose the original buffer
        perror("Memory reallocation failed for appending message");
        return existing_buffer;
    }
    
    // Append message to buffer
    strcat(new_buffer, msg);
    return new_buffer;
}

// Sends messages to log file and console's terminal
void forwardMessage(const char* msg, int fss_out, int log_fd) {
    if (!msg) return;
    
    if (log_fd > 0) {
        // Write message to log file
        write(log_fd, msg, strlen(msg));
    }
    
    if (fss_out != -1) {
        // Write message to console's terminal in chunks
        size_t msg_len = strlen(msg);
        size_t bytes_sent = 0;
        
        while (bytes_sent < msg_len) {
            // Calculate how many bytes to send in this chunk
            size_t chunk_size = (msg_len - bytes_sent) < PIPE_BUF ? 
                               (msg_len - bytes_sent) : PIPE_BUF;
            
            // Write the chunk
            ssize_t written = write(fss_out, msg + bytes_sent, chunk_size);
            
            if (written < 0) {
                if (errno == EINTR) {
                    // Interrupted by signal, try again
                    continue;
                }
                
                // Real error
                perror("Error sending message to console");
                break;
            }
            
            // Update the number of bytes sent
            bytes_sent += written;
        }
    }
}