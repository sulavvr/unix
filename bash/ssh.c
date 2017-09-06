/**************************************
 * File: ssh.c
 * Name: Sulav Regmi
 * Unix Systems Programming
 * Dr. Jeff Jenness
 * Building a simple shell to run built in commands like:
 * - pwd (prints current working directory)
 * - cd (change current directory)
 * - >> (append output redirection)
 * - > (output redirection)
 * - < (input redirection)
 * - ; (run multiple commands) **bonus**
 **************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>		// use for PATH_MAX (pwd)
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

#define MAXLINE 259
#define PROMPT "> "
#define MAX_ARG_LIST 200

extern char **environ;
#define MAX_CMD_SIZE 50
#define SEARCH_FOR_CMD -1
#define STDIN 0
#define STDOUT 1
typedef void(*buildInFunc) (char **);
typedef struct {
	char cmd[MAX_CMD_SIZE];
	buildInFunc func;
} builtInCmd;

// built-in commands
void execExit(char *cmd[]);
void execPwd(char *cmd[]);
void execChdir(char *cmd[]);
void execAppendOut(char *cmd[]);
void execRedirOut(char *cmd[]);
void execRedirIn(char *cmd[]);
void execEchoForRedirection(char *cmd[]);	// used only when testing output redirection
builtInCmd builtInCmds[] = {
	{"exit", execExit},
	{"pwd", execPwd},
	{"cd", execChdir},
};

int builtInCnt = sizeof(builtInCmds)/sizeof(builtInCmd);
int isBuiltIn(char *cmd);
void execBuiltIn(int i, char *cmd[]);

// store number of arguments
int argument_count = 0;
// redirection arguments for checking
char *redir_args[3] = {"<", ">", ">>"};
// checks if a redirection operator exists in the argument list
char *checkIfExists(char *args[], int count);
// set current directory
void setCurrentDir();
// creates child process
void createChildProcess(char *args[], int redir);
// stores the current default directory or working directory
char *default_dir;
// capture SIG_INT and recover
sigjmp_buf ctrlc_buf;
void ctrl_handlr(int signo) {
	siglongjmp(ctrlc_buf, 1);
}

int main(int argc, char *argv[]) {
	char line[MAXLINE];
	int argn;
	char *args[MAX_ARG_LIST];
	int cmdn;
	char *redir_type;
	char *sem_args[MAX_ARG_LIST];	// stores semi-colon separated commands
	int s_argn;						// number of semi-colon separated commands

	if (signal(SIGINT, ctrl_handlr) == SIG_ERR) {
		fputs("ERROR: failed to register interrupts in kernel.\n", stderr);
	}

	// setup SIG_INT handler
	while (sigsetjmp(ctrlc_buf, 1) != 0);

	for (;;) {
		// prompt and get commandline
		setCurrentDir();
		fputs(default_dir, stdout);		// show current directory in console (mimick unix bash)
		fputs("\n", stdout);
		fputs(PROMPT, stdout);
		fgets(line, MAXLINE, stdin);

		if (feof(stdin)) break; //exit on end of input

		// process commandline
		if  (line[strlen(line) - 1] == '\n') {
			line[strlen(line) - 1] = '\0';
		}

		// build argument list
		// split commands on semi-colon
		sem_args[s_argn=0] = strtok(line, ";");
		// store arguments into array
		while (sem_args[s_argn] != NULL && s_argn < MAX_ARG_LIST) {
			sem_args[++s_argn] = strtok(NULL, ";");
		}

		// counter for semi-colon separated argument
		int inc = 0;
		// allow multiple commands
		while (s_argn != 0) {	// loop until number of arguments is 0
			// build argument list
			// now split semi-colon separated single command by space or tab character
			args[argn=0] = strtok(sem_args[inc], " \t");
			// store in args
			while (args[argn] != NULL && argn < MAX_ARG_LIST) {
				args[++argn] = strtok(NULL, " \t");
			}
			// store number of argument in global variable
			argument_count = argn;
			// execute commandline
			// checks if the command contains any of the redirection operators
			// handles accordingly
			if ((redir_type = checkIfExists(args, argn)) != NULL) {
				if (strcmp(redir_type, ">>") == 0) {
					execAppendOut(args);
				} else if (strcmp(redir_type, ">") == 0) {
					execRedirOut(args);
				} else if (strcmp(redir_type, "<") == 0) {
					execRedirIn(args);
				}
			} else if ((cmdn = isBuiltIn(args[0])) > -1) {	// if no redirection operator, run builtin commands
				execBuiltIn(cmdn, args);
			} else {	// else create a child process and run unix commands like /bin/ls, /bin/cat etc.
				createChildProcess(args, 0);
			}
			inc++;	// increment counter
			s_argn--;	// decrement semi-colon arguments counter
		}

		// cleanup
		fflush(stderr);
		fflush(stdout);
	}

	return 0;
}

// abstract creating child process out to a function for redirection testing
void createChildProcess(char *args[], int redir) {
	pid_t childPID;
	childPID = fork();
	if (childPID == 0) {
		if (redir == 0) { // check if called from the main program
			execve(args[0], args, environ);
		} else { // else called from a redirection function for unix command testing
			execv(args[0], (char *[]){args[0], NULL});
		}
		fprintf(stderr, "ERROR: can't execute command.\n\n");
		_exit(1);
	} else {
		waitpid(childPID, NULL, 0);
	}
}

int isBuiltIn(char *cmd) {
	int i = 0;
	while (i < builtInCnt) {
		if (strcmp(cmd, builtInCmds[i].cmd) == 0) {
			break;
		}
		++i;
	}
	return i < builtInCnt ? i : -1;
}

void execBuiltIn(int i, char *cmd[]) {
	if (i == SEARCH_FOR_CMD) {
		i = isBuiltIn(cmd[0]);
	}

	if (i >= 0 && i < builtInCnt) {
		builtInCmds[i].func(cmd);
	} else {
		fprintf(stderr, "ERROR: unknown built-in command\n\n");
	}
}

// set current directory, gets current working directory and assigns to
// the default directory global variable
void setCurrentDir() {
	char *buffer;
	char *result;
	int max_path = PATH_MAX + 1;

	// allocate memory
	buffer = (char *) malloc(max_path);
	result = getcwd(buffer, max_path);
	// free memory
	free(buffer);
	// store current working directory
	default_dir = result;
}

// execPwd prints out the current working directory
void execPwd(char *cmd[]) {
	setCurrentDir();	// calls function that gets current working directory
	fprintf(stdout, "%s\n\n", default_dir);
}

// checks if any redirection operator exists in the argument list,
// if yes, returns that operator else returns NULL
char* checkIfExists(char *args[], int count) {
	int i, j;

	for (i = 0; i < count; i++) {
		for (j = 0; j < 3; j++) {
			if (strcmp(args[i], redir_args[j]) == 0) {
				return redir_args[j];
			}
		}
	}
	return NULL;
}

// execChdir changes the current directory to the specified one
void execChdir(char *cmd[]) {
	// if directory not specified
	if (cmd[1] == NULL) {
		fprintf(stderr, "ERROR: Path not specified.\n\n");
		return;
	}
	// get path from argument
	char *path = cmd[1];
	struct stat buffer;	// stores the path information

	if (stat(path, &buffer) == 0) {	// checks if path exists
		if (buffer.st_mode & S_IFDIR) {	// checks if the path defined is a directory or not
			int v = chdir(path);	// changes working directory if path defined is a directory
			if (v < 0) {	// fails if value invalid
				fprintf(stderr, "ERROR: Something went wrong.\n\n");
				return;
			}
			fprintf(stdout, "Current directory changed to %s\n\n", path);
    	} else { // fails if path is not a directory
    		fprintf(stderr, "ERROR: Path specified is not a directory.\n\n");
    	}
	} else { // fails if path doesn't exist
		fprintf(stderr, "ERROR: Path specified does not exist.\n\n");
	}
}

// echo command for redirection testing
void execEchoForRedirection(char *cmd[]) {
	int i;
	// get all the arguments except the redirection operator (> or >>) and the filename
	// and prints them to console
	for (i = 1; i < argument_count - 2; i++) {
		fprintf(stdout, "%s ", cmd[i]);
	}
	fprintf(stdout, "\n\n");
}

// append standard output ( >> )
void execAppendOut(char *cmd[]) {
	// duplicate STDOUT for restore later
	int curr_out = dup(STDOUT);
	// open file for redirection
	// flags defined as:
	// O_APPEND since the redirection operator is >>
	// O_CREAT to create file if it doesn't exist
	// O_WRONLY for writing to the file
	// mode is 0644, owner has 110 (rw-), group and world has 100 (r--)
	int fd = open(cmd[argument_count-1], O_CREAT|O_APPEND|O_WRONLY, 0644);

	// check if file descriptor is valid
	if (fd < 0) {
		fprintf(stderr, "File couldn't be opened!\n\n");
		return;
	}
	// assign STDOUT to new fd and close STDOUT
	dup2(fd, STDOUT);
	// echo or pwd for redirection
	if (strcmp(cmd[0], "echo") == 0) {	// e.g. echo Hello World >> filename
		execEchoForRedirection(cmd);
	} else if (strcmp(cmd[0], "pwd") == 0) {	// e.g. pwd >> filename
		execPwd(cmd);
	} else {	// e.g. /bin/ls >> filename (using unix bash command to test redirection)
		createChildProcess(cmd, 1);
	}
	// restore stdout to console
	dup2(curr_out, STDOUT);
	// close descriptors
	close(curr_out);
	close(fd);
}

// redirect standard output ( > )
void execRedirOut(char *cmd[]) {
	// duplicate STDOUT for restore later
	int curr_out = dup(STDOUT);
	// open file for redirection
	// flags defined as:
	// O_TRUNC since the redirection operator is >
	// O_CREAT to create file if it doesn't exist
	// O_WRONLY for writing to the file
	// mode is 0644, owner has 110 (rw-), group and world has 100 (r--)
	int fd = open(cmd[argument_count-1], O_CREAT|O_TRUNC|O_WRONLY, 0644);

	// check if file descriptor is valid
	if (fd < 0) {
		fprintf(stderr, "File couldn't be opened!\n\n");
		return;
	}
	// assign STDOUT to new fd and close STDOUT
	dup2(fd, STDOUT);
	// echo or pwd for redirection
	if (strcmp(cmd[0], "echo") == 0) {	// e.g. echo Hello World > filename
		execEchoForRedirection(cmd);
	} else if (strcmp(cmd[0], "pwd") == 0) {	// e.g. pwd > filename
		execPwd(cmd);
	} else {	// e.g. /bin/ls > filename (using unix bash command to test redirection)
		createChildProcess(cmd, 1);
	}
	// restore stdout to console
	dup2(curr_out, STDOUT);
	// close descriptors
	close(curr_out);
	close(fd);
}

// redirect standard input
void execRedirIn(char *cmd[]) {
	// duplicate STDIN for restore later
	int curr_in = dup(STDIN);
	// open file for redirection
	// flags defined as:
	// O_RDONLY for reading the file
	// mode is 0644, owner has 110 (rw-), group and world has 100 (r--)
	int fd = open(cmd[argument_count-1], O_RDONLY, 0644);

	// check if file descriptor is valid
	if (fd < 0) {
		fprintf(stderr, "File couldn't be opened! Make sure the file exists!\n\n");
		return;
	}
	// assign STDIN to new fd and close STDIN
	dup2(fd, STDIN);

	// using bash commands to test redirection
	// e.g. > /bin/cat < filename
	createChildProcess(cmd, 1);

	// restore stdin to console
	dup2(curr_in, STDIN);
	// close descriptors
	close(curr_in);
	close(fd);
}

void execExit(char *cmd[]) {
	exit(0);
}
