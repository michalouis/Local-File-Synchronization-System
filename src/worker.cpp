#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#define BUFFER_SIZE 4096
#define ERROR_BUFFER_SIZE 8192

// Define operation status codes
#define STATUS_SUCCESS 0
#define STATUS_PARTIAL 1
#define STATUS_ERROR 2

// Operation statistics structure
typedef struct {
    int copied;     // Number of files copied
    int skipped;    // Number of files skipped
    int deleted;    // Number of files deleted
    int status;     // Operation status (SUCCESS, PARTIAL, ERROR)
} operation_stats;

///// HELPER FUNCTIONS /////

// Function to copy a file from source to target directory
int copyFile(const char* source, const char* target) {
    int source_fd, target_fd;
    ssize_t bytes_read, bytes_written;
    char buffer[BUFFER_SIZE];
    
    // Open source file for reading
    source_fd = open(source, O_RDONLY);
    if (source_fd < 0) {
        return -1;
    }
    
    // Open file in target dir for writing (O_CREAT -> create if not exists, O_TRUNC -> to be able to ovewrite)
    target_fd = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (target_fd < 0) {
        close(source_fd);
        return -1;
    }
    
    // Copy data
    while ((bytes_read = read(source_fd, buffer, BUFFER_SIZE)) > 0) {
        bytes_written = write(target_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {  // if error
            close(source_fd);
            close(target_fd);
            return -1;
        }
    }
    
    // Close file descriptors
    close(source_fd);
    close(target_fd);
    
    return 0;
}

// Helper function to delete obsolete files in target directory
void deleteObsoleteFile(const char* source, const char* target,
                        char* error_buffer, operation_stats* stats) {
    DIR* target_dir;
    struct dirent* entry;
    char file_src_path[PATH_MAX];
    char file_trg_path[PATH_MAX];
    
    // Open target directory
    target_dir = opendir(target);
    if (target_dir == NULL) {
        sprintf(error_buffer + strlen(error_buffer), 
                "- (Obsolete files deletion) Target directory: %s\n", strerror(errno));
        return;
    }
    
    // Process each entry in target directory
    while ((entry = readdir(target_dir)) != NULL) {
        // Skip . (directory) and .. (parent directory)
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        // Construct paths
        snprintf(file_src_path, PATH_MAX, "%s/%s", source, entry->d_name);
        snprintf(file_trg_path, PATH_MAX, "%s/%s", target, entry->d_name);
        
        // Check if file exists in source
        if (access(file_src_path, F_OK) != 0) {
            // File doesn't exist in source, delete from target
            if (unlink(file_trg_path) == 0) {
                stats->deleted++;  // Successfully deleted
            } else {
                sprintf(error_buffer + strlen(error_buffer), 
                        "- (Obsolete files deletion) File: %s - %s\n", entry->d_name, strerror(errno));
                stats->skipped++;  // Count files that couldn't be deleted
            }
        }
    }
    
    closedir(target_dir);
}

// Print execution report based on operation statistics
void printReport(operation_stats stats, const char* error_buffer, const char* operation, const char* filename) {
    printf("EXEC_REPORT_START\n");
    
    // Print operation status
    printf("STATUS: %s\n", (stats.status == STATUS_SUCCESS) ? "SUCCESS" : 
           (stats.status == STATUS_PARTIAL) ? "PARTIAL" : "ERROR");
    
    // Print details section based on operation type
    printf("DETAILS: ");
    if (strcmp(operation, "FULL") == 0 || strcmp(operation, "SYNC") == 0) {
        // For FULL/SYNC operations, show statistics with non-zero values
        bool print_details = false;
        
        if (stats.copied > 0) {
            printf("%d files copied", stats.copied);
            print_details = true;
        }
        
        if (stats.skipped > 0) {
            printf("%s%d files skipped", print_details ? ", " : "", stats.skipped);
            print_details = true;
        }
        
        if (stats.deleted > 0) {
            printf("%s%d obsolete files deleted", print_details ? ", " : "", stats.deleted);
        }
    } else {
        // For ADDED, MODIFIED & DELETED operations, just show the file
        printf("File: %s", filename);
    }
    
    printf("\n");
    
    // Print errors if any
    if (strlen(error_buffer) > 0) {
        printf("ERRORS:\n%s", error_buffer);
    }
    
    printf("EXEC_REPORT_END\n");
}

///// OPERATIONS /////

// OPERATION: FULL (Syncs all files from source to target)
operation_stats operationFullSync(const char* source, const char* target, char* error_buffer) {
    DIR *source_dir, *target_dir;
    struct dirent* entry;
    operation_stats stats = {0, 0, 0, STATUS_SUCCESS};
    char file_src_path[PATH_MAX];
    char file_trg_path[PATH_MAX];
    
    // Check if target directory exists
    target_dir = opendir(target);
    if (target_dir == NULL) {
        sprintf(error_buffer + strlen(error_buffer), 
                "- Target directory: %s\n", strerror(errno));
        stats.status = STATUS_ERROR;
        return stats;
    }
    closedir(target_dir);  // Close immediately since we just needed to check existence

    // Open source directory
    source_dir = opendir(source);
    if (source_dir == NULL) {
        sprintf(error_buffer + strlen(error_buffer), 
                "- Source directory: %s\n", strerror(errno));
        stats.status = STATUS_ERROR;
        return stats;
    }
    
    // Process each entry in source directory
    while ((entry = readdir(source_dir)) != NULL) {
        // Skip . (directory) and .. (parent directory)
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        // Construct full paths
        snprintf(file_src_path, PATH_MAX, "%s/%s", source, entry->d_name);
        snprintf(file_trg_path, PATH_MAX, "%s/%s", target, entry->d_name);
        
        // Copy the file
        if (copyFile(file_src_path, file_trg_path) == 0) {
            stats.copied++;
        } else {
            stats.skipped++;
            sprintf(error_buffer + strlen(error_buffer), 
                    "- File: %s - %s\n", entry->d_name, strerror(errno));
        }
    }
    
    closedir(source_dir);
    
    // Delete obsolete files in target
    deleteObsoleteFile(source, target, error_buffer, &stats);
    
    // Set status based on the operation's statisitcs
    if (stats.skipped > 0) {
        if (stats.copied > 0 || stats.deleted > 0) {
            stats.status = STATUS_PARTIAL;
        } else {
            stats.status = STATUS_ERROR;
        }
    }
    
    return stats;
}

// OPERATION: ADDED/MODIFIED (Wrte/Overwrite a file from source to target)
operation_stats operationWrite(const char* source, const char* target, const char* filename, char* error_buffer) {
    operation_stats stats = {0, 0, 0, STATUS_SUCCESS};
    char file_src_path[PATH_MAX];
    char file_trg_path[PATH_MAX];
    
    // Check if target directory exists
    DIR* target_dir = opendir(target);
    if (target_dir == NULL) {
        sprintf(error_buffer + strlen(error_buffer), 
                "- File '%s': %s\n", filename, strerror(errno));
        stats.status = STATUS_ERROR;
        return stats;
    }
    closedir(target_dir);
    
    // Construct full paths for the specific file
    snprintf(file_src_path, PATH_MAX, "%s/%s", source, filename);
    snprintf(file_trg_path, PATH_MAX, "%s/%s", target, filename);
    
    // Check if source file exists
    if (access(file_src_path, F_OK) != 0) {
        sprintf(error_buffer + strlen(error_buffer), 
                "- File '%s': %s\n", filename, strerror(errno));
        stats.skipped++;
        stats.status = STATUS_ERROR;
        return stats;
    }
        
    // Copy the file
    if (copyFile(file_src_path, file_trg_path) == 0) {
        stats.copied++;
    } else {
        stats.skipped++;
        sprintf(error_buffer + strlen(error_buffer), 
                "- File: %s - %s\n", filename, strerror(errno));
        stats.status = STATUS_ERROR;
    }
    
    return stats;
}

// OPERATION: DELETED (Remove file from the target directory)
operation_stats operationDelete(const char* target, const char* filename, char* error_buffer) {
    operation_stats stats = {0, 0, 0, STATUS_SUCCESS};
    char file_trg_path[PATH_MAX];
    
    // Check if target directory exists
    DIR* target_dir = opendir(target);
    if (target_dir == NULL) {
        sprintf(error_buffer + strlen(error_buffer), 
                "- File '%s': %s\n", filename, strerror(errno));
        stats.status = STATUS_ERROR;
        return stats;
    }
    closedir(target_dir);
    
    // Construct full path for the file to delete
    snprintf(file_trg_path, PATH_MAX, "%s/%s", target, filename);
    
    // Check if file exists in target (if it's already deleted, consider it a success)
    if (access(file_trg_path, F_OK) != 0) return stats;
    
    // Delete the file
    if (unlink(file_trg_path) == 0) {
        stats.deleted++;
    } else {
        stats.skipped++;
        sprintf(error_buffer + strlen(error_buffer), 
                "- File: %s - %s\n", filename, strerror(errno));
        stats.status = STATUS_ERROR;
    }
    
    return stats;
}

///// MAIN FUNCTION /////

int main(int argc, char* argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <source_dir> <target_dir> <filename> <operation>\n", argv[0]);
        return 1;
    }
    
    char* source_dir = argv[1];
    char* target_dir = argv[2];
    char* filename = argv[3];
    char* operation = argv[4];
    
    // Buffer to store error messages
    char error_buffer[ERROR_BUFFER_SIZE] = "";
    
    operation_stats stats;
    
    // Perform operation
    if (strcmp(operation, "FULL") == 0 || strcmp(operation, "SYNC") == 0) {
        stats = operationFullSync(source_dir, target_dir, error_buffer);
    } else if (strcmp(operation, "ADDED") == 0 || strcmp(operation, "MODIFIED") == 0) {
        stats = operationWrite(source_dir, target_dir, filename, error_buffer);
    } else if (strcmp(operation, "DELETED") == 0) {
        stats = operationDelete(target_dir, filename, error_buffer);
    } else {
        fprintf(stderr, "Unknown operation: %s\n", operation);
        return 1;
    }
    
    // Generate and send report
    printReport(stats, error_buffer, operation, filename);
    
    // Exit with status code
    return (stats.status == STATUS_SUCCESS) ? 0 : 1;
}