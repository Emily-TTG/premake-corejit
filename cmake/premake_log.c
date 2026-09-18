#include "premake_log.h"

#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>

static premake_log_fn s_sink = NULL;
static void*          s_udata = NULL;

void premake_set_log_sink(premake_log_fn fn, void* udata)
{
	s_sink  = fn;
	s_udata = udata;
}

int premake_log_has_sink(void)
{
	return s_sink != NULL;
}

void premake_log_write(int stream, const char* msg, size_t len)
{
	if (s_sink) {
		s_sink(stream, msg, len, s_udata);
	} else {
		FILE* f = (stream == PREMAKE_LOG_ERR) ? stderr : stdout;
		fwrite(msg, 1, len, f);
	}
}

static int l_print(lua_State* L)
{
	int n = lua_gettop(L);
	int i;
	luaL_Buffer b;

	lua_getglobal(L, "tostring"); /* stack: args..., tostring */
	for (i = 1; i <= n; i++) {
		lua_pushvalue(L, n + 1); /* tostring */
		lua_pushvalue(L, i); /* arg i */
		lua_call(L, 1, 1); /* -> string at n+1+i */
	}

	luaL_buffinit(L, &b);
	for (i = 1; i <= n; i++) {
		size_t l;

		const char* s = lua_tolstring(L, n + 1 + i, &l);
		if (s == NULL) {
			return luaL_error(L, "'tostring' must return a string to 'print'");
		}

		if (i > 1) luaL_addchar(&b, '\t');
		luaL_addlstring(&b, s, l);
	}

	luaL_addchar(&b, '\n');
	luaL_pushresult(&b);

	{
		size_t l;
		const char* out = lua_tolstring(L, -1, &l);
		premake_log_write(PREMAKE_LOG_OUT, out, l);
	}

	return 0;
}

static int l_write_impl(lua_State* L, int self_arg)
{
	int stream = (int) lua_tointeger(L, lua_upvalueindex(1));

	int n = lua_gettop(L);
	int i;
	for (i = self_arg; i <= n; i++) {
		size_t l;
		const char* s;
		if (lua_type(L, i) == LUA_TNUMBER) {
			lua_pushvalue(L, i); /* number -> string */
			s = lua_tolstring(L, -1, &l);
			premake_log_write(stream, s, l);
			lua_pop(L, 1);
		} else {
			s = luaL_checklstring(L, i, &l);
			premake_log_write(stream, s, l);
		}
	}

	return 0;
}

static int l_io_write(lua_State* L) { return l_write_impl(L, 1); } /* io.write(...) */
static int l_file_write(lua_State* L) { return l_write_impl(L, 2); } /* file:write(...) */
static int l_noop(lua_State* L) { lua_pushvalue(L, 1); return 1; } /* flush/close -> self */

static void push_stream_proxy(lua_State* L, int stream)
{
	lua_createtable(L, 0, 3);

	lua_pushinteger(L, stream);
	lua_pushcclosure(L, l_file_write, 1);
	lua_setfield(L, -2, "write");

	lua_pushcfunction(L, l_noop);
	lua_setfield(L, -2, "flush");

	lua_pushcfunction(L, l_noop);
	lua_setfield(L, -2, "close");
}

void premake_install_lua_log(lua_State* L)
{
	/* global print */
	lua_pushcfunction(L, l_print);
	lua_setglobal(L, "print");

	lua_getglobal(L, "io");
	if (lua_istable(L, -1)) {
		/* io.write(...) -> stdout/sink */
		lua_pushinteger(L, PREMAKE_LOG_OUT);
		lua_pushcclosure(L, l_io_write, 1);
		lua_setfield(L, -2, "write");

		/* io.stdout / io.stderr proxies */
		push_stream_proxy(L, PREMAKE_LOG_OUT);
		lua_setfield(L, -2, "stdout");
		push_stream_proxy(L, PREMAKE_LOG_ERR);
		lua_setfield(L, -2, "stderr");
	}
	lua_pop(L, 1); /* io */
}
