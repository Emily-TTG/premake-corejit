#ifndef PREMAKE_LOG_H
#define PREMAKE_LOG_H

#include <stddef.h>

struct lua_State;

#define PREMAKE_LOG_OUT 0 /* normal/informational (was stdout) */
#define PREMAKE_LOG_ERR 1 /* warnings and errors  (was stderr) */

/* `msg` is not cstr. */
typedef void (*premake_log_fn)(int stream, const char* msg, size_t len, void* udata);

void premake_set_log_sink(premake_log_fn fn, void* udata);
int  premake_log_has_sink(void);

void premake_log_write(int stream, const char* msg, size_t len);

void premake_install_lua_log(struct lua_State* L);

#endif
