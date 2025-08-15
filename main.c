/**
 * @file main.c
 * @brief Implements DevCLI – a cross-platform CLI automation tool.
 *
 * @details This file contains the main logic for parsing a JSON tasks file,
 *          resolving dependencies, detecting the shell environment, and
 *          executing tasks. It uses the cJSON library for parsing and
 *          supports multiple shells (PowerShell, CMD, Bash).
 *
 * @author Riddhima
 * @date 2025-08-15
 * @version 1.0.0
 * @copyright MIT Licensed
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <stdbool.h>
#include <errno.h>
#include "cJSON.h"
/**
 * @def RED
 * @brief ANSI escape code for red-colored text.
 */
#define RED "\033[31m"

/**
 * @def GREEN
 * @brief ANSI escape code for green-colored text.
 */
#define GREEN "\033[32m"

/**
 * @def YELLOW
 * @brief ANSI escape code for yellow-colored text.
 */
#define YELLOW "\033[33m"

/**
 * @def BLUE
 * @brief ANSI escape code for blue-colored text.
 */
#define BLUE "\033[34m"

/**
 * @def RESET
 * @brief ANSI escape code to reset text formatting to default.
 */
#define RESET "\033[0m"

/**
 * @def LOG(msg, ...)
 * @brief Prints a formatted log message with file name and line number.
 *
 * @details Outputs messages in blue for context and green for the main text.
 *          Useful for debugging and tracking execution flow.
 *
 * @param msg The format string for the log message.
 * @param ... Optional arguments matching the format specifiers in `msg`.
 *
 * Example:
 * @code
 * LOG("Task %s executed successfully", taskName);
 * @endcode
 */
#define LOG(msg, ...) do{printf(BLUE "[%s:Line %d] [log]" RESET GREEN msg RESET "\n",__FILE__,__LINE__, ##__VA_ARGS__);}while(0)

/**
 * @def LOG_ERROR(msg, ...)
 * @brief Prints a formatted error message with file name and line number to stderr.
 *
 * @details Outputs messages in yellow for context and red for the error text.
 *          Helps identify error location quickly during debugging.
 *
 * @param msg The format string for the error message.
 * @param ... Optional arguments matching the format specifiers in `msg`.
 *
 * Example:
 * @code
 * LOG_ERROR("Failed to execute task: %s", taskName);
 * @endcode
 */
#define LOG_ERROR(msg, ...) do{fprintf(stderr,YELLOW "[%s:Line %d] [error]" RESET RED msg RESET "\n",__FILE__,__LINE__,##__VA_ARGS__);}while(0)

/**
 * @var shell
 * @brief Holds the name of the currently detected shell environment.
 *
 * @details This variable is set during platform and shell detection.
 *          It is used throughout the tool to decide which commands or
 *          syntax to use for execution (PowerShell, CMD, or Bash).
 */
const char *shell;

/**
 * @brief Detects the current shell environment and initializes the global `shell` variable.
 *
 * @details This function determines the shell being used by checking platform-specific
 *          indicators. On Windows, it checks for the presence of "PSModulePath" in
 *          environment variables to differentiate between PowerShell and CMD. On
 *          non-Windows systems, it defaults to "Linux".
 *
 *          The detected shell name is stored in the global variable `shell`, which
 *          is later used for selecting shell-specific commands.
 *
 * @note This function is called once during program startup and does not require
 *       manual invocation by the user. It ensures that `shell` is initialized for
 *       the program's entire runtime.
 *
 * @warning Altering this function or removing its call from `main()` may cause the
 *          `shell` variable to remain uninitialized, leading to undefined behavior.
 *
 * @see shell
 */
void detectShell(){
    #ifdef _WIN32
        char *PS= getenv("PSModulePath");
        if(PS && strstr(PS,"WindowsPowerShell")){
            shell="Powershell";
        }
        else{
            shell="CMD";
        }
    #else
        shell="Linux";
    #endif
    LOG("Shell Detected: %s", shell);
}

/**
 * @brief Resolves the file path of the `tasks.json` configuration file.
 *
 * @details This function determines the location of `tasks.json` using the following
 *          priority order:
 *          1. Cached path from `.devcli_config` file (if valid).
 *          2. Parent directory path (`../tasks.json`).
 *          3. Current directory path (`./tasks.json`).
 *          4. Manual user input (prompted if none of the above are found).
 *
 *          If the user provides a valid path manually, it is stored in
 *          `.devcli_config` for subsequent executions in the same environment.
 *
 *          The function allocates memory for the returned string using `strdup()`.
 *          In the current implementation, this memory is always freed later in the
 *          program during normal execution, so there is no risk of leaks unless
 *          this behavior is changed by modifying the code.
 *
 * @return char* A heap-allocated string containing the valid path to `tasks.json`.
 *               The current implementation ensures proper cleanup of this memory.
 *               Returns `NULL` if no valid path is found (including invalid user input).
 *
 * @note This function uses an internal static buffer for temporary storage but
 *       duplicates the final path using `strdup()` before returning it.
 *
 * @warning If `.devcli_config` contains an outdated path (e.g., user moved `tasks.json`
 *          manually), the function will fail on first attempt before falling back to
 *          manual input.
 *
 * @todo Implement validation for cached path before returning it to prevent edge-case errors.
 *
 * Example usage:
 * @code
 * char *jsonPath = resolveJSONPath();
 * if (jsonPath) {
 *     LOG("Resolved tasks.json path: %s", jsonPath);
 *     // In current implementation, memory will be freed later automatically.
 * } else {
 *     LOG_ERROR("Failed to locate tasks.json");
 * }
 * @endcode
 */
char* resolveJSONPath() {
    static char cachedPath[260]={0};
    FILE *f;
    f=fopen(".devcli_config","r");
    if(f && fgets(cachedPath,sizeof(cachedPath),f)){
        fclose(f);
        size_t len=strlen(cachedPath);
        if(len>0 && cachedPath[len-1]=='\n'){
            cachedPath[len-1]='\0';
        }
        f=fopen(cachedPath,"r");
        if(f){
            fclose(f);
            LOG("Using cached path from config: %s",cachedPath);
            return strdup(cachedPath);
        }
    }
    f=fopen("../tasks.json","r");
    if (f) {
        fclose(f);
        LOG("Using \"../tasks.json\"");
        strcpy(cachedPath,"../tasks.json");
        return strdup(cachedPath);
    }
    f=fopen("tasks.json","r");
    if (f) {
        fclose(f);
        LOG("Using \"./tasks.json\"");
        strcpy(cachedPath,"./tasks.json");
        return strdup(cachedPath);
    }
    printf(RED "Warning: tasks.json not found in expected locations.\n");
    printf(YELLOW "You may have altered the cloned setup. Please enter path to tasks.json manually: " RESET);
    fgets(cachedPath,sizeof(cachedPath),stdin);
    size_t len=strlen(cachedPath);
    if(len>0 && cachedPath[len-1]=='\n'){
        cachedPath[len-1]='\0';
    }
    f=fopen(cachedPath,"r");
    if (!f) {
        LOG_ERROR("Provided path is also invalid.");
        return NULL;
    }
    else{
        LOG("Using manually entered path: %s", cachedPath);
    }
    FILE *cfg=fopen(".devcli_config","w");
    if(cfg){
        fprintf(cfg,"%s\n",cachedPath);
        fclose(cfg);
    }
    fclose(f);
    return strdup(cachedPath);
}

/**
 * @brief Reads the entire contents of a file into a dynamically allocated buffer.
 *
 * @details This function opens the specified file in binary mode, determines its size,
 *          allocates a buffer of appropriate size (+1 for null terminator), and reads
 *          the file contents into it. The buffer is null-terminated before returning.
 *
 *          The returned buffer is primarily used as input for `cJSON_Parse()` to create
 *          the root JSON object, which is later freed after all commands are executed.
 *
 *          **Memory Management:** The function allocates memory for the buffer using
 *          `malloc()`. In the current implementation, the memory is always freed later
 *          in `main()` after use. There is no risk of leaks unless this behavior is
 *          changed by altering the code.
 *
 * @param path Path to the file to be read (typically `tasks.json`).
 *
 * @return char* Pointer to the heap-allocated buffer containing the file contents,
 *               or `NULL` if an error occurs (e.g., file not found, allocation failure).
 *
 * @note The buffer returned by this function must remain valid until parsing is complete.
 *       It is freed automatically in the current implementation after all commands execute.
 *
 * @warning If this function or the freeing logic in `main()` is modified incorrectly,
 *          memory leaks or invalid memory access may occur.
 *
 * Example usage:
 * @code
 * char *tasks=readFileToBuffer("tasks.json");
 * if (tasks) {
 *     cJSON *root = cJSON_Parse(tasks);
 *     // Process root...
 *     free(tasks); // Freed later in the current implementation
 *     cJSON_Delete(root);
 * } else {
 *     LOG_ERROR("Failed to load tasks.json");
 * }
 * @endcode
 */
char* readFileToBuffer(char *path){
    LOG("Reading file to buffer...");
    FILE* fptr;
    fptr=fopen(path,"rb");
    if(fptr==NULL){
        LOG_ERROR("File not opened: %s",strerror(errno));
        return NULL;
    }
    fseek(fptr,0,SEEK_END);
    size_t size=ftell(fptr);
    rewind(fptr);
    if(size==0){
        LOG_ERROR("The size of file could not be determined.");
        fclose(fptr);
        return NULL;
    }
    char *tasks=(char*)malloc((size+1)*sizeof(char));
    if(tasks==NULL){
        LOG_ERROR("Dynamic Memory not assigned to buffer: %s",strerror(errno));
        fclose(fptr);
        return NULL;
    }
    size_t read=fread(tasks,sizeof(char),size,fptr);
    if(read!=size){
        LOG_ERROR("The file was read incorrectly to buffer.");
        LOG("Size:%zu,Read:%zu",size,read);
        fclose(fptr);
        return NULL;
    }
    LOG("Data transferred to buffer successfully.");
    fclose(fptr);
    tasks[size]='\0';
    return(tasks);
}

/**
 * @brief Extracts a substring from the given input string based on start and end indices.
 *
 * @details Creates a new string by copying characters from `userInput` starting
 *          at index `start` and ending at index `last` (inclusive).
 *          The result is null-terminated and stored in dynamically allocated memory.
 *
 *          This function is used internally to split commands like
 *          `install.git` into separate components (`install` and `git`)
 *          for searching commands and dependencies in `tasks.json`.
 *
 *          In the current implementation, the function is always called with
 *          valid indices, so there is no risk of out-of-bounds access unless
 *          the code is modified incorrectly.
 *
 * @param userInput The original string from which the slice will be taken.
 * @param start The starting index (0-based) of the slice.
 * @param last The ending index (0-based, inclusive) of the slice.
 *
 * @return char* A heap-allocated substring extracted from the input.
 *               In the current implementation, memory is freed later in the program.
 *
 * @note This function assumes valid index values as provided by the program logic.
 *       It does not perform bounds checking internally.
 *
 * @warning Altering how this function is called or modifying the index logic without
 *          understanding the program may lead to undefined behavior.
 *
 * Example usage:
 * @code
 * char *part = slice("install.git", 0, 6); // "install"
 * // Used internally for command resolution
 * free(part); // Freed later by program logic
 * @endcode
 */
char *slice(char *userInput, int start, int last){
    int length=((last+1)-start);
    char *output=(char*)malloc((length+1)*sizeof(char));
    LOG("Slicing input from %d to %d", start, last);
    for(int i=0; i<length; i++){
        output[i]=userInput[start+i];
    }
    output[length]='\0';
    return output;
}

/**
 * @brief Replaces placeholders in a command string with user-provided values.
 *
 * @details This function searches for specific placeholders within a given command:
 *          - `{{path}}` → Prompts the user to enter a file path.
 *          - `{{name}}` → Prompts the user to enter a name.
 *
 *          For each occurrence of the placeholder, the function:
 *          1. Prompts the user for input.
 *          2. Replaces the placeholder with the entered value.
 *          3. Continues until no more placeholders are found.
 *
 *          If the user enters the placeholder text itself (e.g., `{{path}}`),
 *          the function interprets it as invalid and returns the original command
 *          (duplicated), leaving it unchanged.
 *
 *          The function allocates memory dynamically for the modified string. In the
 *          current implementation, this memory is freed later by the calling function
 *          (`runCommands()`), so there is no memory leak risk unless the code is altered.
 *
 * @param input The original command string that may contain placeholders.
 * @param val The placeholder token to replace (e.g., `{{path}}` or `{{name}}`).
 *
 * @return char* A heap-allocated string with placeholders replaced, or a duplicate
 *               of the original string if no replacements were made or input was invalid.
 *               Returns `NULL` only if memory allocation fails during replacement.
 *
 * @note This function performs in-place replacement by creating temporary buffers
 *       and updating the string iteratively.
 *
 * @warning Altering memory management or placeholder handling logic without care
 *          may cause leaks or incorrect string processing.
 *
 * Example usage:
 * @code
 * char *cmd = replacePlaceholder("copy {{path}} {{name}}", "{{path}}");
 * // User enters a path, placeholder replaced.
 * free(cmd); // Freed later by current implementation logic
 * @endcode
 */
char* replacePlaceholder(char* input, char *val) {
    char *buffer;
    char *input_copy=strdup(input);
    char *pos = strstr(input_copy, val);
    while(pos!=NULL){
        char value[100];
        if(strcmp(val, "{{path}}") == 0){
            LOG("Enter the path: ");
            scanf("%99s",value);
        }
        else if(strcmp(val, "{{name}}") == 0){
            LOG("Enter the name: ");
            scanf("%99s",value);
        }
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF);
        if(strcmp(value, val) == 0){
            LOG("Invalid path. Using Original command.");
            free(input_copy);
            return strdup(input);
        }
        size_t new_len=strlen(input_copy)-strlen(val)+strlen(value)+1;
        buffer = (char *)malloc(new_len*sizeof(char));
        if (!buffer) return NULL;
        pos = strstr(input_copy, val);
        size_t prefix_len = pos - input_copy;
        strncpy(buffer, input_copy, prefix_len);
        buffer[prefix_len] = '\0';
        strcat(buffer, value);
        strcat(buffer, pos + strlen(val));
        free(input_copy);
        input_copy = strdup(buffer);
        free(buffer);
        pos = strstr(input_copy, val);
    }
    LOG("Placeholder Replaced.");
    return input_copy;
}

/**
 * @brief Wraps a command for execution based on the detected shell environment.
 *
 * @details This function ensures that commands intended for PowerShell can still be
 *          executed using `system()`, which runs commands in CMD by default on Windows.
 *          If the detected shell is:
 *          - **PowerShell** → The command is wrapped as:
 *            `powershell -Command "<original command>"`.
 *          - **CMD or Linux** → The command is returned unchanged (but duplicated).
 *
 *          Memory for the wrapped (or duplicated) command is dynamically allocated.
 *          In the current implementation, this memory is freed later in the program,
 *          so there is no leak risk unless the logic is modified.
 *
 * @param command The original command string to wrap.
 *
 * @return char* A heap-allocated string containing the wrapped or original command.
 *               Returns `NULL` only if memory allocation fails.
 *
 * @note This function uses the global `shell` variable to determine which shell is active.
 *
 * @warning Altering memory handling or removing the wrapper logic without understanding
 *          system command behavior may cause execution failures or leaks.
 *
 * Example usage:
 * @code
 * char *cmd = wrap_for_shell("Get-Process");
 * system(cmd);
 * free(cmd); // Freed later by program logic in current implementation
 * @endcode
 */
char* wrap_for_shell(char* command) {
    if (strcmp(shell, "Powershell") == 0) {
        size_t len = strlen(command) + 30;
        char *wrapped = malloc(len);
        snprintf(wrapped, len, "powershell -Command \"%s\"", command);
        return wrapped;
    }
    return strdup(command);
}

/**
 * @brief Checks whether the current process has administrative privileges.
 *
 * @details This function determines if the current shell session is running with
 *          administrator rights. It uses Windows Security APIs to check membership
 *          in the Administrators group.
 *
 *          The result influences the installation behavior:
 *          - **Admin shell** → Use Chocolatey (`choco`) for installations.
 *          - **Non-admin shell** → Use Scoop for installations.
 *
 *          The function is relevant only on Windows and is typically called when
 *          handling `install.*` commands. On Linux platforms, this function
 *          is not used.
 *
 * @return bool `true` if the current process has administrative privileges,
 *              `false` otherwise.
 *
 * @note Uses `AllocateAndInitializeSid` and `CheckTokenMembership` internally.
 *
 * @warning Altering privilege-check logic or skipping this step may cause incorrect
 *          installation command selection.
 *
 * Example usage:
 * @code
 * if (isAdmin()) {
 *     LOG("Admin privileges detected. Using Chocolatey for installation.");
 * } else {
 *     LOG("Non-admin shell detected. Using Scoop for installation.");
 * }
 * @endcode
 */
bool isAdmin() {
        BOOL isAdmin = FALSE;
        PSID adminGroup = NULL;
        SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

        if (AllocateAndInitializeSid(
                &NtAuthority,
                2,
                SECURITY_BUILTIN_DOMAIN_RID,
                DOMAIN_ALIAS_RID_ADMINS,
                0, 0, 0, 0, 0, 0,
                &adminGroup)) {

            if (!CheckTokenMembership(NULL, adminGroup, &isAdmin)) {
                isAdmin = FALSE;
            }
            FreeSid(adminGroup);
        }
    return isAdmin;
}

/**
 * @brief Checks if a tool is already installed and handles PATH adjustments if necessary.
 *
 * @details This function performs a series of checks for a given tool:
 *          1. Executes `foundAtPath` to check if the tool is available in the system PATH.
 *          2. If not found, executes `foundAtDrive` to check if the tool exists on disk.
 *          3. If found on disk but not in PATH:
 *             - On **Windows** → Adds the tool temporarily to PATH using `addFileToPath`.
 *             - On **Linux** → Extracts the tool directory and substitutes it into `addFileToPath`,
 *               then updates PATH temporarily.
 *          4. If not found in either location → Returns `1` to allow installation.
 *
 *          The function dynamically wraps shell commands for execution using `wrap_for_shell()`.
 *          All allocated memory is freed within this function in the current implementation.
 *
 * @param foundAtPath Command to verify if the tool is in the PATH.
 * @param foundAtDrive Command to check if the tool exists on disk (e.g., drive-specific path).
 * @param addFileToPath Command template for temporarily adding the tool to the PATH,
 *                      containing `{{path}}` as a placeholder for the resolved directory.
 *
 * @return int
 *         - `0` → Tool found (either in PATH or added temporarily).
 *         - `1` → Tool not found; installation should proceed.
 *
 * @note This function is used only for `install.*` commands to prevent redundant installations
 *       and ensure tools can be executed immediately after detection.
 *
 * @warning Altering logic for PATH addition or placeholder handling without care may cause
 *          incorrect behavior or PATH corruption.
 *
 * Example usage:
 * @code
 * if (checkAvailability("where tool", "dir tool_path", "set PATH={{path}};%PATH%") == 1) {
 *     LOG("Tool not found. Proceeding with installation.");
 * }
 * @endcode
 */
int checkAvailability(char *foundAtPath, char *foundAtDrive, char *addFileToPath){
    if (!foundAtPath || !foundAtDrive || !addFileToPath) {
        LOG_ERROR("Invalid tool JSON definition. Required keys missing.");
        return 1;
    }
    char* finalCommand=wrap_for_shell(foundAtPath);
    int status=system(finalCommand);
    free(finalCommand);
    LOG("Returned status: %d", status);
    if(status==0){
        LOG_ERROR("File found at path. Terminating request.");
        return 0;
    }
    char* finalCommand1=wrap_for_shell(foundAtDrive);
    int status1=system(finalCommand1);
    free(finalCommand1);
    LOG("Returned status: %d", status1);
    if(status==1 && status1==0){
        if(strcmp(shell, "Linux") != 0){
            printf(YELLOW "Warning! You file is installed, but not added to path.\nAdding temporarily to path.\nKindly permanently add file to path also." RESET);
            char* finalCommand2 = wrap_for_shell(addFileToPath);
            system(finalCommand2);
            free(finalCommand2);
            LOG("Tool path temporarily added.");
            return 0;
        }
        else{
            FILE *fp = popen(foundAtDrive, "r");
            if (!fp) {
                LOG_ERROR("Failed to execute atDrive command.");
                return 1;
            }
            char path[512];
            if (!fgets(path, sizeof(path), fp)) {
                pclose(fp);
                LOG_ERROR("Could not read path from atDrive command.");
                return 1;
            }
            pclose(fp);
            char *lastSlash = strrchr(path, '/');
            if (lastSlash) *lastSlash = '\0';
            path[strcspn(path, "\n")] = 0;
            if (path[strlen(path)-1]=='/') path[strlen(path)-1]=0;
            LOG("Found path: %s", path);
            char *buffer;
            char *pos = strstr(addFileToPath, "{{path}}");
            if (!pos){
                LOG_ERROR("Placeholder {{path}} not found in addToPath.");
                return 1;
            }
            size_t new_len=strlen(addFileToPath) + strlen(path) + 10;
            buffer = (char *)malloc(new_len*sizeof(char));
            if (!buffer){
                LOG_ERROR("Dynamic Memory allocation failed.");
                return 1;
            }
            size_t prefix_len = pos - addFileToPath;
            snprintf(buffer, new_len, "%.*s%s%s", (int)prefix_len, addFileToPath, path, pos+strlen("{{path}}"));
            system(buffer);
            free(buffer);
            LOG("Tool path temporarily added.");
            return 0;
        }
    }
    else{
        LOG("File not present in system. Running command.");
        return 1;
    }
}

/**
 * @brief Executes a user-specified command by resolving it from the JSON configuration.
 *
 * @details This function forms the core execution logic of the tool. It processes
 *          user commands of the format:
 *
 *          @code
 *          category.subcommand
 *          @endcode
 *
 *          Example: `build.cpp`, `install.git`.
 *
 *          The steps performed by this function include:
 *          1. **Validation:** Ensures the command follows the expected format.
 *          2. **Splitting:** Divides the command into `input1` (category) and
 *             `input2` (subcommand) using the `slice()` function.
 *          3. **Command Resolution:** Looks up `input1` and `input2` in the
 *             JSON object (`root`) and retrieves the shell-specific command object.
 *          4. **Dependency Handling:** If the command specifies a `dependsOn` array,
 *             executes dependencies recursively before the main command.
 *          5. **Execution Logic:**
 *             - For `install.*` commands:
 *               - Checks tool availability using `check_availability()`.
 *               - Determines admin privileges via `is_admin()` (Windows only).
 *               - Selects appropriate installation command (Chocolatey, Scoop, or Linux equivalent).
 *             - For other commands:
 *               - Handles placeholder substitution (`{{path}}`, `{{name}}`) via
 *                 `replacePlaceholder()`.
 *               - Wraps commands for shell compatibility using `wrap_for_shell()`.
 *             - Executes final command using `system()`.
 *
 *          6. **Error Handling:** Logs all failures (invalid JSON structure, missing
 *             keys, or execution errors).
 *
 *          **Memory Management:** All intermediate strings (splits, replacements,
 *          wrapped commands) are allocated and freed within the function. The caller
 *          does not need to manage any additional memory.
 *
 * @param root Pointer to the root cJSON object representing the parsed `tasks.json`.
 * @param userInput The user-entered command string (e.g., `install.git`).
 * @param len Length of the userInput string.
 *
 * @return void
 *
 * @note The function uses recursion for handling dependencies. Excessively deep
 *       dependency chains may impact performance or stack usage.
 *
 * @warning Altering the execution flow without understanding the dependency resolution,
 *          privilege checks, and placeholder handling may cause command failures or
 *          unsafe system changes.
 *
 * Example usage:
 * @code
 * runCommands(root, "install.git", strlen("install.git"));
 * @endcode
 */
void runCommands(cJSON *root, char *userInput, int len){
    LOG("Starting command: %s", userInput);
    int index=-1;
    for (int i=0; i<len-1; i++) {
        if (userInput[i] == '.') {
            index=i;
            break;
        }
    }
    if (index==-1) {
        LOG_ERROR("Invalid command format. Expected format like 'build.cpp'");
        return;
    }
    if(index<=0 || index>=len-1){
        LOG_ERROR("Invalid command syntax near '.'");
        return;
    }
    char *input1 = slice(userInput, 0, index - 1);
    char *input2 = slice(userInput, index + 1, len - 1);
    cJSON *command=cJSON_GetObjectItem(root, input1);
    if (!command) {
        LOG_ERROR("No such category: %s", input1);
        return;
    }
    if(cJSON_IsObject(command)){
        cJSON *fullCommand=cJSON_GetObjectItem(command, input2);
        if (!fullCommand) {
            LOG_ERROR("No such category: %s.%s", input1,input2);
            return;
        }
        if(cJSON_IsObject(fullCommand)){
            cJSON *shellCommand=cJSON_GetObjectItem(fullCommand, shell);
            if (!shellCommand) {
                LOG_ERROR("Shell-specific command missing for %s.%s", input1, input2);
                return;
            }
            if(cJSON_IsObject(shellCommand)){
                LOG("Found shell-specific command object");
                cJSON *dependency = cJSON_GetObjectItem(shellCommand, "dependsOn");
                if(cJSON_IsArray(dependency)){
                    int size= cJSON_GetArraySize(dependency);
                    for(int i=0; i<size; i++){
                        cJSON *Item = cJSON_GetArrayItem(dependency,i);
                        if (cJSON_IsString(Item)) {
                            runCommands(root, Item->valuestring, strlen(Item->valuestring));
                        }
                    }
                }
                cJSON *runningCommand=cJSON_GetObjectItem(shellCommand, "cmd");
                LOG("Final command to run: %s", runningCommand->valuestring);
                char value[100];
                if (!runningCommand || (!cJSON_IsString(runningCommand) && (strcmp(shell, "CMD") == 0 || strcmp(shell, "Powershell") == 0) && strcmp(input1, "install")!=0)) {
                    LOG_ERROR("No valid 'cmd' string found in JSON for this command");
                }
                else {
                    if(strcmp(input1, "install")==0){
                        if(strcmp(input2, "all")!=0){
                            cJSON *foundAtPath=cJSON_GetObjectItem(shellCommand, "atPath");
                            cJSON *foundAtDrive=cJSON_GetObjectItem(shellCommand, "atDrive");
                            cJSON *addFileToPath=cJSON_GetObjectItem(shellCommand, "addToPath");
                            LOG("Checking: atPath=%s, atDrive=%s, addToPath=%s", foundAtPath->valuestring, foundAtDrive->valuestring, addFileToPath->valuestring);
                            int result=checkAvailability(foundAtPath->valuestring, foundAtDrive->valuestring, addFileToPath->valuestring);
                            if(result==0){
                                LOG("File is already in path or has been added temporarily.");
                            }
                            else{
                                if (strcmp(shell, "CMD") == 0 || strcmp(shell, "Powershell") == 0){
                                    bool adminCheck=isAdmin();
                                    if(adminCheck==true){
                                        cJSON *adminCommand=cJSON_GetObjectItem(runningCommand, "choco");
                                        if (!adminCommand || !cJSON_IsString(adminCommand)) {
                                            LOG_ERROR("No valid 'cmd' string found in JSON for this command");
                                        }
                                        else{
                                            LOG("Executing: %s", adminCommand->valuestring);
                                            char* finalCommand = wrap_for_shell(adminCommand->valuestring);
                                            int status = system(finalCommand);
                                            free(finalCommand);
                                            if (status != 0) {
                                                LOG_ERROR("Command execution failed with status: %d", status);
                                            }
                                        }
                                    }
                                    else{
                                        cJSON *adminCommand=cJSON_GetObjectItem(runningCommand, "scoop");
                                        if (!adminCommand || !cJSON_IsString(adminCommand)) {
                                            LOG_ERROR("No valid 'cmd' string found in JSON for this command");
                                        }
                                        else{
                                            LOG("Executing: %s", adminCommand->valuestring);
                                            char* finalCommand = wrap_for_shell(adminCommand->valuestring);
                                            int status = system(finalCommand);
                                            free(finalCommand);
                                            if (status != 0) {
                                                LOG_ERROR("Command execution failed with status: %d", status);
                                            }
                                        }
                                    }
                                }
                                else{
                                    LOG("Executing: %s", runningCommand->valuestring);
                                    int status=system(runningCommand->valuestring);
                                    if (status != 0) {
                                        LOG_ERROR("Command execution failed with status: %d", status);
                                    }
                                }
                            }
                        }
                    }
                    else if(strstr(runningCommand->valuestring, "{{path}}") || strstr(runningCommand->valuestring, "{{name}}")){
                        char *commandWithPath;
                        if(strstr(runningCommand->valuestring, "{{path}}")){
                            char *val="{{path}}";
                            commandWithPath= replacePlaceholder(runningCommand->valuestring, val);
                        }
                        if(strstr(runningCommand->valuestring, "{{name}}")){
                            char *val="{{name}}";
                            commandWithPath= replacePlaceholder(runningCommand->valuestring, val);
                        }
                        char* finalCommand = wrap_for_shell(commandWithPath);
                        int status = system(finalCommand);
                        LOG("Executing command: %s", commandWithPath);
                        free(finalCommand);
                        if (status != 0) {
                            LOG_ERROR("Command execution failed with status: %d", status);
                        }
                        free(commandWithPath);
                    }
                    else{
                        char* finalCommand = wrap_for_shell(runningCommand->valuestring);
                        int status = system(finalCommand);
                        free(finalCommand);
                        if (status != 0) {
                            LOG_ERROR("Command execution failed with status: %d", status);
                        }                    
                    }
                }
            if (!cJSON_IsObject(shellCommand)) {
                    LOG_ERROR("Shell command object is not valid");
                }
            }
        }
    }
    free(input1);
    free(input2);
}

/**
 * @brief Displays a list of available commands and their descriptions.
 *
 * @details This function iterates through the parsed JSON configuration (`root`)
 *          and prints all commands in the format:
 *
 *          @code
 *          category.subcommand        Description
 *          @endcode
 *
 *          It fetches shell-specific command objects for the current shell
 *          (using the global `shell` variable) and retrieves the `use` key,
 *          which contains a short description of the command.
 *
 *          Output is formatted into two columns:
 *          - **Command** → `category.subcommand` format.
 *          - **Operation** → Description from the `use` key.
 *
 *          If a shell-specific command object is missing or invalid for any
 *          entry, the function logs an error and stops.
 *
 * @param root Pointer to the root cJSON object representing the parsed `tasks.json`.
 *
 * @return void
 *
 * @note This function is read-only and does not modify any data structures.
 *       It does not allocate dynamic memory.
 *
 * @warning Removing or altering the `use` key in `tasks.json` will cause
 *          descriptions to be missing or incomplete in the help output.
 *
 * Example usage:
 * @code
 * help(root);
 * @endcode
 */
void help(cJSON *root){
    cJSON *object=root->child;
    char *input1;
    char command[100];
    printf("%-30s %-30s\n", "Command", "Operation");
    printf("---------------------------------------------------------------------------------------------\n");
    while(object!=NULL){
        input1=object->string;
        cJSON *input2= object->child;
        while(input2!=NULL){
            snprintf(command,sizeof(command),"%s.%s",input1, input2->string);
            cJSON *shellObject=cJSON_GetObjectItem(input2, shell);
            if (!shellObject) {
                LOG_ERROR("Shell-specific command missing for %s.%s", input1, input2->string);
                return;
            }
            if(cJSON_IsObject(shellObject)){
                cJSON *use=cJSON_GetObjectItem(shellObject, "use");
                if(cJSON_IsString(use)){
                    printf("%-30s %-30s\n",command, use->valuestring);
                }
            }
            command[0] = '\0';
            input2=input2->next;
            }
            object=object->next;
        }
}

/**
 * @brief Entry point for the DevCLI tool.
 *
 * @details This function initializes the CLI tool and orchestrates the entire
 *          workflow for executing user commands. The steps include:
 *          1. **Argument validation:** Ensures the tool is called with exactly
 *             one command-line argument. Logs an error and exits if incorrect.
 *          2. **Path resolution:** Calls `resolveJSONPath()` to determine the
 *             location of `tasks.json`. If not found, logs an error and exits.
 *          3. **File loading:** Reads the contents of `tasks.json` into a buffer
 *             using `readFileToBuffer()`. Logs and exits if reading fails.
 *          4. **Parsing:** Parses the JSON buffer into a cJSON object (`root`).
 *             Logs and exits if parsing fails.
 *          5. **Shell detection:** Calls `detectShell()` to identify the current
 *             shell environment (e.g., CMD, PowerShell, Linux).
 *          6. **Command execution:**
 *             - If the command is `help` → Calls `help()` to display all commands.
 *             - Otherwise → Passes the command to `runCommands()` for execution.
 *          7. **Cleanup:** Frees allocated memory and deletes the cJSON object
 *             before exiting.
 *
 * @param argc Number of command-line arguments. Must be `2` for proper execution.
 * @param argv Array of command-line arguments:
 *             - `argv[1]` → The command to execute (e.g., `install.git` or `help`).
 *
 * @return int Returns:
 *         - `0` → Successful execution.
 *         - `1` → Error occurred (invalid arguments, missing file, parse failure).
 *
 * @note The program expects a valid `tasks.json` file to function correctly.
 *       If not found in default locations, the tool prompts the user for its path.
 *
 * @warning Modifying the initialization or cleanup sequence without understanding
 *          the dependencies may cause crashes, memory leaks, or incorrect behavior.
 *
 * Example usage:
 * @code
 * devcli help
 * devcli install.git
 * @endcode
 */
int main(int argc, char* argv[]){
    if(argc!=2){
        LOG_ERROR("DEVCLI tool was invoked improperly. Kindly try again in format: devcli <command>. Use 'devcli help' command to know more.");
        return 1;
    }
    LOG("Running DEVCLI tool.");
    char *path = resolveJSONPath();
    if(path==NULL){
        LOG_ERROR("JSON file path could not be found.");
    }
    char *tasks=readFileToBuffer(path);
    free(path);
    if(tasks==NULL){
        LOG_ERROR("File was read incorrectly.");
        return 1;
    }
    LOG("File successfully read to buffer.");
    cJSON *root=cJSON_Parse(tasks);
    if(root==NULL){
        LOG_ERROR("Parsing failed before: %s", cJSON_GetErrorPtr());
        free(tasks);
        return 1;
    }
    LOG("File parsed successfully.");
    free(tasks);
    detectShell();
    char *userInput = argv[1];
    if (strcmp(userInput, "help") == 0) help(root);
    else{
        int len = strlen(userInput);
        runCommands(root, userInput, len);
    }
    cJSON_Delete(root);
    return 0;
}