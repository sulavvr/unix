/**************************************
 * File: threads.c
 * Name: Sulav Regmi
 * ID: 50211843
 * Unix Systems Programming
 * Dr. Jeff Jenness
 * Creates N threads which increments a shared variable
 * Not using mutex to lock shared variable
 **************************************/
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// define loops for updating shared variable
#define total 100000
// holds unsigned long value, shared variable
size_t shared = 0;

// calls when thread is created
void *update_shared(void *args) {
	// loops through total value and updates shared variable for each thread
	for (int i = 0; i < total; i++) {
		shared += i;
	}
	// output thread variable and value of variable
	printf("Shared value for thread #%zu - %zu\n", pthread_self(), shared);
	return NULL;
}

// get actual value when updating without threads
size_t get_actual_value(int num_threads) {
	size_t actual_value = 0;
	for (int i = 0; i < num_threads; i++) {
		for (int j = 0; j < total; j++) {
			actual_value += j;
		}
	}

	return actual_value;
}

int main(int argc, char *argv[]) {
	int num_threads = 0;

	// loop until num_threads greater than or equal to 0
	do {
		printf("Enter number of threads.. (-1 to exit) ");
		// exit if input is not an integer
		if (scanf("%d", &num_threads) != 1) {
			printf("Illegal input... Enter number...\n");
			exit(0);
		}
		// check if num threads is 0 or -1
		if (num_threads == 0) {
			printf("Enter thread count more than 0...\n");
			continue;
		} else if (num_threads == -1) {
			printf("Exiting...\n");
			exit(0);
		}

		// initialize threads with size as input
		pthread_t threads[num_threads];
		for (int i = 0; i < num_threads; i++) {
			// create threads based on input
			int p_th = pthread_create(&threads[i], NULL, update_shared, NULL);
			if (p_th != 0) {
				printf("Couldn't create a thread... exiting...\n");
				exit(0);
			}
		}

		// terminating threads by joining them
		for (int i = 0; i < num_threads; i++) {
			int p_join = pthread_join(threads[i], NULL);

			if (p_join != 0) {
				printf("Exiting program!\n");
				exit(0);
			}
		}

		// print the values, shared value & the actual value
		printf("Value of shared variable - %zu\n", shared);
		printf("Actual value of variable - %zu\n", get_actual_value(num_threads));
	} while (num_threads <= 0);

	return 0;
}