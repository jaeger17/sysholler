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

/**
 * Pipe Visualization
 * 
 * stdin --> printf --> pipe1(READ_SIDE) --> pipe1(WRITE_SIDE)
 * stdin --> pipe2[READ_SIZE] --> pipe2(WRITE_SIDE) --> stdout
 * 
 */

typedef struct syscall_macros {
	size_t count;
	char *buffer;
	char *filename;
	FILE *fp;
	int pipe1[2];
	int pipe2[2];
	int pipe3[3];
} syscall_macros_t;

void exec1(int *pipe1, char *macro);
void exec2(int *pipe1, int *pipe2);
void exec3(int *pipe2, int *pipe3);

int main_loop();
int parse_options(syscall_macros_t *sm, int argc, char *argv[]);
int load_input_file(syscall_macros_t *sm);
int execute_lookup(syscall_macros_t *sm);
void usage(void);

int main(int argc, char * argv[])
{
	syscall_macros_t sm = {0};
	int status = EXIT_SUCCESS;

	// check command-line options
	if (parse_options(&sm, argc, argv) == -1) {
		fprintf(stderr, "error parsing options");
		status = EXIT_FAILURE;
		goto CLEANUP;
	}

	// execute syscall lookup
	if (execute_lookup(&sm) == -1) {
		fprintf(stderr, "error executing lookup");
		status = EXIT_FAILURE;
	}

CLEANUP:
	if (sm.count == 1) sm.buffer = NULL;
	if (sm.buffer) free(sm.buffer);
	
	exit(status);
}

void exec1(int *pipe1, char *macro)
{
	
	dup2(pipe1[WRITE_SIDE], STDOUT_FILENO);
	close(pipe1[READ_SIDE]);
	close(pipe1[WRITE_SIDE]);

	execlp("printf", "printf", macro, NULL);
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

void exec3(int *pipe2, int* pipe3)
{
	
	dup2(pipe2[READ_SIDE], STDIN_FILENO);
	dup2(pipe3[WRITE_SIDE], STDOUT_FILENO);
	close(pipe2[READ_SIDE]);
	close(pipe2[WRITE_SIDE]);

	execlp("egrep", "egrep", "^[0-9]+$", NULL);
	// this executes on failure
	perror("execlp egrep");
	exit(1);
}

int parse_options(syscall_macros_t *sm, int argc, char *argv[])
{
	if (argc < 3) {
		usage();
		return -1;
	}
	/**
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

			// open requested filename and assign the fp
			sm->fp = fopen(sm->filename, "r");
			if (NULL == sm->fp) {
				perror(sm->filename);
				return -1;
			}

			// load input file into the syscall_macro_t struct
			if ((load_input_file(sm) == -1)) {
				return -1;
			}
			break;
		case 'm':
			opt = -1;
			sm->count = 1;
			sm->buffer = optarg;
			break;
		default:
			usage();
			return -1;
		}

	}

	if (NULL != sm->fp) {
		fclose(sm->fp);
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

	// allocate syscall_macro_t buffer to hold the contents of the input file
	sm->buffer = malloc(sb.st_size * sizeof(char));
	if (NULL == sm->buffer) {
		return -1;
	}

	// read the contents of the input file into the sys_macro_t buffer
	if (fread(sm->buffer, sizeof(char), sb.st_size, sm->fp) != sb.st_size) {
		return -1;
	}

	// rewind file pointer
	rewind(sm->fp);
	// get the line count and store it in syscall_macro_t
    for (char c = getc(sm->fp); c != EOF; c = getc(sm->fp)) {
        if (c == '\n') {
            sm->count++;
		}
	}
	memset(&sb, 0, sizeof(struct stat));
	return 0;

}

int execute_lookup(syscall_macros_t *sm)
{
	pid_t fork_pid;
	const char *token = "\n";
	char buffer[16] = {0};
	char *macro = NULL;
	size_t bytes;
	if (NULL == sm) {
		return -1;
	}

	if (sm->count > 1) {
		// initial strtok
		macro = strtok(sm->buffer, token);
	} else {
		macro = sm->buffer;
	}

	// for each requested syscall macro, execute a lookup
	for (size_t i = 0; i < sm->count; i++) {
		if (i > 0) {
			macro = strtok(NULL, token);
		}

		// tokenize macros from the buffer

		// create pipe 1
		pipe(sm->pipe1);

		// fork a child process (printf <SYS_xxx>)
		fork_pid = fork();
		
		if (fork_pid == -1) {
			perror("fork printf");
			exit(1);
		} else if (fork_pid == 0) {
			// stdin --> printf --> pipe1
			exec1(sm->pipe1, macro);
		}

		// create pipe 2
		pipe(sm->pipe2);

		// create pipe 3
		pipe(sm->pipe3);
		int flags = fcntl(sm->pipe3[READ_SIDE], F_GETFL, 0);
		fcntl(sm->pipe3[READ_SIDE], F_SETFL, flags | O_NONBLOCK);

		// fork a child process (gcc -include sys/syscall.h -E -)
		fork_pid = fork();
		if (fork_pid == -1) {
			perror("fork gcc");
			exit(1);
		} else if (fork_pid == 0) {
			// pipe1 --> gcc --> pipe2
			exec2(sm->pipe1, sm->pipe2);
		}

		close(sm->pipe1[READ_SIDE]);
		close(sm->pipe1[WRITE_SIDE]);

		// fork a child process (egrep "'^[0-9]+$'")
		fork_pid = fork();
		if (fork_pid == -1) {
			perror("fork egrep");
			exit(1);
		} else if (fork_pid == 0) {
			// pipe2 --> grep --> stdout
			exec3(sm->pipe2, sm->pipe3);
		}

		// close pipe 2 since we don't need it anymore
		close(sm->pipe2[READ_SIDE]);
		close(sm->pipe2[WRITE_SIDE]);

		// duplicate stdin into pipe3 read
		dup2(sm->pipe3[READ_SIDE], STDIN_FILENO);

		waitpid(fork_pid, NULL, 0);

		// read final output into buffer
		bytes = read(sm->pipe3[READ_SIDE], buffer, 16);
		if (-1 == bytes) {
			printf("%-15sNot Found\n", macro);
		} else {
			printf("%-15s%s", macro, buffer);
		}

		fflush(stdout);
		memset(buffer, 0, 16);
		close(sm->pipe3[READ_SIDE]);
		close(sm->pipe3[WRITE_SIDE]);
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