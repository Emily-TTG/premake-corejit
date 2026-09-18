#include "premake.h"
#include "premake_log.h"

#include <locale.h>
#include <stdlib.h>
#include <stdio.h>

static void jitprof_start(lua_State* L) { (void) L; }
static void jitprof_stop(lua_State* L)  { (void) L; }

static void demo_log_sink(int stream, const char* msg, size_t len, void* udata)
{
	(void) udata;
	fprintf(stderr, "[premake:%s] ", stream == PREMAKE_LOG_ERR ? "ERR" : "OUT");
	fwrite(msg, 1, len, stderr);
}

int main(int argc, const char** argv)
{
	lua_State* L;
	int z;

	if (!setlocale(LC_CTYPE, "C.UTF-8"))
		setlocale(LC_CTYPE, "");

	L = luaL_newstate();
	luaL_openlibs(L);

	z = premake_init(L);
	if (z == OKAY) {
		if (getenv("PMK_LOG_DEMO"))
			premake_set_log_sink(demo_log_sink, NULL);

		premake_install_lua_log(L);

		jitprof_start(L);
		z = premake_execute(L, argc, argv, "src/_premake_main.lua");
		jitprof_stop(L);
	}

	lua_close(L);
	return z;
}
