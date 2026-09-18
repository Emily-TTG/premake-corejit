#include "premake.h"

#if PLATFORM_WINDOWS

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct UWideString {
	size_t  len;
	wchar_t s[1];
} UWideString;

#define LUA_WIDESTRING "LUA_WIDESTRING"

static UWideString* newwidestr(lua_State* L, const wchar_t* s, size_t len)
{
	UWideString* ws = (UWideString*)lua_newuserdata(L, sizeof(UWideString) + len * sizeof(wchar_t));
	luaL_newmetatable(L, LUA_WIDESTRING);
	lua_setmetatable(L, -2);
	if (s) memcpy(ws->s, s, len * sizeof(wchar_t));
	ws->s[len] = L'\0';
	ws->len = len;
	return ws;
}

const wchar_t* luaL_convertlstring(lua_State* L, const char* s, size_t nlen, size_t* len)
{
	int size;
	UWideString* ws;
	if (s == NULL) {
		if (len != NULL) *len = 0;
		return NULL;
	}
	size = MultiByteToWideChar(CP_UTF8, 0, s, (int)nlen, NULL, 0);
	if (size == 0) {
		if (len != NULL) *len = 0;
		return NULL;
	}
	ws = newwidestr(L, NULL, (size_t)size);
	MultiByteToWideChar(CP_UTF8, 0, s, (int)nlen, ws->s, size);
	if (len != NULL) *len = (size_t)size;
	return ws->s;
}

const char* luaL_convertlwstring(lua_State* L, const wchar_t* ws, size_t wlen, size_t* len)
{
	int size;
	char* tmp;
	if (ws == NULL) {
		if (len != NULL) *len = 0;
		return NULL;
	}
	size = WideCharToMultiByte(CP_UTF8, 0, ws, (int)wlen, NULL, 0, NULL, NULL);
	if (size == 0) {
		if (len != NULL) *len = 0;
		return NULL;
	}
	tmp = (char*)malloc((size_t)size);
	WideCharToMultiByte(CP_UTF8, 0, ws, (int)wlen, tmp, size, NULL, NULL);
	lua_pushlstring(L, tmp, (size_t)size);
	free(tmp);
	if (len != NULL) *len = (size_t)size;
	return lua_tostring(L, -1);
}

const char* luaL_convertwstring(lua_State* L, const wchar_t* ws, size_t* len)
{
	int size, wlen;
	char* tmp;
	if (ws == NULL) {
		if (len != NULL) *len = 0;
		return NULL;
	}
	wlen = (int)wcslen(ws);
	size = WideCharToMultiByte(CP_UTF8, 0, ws, wlen, NULL, 0, NULL, NULL);
	if (size == 0) {
		if (len != NULL) *len = 0;
		return NULL;
	}
	tmp = (char*)malloc((size_t)size);
	WideCharToMultiByte(CP_UTF8, 0, ws, wlen, tmp, size, NULL, NULL);
	lua_pushlstring(L, tmp, (size_t)size);
	free(tmp);
	if (len != NULL) *len = (size_t)size;
	return lua_tostring(L, -1);
}

const wchar_t* luaL_convertlstringi(lua_State* L, int idx, size_t* len)
{
	size_t nlen;
	const char* s = lua_tolstring(L, idx, &nlen);
	return luaL_convertlstring(L, s, nlen, len);
}

const wchar_t* luaL_checkconvertlstring(lua_State* L, int idx, size_t* len)
{
	size_t nlen;
	const char* s = luaL_checklstring(L, idx, &nlen);
	const wchar_t* ws = luaL_convertlstring(L, s, nlen, len);
	if (ws == NULL) luaL_error(L, "conversion failure");
	return ws;
}

const wchar_t* luaL_optconvertlstring(lua_State* L, int idx, const wchar_t* def, size_t* len)
{
	if (lua_isnoneornil(L, idx)) {
		if (len != NULL) *len = (def ? wcslen(def) : 0);
		return def;
	}
	return luaL_checkconvertlstring(L, idx, len);
}

#endif
