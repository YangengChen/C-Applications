#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

typedef struct process
{
  struct process *next;       /* next process in pipeline */
  pid_t pid; 
  pid_t pgid;                 /* process ID */                 /* reported status value */	
  char status;               /* true if process has stopped */
} process;

typedef struct job
{
  process *first_process;     /* list of processes in this job */	
  struct job *next;           /* next active job */
  pid_t pid;
  pid_t pgid;	
  int jid;                 /* process group ID */
  bool bg;
  char status[144];
  char name[144];              /* command line, used for messages */	
  char time[64];
} job;

void changeDIR(char *path);

void buildShellPrompt();

void changeHomePrompt(char* pointer);

void updatePromptSettings(char** token);

void updateColorSettings(char** token);

void getProgramArgs(char** argv, char** args);

void parseIntoArgs(char** argv, char* cmd);

int parsePipingArgs(char** argv, char* cmd);

void reconstructCommandLine(char newLine[256], char* cmd);

bool fileExists(const char* file);

void sigchld_handler(int sig);

void setOutputRedirection(char** argv);

void executePiping(int programIndex, int pipefds[], char** programArgs, int numPipes);

void addProcess(process** first_process, process** p);
 
void addJob(job** j);

void printingbgJobs();

void getJobStatus(job* j, pid_t pid);

void printJob(int jid, char* status, pid_t pid, char* name);

job* getJob(pid_t pgid);

void removeJob(job* j);

int update_jids();

void set_to_foreground(char* id);

void resume_background_execution(char* id);

void disown_job(char* id);

void print_help_menu(void);

void print_sfish_info(void);

void setup_time(job* j);

void print_process_table(void);

void kill_to_send_signals(char* signal, char* id);

char* convert_int_to_string(int i, char* string);

#define SIGHUP       	1
#define SIGINT       	2
#define SIGQUIT      	3
#define SIGILL       	4
#define SIGTRAP      	5
#define SIGABRT      	6
#define SIGIOT       	6
#define SIGBUS       	7
#define SIGFPE       	8
#define SIGKILL      	9
#define SIGUSR1     	10
#define SIGSEGV     	11
#define SIGUSR2     	12
#define SIGPIPE     	13
#define SIGALRM     	14
#define SIGTERM     	15
#define SIGSTKFLT   	16
#define SIGCHLD     	17
#define SIGCONT     	18
#define SIGSTOP     	19
#define SIGTSTP     	20
#define SIGTTIN     	21
#define SIGTTOU     	22
#define SIGURG      	23
#define SIGXCPU     	24
#define SIGXFSZ     	25
#define SIGVTALRM   	26
#define SIGPROF     	27
#define SIGWINCH    	28
#define SIGIO       	29
#define SIGPWR      	30  
#define SIGSYS      	31