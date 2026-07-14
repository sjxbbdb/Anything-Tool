cmake_minimum_required(VERSION 3.20)
project(AnythingTool VERSION 0.1.2 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

enable_testing()

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_test(
  NAME v0_1_0_contract
  COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_SOURCE_DIR} python ${CMAKE_SOURCE_DIR}/tests/contract/test_v0_1_0_contract.py
)

add_test(
  NAME v0_1_1_security_contract
  COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_SOURCE_DIR} python ${CMAKE_SOURCE_DIR}/tests/security/test_v0_1_1_security_contract.py
)

add_test(
  NAME v0_1_2_security_behavior
  COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_SOURCE_DIR} python ${CMAKE_SOURCE_DIR}/tests/security/test_v0_1_2_security_behavior.py
)

if(NOT UNIX)
  message(WARNING "anythingd and anythingctl are Linux/Unix targets; this host can run contract tests only.")
  add_custom_target(anythingd COMMAND ${CMAKE_COMMAND} -E echo "anythingd is Linux-only in v0.1.0")
  add_custom_target(anythingctl COMMAND ${CMAKE_COMMAND} -E echo "anythingctl is Linux-only in v0.1.0")
  return()
endif()

add_library(anything_common
  src/common/approval.c
  src/common/audit.c
  src/common/config.c
  src/common/json.c
  src/common/policy.c
  src/common/rpc.c
  src/common/transport.c
  src/tools/sys_info.c
)

target_include_directories(anything_common PUBLIC include)
target_compile_definitions(anything_common PRIVATE _GNU_SOURCE)

add_executable(anythingd src/daemon/anythingd.c)
target_link_libraries(anythingd PRIVATE anything_common)
target_compile_definitions(anythingd PRIVATE _GNU_SOURCE)

add_executable(anythingctl src/cli/anythingctl.c)
target_link_libraries(anythingctl PRIVATE anything_common)
target_compile_definitions(anythingctl PRIVATE _GNU_SOURCE)
