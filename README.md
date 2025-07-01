# Local File Synchronization System

This project implements a local file synchronization system in C++. The system automatically monitors directories for changes and synchronizes them with corresponding target directories in real-time. It is composed of three main parts: the `fss_manager`, which acts as the central coordinator for all operations; the `fss_console`, which provides a user interface for sending commands to the manager via named pipes; and the `worker` executable, which is spawned by the manager to handle the actual file copy and delete operations. The system uses the **inotify** API to efficiently monitor filesystem events and manages a pool of workers to handle multiple synchronization tasks concurrently.

---

## Compilation Instructions

A `Makefile` is provided for easy compilation of all project components.

* **Compile all components:**
    ```bash
    make all
    ```
    This command will create a `bin/` directory and place the `fss_manager`, `fss_console`, and `worker` executables inside it.

* **Clean the project:**
    ```bash
    make clean
    ```
    This will remove the `bin/` directory and all compiled executables.

* **Compile a specific component:**
    You can compile each program individually. The `bin/` directory will be created if it doesn't exist.
    ```bash
    make fss_manager
    make fss_console
    make worker
    ```

* **Clean a specific component:**
    ```bash
    make clean-fss_manager
    make clean-fss_console
    make clean-worker
    ```

---

## Running the System

To run the system, you must first start the `fss_manager` process, which runs in the background. Then, you can use the `fss_console` to send it commands.

**Important Note:** The `fss_manager` and `fss_console` must be executed from the root directory of the project repository.

### 1. Start the fss_manager

The manager is the core component that listens for file system changes and console commands.

* **Execution Command:**
    ```bash
    ./bin/fss_manager -l <log_file> -c <config_file> -n <worker_limit>
    ```
    * `<log_file>`: The path to the log file for manager and worker operations.
    * `<config_file>`: The path to the configuration file that specifies the initial source/target directory pairs to synchronize.
    * `<worker_limit>`: The maximum number of concurrent worker processes.

### 2. Use the fss_console

The console connects to the running `fss_manager` to issue commands.

* **Execution Command:**
    ```bash
    ./bin/fss_console -l <log_file>
    ```
    * `<log_file>`: The path to the log file for console commands and their responses.

* **Available Commands (in console):**
    * `add <source_dir> <target_dir>`: Adds a new directory pair to monitor and synchronize.
    * `sync <source_dir>`: Manually triggers a full synchronization for a monitored directory.
    * `cancel <source_dir>`: Stops monitoring a directory for changes.
    * `status <source_dir | all>`: Displays the synchronization status for a specific directory or for all monitored directories.
    * `shutdown`: Terminates all pending tasks and shuts down the `fss_manager` gracefully.

### 3. Utility Script

A helper script, `fss_script.sh`, is also provided for querying log data.

* **Execution Command:**
    ```bash
    ./fss_script.sh -p <path> -c <command>
    ```
    * `<path>`: The path to the manager's log file.
    * `<command>`: Can be one of the following:
        * `listAll`: Shows the latest synchronization status for all directories found in the log.
        * `listMonitored`: Shows the status for directories that are currently being monitored.
        * `listStopped`: Shows the status for directories that are no longer monitored.
        * `purge`: Deletes the specified log file or directory.
