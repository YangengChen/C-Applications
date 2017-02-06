#include "sfish.h"

char shellPrompt[1024];
char cwdbuf[512];
char hostname[256];
char username[256];
char* oldCWD;
pid_t shell_pgid;

job* first_job;

static int USER = 1;
static int MACHINE = 1;
// static char* BOLD = "/e[0;";
// static char* COLOR = "37m";
static char* RESET = "\001\e[0m\002";

int cmd_count;
int returnValue;
pid_t SPID;
const char* syntaxError = "syntax error near unexpected token `newline'\n";
const char* dneError = "No such file or directory\n"; 

int main(int argc, char** argv) {
    //DO NOT MODIFY THIS. If you do you will get a ZERO.
    rl_catch_signals = 0;
    //This is disable readline's default signal handlers, since you are going
    //to install your own.

    returnValue = 0;
    char *cmd;
    char** pipeArgs = malloc(sizeof(char*)*256);
    char** args = malloc(sizeof(char*)*256);


    strcpy(username, getenv("USER"));
    
    gethostname(hostname, 256);
    getcwd(cwdbuf, 512);
    char* pointer = strstr(cwdbuf, getenv("HOME"));
    if(pointer != NULL)
        changeHomePrompt(pointer); 

    first_job = NULL;

    // oldCWD = malloc(sizeof(cwdbuf) + 1);
    // // strcpy(oldCWD, cwdbuf);

    buildShellPrompt();

    signal(SIGCHLD, sigchld_handler);


    rl_command_func_t ctrl_b_handler;
    rl_bind_keyseq("\\C-b", ctrl_b_handler);
    rl_command_func_t ctrl_g_handler;
    rl_bind_keyseq("\\C-g", ctrl_g_handler);
    rl_command_func_t ctrl_h_handler;
    rl_bind_keyseq("\\C-h", ctrl_h_handler);
    rl_command_func_t ctrl_p_handler;
    rl_bind_keyseq("\\C-p", ctrl_p_handler);
    shell_pgid = getpgrp();

    while((cmd = readline(shellPrompt)) != NULL) {
        cmd_count++;
        if (strcmp(cmd,"quit")==0)
            break;
        // printf("%s\n",cmd);
        char*  program;
        // pid_t pgid;
        int status;
        // Add command line to history list
        if (cmd && *cmd)
            add_history(cmd);

        char newCommandLine[256];
        char* newcopy    = malloc(strlen(cmd)+1);
        strcpy(newcopy, cmd);
        reconstructCommandLine(newCommandLine, cmd); // THIS BUILDS A NEW LINE, SEPARATES SYMBOLS BY SPACES
        char cmdcopy[256];      // Ex: cmd = ls -l | grep .c | wc -l
        char bgtest[256];
        strcpy(cmdcopy, newCommandLine);
        strcpy(bgtest, newCommandLine);

        int numPipes = parsePipingArgs(pipeArgs, cmdcopy); // Parse cmdcopy into: {"ls -l", "grep .c", "wc -l"}
        char** bgArgs = malloc(sizeof(char*)*128);
        int bg = 0;
        parseIntoArgs(bgArgs, bgtest); // SPLITS THE NEW SPACED LINE, PUTS EACH ARGUMENT INTO bgArgs
        char* jobname = malloc(strlen(cmd)+1);
        char jobnamecopy[256];
        // sumParsedArgs(bgArgs, jobname);  // SHOULD CONTAIN THE JOB NAME, ISOLATES "&"  
        if(numPipes) {
            if(strchr(newCommandLine, '&') != NULL) {
                int i = 0;
                while(bgArgs[i] != NULL) {
                    if(bgArgs[i+1] == NULL) {
                        if(!strcmp(bgArgs[i], "&")) {
                            jobname = strsep(&newcopy, "&");
                            jobname[strlen(jobname)] = '\0';
                            strcpy(jobnamecopy, jobname);
                            bg = 1;
                        }
                    }
                    i++;
                }
            } else {
                strcpy(jobnamecopy, cmd);
            }
        } else {
            if(strchr(newCommandLine, '&') != NULL){
                jobname = strsep(&newcopy, "&");
                jobname[strlen(jobname)] = '\0';
                strcpy(jobnamecopy, jobname);
                bg = 1;
            } else {
                strcpy(jobnamecopy, cmd);
            }
        } 
        // job* pJob = (job*)malloc(sizeof(job));
        // if((pgid = fork()) == 0) {

        int* pipefds = malloc(sizeof(int)*numPipes);
        int programIndex = 0;
        int pipePid_isSet = 0;
        int jobflag = 0;
        int valid_cmd = 1;
        for(int i = 0; i < numPipes; i++){
            if(pipe(pipefds + i*2) < 0) {
                // print error
                exit(EXIT_FAILURE);
            }
        }
        job* pJob = NULL;
        while(pipeArgs[programIndex] != NULL) {    //Now we pass in each program, ls -l, then grep .c .. etc
            char argcopy[256]; // Copy cmd into argcopy
            strcpy(argcopy, pipeArgs[programIndex]);

            parseIntoArgs(args, argcopy); 
            program = args[0];          
            if(program == NULL) {
                break;
            }
            if(strcmp(program, "exit")==0) {
                exit(0);
            }
            else if(strcmp(program, "help")==0) {
                int child_status;
                if(fork() == 0) {
                    print_help_menu();
                    exit(0);
                } else {
                    wait(&child_status);
                }
            } else if(strcmp(program, "cd")== 0) {
                if(!numPipes) {
                    char* path = args[1];
                    changeDIR(path);
                    getcwd(cwdbuf, 512);
                    char* pointer = strstr(cwdbuf, getenv("HOME"));
                    if(strcmp(cwdbuf, getenv("HOME")) == 0)
                        strcpy(cwdbuf, "~");
                    else if(pointer != NULL) {
                        changeHomePrompt(pointer);
                    }
                    buildShellPrompt();
                }
            } else if(strcmp(program, "pwd")==0) {
                int child_status;
                getcwd(cwdbuf, 512);
                if((fork()) == 0) {
                    if(numPipes)
                        executePiping(programIndex, pipefds, pipeArgs, numPipes);
                    setOutputRedirection(args);
                    write(STDOUT_FILENO, cwdbuf, strlen(cwdbuf));
                    write(STDOUT_FILENO, "\n", 1);
                    exit(0);
                } else {
                    wait(&child_status);
                }
            } else if(strcmp(program, "prt")==0) {
                int child_status;
                char buf[100];
                sprintf(buf, "%d\n", returnValue);
                if((fork()) == 0) {
                    if(numPipes)
                        executePiping(programIndex, pipefds, pipeArgs, numPipes);
                    setOutputRedirection(args);
                    write(STDOUT_FILENO, buf, strlen(buf));
                    exit(0);
                } else {
                    wait(&child_status);
                }
                returnValue = 0;
            } else if(strcmp(program, "chpmt")==0) {
                updatePromptSettings(args);
                buildShellPrompt();
            } else if(strcmp(program, "chclr")==0) {
                updateColorSettings(args);
                buildShellPrompt();
            } else if (strcmp(program, "jobs")==0) {
                int child_status;
                if((fork()) == 0) {
                    printingbgJobs(first_job);
                    exit(0);
                } else {
                    wait(&child_status);
                }
            } else if (strcmp(program, "fg")==0) {
                update_jids();
                set_to_foreground(args[1]);
            } else if (strcmp(program, "bg")==0) {
                update_jids();
                resume_background_execution(args[1]);
            } else if (strcmp(program, "kill")==0) {
                update_jids();
                kill_to_send_signals(args[1], args[2]);
            } else if (strcmp(program, "disown")==0) {
                update_jids();
                disown_job(args[1]);
            }else {
                if(strcmp(program, "") != 0) {
                    char* slash = strchr(program, '/');
                    char* path = getenv("PATH");
                    char temp[256];
                    strcpy(temp, path);
                    char* path_list[256]= {0};
                    parseIntoArgs(path_list, temp);
                    if(slash == NULL) {
                        char* holder = malloc(strlen(program)+2);
                        strcpy(holder, "/");
                        strcat(holder, program);  //   /ls
                        int i = 0;
                        int cmdExists = 0;
                        while(path_list[i] != NULL) {
                            char combined_path[512] = {0}; 
                            strcpy(combined_path, path_list[i]);
                            strcat(combined_path, holder);
                            if(fileExists(combined_path)) {
                                cmdExists = 1;
                                pid_t pid;
                                if(pipePid_isSet == 0) {
                                    pJob = (job*)(calloc(sizeof(job), sizeof(job)));
                                    pJob->first_process = NULL;
                                    pJob->next = NULL;
                                    if(bg == 1)
                                        pJob->bg = true;
                                    else
                                        pJob->bg = false;
                                    setup_time(pJob);
                                    addJob(&pJob);
                                }
                                process* pro = (process*)(calloc(sizeof(process), sizeof(process)));
                                pro->next = NULL;
                                addProcess(&pJob->first_process, &pro);
                                char** programArgs = malloc(sizeof(args) + 1);
                                getProgramArgs(programArgs, args);
                                if((pid = fork()) == 0) { // Child process runs the program
                                    if(numPipes)
                                        executePiping(programIndex, pipefds, pipeArgs, numPipes);
                                    setOutputRedirection(args);
                                    signal (SIGINT, SIG_DFL);
                                    signal (SIGQUIT, SIG_DFL);
                                    signal (SIGTSTP, SIG_DFL);
                                    signal (SIGTTIN, SIG_DFL);
                                    signal (SIGTTOU, SIG_DFL);
                                    signal (SIGCHLD, SIG_DFL);
                                    if(execv(combined_path, programArgs) < 0) { 
                                        free(holder);
                                        free(programArgs);
                                        free(pro);
                                        returnValue = 1;
                                        exit(0); // break?
                                    }
                                    exit(0);
                                } 
                                else {
                                    if(pipePid_isSet == 0) {
                                        strcpy(pJob->name, jobnamecopy);
                                        pJob->pid = pid;
                                        pJob->pgid = pid;
                                        strcpy(pJob->status, "Running\0");
                                    }
                                    pro->pid = pid;   
                                    // printf("%d\n", );
                                    setpgid(pid, pJob->pgid);   // SETS THE PGID OF ALL PROCESSES TO  PGID OF THE JOB
                                    pro->pgid = pJob->pgid;
                                    pro->status = 'R';
                                    if(!numPipes && !bg) {
                                        signal(SIGTTIN, SIG_IGN);
                                        signal(SIGTTOU, SIG_IGN);
                                        tcsetpgrp(STDIN_FILENO, pJob->pgid);
                                        if(waitpid(pid, &status, WUNTRACED) > 0) { // Child is reaped
                                            if(WIFSTOPPED(status)) {
                                                printf("%s has stopped\n", pJob->name);
                                                strcpy(pJob->status, "Stopped\0");
                                                pJob->bg = true;
                                            } 
                                            if(WIFSIGNALED(status)) {
                                                if(pipePid_isSet == 0) 
                                                    removeJob(pJob);
                                                update_jids();
                                            }
                                        } 
                                        tcsetpgrp(STDIN_FILENO, shell_pgid);
                                        signal(SIGTTIN, SIG_DFL);
                                        signal(SIGTTOU, SIG_DFL);
                                    }
                                }
                                if(numPipes)
                                    pipePid_isSet = 1;
                            }
                            i++;
                        }
                        if(!cmdExists) {
                            char buf[256];
                            sprintf(buf, "%s: command not found\n", program);
                            write(STDOUT_FILENO, buf, strlen(buf));
                            returnValue = 1;
                            valid_cmd = 0;
                            if(pJob != NULL)
                                removeJob(pJob);
                        }   
                    } else {
                        if(fileExists(program)) {
                            char** programArgs = malloc(sizeof(args) + 1);
                            getProgramArgs(programArgs, args);
                            if(pipePid_isSet == 0) {
                                pJob = (job*)(calloc(sizeof(job), sizeof(job)));
                                pJob->first_process = NULL;
                                pJob->next = NULL;
                                if(bg == 1)
                                    pJob->bg = true;
                                else
                                    pJob->bg = false;
                                setup_time(pJob);
                                addJob(&pJob);
                            }
                            process* pro = (process*)(calloc(sizeof(process), sizeof(process)));
                            pro->next = NULL;
                            addProcess(&pJob->first_process, &pro);
                            pid_t pid;
                            if((pid = fork()) == 0) { // Child process
                                if(numPipes)
                                    executePiping(programIndex, pipefds, pipeArgs, numPipes);
                                setOutputRedirection(args);
                                signal (SIGINT, SIG_DFL);
                                signal (SIGQUIT, SIG_DFL);
                                signal (SIGTSTP, SIG_DFL);
                                signal (SIGTTIN, SIG_DFL);
                                signal (SIGTTOU, SIG_DFL);
                                signal (SIGCHLD, SIG_DFL);
                                if(execvp(program, programArgs) < 0) {
                                    returnValue = 1;
                                    exit(0);
                                }
                                exit(0);        
                            } 
                            else {
                                if(pipePid_isSet == 0) {
                                    strcpy(pJob->name, jobnamecopy);
                                    strcpy(pJob->status, "Running");
                                    pJob->pid = pid;
                                    pJob->pgid = pid;
                                }
                                pro->pid = pid;  
                                setpgid(pid, pJob->pgid);   // SETS THE PGID OF ALL PROCESSES TO  PGID OF THE JOB
                                pro->pgid = pJob->pgid;
                                if(!numPipes && !bg) {
                                    signal(SIGTTIN, SIG_IGN);
                                    signal(SIGTTOU, SIG_IGN);
                                    tcsetpgrp(STDIN_FILENO, pJob->pgid);
                                    if(waitpid(pid, &status, WUNTRACED) > 0) { // Child is reaped
                                        if(WIFSTOPPED(status)) {
                                            printf("%d has stopped\n", pJob->pgid);
                                            strcpy(pJob->status, "Stopped\0");
                                            pJob->bg = true;
                                        } if(WIFSIGNALED(status)) {
                                            if(pipePid_isSet == 0)
                                                removeJob(pJob);
                                            update_jids();
                                        }
                                    }    
                                    tcsetpgrp(STDIN_FILENO, shell_pgid);
                                    signal(SIGTTIN, SIG_DFL);
                                    signal(SIGTTOU, SIG_DFL);
                                }
                            }
                            if(numPipes)
                                pipePid_isSet = 1;
                        }
                    }
                }
            }
            programIndex++;
        }
        //All your debug print statments should be surrounded by this #ifdef
        //block. Use the debug target in the makefile to run with these enabled.
        // #ifdef DEBUG
        // fprintf(stderr, "Length of command entered: %ld\n", strlen(cmd));
        // #endif
        //You WILL lose points if your shell prints out garbage values.
        // free(cmdcopy);
        if(valid_cmd) {
            for(int i = 0; i < 2 * numPipes; i++){
                close(pipefds[i]);
            }
            if(numPipes && !bg) {
                signal(SIGTTIN, SIG_IGN);
                signal(SIGTTOU, SIG_IGN);
                tcsetpgrp(STDIN_FILENO, pJob->pgid);
                for(int i = 0; i < numPipes + 1; i++) {
                    if(waitpid(-1, &status, WUNTRACED) > 0) {
                        if(WIFSTOPPED(status)) {
                            if(jobflag == 0) { 
                                printf("%d has stopped\n", pJob->pgid);
                                jobflag++;
                            }
                            strcpy(pJob->status, "Stopped\0");
                            pJob->bg = true;
                        } if(WIFSIGNALED(status)) {
                            if(jobflag == 0) {
                                if(pJob != NULL)
                                    removeJob(pJob);
                                jobflag++;
                            }
                            update_jids();
                        }
                    }
                }
                tcsetpgrp(STDIN_FILENO, shell_pgid);
                signal(SIGTTIN, SIG_DFL);
                signal(SIGTTOU, SIG_DFL); 
            }
        }
        pipePid_isSet = 0;
        jobflag = 0;
        free(pipefds);
        free(bgArgs);
        free(jobname);
    } 
    //Don't forget to free allocated memory, and close file descriptors.
    free(cmd);
    free(oldCWD);
    free(args);
    free(pipeArgs);
    // free(hostname);
    //WE WILL CHECK VALGRIND!

    return EXIT_SUCCESS;
}

bool fileExists(const char* file) {
    struct stat buf;
    return (stat(file, &buf) == 0);
}

void changeDIR(char* path) {
    if(path != NULL) {
        if(strcmp(path, "-") != 0 && (fileExists(path) || strcmp(path, "") == 0)) {
            getcwd(cwdbuf, 512);
            oldCWD = realloc(oldCWD, sizeof(cwdbuf) + 1);
            strcpy(oldCWD, cwdbuf);
        } 
        if(strcmp(path, "-") == 0) {
            if(oldCWD == NULL) {
                returnValue = 1;
                printf("Previous working directory not set.\n");
            }
            else {
                getcwd(cwdbuf, 512);
                chdir(oldCWD);
                oldCWD = realloc(oldCWD, sizeof(cwdbuf) + 1);
                strcpy(oldCWD, cwdbuf);
                getcwd(cwdbuf, 512);
            }
        } else if(strcmp(path, "") == 0) {
            chdir(getenv("HOME"));
        } 
        else if((chdir(path)) == -1) {
            printf("No such file or directory\n");
            returnValue = 1;
        }
    } else {
        getcwd(cwdbuf, 512);
        oldCWD = realloc(oldCWD, sizeof(cwdbuf) + 1);
        strcpy(oldCWD, cwdbuf);
        chdir(getenv("HOME"));
    }
}

void changeHomePrompt(char* pointer) {
    pointer = pointer + strlen(getenv("HOME"));
    char* newString = malloc(strlen(cwdbuf) - strlen(getenv("HOME"))+2);
    newString[0] = '~';
    int i;
    for(i = 0; i < (strlen(cwdbuf)-strlen(getenv("HOME")));i++) {
        newString[i+1] = *pointer;
        pointer++;
    }
    newString[i+1] = '\0';
    strcpy(cwdbuf, newString);
    free(newString);
}

void updatePromptSettings(char** argv) {
    char* setting = argv[1];
    if(setting == NULL) 
        returnValue = 1;
    else if(strcmp(setting, "user") == 0) {
        char* toggle = argv[2];
        if(toggle == NULL)
            returnValue = 1;
        else if(strcmp(toggle, "0") == 0)
            USER = 0;
        else if(strcmp(toggle, "1") == 0)
            USER = 1;
        else
            returnValue = 1;
    } else if(strcmp(setting, "machine") == 0) {
        char* toggle = argv[2];
        if(toggle == NULL)
            returnValue = 1;
        else if(strcmp(toggle, "0") == 0)
            MACHINE = 0;
        else if(strcmp(toggle, "1") == 0)
            MACHINE = 1;
        else
            returnValue = 1;
    }
}

void buildShellPrompt() {
    strcpy(shellPrompt, "sfish");
    if(USER == 1 && MACHINE == 1) {
        strcat(shellPrompt, "-");
        strcat(shellPrompt, username);
        strcat(shellPrompt, "@");
        strcat(shellPrompt, hostname);
    } else if(USER == 1 && MACHINE == 0) {
        strcat(shellPrompt, "-");
        strcat(shellPrompt, username);
    } else if(USER == 0 && MACHINE == 1) {
        strcat(shellPrompt, "-");
        strcat(shellPrompt, hostname);
    }
    strcat(shellPrompt, ":[");
    strcat(shellPrompt, cwdbuf);
    strcat(shellPrompt, "]> ");
}

void getProgramArgs(char** argv, char** args) {
    int i = 0;

    while(1) {
        if(args[i] == NULL || !strcmp(args[i], "<") || !strcmp(args[i], ">") || !strcmp(args[i], "2>") || !strcmp(args[i], "&"))
            break;
        else {
            argv[i] = args[i];
            i++;
        }
    }
    argv[i] = NULL;
}

void parseIntoArgs(char** argv, char* cmdLine) {
    int i = 0;
    char* token;

    while((token = strsep(&cmdLine, " :")) != NULL) {
        if(strcmp(token, "") != 0) {
            argv[i] = malloc(strlen(token));
            argv[i] = token;
            i++;
        }
    }
    argv[i] = NULL;
}

int parsePipingArgs(char** argv, char* cmdLine) {
    int i = 0;
    char* token;

    while((token = strsep(&cmdLine, "|")) != NULL) {
        if(strcmp(token, "") != 0) {
            argv[i] = malloc(strlen(token));
            argv[i] = token;
            i++;
        }
    }

    argv[i] = NULL;
    if(i > 0)
        i--;
    return i;
}

void reconstructCommandLine(char newLine[256], char* cmd) {
    int count = 0; 

    for(int i = 0; i < strlen(cmd); i++) {
        if(cmd[i] == '>' || cmd[i] == '<' || cmd[i] == '|' || cmd[i] == '&' || cmd[i] == '2') {
            if(cmd[i+1]  == '>') {   
                newLine[count] = ' ';  
                newLine[++count] = cmd[i];
                newLine[++count] = cmd[++i];
                newLine[++count] = ' ';
                count++;
            } else if(cmd[i] != '2') {
                newLine[count] = ' ';
                newLine[++count] = cmd[i];
                newLine[++count] = ' ';
                count++;
            } else {
                newLine[count] = cmd[i];
                count++; 
            }
        } else {
            newLine[count] = cmd[i];
            count++;
        }
    }
    newLine[count] = '\0';
}

void printingbgJobs() {
    job* temp = first_job;
    while(temp != NULL) {
        // getJobStatus(temp, temp->pid);
        if(temp->bg == true) {
            update_jids();
            printJob(temp->jid, temp->status, temp->pid, temp->name);
        }
        temp = temp->next;
    }
}

void printJob(int jid, char status[], pid_t pid, char name[]) {
    printf("[%d]    %s    %d    %s\n", jid, status, pid, name);
}

void print_process_table() {
    job* j = first_job;
    while(j != NULL) {
        printf("%d    %d    %s    %s\n", j->pgid, j->pid, j->time, j->name);
        j=j->next;
    }
}

void set_to_foreground(char* id) {
    int count = update_jids();
    if(id[0] == '%') {
        id[0] = ' ';
        int jid = atoi(id);
        if(jid > 0 && jid <= count) {
            job* j = first_job;
            while(j != NULL) {
                if(j->jid == jid) {
                    j->bg = false;
                    if(!strcmp(j->status, "Stopped\0")) {
                        killpg(j->pgid, SIGCONT);
                        printf("%d has resumed\n", j->pgid);
                    }
                    process* temp = j->first_process;
                    int count = 0;
                    while(temp != NULL) {
                        signal(SIGTTIN, SIG_IGN);
                        signal(SIGTTOU, SIG_IGN);
                        tcsetpgrp(STDIN_FILENO, temp->pgid);
                        int status;
                        if(waitpid(temp->pid, &status, WUNTRACED) > 0) {
                            strcpy(j->status, "Stopped\0");
                            j->bg = true;
                            if(WIFSIGNALED(status)) {
                                if(count == 0) {
                                    printf("%d has terminated\n", j->pgid);
                                    removeJob(j);
                                    count++;
                                }
                            }
                        }
                        tcsetpgrp(STDIN_FILENO, shell_pgid);
                        signal(SIGTTIN, SIG_DFL);
                        signal(SIGTTOU, SIG_DFL);   
                        temp = temp->next;
                    }
                } j = j->next; 
            }
        } else {write(STDOUT_FILENO, "No such  job\n", 13); returnValue = 1;}
    } else {
        int pid = atoi(id); // converts char* to int
        if(pid > 0) {
            job* j = getJob(pid);
            if(j != NULL) {
                j->bg = false;
                if(!strcmp(j->status, "Stopped\0")) {
                    killpg(j->pgid, SIGCONT);
                    printf("%d has resumed\n", j->pgid);
                }
                process* temp = j->first_process;
                int count = 0;
                while(temp != NULL) {
                    signal(SIGTTIN, SIG_IGN);
                    signal(SIGTTOU, SIG_IGN);
                    tcsetpgrp(STDIN_FILENO, temp->pgid);
                    int status;
                    if(waitpid(temp->pid, &status, WUNTRACED) > 0) {
                        strcpy(j->status, "Stopped\0");
                        j->bg = true;
                        if(WIFSIGNALED(status)) {
                            if(count == 0) {
                                printf("%d has terminated\n", j->pgid);
                                removeJob(j);
                                count++;
                            }
                        }
                    }
                    tcsetpgrp(STDIN_FILENO, shell_pgid);
                    signal(SIGTTIN, SIG_DFL);
                    signal(SIGTTOU, SIG_DFL);
                    temp = temp->next;
                }
            } else {
                write(STDOUT_FILENO, "No such job\n", 13);
                returnValue = 1; 
            } 
        } else {
            write(STDOUT_FILENO, "No such job\n", 13);
            returnValue = 1;
        }
    }
}

void resume_background_execution(char* id) {
    int count = update_jids();
    if(id[0] == '%') {
        id[0] = ' ';
        int jid = atoi(id);
        if(jid > 0 && jid <= count) {
            job* j = first_job;
            while(j != NULL) {
                if(j->jid == jid) {
                    killpg(j->pgid, SIGCONT);
                    strcpy(j->status, "Running\0");
                    printf("%d has resumed\n", j->pgid);
                }
                j = j->next;
            }
        } else {
            write(STDOUT_FILENO, "No such job\n", 13);
            returnValue = 1;
        }
    } else {
        int pid = atoi(id); // converts char* to int
        if(pid > 0) {
            job* j = getJob(pid);
            if(j != NULL) {
                killpg(j->pgid, SIGCONT);
                strcpy(j->status, "Running\0");
                printf("%d has resumed\n", j->pgid);
            } else {
                write(STDOUT_FILENO, "No such job\n", 13);
                returnValue = 1;
            }
        } else {
            write(STDOUT_FILENO, "No such job\n", 13);
            returnValue = 1;
        }
    }
}

void kill_to_send_signals(char* arg1, char* arg2) {
    int count = update_jids();
    if(arg2 == NULL) { // Meaning only two arguments were passed, Will be sending SIGTERM, arg1 IS PID/JID 
        if(arg1[0] == '%'){
            arg1[0] = ' ';
            int jid = atoi(arg1);
            if(jid > 0 && jid <= count) {
                job* j = first_job;
                while(j != NULL) {
                    if(j->jid == jid) {
                        killpg(j->pgid, SIGTERM);
                    }
                    j = j->next;
                }
            } else {
                write(STDOUT_FILENO, "No such job\n", 13);
                returnValue = 1;
            }
        } else {
            int pid = atoi(arg1); // converts char* to int
            if(pid > 0) {
                job* j = getJob(pid);
                if(j != NULL) {
                    killpg(j->pgid, SIGTERM);
                } else {
                    write(STDOUT_FILENO, "No such job\n", 13);
                    returnValue = 1;
                }
            } {
                write(STDOUT_FILENO, "No such job\n", 13);
                returnValue = 1;
            } 
        }
    } else { // arg1 should be signal, arg2 should be pid/jid
        int signal = atoi(arg1);
        if(signal >= 1 && signal <= 31) {
            if(arg2[0] == '%') { // Using JID
                arg2[0] = ' ';
                int jid = atoi(arg2);
                if(jid > 0 && jid <= count) {
                    job* j = first_job;
                    while(j != NULL) {
                        if(j->jid == jid) {
                            killpg(j->pgid, signal);
                            if(signal == SIGSTOP || signal == SIGTSTP) {
                                printf("%d has stopped\n", j->pgid);
                                strcpy(j->status, "Stopped\0");
                                j->bg = true;
                            }
                            if(signal == SIGCONT) {
                                printf("%d has resumed\n", j->pgid);
                                strcpy(j->status, "Running\0");
                            } 

                        }
                        j = j->next;
                    }
                } else {
                    write(STDOUT_FILENO, "No such job\n", 13);
                    returnValue = 1;
                }
            } else { // Using PID
                int pid = atoi(arg2); // converts char* to int
                if(pid > 0) {
                    job* j = getJob(pid);
                    if(j != NULL) {
                        killpg(j->pgid, signal);
                        if(signal == SIGSTOP || signal == SIGTSTP) {
                            printf("%d has stopped\n", j->pgid);
                            strcpy(j->status, "Stopped\0");
                            j->bg = true;
                        }
                        if(signal == SIGCONT) {
                            printf("%d has stopped\n", j->pgid);
                            strcpy(j->status, "Running\0");
                        }
                    } else {
                        write(STDOUT_FILENO, "No such job\n", 13);
                        returnValue = 1;
                    }
                } {
                    write(STDOUT_FILENO, "No such job\n", 13);
                    returnValue = 1;
                }
            }
        } else {
            write(STDOUT_FILENO, "invalid signal specification", 28);
            returnValue = 1;
        }
    }
}

void executePiping(int programIndex, int pipefds[], char** programArgs, int numPipes) {
    if(programIndex != 0) { // Not first command
        dup2(pipefds[(programIndex-1)*2], 0);
    }
    if(programArgs[programIndex+1] != NULL) { // Not last command
        dup2(pipefds[programIndex*2 +1], 1);
    }
    for(int i = 0; i < numPipes*2; i++) {
        close(pipefds[i]);
    }
}

void setOutputRedirection(char** argv) {    
    int i = 0;
    int inputfd, outputfd;
    // FILE* fp;
    while(argv[i] != NULL) {
        if(!strcmp(argv[i], "<")) { // if argv[i] = "<", then return true
            char* inputFile = argv[i+1];
        if(inputFile != NULL) {
            inputfd = open(inputFile, O_RDONLY);
            if(inputfd == -1) {
                write(STDOUT_FILENO, dneError, strlen(dneError));
                exit(0);
            }
            else 
                dup2(inputfd, 0);
            close(inputfd);
        } else {
            write(STDOUT_FILENO, syntaxError, strlen(syntaxError));
            exit(0);
        }
    } else if(!strcmp(argv[i], ">") || !strcmp(argv[i], "2>")) {
        char* outputFile = argv[i+1];
        if(outputFile != NULL) {
            outputfd = open(outputFile, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
            if(!strcmp(argv[i], ">"))
                dup2(outputfd, 1);
            else
                dup2(outputfd, 2); 
            close(outputfd);
        } else {
            write(STDOUT_FILENO, syntaxError, strlen(syntaxError));
            exit(0);
        }   
    } 
    i++;
}
}

void addProcess(process** first_pro, process** pp) {
    process* first_process = *first_pro;
    process* p = *pp;
    if(first_process == NULL) // Puts process into job's process list
        *first_pro = p;
    else if(first_process->next == NULL)
        (*first_pro)->next = p;
    else {
        process* protemp;
        protemp = first_process;
        while(protemp != NULL) {
            if(protemp->next == NULL) {
                protemp->next = p;
                break;
            }
            else
                protemp = protemp->next;
        }
    }
}

void addJob(job** jp) {
    job* j = *jp;
    if(first_job == NULL) // Puts process into job's process list
        first_job = j;
    else if(first_job->next == NULL)
        first_job->next = j;
    else {
        job* protemp;
        protemp = first_job;
        while(protemp != NULL) {
            if(protemp->next == NULL) {
                protemp->next = j;
                break;
            }
            else
                protemp = protemp->next;
        }
    }
    update_jids();
}
int ctrl_h_handler(int count, int key) {
    printf("\n");
    print_help_menu();
    rl_on_new_line();
    return 0;
}

void setup_time(job* j) {
    struct tm* tm_info;
    time_t timer;
    time(&timer);
    tm_info = localtime(&timer);
    char buf[16];
    strftime(buf, 30, "%H:%M", tm_info);
    strcpy(j->time, buf);
}

void print_sfish_info() {
    write(STDOUT_FILENO, "----Info----\n", 14);
    write(STDOUT_FILENO, "help\n", 6);
    write(STDOUT_FILENO, "prt\n", 5);
    write(STDOUT_FILENO, "----CTRL----\n", 14);
    write(STDOUT_FILENO, "cd\n", 4);
    write(STDOUT_FILENO, "chclr\n", 7);
    write(STDOUT_FILENO, "chpmt\n", 7);
    write(STDOUT_FILENO, "pwd\n", 5);
    write(STDOUT_FILENO, "exit\n", 6);
    write(STDOUT_FILENO, "----Job Control----\n", 21);
    write(STDOUT_FILENO, "bg\n", 4);
    write(STDOUT_FILENO, "fg\n", 4);
    write(STDOUT_FILENO, "disown\n", 8);
    write(STDOUT_FILENO, "jobs\n", 6);
    write(STDOUT_FILENO, "----Number of Commands Run----\n", 32);
    printf("%d\n", cmd_count);
    write(STDOUT_FILENO, "----Process Table----\n", 23);
    printf("PGID     PID      TIME     CMD\n");
    print_process_table();
}

int ctrl_p_handler(int count, int key) {
    printf("\n");
    print_sfish_info();
    rl_on_new_line ();
    return 0;
}

int ctrl_b_handler(int count, int key) {
    printf("\n");
    if(first_job != NULL)
        SPID = first_job->pid;
    else
        SPID = -1;
    rl_on_new_line ();
    return 0;
}

int ctrl_g_handler(int count, int key) {
    printf("\n");
    if(SPID != 0 && SPID != -1) {
        job* j = getJob(SPID);
        killpg(SIGTERM, j->pgid);
        printf("%d stopped by signal 15\n", SPID);
        SPID = -1;
    } else {
        printf("SPID does not exist and has been set to -1\n");
    }
    rl_on_new_line ();
    return 0;
}


void disown_job(char* id) {
    if(id == NULL) {
        first_job->next = NULL;
        first_job = NULL;
    }
    else if(id[0] == '%') {
        id[0] = ' ';
        int jid = atoi(id);
        if(jid > 0) {
            job* j = first_job;
            while(j != NULL) {
                if(j->jid == jid) {
                    removeJob(j);
                    return;
                }
                j = j->next;
            }
        } else {
            write(STDOUT_FILENO, "No such job\n", 13);
            returnValue = 1;
        }
    } else {
        int pid = atoi(id); // converts char* to int
        if(pid > 0) {
            job* j = getJob(pid);
            if(j != NULL) {
                removeJob(j);
                return;
            } else {
                write(STDOUT_FILENO, "No such job\n", 13);
                returnValue = 1;
            }
        } else {
            write(STDOUT_FILENO, "No such job\n", 13);
            returnValue = 1;
        }
    }
}


void sigchld_handler(int sig) {
    int status, olderrno = errno;
    sigset_t mask, prev;
    pid_t pid;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    while((pid= waitpid(-1, &status, WNOHANG)) > 0) { /* Reap child */
    sigprocmask(SIG_BLOCK, &mask, &prev);
        // deletejob(pid); /* Delete the child from the job list */
    job* temp = first_job;
    pid_t pgid;
    while(temp != NULL) {                          // LOOKS FOR THE SPECIFIC PROCESS, SETS STATUS TO T, 
        process* ptemp = temp->first_process;
        // printf("Here?\n");
        while(ptemp != NULL) {
            if(ptemp->pid == pid) {
                // printf("Did it come here?\n");
                ptemp->status = 'T';
                pgid = ptemp->pgid;
                break;
            } else if(ptemp->next != NULL) {
                ptemp = ptemp->next;
            } else
            break;
        } 
        if(temp->next != NULL) temp = temp->next;
        else break;
    }
    // THIS SHOULD GET PGID THAT THE PROCESS IS IN
    // NOW WE IDENTIFY WHICH JOB IT IS
    // printf("Middle\n");
    temp = first_job;
    int all_terminated_flag = 1; // If all processes not terminated, then this flag is set to 0
    while(temp != NULL) {
        if(temp->pgid == pgid) { // CHECK IF ALL PROCESSES IN THE JOB ARE TERMINATED, IF TERMINATED REMOVE FROM THE JOB LIST
            process* ptemp = temp->first_process;
            while(ptemp != NULL) {
                if(ptemp->status != 'T') {
                    // printf("Am I here?\n");
                    all_terminated_flag = 0;
                    break;
                } else if(ptemp->next != NULL) {
                    ptemp = ptemp->next;
                } else
                break;
            }
        } 
        if(temp->next != NULL) temp = temp->next;
        else break;
    }
    if(all_terminated_flag) { // REMOVE THAT PROCESS
        // printf("The DREAM\n");
        printf("PGID %d has terminated\n", pgid);
        removeJob(getJob(pgid));
        update_jids();
    } 
    sigprocmask(SIG_SETMASK, &prev, NULL);
}

errno= olderrno;
}

job* getJob(pid_t pgid) {
    job* j = first_job;
    while(j != NULL) {
        if(j->pgid == pgid)
            return j;
        j = j->next;
    }
    return j;
}

void removeJob(job* j) {
    if(j == first_job) {
        if(first_job->next != NULL) {
            first_job = first_job->next;
        } else
        first_job = NULL;
    } else {
        job* temp = first_job;
        while(temp != NULL) {
            if(temp->next == j) {
                if(j->next == NULL)
                    temp->next = NULL;
                else
                    temp->next = j->next;
                break;
            }
            temp = temp->next;
        }
    }
}

int update_jids() {
    job* j = first_job;
    int i = 1;
    while(j != NULL) {
        if(j->bg == true) {
            j->jid = i;
            i++;
        }
        j = j->next;
    }
    return i-1;
}

char* convert_int_to_string(int i, char b[]){
    char const digit[] = "0123456789";
    char* index = b;
    if(i<0){
        *index++ = '-';
        i *= -1;
    }
    int position = i;
    do{ //Move to where representation ends
        ++index;
        position = position/10;
    }while(position);
    *index = '\0';
    do{ //Move back, inserting digits as u go
        *--index = digit[i%10];
        i = i/10;
    }while(i);
    return b;
}

void print_help_menu() {
    printf("help [-dms] [pattern ...]\n");
    printf("exit [n]\n");
    printf("cd [-] [.] [..] [dir]\n");
    printf("pwd [-LP]\n");
    printf("prt Returns code of last command executed\n");
    printf("chpmt [user|machine] [toggle]\n");
    printf("chclr [user|machine] [color] [bold]\n");
    printf("jobs Lists of all background jobs, with their name, PID, job number and status\n");
    printf("fg [PID|JID]\n");
    printf("bg [PID|JID]\n");
    printf("kill [signal] [PID|JID\n");
    printf("disown [PID|JID]\n");
}


void updateColorSettings(char** argv) {
    char* setting = argv[1];
    if(setting == NULL) 
        returnValue = 1;
    else if(strcmp(setting, "user") == 0) {
        char* color = argv[2];
        char* bold = NULL;
        if(color != NULL)
            bold = argv[3];
        if(color == NULL)
            returnValue = 1;
        else if((bold != NULL) && ((strcmp(bold, "0") == 0) || strcmp(bold, "1") == 0)) {
            strcpy(username, getenv("USER"));
            char* bolding = malloc(108);
            if(strcmp(color, "red") == 0) {
                if(strcmp(bold, "0") == 0)
                    strcpy(bolding, "\001\e[0;");
                else 
                    strcpy(bolding, "\001\e[1;");
                strcat(bolding, "31m\002");
                strcpy(username,strcat(bolding, username));
                strcat(username, RESET);
            } else if(strcmp(color, "blue") == 0) {
                if(strcmp(bold, "0") == 0)
                    strcpy(bolding, "\001\e[0;");
                else 
                    strcpy(bolding, "\001\e[1;");
                strcat(bolding, "34m\002");
                strcpy(username,strcat(bolding, username));
                strcat(username, RESET);
            } else if(strcmp(color, "green") == 0) {
                if(strcmp(bold, "0") == 0)
                    strcpy(bolding, "\001\e[0;");
                else 
                    strcpy(bolding, "\001\e[1;");
                strcat(bolding, "32m\002");
                strcpy(username,strcat(bolding, username));
                strcat(username, RESET);
            } else if(strcmp(color, "yellow") == 0) {
                if(strcmp(bold, "0") == 0)
                    strcpy(bolding, "\001\e[0;");
                else 
                    strcpy(bolding, "\001\e[1;");
                strcat(bolding, "33m\002");
                strcpy(username,strcat(bolding, username));
                strcat(username, RESET);
            } else if(strcmp(color, "cyan") == 0) {
                if(strcmp(bold, "0") == 0)
                    strcpy(bolding, "\001\e[0;");
                else 
                    strcpy(bolding, "\001\e[1;");
                strcat(bolding, "36m\002");
                strcpy(username,strcat(bolding, username));
                strcat(username, RESET);
            } else if(strcmp(color, "magneta") == 0) {
                if(strcmp(bold, "0") == 0)
                    strcpy(bolding, "\001\e[0;");
                else 
                    strcpy(bolding, "\001\e[1;");
                strcat(bolding, "35m\002");
                strcpy(username,strcat(bolding, username));
                strcat(username, RESET);
            } else if(strcmp(color, "black") == 0) {
                if(strcmp(bold, "0") == 0)
                    strcpy(bolding, "\001\e[0;");
                else 
                    strcpy(bolding, "\001\e[1;");
                strcat(bolding, "30m\002");
                strcpy(username,strcat(bolding, username));
                strcat(username, RESET);
            } else if(strcmp(color, "white") == 0) {
                if(strcmp(bold, "0") == 0)
                    strcpy(bolding, "\001\e[0;");
                else 
                    strcpy(bolding, "\001\e[1;");
                strcat(bolding, "37m\002");
                strcpy(username,strcat(bolding, username));
                strcat(username, RESET);
            }
            free(bolding);
        } else
        returnValue = 1;
    } else if(strcmp(setting, "machine") == 0) {
        char* color = argv[2];
        char* bold = NULL;
        if(color != NULL)
            bold = argv[3];
        if(color == 0)
            returnValue = 1;
        else if(bold != NULL && ((strcmp(bold, "0") == 0) || strcmp(bold, "1") == 0)) {
            gethostname(hostname, 256);
            char* bolding = malloc(108);
            if(strcmp(color, "red") == 0) {
                if(strcmp(bold, "0") == 0)
                    strcpy(bolding, "\001\e[0;");
                else 
                    strcpy(bolding, "\001\e[1;");
                strcat(bolding, "31m\002");
                strcpy(hostname,strcat(bolding, hostname));
                strcat(hostname, RESET);
            } else if(strcmp(color, "blue") == 0) {
                if(strcmp(bold, "0") == 0)
                    strcpy(bolding, "\001\e[0;");
                else 
                    strcpy(bolding, "\001\e[1;");
                strcat(bolding, "34m\002");
                strcpy(hostname,strcat(bolding, hostname));
                strcat(hostname, RESET);
            } else if(strcmp(color, "green") == 0) {
                if(strcmp(bold, "0") == 0)
                    strcpy(bolding, "\001\e[0;");
                else 
                    strcpy(bolding, "\001\e[1;");
                strcat(bolding, "32m\002");
                strcpy(hostname,strcat(bolding, hostname));
                strcat(hostname, RESET);
            } else if(strcmp(color, "yellow") == 0) {
                if(strcmp(bold, "0") == 0)
                    strcpy(bolding, "\001\e[0;");
                else 
                    strcpy(bolding, "\001\e[1;");
                strcat(bolding, "33m\002");
                strcpy(hostname,strcat(bolding, hostname));
                strcat(hostname, RESET);
            } else if(strcmp(color, "cyan") == 0) {
                if(strcmp(bold, "0") == 0)
                    strcpy(bolding, "\001\e[0;");
                else 
                    strcpy(bolding, "\001\e[1;");
                strcat(bolding, "36m\002");
                strcpy(hostname,strcat(bolding, hostname));
                strcat(hostname, RESET);
            } else if(strcmp(color, "magneta") == 0) {
                if(strcmp(bold, "0") == 0)
                    strcpy(bolding, "\001\e[0;");
                else 
                    strcpy(bolding, "\001\e[1;");
                strcat(bolding, "35m\002");
                strcpy(hostname,strcat(bolding, hostname));
                strcat(hostname, RESET);
            } else if(strcmp(color, "black") == 0) {
                if(strcmp(bold, "0") == 0)
                    strcpy(bolding, "\001\e[0;");
                else 
                    strcpy(bolding, "\001\e[1;");
                strcat(bolding, "30m\002");
                strcpy(hostname,strcat(bolding, hostname));
                strcat(hostname, RESET);
            } else if(strcmp(color, "white") == 0) {
                if(strcmp(bold, "0") == 0)
                    strcpy(bolding, "\001\e[0;");
                else 
                    strcpy(bolding, "\001\e[1;");
                strcat(bolding, "37m\002");
                strcpy(hostname,strcat(bolding, hostname));
                strcat(hostname, RESET);
            }
            free(bolding); 
        } else
        returnValue = 1;
    }
}
