cmake_minimum_required(VERSION 2.9)
project(SPLONEBOX)

option(CLANG_ADDRESS_SANITIZER "Enable clang address sanitizer." OFF)
option(CLANG_MEMORY_SANITIZER "Enable clang memory sanitizer." OFF)
option(CLANG_THREAD_SANITIZER "Enable clang thread sanitizer." OFF)
option(CLANG_ANALYZER "Enable clang static analyzer." OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "CMAKE_BUILD_TYPE not given; setting to 'Debug'.")
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
  SET_PROPERTY(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
      Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wconversion")

add_definitions(-Wall -Wextra -pedantic -Wstrict-prototypes -std=gnu99
    -Wvariadic-macros -Wcast-align -Wcast-qual -Wshadow -Wwrite-strings
    -Wmissing-field-initializers -Wmissing-format-attribute -Wfloat-equal
    -Wundef -Wpointer-arith -Wstrict-overflow=5 -Wwrite-strings
    -Wswitch-default -Wswitch-enum -Wunreachable-code -Wformat-security)

set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g" CACHE STRING
    "Compiler flags for release builds with debug info." FORCE)

if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(DEBUG 1)
else()
  set(DEBUG 0)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND CMAKE_GENERATOR MATCHES "Ninja" AND
    NOT CLANG_ANALYZER)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcolor-diagnostics")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
endif()

if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-undefined")
  set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
  set(CMAKE_MODULE_LINKER_FLAGS
      "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--no-undefined")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(BSD REQUIRED)
endif()
find_package(LIBUV REQUIRED)
find_package(MSGPACK REQUIRED)
find_package(HIREDIS REQUIRED)
find_package(SODIUM REQUIRED)
find_package(CMOCKA REQUIRED)

include(CheckLibraryExists)

if((CLANG_ADDRESS_SANITIZER AND CLANG_MEMORY_SANITIZER)
    OR (CLANG_ADDRESS_SANITIZER AND CLANG_THREAD_SANITIZER)
    OR (CLANG_MEMORY_SANITIZER AND CLANG_THREAD_SANITIZER))
  message(FATAL_ERROR "Sanitizers cannot be enabled simultaneously.")
endif()

if(CLANG_ANALYZER)
  # Note that the default location of ccc-analyzer on Archlinux is used. this
  # may be different in other environments.
  set(CMAKE_C_COMPILER /usr/lib/clang-analyzer/scan-build/ccc-analyzer)
endif()

if((CLANG_ADDRESS_SANITIZER OR CLANG_MEMORY_SANITIZER OR CLANG_THREAD_SANITIZER)
    AND NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "Sanitizers are only supported for clang.")
endif()

if(CLANG_ADDRESS_SANITIZER)
  message(STATUS "Enabling clang address sanitizer and undefined behavior sanitizer.")
elseif(CLANG_MEMORY_SANITIZER)
  message(STATUS "Enabling Clang memory sanitizer.")
elseif(CLANG_MEMORY_SANITIZER)
  message(STATUS "Enabling Clang thread sanitizer.")
endif()

set(CMAKE_THREAD_PREFER_PTHREAD ON)
find_package(Threads REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

file(GLOB MANPAGES
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  man/splonebox.1)

# splonebox target sources
set(SPLONEBOX-SOURCES
  src/main.c
  src/sb-common.h
  src/khash.h
  src/kvec.h
  src/queue.h
  src/string.c
  src/reallocarray.c
  src/hashmap-string.c
  src/hashmap-uint64.c
  src/random.c
  src/optparser.c
  src/api/sb-api.h
  src/api/apikey.c
  src/api/register.c
  src/api/run.c
  src/rpc/sb-rpc.h
  src/rpc/connection/event.c
  src/rpc/connection/streamhandle.c
  src/rpc/connection/inputstream.c
  src/rpc/connection/outputstream.c
  src/rpc/connection/server.c
  src/rpc/connection/connection.c
  src/rpc/connection/dispatch.c
  src/rpc/msgpack/sb-msgpack-rpc.h
  src/rpc/msgpack/message.c
  src/rpc/msgpack/pack.c
  src/rpc/msgpack/unpack.c
  src/rpc/db/sb-db.h
  src/rpc/db/connect.c
  src/rpc/db/plugin.c
  src/rpc/db/function.c
)

# sb-plugin target sources
set(SB-PLUGIN-SOURCES
  src/sb-plugin.c
  src/sb-common.h
  src/string.c
  src/reallocarray.c
  src/api/sb-api.h
  src/api/apikey.c
  src/rpc/db/sb-db.h
  src/rpc/db/connect.c
  src/rpc/db/plugin.c
)

# splonebox test(s) sources
set(TEST-SOURCES
  src/sb-common.h
  src/khash.h
  src/queue.h
  src/string.c
  src/reallocarray.c
  src/hashmap-string.c
  src/hashmap-uint64.c
  src/random.c
  src/optparser.c
  src/api/sb-api.h
  src/api/apikey.c
  src/api/register.c
  src/api/run.c
  src/rpc/sb-rpc.h
  src/rpc/connection/event.c
  src/rpc/connection/streamhandle.c
  src/rpc/connection/inputstream.c
  src/rpc/connection/outputstream.c
  src/rpc/connection/server.c
  src/rpc/connection/connection.c
  src/rpc/connection/dispatch.c
  src/rpc/msgpack/sb-msgpack-rpc.h
  src/rpc/msgpack/message.c
  src/rpc/msgpack/pack.c
  src/rpc/msgpack/unpack.c
  src/rpc/db/sb-db.h
  src/rpc/db/connect.c
  src/rpc/db/plugin.c
  src/rpc/db/function.c
  test/main.c
  test/test-list.h
  test/helper-unix.h
  test/wrapper-functions.c
  test/helper-all.c
  test/helper-all.h
  test/unit/server-start.c
  test/unit/server-stop.c
  test/unit/pack-string.c
  test/unit/pack-uint64.c
  test/unit/pack-array.c
  test/unit/regression-issue-60.c
  test/unit/unpack-string.c
  test/unit/unpack-uint.c
  test/unit/unpack-array.c
  test/unit/apikey.c
  test/unit/dispatch-table-get.c
  test/unit/dispatch-handle-register.c
  test/unit/dispatch-handle-run.c
  test/unit/event-queue-put.c
  test/unit/event-queue-get.c
  test/functional/db-connect.c
  test/functional/db-plugin-add.c
  test/functional/db-apikey-verify.c
  test/functional/db-apikey-store.c
  test/functional/db-function-register.c
  test/functional/db-function-verify.c
  test/functional/db-function-flush-args.c
)

if(CLANG_ADDRESS_SANITIZER OR CLANG_MEMORY_SANITIZER OR CLANG_TSAN)
  list(APPEND gen_cflags "-DEXITFREE")
endif()

# sb target
add_executable(sb ${SPLONEBOX-SOURCES})
target_include_directories(sb PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_include_directories(sb SYSTEM PUBLIC
  ${BSD_INCLUDE_DIRS}
  ${LIBUV_INCLUDE_DIRS}
  ${MSGPACK_INCLUDE_DIRS}
  ${HIREDIS_INCLUDE_DIRS}
  ${SODIUM_INCLUDE_DIRS}
)
target_link_libraries(sb
  ${BSD_LIBRARIES}
  ${LIBUV_LIBRARIES}
  ${MSGPACK_LIBRARIES}
  ${HIREDIS_LIBRARIES}
  ${SODIUM_LIBRARIES}
)

# sb-plugin target
add_executable(sb-plugin ${SB-PLUGIN-SOURCES})
target_include_directories(sb-plugin PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_include_directories(sb-plugin SYSTEM PUBLIC
  ${BSD_INCLUDE_DIRS}
  ${HIREDIS_INCLUDE_DIRS}
)
target_link_libraries(sb-plugin ${BSD_LIBRARIES} ${HIREDIS_LIBRARIES})

# sb-test target
add_executable(sb-test ${TEST-SOURCES})
target_include_directories(sb-test PRIVATE "${PROJECT_SOURCE_DIR}/src"
  "${PROJECT_SOURCE_DIR}/test")
target_include_directories(sb-test SYSTEM PUBLIC
  ${BSD_INCLUDE_DIRS}
  ${LIBUV_INCLUDE_DIRS}
  ${MSGPACK_INCLUDE_DIRS}
  ${HIREDIS_INCLUDE_DIRS}
  ${SODIUM_INCLUDE_DIRS}
  ${CMOCKA_INCLUDE_DIRS}
)

# wrap some functions for testing
set_property(TARGET sb-test APPEND_STRING PROPERTY LINK_FLAGS "-Wl,--wrap=outputstream_write,--wrap=wait_for_response")

target_link_libraries(sb-test
  ${BSD_LIBRARIES}
  ${LIBUV_LIBRARIES}
  ${MSGPACK_LIBRARIES}
  ${HIREDIS_LIBRARIES}
  ${SODIUM_LIBRARIES}
  ${CMOCKA_LIBRARIES}
)

if(CLANG_ADDRESS_SANITIZER)
  set_property(TARGET sb APPEND_STRING PROPERTY COMPILE_FLAGS "-DEXITFREE ")
  set_property(TARGET sb APPEND_STRING PROPERTY COMPILE_FLAGS "-fno-sanitize-recover=undefined,integer -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize=address -fsanitize=undefined ")
  set_property(TARGET sb APPEND_STRING PROPERTY LINK_FLAGS "-fsanitize=address -fsanitize=undefined ")
  set_property(TARGET sb-plugin APPEND_STRING PROPERTY COMPILE_FLAGS "-DEXITFREE ")
  set_property(TARGET sb-plugin APPEND_STRING PROPERTY COMPILE_FLAGS "-fno-sanitize-recover=undefined,integer -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize=address -fsanitize=undefined ")
  set_property(TARGET sb-plugin APPEND_STRING PROPERTY LINK_FLAGS "-fsanitize=address -fsanitize=undefined ")
  set_property(TARGET sb-test APPEND_STRING PROPERTY COMPILE_FLAGS "-DEXITFREE ")
  set_property(TARGET sb-test APPEND_STRING PROPERTY COMPILE_FLAGS "-fno-sanitize-recover=undefined,integer -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize=address -fsanitize=undefined ")
  set_property(TARGET sb-test APPEND_STRING PROPERTY LINK_FLAGS "-fsanitize=address -fsanitize=undefined ")
elseif(CLANG_MEMORY_SANITIZER)
  set_property(TARGET sb APPEND_STRING PROPERTY COMPILE_FLAGS "-DEXITFREE ")
  set_property(TARGET sb APPEND_STRING PROPERTY COMPILE_FLAGS "-fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer -fno-optimize-sibling-calls ")
  set_property(TARGET sb APPEND_STRING PROPERTY LINK_FLAGS "-fsanitize=memory -fsanitize-memory-track-origins ")
  set_property(TARGET sb-plugin APPEND_STRING PROPERTY COMPILE_FLAGS "-DEXITFREE ")
  set_property(TARGET sb-plugin APPEND_STRING PROPERTY COMPILE_FLAGS "-fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer -fno-optimize-sibling-calls ")
  set_property(TARGET sb-plugin APPEND_STRING PROPERTY LINK_FLAGS "-fsanitize=memory -fsanitize-memory-track-origins ")
  set_property(TARGET sb-test APPEND_STRING PROPERTY COMPILE_FLAGS "-DEXITFREE ")
  set_property(TARGET sb-test APPEND_STRING PROPERTY COMPILE_FLAGS "-fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer -fno-optimize-sibling-calls ")
  set_property(TARGET sb-test APPEND_STRING PROPERTY LINK_FLAGS "-fsanitize=memory -fsanitize-memory-track-origins ")
elseif(CLANG_TSAN)
  set_property(TARGET sb APPEND_STRING PROPERTY COMPILE_FLAGS "-DEXITFREE ")
  set_property(TARGET sb APPEND_STRING PROPERTY COMPILE_FLAGS "-fsanitize=thread ")
  set_property(TARGET sb APPEND_STRING PROPERTY LINK_FLAGS "-fsanitize=thread ")
  set_property(TARGET sb-plugin APPEND_STRING PROPERTY COMPILE_FLAGS "-DEXITFREE ")
  set_property(TARGET sb-plugin APPEND_STRING PROPERTY COMPILE_FLAGS "-fsanitize=thread ")
  set_property(TARGET sb-plugin APPEND_STRING PROPERTY LINK_FLAGS "-fsanitize=thread ")
  set_property(TARGET sb-test APPEND_STRING PROPERTY COMPILE_FLAGS "-DEXITFREE ")
  set_property(TARGET sb-test APPEND_STRING PROPERTY COMPILE_FLAGS "-fsanitize=thread ")
  set_property(TARGET sb-test APPEND_STRING PROPERTY LINK_FLAGS "-fsanitize=thread ")
endif()
