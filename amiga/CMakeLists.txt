cmake_minimum_required(VERSION 3.22)
project(ReleasePacker C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Pure pack logic, no UI and no OS calls: the host build unit-tests it, the
# Amiga build compiles the same files for 68020.
add_library(packer STATIC src/template.c src/naming.c src/json.c src/groups.c src/date.c src/wrap.c src/nfo_uprough.c src/nfo_x.c
            src/stage.c src/archive.c)
target_include_directories(packer PUBLIC src)

# The UI is per platform: the MUI shell on Amiga (release headers only,
# classes by name, like UP-Demo), a version stub on host.
if(CMAKE_CROSSCOMPILING)
    add_executable(packer-cli src/main.c src/ui_amiga.c src/archive_run.c)
    target_include_directories(packer-cli PRIVATE vendor/up-mcc)
    target_link_libraries(packer-cli PRIVATE mui)
else()
    add_executable(packer-cli src/main.c src/ui_host.c)
endif()
target_link_libraries(packer-cli PRIVATE packer)

# 68k binaries cannot run here (vamos roundtrips come with the MUI shell);
# host builds run the suites through ctest.
if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    foreach(suite template naming json groups date wrap nfo_uprough nfo_x stage)
        add_executable(test_${suite} tests/test_${suite}.c)
        target_link_libraries(test_${suite} PRIVATE packer)
        add_test(NAME ${suite} COMMAND test_${suite} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endforeach()
endif()
