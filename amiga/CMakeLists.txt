cmake_minimum_required(VERSION 3.22)
project(ReleasePacker C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Pure pack logic, no UI and no OS calls: the host build unit-tests it, the
# Amiga build compiles the same files for 68020.
add_library(packer STATIC src/template.c src/naming.c src/json.c src/groups.c src/date.c src/wrap.c src/nfo_uprough.c src/nfo_x.c
            src/releases_client.c src/online.c src/json_write.c src/multipart.c src/door_repo_client.c src/stage.c src/archive.c src/archive_preview.c src/aminet.c src/credits.c src/aminet_types.c src/stale_drafts.c src/release_seed.c src/aminet_preview.c src/text_norm.c src/text_rows.c src/release_preview.c src/build_gate.c src/profile_save.c src/editor_logic.c src/last_choice.c src/platform_tag.c src/aminet_short.c src/paths_list.c)
target_include_directories(packer PUBLIC src)

# The UI is per platform: the MUI shell on Amiga (release headers only,
# classes by name, like UP-Demo), a version stub on host.
if(CMAKE_CROSSCOMPILING)
    add_executable(packer-cli src/main.c src/ui_amiga.c src/ui_editor.c src/archive_run.c)
    target_include_directories(packer-cli PRIVATE vendor/up-mcc/include)
    target_link_libraries(packer-cli PRIVATE mui http_amiga)
    # HTTP(S) transport: bsdsocket + AmiSSL (libraries opened by hand, no libamisslauto).
    add_library(http_amiga STATIC src/http_amiga.c)
    target_include_directories(http_amiga PUBLIC src vendor/amissl-sdk/include)
    add_executable(net-probe src/net_probe.c)
    target_link_libraries(net-probe PRIVATE http_amiga)
else()
    add_executable(packer-cli src/main.c src/ui_host.c)
endif()
target_link_libraries(packer-cli PRIVATE packer)

# 68k binaries cannot run here (vamos roundtrips come with the MUI shell);
# host builds run the suites through ctest.
if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    foreach(suite template naming json groups date wrap nfo_uprough nfo_x stage archive_preview releases_client online_writers online aminet aminet_types credits stale_drafts release_seed aminet_preview text_norm text_rows release_preview build_gate profile_save editor_logic http_timeout last_choice platform_tag aminet_short paths_list)
        add_executable(test_${suite} tests/test_${suite}.c)
        target_link_libraries(test_${suite} PRIVATE packer)
        add_test(NAME ${suite} COMMAND test_${suite} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endforeach()
endif()
