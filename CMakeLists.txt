cmake_minimum_required(VERSION 3.16)
project(premake_jit C)

set(CMAKE_C_STANDARD 99)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()
if(NOT MSVC)
	set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

set(PMK_DEFS)
if(WIN32)
	list(APPEND PMK_DEFS UNICODE _UNICODE)
	set(PMK_LIBS ole32 advapi32 user32 ws2_32 version)
elseif(APPLE)
	set(PMK_LIBS m "-framework CoreFoundation")
else()
	list(APPEND PMK_DEFS _GNU_SOURCE)
	set(PMK_LIBS m ${CMAKE_DL_LIBS} rt uuid)
endif()

set(LJ_COMPAT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/luajit_compat.h")
if(MSVC)
	set(PMK_FORCE_INCLUDE "/FI${LJ_COMPAT}")
else()
	set(PMK_FORCE_INCLUDE "-include" "${LJ_COMPAT}")
endif()

include(ExternalProject)
set(LUAJIT_GIT_TAG "v2.1" CACHE STRING "LuaJIT git tag/branch/commit to build")
set(LUAJIT_SRC "${CMAKE_BINARY_DIR}/luajit-src")
set(LUAJIT_INCLUDE_DIRS "${LUAJIT_SRC}/src")

if(WIN32)
	set(LUAJIT_LIB "${LUAJIT_SRC}/src/lua51.lib")
	set(LUAJIT_EXE "${LUAJIT_SRC}/src/luajit.exe")
	ExternalProject_Add(luajit_ext
		GIT_REPOSITORY https://github.com/LuaJIT/LuaJIT.git
		GIT_TAG ${LUAJIT_GIT_TAG}
		GIT_SHALLOW TRUE
		SOURCE_DIR "${LUAJIT_SRC}"
		BUILD_IN_SOURCE TRUE
		CONFIGURE_COMMAND ""
		BUILD_COMMAND ${CMAKE_COMMAND} -E env "CL=/DLUAJIT_ENABLE_LUA52COMPAT"
			cmd /c "cd /d ${LUAJIT_SRC}\\src && msvcbuild.bat static"
		INSTALL_COMMAND ""
		BUILD_BYPRODUCTS "${LUAJIT_LIB}" "${LUAJIT_EXE}")
else()
	set(LUAJIT_LIB "${LUAJIT_SRC}/src/libluajit.a")
	set(LUAJIT_EXE "${LUAJIT_SRC}/src/luajit")
	set(LUAJIT_BUILD_ENV)
	if(APPLE)
		set(_dep "${CMAKE_OSX_DEPLOYMENT_TARGET}")
		if(NOT _dep)
			set(_dep "11.0")
		endif()
		set(LUAJIT_BUILD_ENV "MACOSX_DEPLOYMENT_TARGET=${_dep}")
	endif()
	ExternalProject_Add(luajit_ext
		GIT_REPOSITORY https://github.com/LuaJIT/LuaJIT.git
		GIT_TAG ${LUAJIT_GIT_TAG}
		GIT_SHALLOW TRUE
		SOURCE_DIR "${LUAJIT_SRC}"
		BUILD_IN_SOURCE TRUE
		CONFIGURE_COMMAND ""
		BUILD_COMMAND ${CMAKE_COMMAND} -E env ${LUAJIT_BUILD_ENV}
			make amalg "XCFLAGS=-DLUAJIT_ENABLE_LUA52COMPAT"
		INSTALL_COMMAND ""
		BUILD_BYPRODUCTS "${LUAJIT_LIB}" "${LUAJIT_EXE}")
endif()

file(MAKE_DIRECTORY "${LUAJIT_INCLUDE_DIRS}")

add_library(luajit STATIC IMPORTED)
set_target_properties(luajit PROPERTIES IMPORTED_LOCATION "${LUAJIT_LIB}")
add_dependencies(luajit luajit_ext)
set(LUAJIT_LINK_LIBRARIES luajit)

set(LUACOMPAT_DIR "${CMAKE_CURRENT_BINARY_DIR}/luacompat")
file(WRITE "${LUACOMPAT_DIR}/lua5.3/lua.h" "#include <lua.h>\n")
file(WRITE "${LUACOMPAT_DIR}/lua5.3/lauxlib.h" "#include <lauxlib.h>\n")
file(WRITE "${LUACOMPAT_DIR}/lua5.3/lualib.h" "#include <lualib.h>\n")

# TODO(Emily): May want to provide Python here for LE usecase since Lua is an
#              uncommon system binary.
find_program(LUA_EXE NAMES luajit lua lua5.4 lua5.3 lua5.1 REQUIRED)
message(STATUS "Embed step interpreter: ${LUA_EXE}")

set(GENERATED_SCRIPTS "${CMAKE_CURRENT_BINARY_DIR}/scripts.c")
file(GLOB_RECURSE EMBED_INPUTS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.lua"
	"${CMAKE_CURRENT_SOURCE_DIR}/modules/*.lua")

file(GLOB_RECURSE MANIFESTS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/_manifest.lua"
	"${CMAKE_CURRENT_SOURCE_DIR}/modules/_manifest.lua")

add_custom_command(
	OUTPUT "${GENERATED_SCRIPTS}"
	COMMAND "${LUAJIT_EXE}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed.lua"
		"${CMAKE_CURRENT_SOURCE_DIR}" "${GENERATED_SCRIPTS}" ${MANIFESTS}
	DEPENDS luajit_ext "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed.lua" ${EMBED_INPUTS}
	COMMENT "Embedding Lua scripts"
	VERBATIM)

file(GLOB HOST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/host/*.c")
list(FILTER HOST_SOURCES EXCLUDE REGEX
	"(curl_utils|http_download|http_get|http_post|zip_extract|zip_list|premake_main|debug_prompt)\\.c$")
list(APPEND HOST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/debug_prompt_stub.c")
list(APPEND HOST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/premake_log.c")
if(WIN32)
	list(APPEND HOST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/win_utf8_compat.c")
endif()

set(PMK_INCLUDES
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/host"
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake"
	"${LUACOMPAT_DIR}"
	${LUAJIT_INCLUDE_DIRS})

add_library(premake_core OBJECT ${HOST_SOURCES} "${GENERATED_SCRIPTS}")
add_dependencies(premake_core luajit_ext)
target_compile_definitions(premake_core PRIVATE ${PMK_DEFS})
target_compile_options(premake_core PRIVATE ${PMK_FORCE_INCLUDE})
target_include_directories(premake_core PUBLIC ${PMK_INCLUDES})

add_library(libpremake STATIC $<TARGET_OBJECTS:premake_core>)
set_target_properties(libpremake PROPERTIES OUTPUT_NAME premake)
target_include_directories(libpremake PUBLIC ${PMK_INCLUDES})
target_link_libraries(libpremake PUBLIC ${LUAJIT_LINK_LIBRARIES} ${PMK_LIBS})

add_executable(premake5-jit
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/premake_entry.c"
	$<TARGET_OBJECTS:premake_core>)
target_compile_definitions(premake5-jit PRIVATE ${PMK_DEFS})
target_include_directories(premake5-jit PRIVATE ${PMK_INCLUDES})
target_compile_options(premake5-jit PRIVATE ${PMK_FORCE_INCLUDE})
target_link_libraries(premake5-jit PRIVATE ${LUAJIT_LINK_LIBRARIES} ${PMK_LIBS})

if(WIN32 AND NOT MSVC)
	target_link_options(premake5-jit PRIVATE -municode)
endif()
