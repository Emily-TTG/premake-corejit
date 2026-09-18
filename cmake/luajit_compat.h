/* Polyfill for Lua 5.3->5.2*
 * (*LuaJIT is 5.1 but provides a `LUAJIT_ENABLE_LUA52COMPAT` flag) */

#ifndef PREMAKE_LUAJIT_COMPAT_H
#define PREMAKE_LUAJIT_COMPAT_H

#include <lua.h>
#include <lauxlib.h>

#if LUA_VERSION_NUM < 503

#include <stdint.h>

/* premake.c defines a clashing `luaL_register` as 5.3 polyfill. */
#define luaL_register premake_luaL_register

#ifndef lua_rawlen
# define lua_rawlen(L, i) lua_objlen((L), (i))
#endif

/* 5.3 continuation API. LuaJIT lacks yield-across-C-call continuations, but
 * premake never yields on use. */
typedef intptr_t lua_KContext;
#ifndef lua_callk
# define lua_callk(L, na, nr, ctx, k) lua_call((L), (na), (nr))
#endif

#ifndef LUA_PATH_SEP
# define LUA_PATH_SEP LUA_PATHSEP
#endif

#ifndef lua_pushglobaltable
# define lua_pushglobaltable(L) lua_pushvalue((L), LUA_GLOBALSINDEX)
#endif

static inline int luaL_getsubtable(lua_State *L, int idx, const char *fname) {
	lua_getfield(L, idx, fname);
	if (lua_istable(L, -1)) return 1;
	lua_pop(L, 1);
	lua_newtable(L);
	lua_pushvalue(L, -1);
	lua_setfield(L, idx, fname);
	return 0;
}

#endif

#endif
