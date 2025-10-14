#include "olog.h"
#include <pthread.h>
#include <sched.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

/*
 * ctime is guranteed to always 
 * return a buf of length 25
 */
#define CTIME_SZ 25
#define DELIMS_SZ 8
#define BUF_SZ 4096
#define FLUSH_WAIT_TM 2
#define FMT_STYLE "[ %s ] %s : %s"

static enum Olog_Cntxt curr_lvl = info;
static FILE *olog_file = NULL;
static alignas(BUF_SZ) char olog_buf[BUF_SZ];
static size_t olog_offset = 0;
static time_t lst_flush_tm = 0;
static const char *str_of_lvls[] = { "DEBUG", "INFO", "WARNING", "ERROR" };
static size_t lvls_lens[] = { 5, 4, 7, 5 };
static pthread_mutex_t olog_mutex = PTHREAD_MUTEX_INITIALIZER;

void
olog_init(const char *olog_file_path)
{
	if (!(olog_file = fopen(olog_file_path, "w")))
		olog_file = stdout;

	lst_flush_tm = time(NULL);
}

void
olog_flush()
{
	if (olog_offset == 0)
		return;
	fwrite(olog_buf, sizeof(char), olog_offset, olog_file);
	fflush(olog_file);
	olog_offset = 0;
	lst_flush_tm = time(NULL);
}

void
olog_close()
{
	pthread_mutex_lock(&olog_mutex);
	olog_flush();
	pthread_mutex_unlock(&olog_mutex);
	if (olog_file != stdout || olog_file != stderr)
		fclose(olog_file);
}

void
olog_set_cntxt(const enum Olog_Cntxt lvl)
{
	curr_lvl = lvl;
}

void
olog_msg(const char *msg)
{
	if (!msg || !olog_file)
		return;

	time_t now = time(NULL);

	pthread_mutex_lock(&olog_mutex);

	if (strlen(msg) + CTIME_SZ + lvls_lens[curr_lvl] + DELIMS_SZ >=
	    BUF_SZ - olog_offset)
		OLOG_FMT(FMT_STYLE, olog_file, , str_of_lvls[curr_lvl], msg,
			 ctime(&now));
	else {
		int len = snprintf(olog_buf + olog_offset, BUF_SZ - olog_offset,
				   FMT_STYLE, str_of_lvls[curr_lvl], msg,
				   ctime(&now));
		if (!(len < 0))
			olog_offset += len;
	}

	if (difftime(now, lst_flush_tm) >= FLUSH_WAIT_TM)
		olog_flush();

	pthread_mutex_unlock(&olog_mutex);
}
