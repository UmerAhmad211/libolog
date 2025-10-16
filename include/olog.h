#ifndef OLOG_H
#define OLOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bits/pthreadtypes.h>
#include <pthread.h>

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
olog(const char *, ...);

#define olog_msg(fmt, ...) olog(fmt, ##__VA_ARGS__)

void
olog_flush();

void
olog_close();

#ifdef __cplusplus
}
#endif

#endif
