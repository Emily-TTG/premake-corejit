cmake_minimum_required(VERSION 3.16)
project(premake_jit C)

set(CMAKE_C_STANDARD 99)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

include(ExternalProject)
set(LUAJIT_PREFIX "${CMAKE_BINARY_DIR}/luajit")
set(LUAJIT_LIB "${LUAJIT_PREFIX}/lib/libluajit-5.1.a")
set(LUAJIT_INCLUDE_DIRS "${LUAJIT_PREFIX}/include/luajit-2.1")
set(LUAJIT_GIT_TAG "v2.1" CACHE STRING "LuaJIT git tag/branch/commit to build")

ExternalProject_Add(luajit_ext
	GIT_REPOSITORY https://github.com/LuaJIT/LuaJIT.git
	GIT_TAG ${LUAJIT_GIT_TAG}
	GIT_SHALLOW TRUE
	BUILD_IN_SOURCE TRUE
	CONFIGURE_COMMAND ""
	BUILD_COMMAND make amalg "XCFLAGS=-DLUAJIT_ENABLE_LUA52COMPAT" "PREFIX=${LUAJIT_PREFIX}"
	INSTALL_COMMAND make install "PREFIX=${LUAJIT_PREFIX}"
	BUILD_BYPRODUCTS "${LUAJIT_LIB}")

file(MAKE_DIRECTORY "${LUAJIT_INCLUDE_DIRS}")

add_library(luajit STATIC IMPORTED)
set_target_properties(luajit PROPERTIES IMPORTED_LOCATION "${LUAJIT_LIB}")
add_dependencies(luajit luajit_ext)
set(LUAJIT_LINK_LIBRARIES luajit)

set(LUACOMPAT_DIR "${CMAKE_BINARY_DIR}/luacompat")
file(WRITE "${LUACOMPAT_DIR}/lua5.3/lua.h" "#include <lua.h>\n")
file(WRITE "${LUACOMPAT_DIR}/lua5.3/lauxlib.h" "#include <lauxlib.h>\n")
file(WRITE "${LUACOMPAT_DIR}/lua5.3/lualib.h" "#include <lualib.h>\n")

# TODO(Emily): May want to provide Python here for LE usecase since Lua is an
#              uncommon system binary.
find_program(LUA_EXE NAMES luajit lua lua5.4 lua5.3 lua5.1 REQUIRED)
message(STATUS "Embed step interpreter: ${LUA_EXE}")

set(GENERATED_SCRIPTS "${CMAKE_BINARY_DIR}/scripts.c")
file(GLOB_RECURSE EMBED_INPUTS
	"${CMAKE_SOURCE_DIR}/src/*.lua"
	"${CMAKE_SOURCE_DIR}/modules/*.lua")

add_custom_command(
	OUTPUT "${GENERATED_SCRIPTS}"
	COMMAND "${LUA_EXE}" "${CMAKE_SOURCE_DIR}/cmake/embed.lua" "${CMAKE_SOURCE_DIR}" "${GENERATED_SCRIPTS}"
	DEPENDS "${CMAKE_SOURCE_DIR}/cmake/embed.lua" ${EMBED_INPUTS}
	COMMENT "Embedding Lua scripts"
	VERBATIM)

file(GLOB HOST_SOURCES "${CMAKE_SOURCE_DIR}/src/host/*.c")
list(FILTER HOST_SOURCES EXCLUDE REGEX "(curl_utils|http_download|http_get|http_post|zip_extract|zip_list|premake_main|debug_prompt)\\.c$")
list(APPEND HOST_SOURCES "${CMAKE_SOURCE_DIR}/cmake/debug_prompt_stub.c")

add_library(premake_core OBJECT ${HOST_SOURCES} "${GENERATED_SCRIPTS}")
add_dependencies(premake_core luajit_ext)

# TODO(Emily): This is currently Linux'd.
target_compile_definitions(premake_core PRIVATE _GNU_SOURCE LUA_USE_POSIX LUA_USE_DLOPEN)

target_compile_options(premake_core PRIVATE "-include" "${CMAKE_SOURCE_DIR}/cmake/luajit_compat.h")
target_include_directories(premake_core PUBLIC
	"${CMAKE_SOURCE_DIR}/src"
	"${CMAKE_SOURCE_DIR}/src/host"
	"${LUACOMPAT_DIR}"
	${LUAJIT_INCLUDE_DIRS})

target_compile_options(premake_core PRIVATE ${LUAJIT_CFLAGS_OTHER})

add_library(libpremake STATIC $<TARGET_OBJECTS:premake_core>)
set_target_properties(libpremake PROPERTIES OUTPUT_NAME premake)
target_include_directories(
	libpremake PUBLIC
	"${CMAKE_SOURCE_DIR}/src"
	"${CMAKE_SOURCE_DIR}/src/host"
	"${LUACOMPAT_DIR}"
	${LUAJIT_INCLUDE_DIRS})

target_link_libraries(libpremake PUBLIC ${LUAJIT_LINK_LIBRARIES} m ${CMAKE_DL_LIBS} uuid)

add_executable(premake5-jit" ${CMAKE_SOURCE_DIR}/cmake/premake_entry.c" $<TARGET_OBJECTS:premake_core>)
target_include_directories(
	premake5-jit PRIVATE
	"${CMAKE_SOURCE_DIR}/src"
	"${CMAKE_SOURCE_DIR}/src/host"
	"${LUACOMPAT_DIR}"
	${LUAJIT_INCLUDE_DIRS})

target_compile_options(
	premake5-jit PRIVATE
	"-include" "${CMAKE_SOURCE_DIR}/cmake/luajit_compat.h")

target_link_libraries(
	premake5-jit PRIVATE
	${LUAJIT_LINK_LIBRARIES}
	m
	${CMAKE_DL_LIBS}
	uuid)

set_target_properties(premake5-jit PROPERTIES ENABLE_EXPORTS TRUE)
