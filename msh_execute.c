#include <signal.h>
#include <msh.h>
#include <msh_parse.h>
#include <stdio.h>
#include <unistd.h>     // fork(), getpid()
#include <sys/types.h>  // pid_t
#include <sys/wait.h>   // wait()
#include <stdlib.h>     // exit()
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/mman.h>
#include <fcntl.h>

//My struct for a command
//comEntries is a string array of args
//comString is the command line it wants to store
//numArgs is the number of Arguments in the command
//final is a boolean which checks if it's the final command in a pipeline
struct msh_command {
	char* comEntires[MSH_MAXARGS];
	char* comString;
	int numArgs;
	//bool
	int final;
	void *data;
	msh_free_data_fn_t freefn;
};
//My struct for Pipeline
//pipEntries is a command array 
//pipString is the pipeline it wants to store
//numCommand is the number of commands in the pipeline
struct msh_pipeline {
	struct msh_command* pipEntries[MSH_MAXCMNDS];
	char* pipString;
	int numCommand;
};
//My struct for Sequence
//seqEntries is an array of pipelines
//numPipeline is the number of pipelines in the sequence
struct msh_sequence {
	struct msh_pipeline* seqEntries[MSH_MAXBACKGROUND];
	int numPipeline;
};

// Struct in order to keep track of the jobs
struct msh_jobcontrol {
	pid_t pid;
	struct msh_pipeline* pip;
}
//Initialize the foreground pid, and amount of commands in the jobcontrol
msh_jobcontrol[MSH_MAXBACKGROUND];
int numjobs = 0;
pid_t foreground_pid;

//Typedef for sig handler
void sig_handler(int signal_number, siginfo_t *info, void *context);


void
msh_execute(struct msh_pipeline *p)
{
	//Base case
	if(p == NULL){
		return;
	}
	if(p->numCommand == 0){
		return;
	}
	// Creates the builtin command exit which would exit the program, if it found exit
	if(strcmp(p->pipEntries[0]->comEntires[0], "exit") == 0){
		exit(0);
	}
	// n is the number of process, we want to create or number of pipelines
	int n = p->numCommand;
	//Array of pipes to connect processes
	int pipes[n-1][2];
	//Array to store child process
	pid_t pids[n];

	// struct msh_pipeline fg = NULL;
	// struct msh_pipeline bg = NULL;
	// int outfile = STDOUT_FILENO;


	//Create the processes
	for(int i = 0; i < n - 1; i++){
		//Check for process errors
		if(pipe(pipes[i]) == -1){
			return;
		}
	}

	//Fork the child processess
	for(int i = 0; i < n; i++){
		//Fork()
		pids[i] = fork();
		//Check for Fork error
		if(pids[i] == -1){
			return;
		}
		//Check if it's in the child process
		if(pids[i] == 0){
			//Checks if it's the first child process
			if(i == 0){
				//Writes to the next pipe
				close(pipes[i][0]);
				dup2(pipes[i][1], STDOUT_FILENO);
				close(pipes[i][1]);
				

				//Removes the & so it won't interfere with the program
				if(msh_pipeline_background(p) == 1){
				p->pipEntries[i]->comEntires[p->pipEntries[i]->numArgs - 1] = NULL;
				} 
			}
			//Checks if the child process is in the middle
			else if (i > 0 && i < n - 1){
				//Read from the previous pipe
				close(pipes[i - 1][1]);
				dup2(pipes[i - 1][0], STDIN_FILENO);
				close(pipes[i - 1][0]);

				//Write to the next pipe
				close(pipes[i][0]);
				dup2(pipes[i][1], STDOUT_FILENO);
				close(pipes[i][1]);
				
				//Removes the & so it won't interfere with the program
				if(msh_pipeline_background(p) == 1){
				p->pipEntries[i]->comEntires[p->pipEntries[i]->numArgs - 1] = NULL;
				}
			}
			//Checks if the Child process is the last process
			else {
				//Read from the previous pipe
				close(pipes[i - 1][1]);
				dup2(pipes[i - 1][0], STDIN_FILENO);
				close(pipes[i - 1][0]);
				
				//Removes the & so it won't interfere with the program
				if(msh_pipeline_background(p) == 1){
				p->pipEntries[i]->comEntires[p->pipEntries[i]->numArgs - 1] = NULL;
				}
			}
			//This portion is used to check if there are any file redirection "<" in the args
			//If not than nothing here would matter
			struct msh_command* cmd = p->pipEntries[i];
			//Itterates through every command in the program to find the & arg
			for(int j = 0; j < cmd->numArgs; j++){
				// if(cmd->comEntires[j] == NULL){
				// 	continue;
				// }
				//If the argument is 1> that means it writes to a stoudt file
				if(strcmp(cmd->comEntires[j], "1>") == 0 ){
					int fd = open(cmd->comEntires[j + 1], O_WRONLY | O_CREAT, 0700);
					if(fd == -1){
						perror("Output redirection error");
						exit(0);
					}
					dup2(fd, STDOUT_FILENO);
					close(fd);
		            cmd->comEntires[j] = NULL; 
					break;
				}
				// If the argument is 1>> that means it appends to a stoudt file
				if(strcmp(cmd->comEntires[j], "1>>") == 0){
					int fd = open(cmd->comEntires[j + 1], O_WRONLY | O_CREAT | O_APPEND, 0700);
					if(fd == -1){
						perror("Output redirection error");
						exit(0);
					}
					dup2(fd, STDOUT_FILENO);
					close(fd);
		            cmd->comEntires[j] = NULL; 
					break;
				}
    				
				//If the argument is 2> that means it writes to a stderr file
				if(strcmp(cmd->comEntires[j], "2>") == 0){
					int fd = open(cmd->comEntires[j + 1], O_WRONLY | O_CREAT, 0700);
					if(fd == -1){
						perror("Output redirection error");
						exit(0);
					}
					dup2(fd, STDERR_FILENO);
					close(fd);
		            cmd->comEntires[j] = NULL; 
					break;

				}
				//If the argument is 2>> that means it appends to a stderr file
				if(strcmp(cmd->comEntires[j], "2>>") == 0){
					int fd = open(cmd->comEntires[j + 1], O_WRONLY | O_CREAT | O_APPEND, 0700);
					if(fd == -1){
						perror("Output redirection error");
						exit(0);
					}
					dup2(fd, STDERR_FILENO);
					close(fd);
		            cmd->comEntires[j] = NULL; 
					break;
				}
			}
			//Execute the program
			execvp(cmd->comEntires[0], cmd->comEntires);	
			//Inside the Parent process
		} else {
			//Checks if the command is in the background or not
			//If so than add it to the job control to keep track
			if (msh_pipeline_background(p) == 1){
				msh_jobcontrol[numjobs].pid = pids[n - 1];
				msh_jobcontrol[numjobs].pip = p;
				numjobs++;
			//Assign the foreground pid
			} else {
				foreground_pid = pids[n - 1];
			}
			//Close all remaining pipes
			if (i > 0) {
				close(pipes[i - 1][0]);
				close(pipes[i - 1][1]);
			}
		}
	}
	//From the parent process close all the pipe ends
	for(int i = 0; i < n - 1; i++){
		close(pipes[i][0]);
		close(pipes[i][1]);
	}
	//Wait but not block
	if(msh_pipeline_background(p) == 1){
	for(int i = 0; i < numjobs; i++){
		int status;
		waitpid(msh_jobcontrol[i].pid, &status, WNOHANG);
	}
	}
	//  Wait for all child process to finish 
	for(int i = 0; i < n; i++){
		waitpid(pids[i], NULL, 0);
	}
}

//Signal handler Function
void
sig_handler(int signal_number, siginfo_t *info, void* context){
	//Voids theese two because they aren't used and will cause a warning error
	(void) context;
	(void) info;
	switch(signal_number) {
		//ctrl c would kill the program
		case SIGINT: {
			if(foreground_pid > 0){
			kill(foreground_pid, SIGINT);
			}
			fflush(stdout);
			break;
		}
		//ctrl z would suspend the program
		case SIGTSTP: {
			if(foreground_pid > 0){
			kill(foreground_pid, SIGTSTP);
			foreground_pid = 0;
			}
			fflush(stdout);
			break;
		}
		//Would terminate the program
		case SIGTERM: {
			if(foreground_pid > 0){
			kill(foreground_pid, SIGTERM);
			}
			fflush(stdout);
			break;
		}}
		return;
	}



void
msh_init(void)
{
	//Setting up the sigaction to detect ctrl c and z
	sigset_t masked;
	struct sigaction siginfo;
	int flags = SA_SIGINFO;
	sigemptyset(&masked);
	sigaddset(&masked, SIGINT);
	sigaddset(&masked, SIGTSTP);
	sigaddset(&masked, SIGTERM);
	//Assigning the sigaction to the handler function from before
	siginfo = (struct sigaction){
		.sa_sigaction = sig_handler,
		.sa_mask = masked,
		.sa_flags = flags,
	};


	//Error checks
	if (sigaction(SIGINT, &siginfo, NULL) == -1) {
		perror("sigaction error for SIGINT");
		exit(EXIT_FAILURE);
	}
	if (sigaction(SIGTSTP, &siginfo, NULL) == -1) {
		perror("sigaction error for SIGSTP");
		exit(EXIT_FAILURE);
	}
	if (sigaction(SIGTERM, &siginfo, NULL) == -1) {
		perror("sigaction error for SIGTERM");
		exit(EXIT_FAILURE);
	}


}