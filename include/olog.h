#ifndef OLOG_H
#define OLOG_H

#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUF_LEN_NV 256
#define BUF_LEN_V 512
#define FILENAME \
	(strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

enum Olog_Cntxt {
	debug,
	info,
	warn,
	error,
};

#define OLOG_FMT(str, fout, stmt, ...)                         \
	do {                                                   \
		fprintf(fout, str __VA_OPT__(, ) __VA_ARGS__); \
		stmt                                           \
	} while (0)

void
olog_init(const char *);

void
olog_set_cntxt(const enum Olog_Cntxt);

void
olog(size_t, const char *, ...);

#define olog_msg(fmt, ...) olog(BUF_LEN_NV, fmt __VA_OPT__(, ) __VA_ARGS__)
#define olog_msg_verbose(fmt, ...)                                  \
	olog(BUF_LEN_V, "[%s:%d:%s(...)] " fmt, FILENAME, __LINE__, \
	     __func__ __VA_OPT__(, ) __VA_ARGS__)

void
olog_flush();

void
olog_close();

#ifdef __cplusplus
}
#endif

#endif
