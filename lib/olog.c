#include "olog.h"
#include <pthread.h>
#include <sched.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <stdatomic.h>

/*
 * ctime is guranteed to always 
 * return a buf of length 25
 */
#define CTIME_SZ 25
#define DELIMS_SZ 8
#define TOTAL_BUF_SZ 8192
#define FLUSH_WAIT_TM 2
#define FMT_STYLE "[ %s ] %s : %s"

static enum Olog_Cntxt curr_lvl = info;
static FILE *file = NULL;
static alignas(TOTAL_BUF_SZ / 2) char olog_buf[TOTAL_BUF_SZ];
static size_t buf_offset = 0;
static time_t last_flush_tm = 0;
static const char *str_of_lvls[] = { "DEBUG", "INFO", "WARNING", "ERROR" };
static size_t lvls_lens[] = { 5, 4, 7, 5 };
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static _Bool unmanaged = 0;

void
olog_init(const char *file_path)
{
	if (!(file = fopen(file_path, "w")))
		file = stdout;

	last_flush_tm = time(NULL);
}

void
olog_init_unmanaged(FILE *filep)
{
	file = filep;
	unmanaged = 1;
	last_flush_tm = time(NULL);
}

void
olog_flush()
{
	if (buf_offset == 0)
		return;
	fwrite(olog_buf, sizeof(char), buf_offset, file);
	fflush(file);
	buf_offset = 0;
	last_flush_tm = time(NULL);
}

void
olog_close()
{
	pthread_mutex_lock(&mutex);
	olog_flush();
	pthread_mutex_unlock(&mutex);
	if ((file != stdout || file != stderr) && !unmanaged)
		fclose(file);
}

void
olog_set_cntxt(const enum Olog_Cntxt lvl)
{
	curr_lvl = lvl;
}

void
olog(size_t buf_len, const char *fmt, ...)
{
	if (!fmt || !file)
		return;

	time_t now = time(NULL);

	pthread_mutex_lock(&mutex);

	va_list args;
	va_start(args, fmt);

	char msg_buf[buf_len];
	int fmt_len = vsnprintf(msg_buf, buf_len, fmt, args);
	if (fmt_len < 0)
		return;
	va_end(args);

	if (sizeof(msg_buf) + CTIME_SZ + lvls_lens[curr_lvl] + DELIMS_SZ >=
	    TOTAL_BUF_SZ - buf_offset)
		olog_flush();

	int msg_len = snprintf(olog_buf + buf_offset, TOTAL_BUF_SZ - buf_offset,
			       FMT_STYLE, str_of_lvls[curr_lvl], msg_buf,
			       ctime(&now));
	if (msg_len < 0)
		return;
	else
		buf_offset += msg_len;

	if (difftime(now, last_flush_tm) >= FLUSH_WAIT_TM)
		olog_flush();

	pthread_mutex_unlock(&mutex);
}
