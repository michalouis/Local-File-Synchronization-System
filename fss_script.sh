#!/bin/bash

# Default values
path=""
command=""

# Display usage function
usage() {
    echo "Usage: $0 -p <path> -c <command>"
    echo "Commands:"
    echo "  purge - Deletes the file or directory at path"
    echo "  listAll - Lists all directories with last sync time and status"
    echo "  listMonitored - Lists all directories that are currently being monitored"
    echo "  listStopped   - Lists all directories that are no longer being monitored"
    exit 1
}

# Parse command line arguments
while getopts "p:c:" opt; do
    case $opt in
        p) path="$OPTARG" ;;
        c) command="$OPTARG" ;;
        *) usage ;;
    esac
done

# Check if both parameters are provided
if [ -z "$path" ] || [ -z "$command" ]; then
    usage
fi

# ListAll Command
if [ "$command" = "listAll" ]; then
    if [ ! -f "$path" ]; then
        echo "Error: Log file not found at $path"
        exit 1
    fi

    # Format: [DATE TIME] [SOURCE_DIR] [TARGET_DIR] [WORKER_PID] [OPERATION] [STATUS] [DETAILS]
    # Field:   1    2      3            4            5            6           7        8+
    awk '
    {
        # Check if it is a sync output message, if yes store the info we need
        if ($7 ~ /^\[(SUCCESS|PARTIAL|ERROR)\]$/) {
            src = substr($3, 2, length($3)-2)

            src_path[src] = src
            trg_path[src] = substr($4, 2, length($4)-2)
            date[src] = substr($1, 2, length($1)-1)
            time[src] = substr($2, 1, length($2)-1)
            status[src] = $7
        }
    }

    END {
        for (s in src_path) {
            print src_path[s], "->", trg_path[s], "[Last Sync:", date[s], time[s] "]", status[s]
        }
    }
    ' "$path"

# listMonitored and listStopped Commands
elif [ "$command" = "listMonitored" ] || [ "$command" = "listStopped" ]; then
    if [ ! -f "$path" ]; then
        echo "Error: Log file not found at $path"
        exit 1
    fi

    # Get monitoring status from messages of this format:
    # Format: [DATE TIME] Monitoring started/stopped for <source_dir>
    # Field:   1    2     3          4               5    6

    # Get latest sync info from messages of this format:
    # Format: [DATE TIME] [SOURCE_DIR] [TARGET_DIR] [WORKER_PID] [OPERATION] [STATUS] [DETAILS]
    # Field:   1    2      3            4            5            6           7        8+
    awk -v command="$command" '
    {
        # Check if it is a monitoring status message
        if ($3 == "Monitoring") {
            src = $6
            if ($4 == "started") {
                isMonitoring[src] = 1
            } else if ($4 == "stopped") {
                isMonitoring[src] = 0
            }
        }

        # Check if it is a sync output message, if yes store the last sync info
        if ($7 ~ /^\[(SUCCESS|PARTIAL|ERROR)\]$/) {
            src = substr($3, 2, length($3)-2)
            src_path[src] = src
            trg_path[src] = substr($4, 2, length($4)-2)
            date[src] = substr($1, 2, length($1)-1)
            time[src] = substr($2, 1, length($2)-1)
        }
    }
    END {
        foundResults = 0
        for (s in src_path) {
            if ((command == "listMonitored" && isMonitoring[s] == 1) || (command == "listStopped" && isMonitoring[s] == 0)) {
                print src_path[s], "->", trg_path[s], "[Last Sync:", date[s], time[s] "]"
                foundResults = 1
            }
        }
        if (foundResults == 0) print "No directories found for this command."
    }
    ' "$path"

# Purge Command
elif [ "$command" = "purge" ]; then
    # checks if file/directory exists
    if [ ! -e "$path" ]; then
        echo "Error: File or directory not found at $path"
        exit 1
    fi
    
    echo "Deleting $path..."
    
    if [ -d "$path" ]; then # if it's a directory
        rm -rf "$path"      # delete directory and its contents (recursively)
    else
        rm -f "$path"       # just delete file
    fi
    
    echo "Purge complete."
else
    echo "Error: Unknown command '$command'"
    usage
fi

exit 0