#include "olog.h"

#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define CTIME_SZ 25
#define DELIMS_SZ 9
#define TOTAL_BUF_SZ 8192
#define BUF_LEN 512
#define FLUSH_WAIT_TM 10000
#define FMT_STYLE "[ %s ] %s : %s\n"

static enum Olog_Context curr_lvl = info;
static FILE *file = NULL;

static const char *str_of_lvls[] = { "DEBUG", "INFO", "WARNING", "ERROR" };
static size_t lvls_lens[] = { 5, 4, 7, 5 };

static _Bool unmanaged = 0;
static atomic_int running = ATOMIC_VAR_INIT(1);
static atomic_int closed = ATOMIC_VAR_INIT(0);

static pthread_t flush_thrd;

typedef struct buf_t {
	char buf[BUF_LEN];
} buf_t;

static buf_t qdata[TOTAL_BUF_SZ];

struct Lq {
	buf_t *qbuf;
	alignas(64) atomic_size_t head;
	alignas(64) atomic_size_t tail;
};

static struct Lq olog_q = {
	.qbuf = qdata,
	.head = ATOMIC_VAR_INIT(0),
	.tail = ATOMIC_VAR_INIT(0),
};

static inline size_t
next_index(size_t i)
{
	return (i + 1) & (TOTAL_BUF_SZ - 1);
}

static void
lq_enqueue(const char *buf)
{
	size_t tail;
	size_t next;
	size_t head;

	do {
		tail = atomic_load_explicit(&olog_q.tail, memory_order_relaxed);
		head = atomic_load_explicit(&olog_q.head, memory_order_acquire);
		next = next_index(tail);

		if (next ==
		    atomic_load_explicit(&olog_q.head, memory_order_acquire))
			atomic_store_explicit(&olog_q.head, next_index(head),
					      memory_order_release);

	} while (!atomic_compare_exchange_strong_explicit(
		&olog_q.tail, &tail, next, memory_order_release,
		memory_order_relaxed));

	strncpy(olog_q.qbuf[tail].buf, buf, BUF_LEN);
	olog_q.qbuf[tail].buf[BUF_LEN - 1] = '\0';
}

static _Bool
lq_dequeue(buf_t *out)
{
	size_t head = atomic_load_explicit(&olog_q.head, memory_order_relaxed);
	size_t tail = atomic_load_explicit(&olog_q.tail, memory_order_acquire);

	if (head == tail)
		return 0;

	*out = olog_q.qbuf[head];
	atomic_store_explicit(&olog_q.head, next_index(head),
			      memory_order_release);
	return 1;
}

static void *
olog_flush([[maybe_unused]] void *arg)
{
	struct timespec tm;
	tm.tv_sec = 0;
	tm.tv_nsec = FLUSH_WAIT_TM;

	buf_t buf;

	while (atomic_load(&running)) {
		while (atomic_load_explicit(&olog_q.head,
					    memory_order_relaxed) ==
		       atomic_load_explicit(&olog_q.tail, memory_order_acquire))
			nanosleep(&tm, NULL);

		while (lq_dequeue(&buf))
			fputs(buf.buf, file);

		fflush(file);
	}
	while (lq_dequeue(&buf))
		fputs(buf.buf, file);

	fflush(file);
	return NULL;
}

static void
sighandle([[maybe_unused]] int signo)
{
	atomic_store(&running, 0);
}

static void
set_sighandle(void)
{
	struct sigaction sa;
	sa.sa_handler = sighandle;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
}

void
olog_init(const char *file_path)
{
	if (!(file = fopen(file_path, "w")))
		file = stdout;

	atexit(olog_close);
	set_sighandle();

	pthread_create(&flush_thrd, NULL, olog_flush, NULL);
}

void
olog_init_unmanaged(FILE *filep)
{
	file = filep;
	unmanaged = 1;

	atexit(olog_close);
	set_sighandle();

	pthread_create(&flush_thrd, NULL, olog_flush, NULL);
}

void
olog_close()
{
	int expected = 0;
	if (atomic_compare_exchange_strong(&closed, &expected, 1))
		return;

	atomic_store(&running, 0);
	pthread_detach(flush_thrd);
	if (file != stdout && file != stderr && !unmanaged)
		fclose(file);
}

void
olog_set_context(const enum Olog_Context lvl)
{
	curr_lvl = lvl;
}

void
olog(const char *fmt, ...)
{
	if (!fmt || !file)
		return;

	buf_t msg_buf;
	va_list args;
	va_start(args, fmt);
	int fmt_len =
		vsnprintf(msg_buf.buf,
			  BUF_LEN - CTIME_SZ - DELIMS_SZ - lvls_lens[curr_lvl],
			  fmt, args);
	if (fmt_len <= 0)
		goto cleanup;

	time_t now = time(NULL);
	struct tm tm_info;
	char time_str[CTIME_SZ];

	localtime_r(&now, &tm_info);
	strftime(time_str, CTIME_SZ, "%a %b %e %T %Y", &tm_info);

	buf_t msg_full_tm;
	snprintf(msg_full_tm.buf, BUF_LEN, FMT_STYLE, str_of_lvls[curr_lvl],
		 msg_buf.buf, time_str);

	lq_enqueue(msg_full_tm.buf);

cleanup:
	va_end(args);
}
