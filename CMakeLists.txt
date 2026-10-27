cmake_minimum_required(VERSION 3.16...3.31)
project(crawl LANGUAGES C)

option(CRAWL_SYSTEM_SDL "Use system SDL3 (find_package) instead of vendor/SDL" OFF)
option(CRAWL_SAN "Build with ASan + UBSan" OFF)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# all binaries in b/bin, multi-config too
set(CRAWL_BIN ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CRAWL_BIN})
foreach(cfg Debug Release RelWithDebInfo MinSizeRel ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER ${cfg} CFG)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG} ${CRAWL_BIN})
endforeach()

# --- SDL3 ---
if(CRAWL_SYSTEM_SDL)
  find_package(SDL3 REQUIRED CONFIG)
  set(CRAWL_SDL SDL3::SDL3)
else()
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
  add_subdirectory(vendor/SDL EXCLUDE_FROM_ALL)
  set(CRAWL_SDL SDL3::SDL3-static)
endif()

# --- flags ---
function(crawl_std t)
  set_target_properties(${t} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS ON)
  if(MSVC)
    target_compile_definitions(${t} PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
  if(CRAWL_SAN AND NOT MSVC)
    set(san -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    target_compile_options(${t} PRIVATE ${san})
    target_link_options(${t} PRIVATE ${san})
  endif()
endfunction()

function(crawl_warn t)
  crawl_std(${t})
  if(MSVC)
    target_compile_options(${t} PRIVATE /W3)
  else()
    target_compile_options(${t} PRIVATE -Wall -Wextra -Wno-unused-parameter)
  endif()
endfunction()

# --- vendor code (no warnings) ---
add_library(crawlvendor STATIC vendor/tomlc17.c vendor/miniz.c)
crawl_std(crawlvendor)
target_include_directories(crawlvendor PUBLIC vendor)
if(UNIX AND NOT APPLE)
  target_link_libraries(crawlvendor PUBLIC m)
endif()

add_library(crawlui STATIC vendor/microui.c)
crawl_std(crawlui)
target_include_directories(crawlui PUBLIC vendor)

# --- core: no SDL ---
file(GLOB CORE_SRC CONFIGURE_DEPENDS src/core/*.c)
add_library(crawlcore STATIC ${CORE_SRC})
crawl_warn(crawlcore)
target_include_directories(crawlcore PUBLIC src vendor)
target_link_libraries(crawlcore PUBLIC crawlvendor)

# --- crawl / crawl-rt ---
file(GLOB APP_SRC CONFIGURE_DEPENDS
  src/gfx/*.c src/game/*.c src/plat/*.c src/rules/*.c src/rules/*/*.c)
file(GLOB EDIT_SRC CONFIGURE_DEPENDS src/edit/*.c)

foreach(t crawl crawl-rt)
  add_executable(${t} src/main.c ${APP_SRC})
  crawl_warn(${t})
  target_link_libraries(${t} PRIVATE crawlcore crawlui ${CRAWL_SDL})
endforeach()
if(EDIT_SRC)
  target_sources(crawl PRIVATE ${EDIT_SRC})
endif()
target_compile_definitions(crawl-rt PRIVATE CRAWL_NO_EDITOR)

# --- tests: tests/t_*.c, run from the source root ---
enable_testing()
file(GLOB TEST_SRC CONFIGURE_DEPENDS tests/t_*.c)
foreach(src ${TEST_SRC})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src})
  crawl_warn(${name})
  target_link_libraries(${name} PRIVATE crawlcore)
  add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endforeach()

add_test(NAME check_demo COMMAND crawl check examples/demo
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(check_demo PROPERTIES ENVIRONMENT SDL_VIDEODRIVER=dummy)
