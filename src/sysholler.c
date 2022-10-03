#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/wait.h>

#define READ_SIDE 0
#define WRITE_SIDE 1

// TODO: make one pipe exec function to rule them all that takes VARGS

typedef struct syscall_macros {
	size_t count;
	char **list;
	char *filename;
	int fd;
} syscall_macros_t;

void exec1(int *pipe1);
void exec2(int *pipe1, int *pipe2);
void exec3(int *pipe2);

int main_loop();

int main(int argc, char * argv[])
{
	int check;
	int pipe1[2];
	int pipe2[2];
	int stdout_copy;
	pid_t fork_pid;
	syscall_macros_t s = {0};
	
	// TODO: arg check and options handler

	// TODO: open and read into Syscall macro struct

	// create pipe 1
	pipe(pipe1);

	// fork a child process (printf <SYS_xxx>)
	fork_pid = fork();
	if (fork_pid == -1) {
		perror("fork printf");
		exit(1);
	} else if (fork_pid == 0) {
		// stdin --> printf --> pipe1
		exec1(pipe1);
	}

	// create pipe 2
	pipe(pipe2);

	// fork a child process (gcc -include sys/syscall.h -E -)
	fork_pid = fork();
	if (fork_pid == -1) {
		perror("fork gcc");
		exit(1);
	} else if (fork_pid == 0) {
		// pipe1 --> gcc --> pipe2
		exec2(pipe1, pipe2);
	}

	close(pipe1[READ_SIDE]);
	close(pipe1[WRITE_SIDE]);

	// fork a child process (egrep "'^[0-9]+$'")
	fork_pid = fork();
	if (fork_pid == -1) {
		perror("fork egrep");
		exit(1);
	} else if (fork_pid == 0) {
		// pipe1 --> gcc --> pipe2
		exec3(pipe2);
	}

	waitpid(fork_pid, NULL, 0);
	
	// clean up
cleanup:
	exit(0);
}

void exec1(int *pipe1)
{
	
	dup2(pipe1[WRITE_SIDE], STDOUT_FILENO);
	close(pipe1[READ_SIDE]);
	close(pipe1[WRITE_SIDE]);

	execlp("printf", "printf", "SYS_read", NULL);
	// this executes on failure
	perror("execlp printf");
	exit(1);
}

void exec2(int *pipe1, int *pipe2)
{
	
	dup2(pipe1[READ_SIDE], STDIN_FILENO);
	dup2(pipe2[WRITE_SIDE], STDOUT_FILENO);

	close(pipe1[READ_SIDE]);
	close(pipe1[WRITE_SIDE]);
	close(pipe2[READ_SIDE]);
	close(pipe2[WRITE_SIDE]);

	execlp("gcc", "gcc", "-include", "sys/syscall.h", "-E", "-",  NULL);
	// this executes on failure
	perror("execlp gcc");
	exit(1);
}

void exec3(int *pipe2)
{
	
	dup2(pipe2[READ_SIDE], STDIN_FILENO);
	close(pipe2[READ_SIDE]);
	close(pipe2[WRITE_SIDE]);

	execlp("egrep", "egrep", "^[0-9]+$", NULL);
	// this executes on failure
	perror("execlp egrep");
	exit(1);
}