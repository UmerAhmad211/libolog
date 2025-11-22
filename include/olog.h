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

/**
 * takes a filename or NULL, if NULL then flushes
 * to stdout or to filename
 * handled by callee
 */
void
olog_init(const char *);

/**
 * takes FILE*, handled by the caller
 */
void
olog_init_unmanaged(FILE *);

/**
 * sets context.
 * takes either info, warn, debug or error.
 * defaults to info
 */
void
olog_set_context(const enum Olog_Context);

/**
 * olog, takes fmt then var args,
 * not recommended, use olog_msg or olog_msg_verbose.
 * verbose includes filename, line no and func name
 */
void
olog(const char *, ...);

#define olog_msg(fmt, ...) olog(fmt __VA_OPT__(, ) __VA_ARGS__)
#define olog_msg_verbose(fmt, ...)                      \
	olog("[%s:%d:%s(..)] " fmt, FILENAME, __LINE__, \
	     __func__ __VA_OPT__(, ) __VA_ARGS__)

/**
 * automatically called on any kind of exit,
 * if called more then once then only first one works
 */
void
olog_close();

#ifdef __cplusplus
}
#endif

#endif
