// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <stdbool.h>
#include <errno.h>
#include "cJSON.h"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define RESET "\033[0m"
#define LOG(msg, ...) do{printf(BLUE "[%s:Line %d] [log]" RESET GREEN msg RESET "\n",__FILE__,__LINE__, ##__VA_ARGS__);}while(0)
#define LOG_ERROR(msg, ...) do{fprintf(stderr,YELLOW "[%s:Line %d] [error]" RESET RED msg RESET "\n",__FILE__,__LINE__,##__VA_ARGS__);}while(0)
const char *shell;
void detect_shell(){
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
char* read_file_to_buffer(char *path){
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
char* replace_placeholder(char* input) {
    char *buffer;
    char *input_copy=strdup(input);
    char *pos = strstr(input_copy, "{{path}}");
    while(pos!=NULL){
        char value[100];
        LOG("Enter the path: ");
        scanf("%99s",value);
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF);
        if(strcmp(value,"{{path}}") == 0){
            LOG("Invalid path. Using Original command.");
            free(input_copy);
            return strdup(input);
        }
        size_t new_len=strlen(input_copy)-strlen("{{path}}")+strlen(value)+1;
        buffer = (char *)malloc(new_len*sizeof(char));
        if (!buffer) return NULL;
        pos = strstr(input_copy, "{{path}}");
        size_t prefix_len = pos - input_copy;
        strncpy(buffer, input_copy, prefix_len);
        buffer[prefix_len] = '\0';
        strcat(buffer, value);
        strcat(buffer, pos + strlen("{{path}}"));
        free(input_copy);
        input_copy = strdup(buffer);
        free(buffer);
        pos = strstr(input_copy, "{{path}}");
    }
    LOG("Placeholder Replaced.");
    return input_copy;
}
char* wrap_for_shell(char* command) {
    if (strcmp(shell, "Powershell") == 0) {
        size_t len = strlen(command) + 30;
        char *wrapped = malloc(len);
        snprintf(wrapped, len, "powershell -Command \"%s\"", command);
        return wrapped;
    }
    return strdup(command);
}
bool is_admin() {
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
int check_availability(char *foundAtPath, char *foundAtDrive, char *addFileToPath){
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
void run_commands(cJSON *root, char *userInput, int len){
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
                            run_commands(root, Item->valuestring, strlen(Item->valuestring));
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
                            int result=check_availability(foundAtPath->valuestring, foundAtDrive->valuestring, addFileToPath->valuestring);
                            if(result==0){
                                LOG("File is already in path or has been added temporarily.");
                            }
                            else{
                                if (strcmp(shell, "CMD") == 0 || strcmp(shell, "Powershell") == 0){
                                    bool adminCheck=is_admin();
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
                    else if(strstr(runningCommand->valuestring, "{{path}}")){
                        char *commandWithPath= replace_placeholder(runningCommand->valuestring);
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
    char *tasks=read_file_to_buffer(path);
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
    detect_shell();
    char *userInput = argv[1];
    if (strcmp(userInput, "help") == 0) help(root);
    else{
        int len = strlen(userInput);
        run_commands(root, userInput, len);
    }
    cJSON_Delete(root);
    return 0;
}