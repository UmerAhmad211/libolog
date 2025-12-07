#include "olog.h"

#include <bits/pthreadtypes.h>
#include <lz4frame_static.h>
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
#include <lz4file.h>

#ifndef TOTAL_BUF_SZ
#define TOTAL_BUF_SZ 8192
#endif

/*
 * 2GiB default
 */

#ifndef COMP_LIMIT
#define COMP_LIMIT (2L * (1 << 30))
#endif

#define CTIME_SZ 25
#define DELIMS_SZ 9
#define BUF_LEN 512
#define FLUSH_WAIT_TM 10000
#define FMT_STYLE "[ %s ] %s : %s\n"

static enum Olog_Context _Atomic curr_lvl = ATOMIC_VAR_INIT(info);
static FILE *file;
static char olog_fname[BUF_LEN];

static const char *str_of_lvls[] = { "DEBUG", "INFO", "WARNING", "ERROR" };
static size_t lvls_lens[] = { 5, 4, 7, 5 };

static atomic_int running = ATOMIC_VAR_INIT(1);
static atomic_int closed = ATOMIC_VAR_INIT(0);

static size_t bytes_wrtn = 0;

static pthread_t flush_thrd;

static unsigned int comp_fname_gen = 1;

typedef struct buf_t {
	char buf[BUF_LEN];
	atomic_bool has_data;
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
olog_strnlen(const char *buf, const size_t maxsz)
{
	const char *found = memchr(buf, '\0', maxsz);
	return found ? (size_t)(found - buf) : maxsz;
}

static inline size_t
next_index(size_t i)
{
#if (TOTAL_BUF_SZ & (TOTAL_BUF_SZ - 1)) == 0
	return (i + 1) & (TOTAL_BUF_SZ - 1);
#else
	return (i + 1) & TOTAL_BUF_SZ;
#endif
}

static void
lq_enqueue(const char *buf)
{
	struct timespec tm;
	tm.tv_sec = 0;
	tm.tv_nsec = FLUSH_WAIT_TM;

	size_t tail;
	size_t next;

	do {
		tail = atomic_load(&olog_q.tail);
		next = next_index(tail);

		while (next == atomic_load(&olog_q.head))
			nanosleep(&tm, NULL);

	} while (!atomic_compare_exchange_strong(&olog_q.tail, &tail, next));

	memcpy(olog_q.qbuf[tail].buf, buf, BUF_LEN);
	atomic_store(&olog_q.qbuf[tail].has_data, 1);
}

static _Bool
lq_dequeue(buf_t *out)
{
	size_t head = atomic_load(&olog_q.head);
	size_t tail = atomic_load(&olog_q.tail);

	if (head == tail)
		return 0;

	while (!atomic_load(&olog_q.qbuf[head].has_data))
		;

	*out = olog_q.qbuf[head];
	atomic_store(&olog_q.qbuf[head].has_data, 0);
	atomic_store(&olog_q.head, next_index(head));
	return 1;
}

#define CHUNK_SZ (128 * (1 << 10))

static void
olog_compress(void *buf, void *dst_buf)
{
	char comp_fname[BUF_LEN];

	if (!buf || !dst_buf) {
		return;
	}

	/*
	 * compressed file always starts with a hex number,
	 * compression uses lz4frame
	 */

	snprintf(comp_fname, BUF_LEN, "%x.lz4", comp_fname_gen);
	++comp_fname_gen;

	LZ4F_compressionContext_t context;
	size_t err = LZ4F_createCompressionContext(&context, LZ4F_VERSION);
	if (LZ4F_isError(err))
		return;

	FILE *comp_fin = fopen(olog_fname, "rb");
	FILE *comp_fout = fopen(comp_fname, "wb");

	if (!comp_fin || !comp_fout)
		return;

	LZ4F_preferences_t prefs = { 0 };
	size_t hdr_sz = LZ4F_compressBegin(context, dst_buf,
					   LZ4F_HEADER_SIZE_MAX, &prefs);
	fwrite(dst_buf, 1, hdr_sz, comp_fout);

	size_t read;
	while ((read = fread(buf, 1, CHUNK_SZ, comp_fin))) {
		size_t c = LZ4F_compressUpdate(
			context, dst_buf, LZ4F_compressFrameBound(read, &prefs),
			buf, read, NULL);
		fwrite(dst_buf, 1, c, comp_fout);
	}

	size_t end = LZ4F_compressEnd(context, dst_buf,
				      LZ4F_compressFrameBound(0, &prefs), NULL);
	fwrite(dst_buf, 1, end, comp_fout);
	LZ4F_freeCompressionContext(context);

	fclose(comp_fin);
	fclose(comp_fout);

	/*
	 * Reset file after it has been, compressed
	 */

	fclose(file);
	file = fopen(olog_fname, "w");
	bytes_wrtn = 0;

	return;
}

static void *
olog_flush([[maybe_unused]] void *arg)
{
	void *const src_buf = malloc(CHUNK_SZ);
	void *const dst_buf = malloc(LZ4F_compressFrameBound(CHUNK_SZ, NULL));

	struct timespec tm;
	tm.tv_sec = 0;
	tm.tv_nsec = FLUSH_WAIT_TM;

	buf_t buf;

	while (atomic_load(&running)) {
		while (atomic_load(&olog_q.head) == atomic_load(&olog_q.tail))
			nanosleep(&tm, NULL);

		while (lq_dequeue(&buf)) {
			size_t buflen = olog_strnlen(buf.buf, BUF_LEN);
			bytes_wrtn += buflen;
			fputs(buf.buf, file);
		}
		fflush(file);

		if (bytes_wrtn > COMP_LIMIT && file != stdout)
			olog_compress(src_buf, dst_buf);
	}

	while (lq_dequeue(&buf)) {
		size_t buflen = olog_strnlen(buf.buf, BUF_LEN);
		bytes_wrtn += buflen;
		fputs(buf.buf, file);
	}

	if (bytes_wrtn > COMP_LIMIT && file != stdout)
		olog_compress(src_buf, dst_buf);

	free(src_buf);
	free(dst_buf);
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
	if (file_path == NULL)
		file = stdout;
	else if (!(file = fopen(file_path, "w")))
		file = stdout;

	if (file != stdout)
		strncpy(olog_fname, file_path, BUF_LEN);

	atexit(olog_close);
	set_sighandle();

	pthread_create(&flush_thrd, NULL, olog_flush, NULL);
}

void
olog_close()
{
	int expected = 0;
	if (!atomic_compare_exchange_strong(&closed, &expected, 1))
		return;

	atomic_store(&running, 0);
	pthread_join(flush_thrd, NULL);

	if (file != stdout)
		fclose(file);
}

void
olog_set_context(const enum Olog_Context lvl)
{
	atomic_store(&curr_lvl, lvl);
}

void
olog(const char *fmt, ...)
{
	if (!fmt || !file || !atomic_load(&running))
		return;

	buf_t msg_buf;
	va_list args;
	va_start(args, fmt);
	if (vsnprintf(msg_buf.buf,
		      BUF_LEN - CTIME_SZ - DELIMS_SZ -
			      lvls_lens[atomic_load(&curr_lvl)],
		      fmt, args) < 0)
		goto cleanup;

	time_t now = time(NULL);
	struct tm tm_info;
	char time_str[CTIME_SZ];

	localtime_r(&now, &tm_info);
	if (!strftime(time_str, CTIME_SZ, "%a %b %e %T %Y", &tm_info))
		goto cleanup;

	buf_t msg_full_tm;
	if (snprintf(msg_full_tm.buf, BUF_LEN, FMT_STYLE,
		     str_of_lvls[atomic_load(&curr_lvl)], msg_buf.buf,
		     time_str) < 0)
		goto cleanup;

	lq_enqueue(msg_full_tm.buf);

cleanup:
	va_end(args);
}
