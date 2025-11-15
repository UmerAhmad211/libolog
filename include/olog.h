#ifndef OLOG_H
#define OLOG_H

#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FILENAME \
	(strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

enum Olog_Context {
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
olog_init_unmanaged(FILE *);

void
olog_set_context(const enum Olog_Context);

void
olog(const char *, ...);

#define olog_msg(fmt, ...) olog(fmt __VA_OPT__(, ) __VA_ARGS__)
#define olog_msg_verbose(fmt, ...)                      \
	olog("[%s:%d:%s(..)] " fmt, FILENAME, __LINE__, \
	     __func__ __VA_OPT__(, ) __VA_ARGS__)

void
olog_close();

#ifdef __cplusplus
}
#endif

#endif
