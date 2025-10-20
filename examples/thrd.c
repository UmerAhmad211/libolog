#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <olog.h>

#define NUM_THREADS 5

void *
log_from_thread(void *thread_id)
{
	int id = *((int *)thread_id);

	olog_msg("Logging from thread %d", id);

	pthread_exit(NULL);
}

int
main()
{
	olog_init(NULL);
	olog_msg("Starting multithreaded logging");

	pthread_t threads[NUM_THREADS];
	int thread_ids[NUM_THREADS];

	for (int i = 0; i < NUM_THREADS; ++i) {
		thread_ids[i] = i;
		int ret = pthread_create(&threads[i], NULL, log_from_thread,
					 &thread_ids[i]);
		if (ret) {
			exit(EXIT_FAILURE);
		}
	}

	for (int i = 0; i < NUM_THREADS; ++i) {
		pthread_join(threads[i], NULL);
	}

	olog_msg("Multithreaded logging finished");
	olog_close();

	return 0;
}
