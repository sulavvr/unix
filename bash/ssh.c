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
void execEchoForRedirection(char *cmd[]);
builtInCmd builtInCmds[] = {
	{"exit", execExit},
	{"pwd", execPwd},
	{"cd", execChdir},
};

int builtInCnt = sizeof(builtInCmds)/sizeof(builtInCmd);
int isBuiltIn(char *cmd);
void execBuiltIn(int i, char *cmd[]);

int argument_count = 0;
// redirection arguments
char *redir_args[3] = {"<", ">", ">>"};
char *checkIfExists(char *array[], int count);
void setCurrentDir();
void createChildProcess(char *args[], int redir);
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

	if (signal(SIGINT, ctrl_handlr) == SIG_ERR) {
		fputs("ERROR: failed to register interrupts in kernel.\n", stderr);
	}

	// setup SIG_INT handler
	while (sigsetjmp(ctrlc_buf, 1) != 0);

	for (;;) {
		// prompt and get commandline
		setCurrentDir();
		fputs(default_dir, stdout);
		fputs("\n", stdout);
		fputs(PROMPT, stdout);
		fgets(line, MAXLINE, stdin);

		if (feof(stdin)) break; //exit on end of input

		// process commandline
		if  (line[strlen(line) - 1] == '\n') {
			line[strlen(line) - 1] = '\0';
		}

		// build argument list
		args[argn=0] = strtok(line, " \t");

		while (args[argn] != NULL && argn < MAX_ARG_LIST) {
			args[++argn] = strtok(NULL, " \t");
		}

		argument_count = argn;

		// execute commandline
		if ((redir_type = checkIfExists(args, argn)) != NULL) {
			if (strcmp(redir_type, ">>") == 0) {
				execAppendOut(args);
			} else if (strcmp(redir_type, ">") == 0) {
				execRedirOut(args);
			} else if (strcmp(redir_type, "<") == 0) {
				execRedirIn(args);
			}
		} else if ((cmdn = isBuiltIn(args[0])) > -1) {
			execBuiltIn(cmdn, args);
		} else {
			createChildProcess(args, 0);
		}

		// cleanup
		fflush(stderr);
		fflush(stdout);
	}

	return 0;

}

void createChildProcess(char *args[], int redir) {
	pid_t childPID;
	childPID = fork();
	if (childPID == 0) {
		if (redir == 0) {
			execve(args[0], args, environ);
		} else {
			execv(args[0], (char *[]){args[0], NULL});
		}
		// execv(line, argv);
		fprintf(stderr, "ERROR: can't execute command.\n");

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
		fprintf(stderr, "ERROR: unknown built-in command\n");
	}
}


void setCurrentDir() {
	char *buffer;
	char *result;
	int max_path = PATH_MAX + 1;

	// allocate memory
	buffer = (char *) malloc(max_path);
	result = getcwd(buffer, max_path);
	// free memory
	free(buffer);

	default_dir = result;
}

// execPwd prints out the current working directory
void execPwd(char *cmd[]) {
	setCurrentDir();
	fprintf(stdout, "%s\n\n", default_dir);
}

char* checkIfExists(char *array[], int count) {
	int i, j;

	for (i = 0; i < count; i++) {
		for (j = 0; j < 3; j++) {
			if (strcmp(array[i], redir_args[j]) == 0) {
				return redir_args[j];
			}
		}
	}
	return NULL;
}

// execChdir changes the current directory to the specified
void execChdir(char *cmd[]) {
	if (cmd[1] == NULL) {
		fprintf(stderr, "ERROR: Path not specified.\n\n");
		return;
	}
	// get path from argument
	char *path = cmd[1];
	struct stat buffer;

	if (stat(path, &buffer) == 0) {
		if (buffer.st_mode & S_IFDIR) {
			int v = chdir(path);
			if (v < 0) {
				fprintf(stderr, "ERROR: Something went wrong.");
				return;
			}
			fprintf(stdout, "Current directory changed to %s\n\n", path);
    	} else {
    		fprintf(stderr, "ERROR: Path specified is not a directory.\n\n");
    	}
	} else {
		fprintf(stderr, "ERROR: Path specified does not exist.\n");
	}
}

// echo command for redirection testing
void execEchoForRedirection(char *cmd[]) {
	int i;

	for (i = 1; i < argument_count - 2; i++) {
		fprintf(stdout, "%s ", cmd[i]);
	}
	fprintf(stdout, "\n");
}

// append standard output ( >> )
void execAppendOut(char *cmd[]) {
	// duplicate STDOUT for restore later
	int curr_out = dup(STDOUT);
	// open file for redirection
	int fd = open(cmd[argument_count-1], O_CREAT|O_APPEND|O_WRONLY, 0644);

	// check if file descriptor is valid
	if (fd < 0) {
		fprintf(stderr, "File couldn't be opened!");
		return;
	}
	// assign STDOUT to new fd and close STDOUT
	dup2(fd, STDOUT);
	// echo or pwd for redirection
	if (strcmp(cmd[0], "echo") == 0) {
		execEchoForRedirection(cmd);
	} else if (strcmp(cmd[0], "pwd") == 0) {
		execPwd(cmd);
	} else {
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
	int fd = open(cmd[argument_count-1], O_CREAT|O_TRUNC|O_WRONLY, 0644);

	// check if file descriptor is valid
	if (fd < 0) {
		fprintf(stderr, "File couldn't be opened!");
		return;
	}
	// assign STDOUT to new fd and close STDOUT
	dup2(fd, STDOUT);
	// echo or pwd for redirection
	if (strcmp(cmd[0], "echo") == 0) {
		execEchoForRedirection(cmd);
	} else if (strcmp(cmd[0], "pwd") == 0) {
		execPwd(cmd);
	} else {
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
	// execPwd
	printf("%s\n", cmd[0]);
}

void execExit(char *cmd[]) {
	exit(0);
}