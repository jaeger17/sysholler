#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 2 // getopt()
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <sys/wait.h>
#include <sys/stat.h>

#define READ_SIDE 0
#define WRITE_SIDE 1

// TODO: make one pipe exec function to rule them all that takes VARGS

typedef struct syscall_macros {
	size_t count;
	char *buffer;
	char *filename;
	int fd;
	int pipe1[2];
	int pipe2[2];
} syscall_macros_t;

void exec1(int *pipe1);
void exec2(int *pipe1, int *pipe2);
void exec3(int *pipe2);

int main_loop();
int parse_options(syscall_macros_t *sm, int argc, char *argv[]);
int load_input_file(syscall_macros_t *sm);
int execute_lookup(syscall_macros_t *sm);
void usage(void);

int main(int argc, char * argv[])
{
	int pipe1[2];  // array to store first pipe fds
	int pipe2[2];  // array to store second pipe fds
	pid_t fork_pid;
	syscall_macros_t sm = {0};

	// check command-line options
	if (parse_options(&sm, argc, argv) == -1) {
		goto failure;
	}

	// execute syscall lookup
	if (execute_lookup(&sm) == -1) {
		goto failure;
	}

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
	
	exit(EXIT_SUCCESS);
failure:
	exit(EXIT_FAILURE);
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

int parse_options(syscall_macros_t *sm, int argc, char *argv[])
{
	/**
	 * a - All.
	 * h - display help.
	 * i - input file.
	 * m - check for a specific macro.
	 */
	int opt;
	while (( opt = getopt(argc, argv, "hi:m:")) != -1) {
		switch (opt) {
		case 'h':
			usage(); 
			return -1;
		case 'i':
			sm->filename = optarg;

			// open requested filename and assign the fd
			sm->fd = open(sm->filename, O_RDONLY);
			if (sm->fd == -1) {
				perror(sm->filename);
				return -1;
			}

			// load input file into the syscall_macro_t struct
			if ((load_input_file(sm) == -1)) {
				return -1;
			}
			break;
		case 'm':
			printf("TODO: Lookup specific macro\n");
			opt = -1;
			break;
		default:
			usage();
			return -1;
		}

	}

	return 0;
}

int load_input_file(syscall_macros_t *sm)
{
	struct stat sb;
	if (NULL == sm) {
		return -1;
	}

	if (stat(sm->filename, &sb) ==  -1) {
		perror(sm->filename);
		return -1;
	}

	// allocate sys_macro_t buffer to hold the contents of the input file
	sm->buffer = malloc(sb.st_size * sizeof(char));
	if (NULL == sm->buffer) {
		return -1;
	}

	// read the contents of the input file into the sys_macro_t buffer
	if (read(sm->fd, sm->buffer, sb.st_size) == -1) {
		perror(sm->filename);
		return -1;
	}

	memset(&sb, 0, sizeof(struct stat));
	return 0;

}

int execute_lookup(syscall_macros_t *sm)
{
	if (NULL == sm) {
		return -1;
	}

	return 0;
}

void usage(void)
{
	printf("\nUsage:\n  sysholler [options] <file/macro>\n\n");
	printf("A tool for finding syscall numbers\n\n");
	printf("Options:\n");
	printf(" -h\t\tdisplay this help\n");
	printf(" -i\t\tsearch for syscall numbers usings macros from an input file\n");
	printf(" -m\t\tsearch for a specific macro\n");
}