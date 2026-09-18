#include "premake.h"

#include <locale.h>
#include <stdlib.h>
#include <stdio.h>

static void jitprof_start(lua_State* L) { (void) L; }
static void jitprof_stop(lua_State* L)  { (void) L; }

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
		jitprof_start(L);
		z = premake_execute(L, argc, argv, "src/_premake_main.lua");
		jitprof_stop(L);
	}

	lua_close(L);
	return z;
}
