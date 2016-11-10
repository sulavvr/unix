#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>		// use for PATH_MAX (pwd)
#include <sys/stat.h>

#define MAXLINE 259
#define PROMPT ">"
#define MAX_ARG_LIST 200

extern char **environ;
#define MAX_CMD_SIZE 50
#define SEARCH_FOR_CMD -1
typedef void(*buildInFunc) (char **);
typedef struct {
	char cmd[MAX_CMD_SIZE];
	buildInFunc func;
} builtInCmd;

// built-in commands
void execExit(char *cmd[]);
void execPwd(char *cmd[]);
void execChdir(char *cmd[]);
builtInCmd builtInCmds[] = {
	{"exit", execExit},
	{"pwd", execPwd},
	{"cd", execChdir}
};

int builtInCnt = sizeof(builtInCmds)/sizeof(builtInCmd);
int isBuiltIn(char *cmd);
void execBuiltIn(int i, char *cmd[]);

// capture SIG_INT and recover
sigjmp_buf ctrlc_buf;
void ctrl_handlr(int signo) {
	siglongjmp(ctrlc_buf, 1);
}

int main(int argc, char *argv[]) {

	char line[MAXLINE];
	pid_t childPID;
	int argn;
	char* args[MAX_ARG_LIST];
	int cmdn;

	if (signal(SIGINT, ctrl_handlr) == SIG_ERR) {
		fputs("ERROR: failed to register interrupts in kernel.\n", stderr);
	}

	// setup SIG_INT handler
	while (sigsetjmp(ctrlc_buf, 1) != 0);

	for (;;) {
		// prompt and get commandline
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
		// execute commandline
		if ((cmdn = isBuiltIn(args[0])) > -1) {
			execBuiltIn(cmdn, args);
		} else {
			childPID = fork();
			if (childPID == 0) {
				execv(line, argv);
				fputs("ERROR: can't execute command.\n", stderr);

				_exit(1);
			} else {
				waitpid(childPID, NULL, 0);
			}
		}

		// cleanup
		fflush(stderr);
		fflush(stdout);
	}

	return 0;

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

// execPwd prints out the current working directory
void execPwd(char *cmd[]) {
	char *buffer;
	char *result;
	int max_path = PATH_MAX + 1;

	buffer = (char *) malloc(max_path);
	result = getcwd(buffer, max_path);

	if (result != NULL) {
		fprintf(stdout, "%s\n", buffer);
	}
	free(buffer);
}

void execChdir(char *cmd[]) {
	if (cmd[1] == NULL) {
		fprintf(stderr, "ERROR: Path not specified.\n");
		return;
	}
	char *path = cmd[1];
	struct stat buffer;
	if (stat(path, &buffer) == 0) {
		fprintf(stdout, "%hu\n" ,buffer.st_mode);
		if (buffer.st_mode & S_IFDIR) {
			int v = chdir(path);
			if (v < 0) {
				fprintf(stderr, "ERROR: Something went wrong.");
    			return;
			}
    	} else {
    		fprintf(stderr, "ERROR: Path specified is not a directory.");
    		return;
    	}
	}



}

void execExit(char *cmd[]) {
	exit(0);
}