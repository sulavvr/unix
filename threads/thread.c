#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#define THRD_CNT 4

pthread_t threads[THRD_CNT];

void *thread_job(void *args) {
	clock_t ms;
	int cnt;
	cnt = rand() % 3 + 1;
	printf("Thread %u: starting: thread # = %u: iteration s = %u\n", pthread_self(), args, cnt);
	
	for (int i = 0; i < cnt; i++) {
		ms = rand() % 3 + 1;
		printf("Thread %u: running %u seconds\n", pthread_self(), ms);
		sleep(ms);
	}

	switch((unsigned int) args % 3) {
		case 0:
			printf("Thread %u: cancelling\n", pthread_self());
			pthread_cancel(pthread_self());
			break;
		case 1:
			printf("Thread %u: exiting\n", pthread_self());
			pthread_exit(args);
			break;
		default:
			printf("");
	}
}

int main(int argc, char *argv[]) {
	int retval;

	srand((unsigned int) clock());
	setvbuf(stdout, NULL, _IONBF, 0);

	printf("Process %u: started with thread %u\n", getpid(), pthread_self());

	for (int i = 0; i < THRD_CNT; ++i) {
		if (pthread_create(&(threads[i]), NULL, thread_job, i+1) == 0) {
			printf("Thread %u: starting thread %u\n", pthread_self(), threads[i]);
		} else {
			printf("Thread %u: error in creating thread %d\n", pthread_self(), i);
		}
	}

	for (int i = 0; i < THRD_CNT; ++i) {
		printf("Thread %u: manager ready to join thread %u\n", pthread_self(), threads[i]);
		int err = pthread_join(threads[i], &retval);
		if (err == 0) {
			printf("Thread %u: manager joined thread %u: retval = %d\n", pthread_self(), threads[i], retval);
		} else if (err == ESRCH) {
			printf("Thread %u: manager failed to find thread %u\n", pthread_self(), threads[i]);
		} else {
			printf("Thread %u: manager can't join thread %u\n", pthread_self(), threads[i]);
		}
	}
	
	return 0;
}
