#include <stdio.h>

char *args[3] = {"<", ">", "<<"};
char* checkIfExists(char *array[], int count) {
	int i, j;

	for (i = 0; i < count; i++) {
		for (j = 0; j < 3; j++) {
			printf("%s - %s\n", array[i], args[j]);
			if (array[i] == args[j]) {
				printf("%s found it here\n", array[i]);
				return args[j];
			}
		}
	}
	return NULL;
}

int main() {

	int i;
	char *argus[3] = {"test", "<", "hello"};

	char *test = checkIfExists(argus, 3);

	printf("%s found", test);

	return 0;
}
