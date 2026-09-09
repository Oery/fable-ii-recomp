# fable_ii - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(fable_ii LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(FABLEII_SOURCES
    src/main.cpp
    src/hooks.cpp
)

if(WIN32)
    add_executable(fable_ii WIN32 ${FABLEII_SOURCES})
else()
    add_executable(fable_ii ${FABLEII_SOURCES})
endif()

rexglue_setup_target(fable_ii GPU_PLUGINS xenos)

# Workaround for SDK 0.10.0-dev.gc94f5eb: host sources added by
# rexglue_configure_target (windowed_app_main_sdl.cpp, rex_app.cpp) need
# imgui headers and the rexui object library. Upstream's host template
# does not propagate these via rex::runtime.
if(TARGET imgui::imgui)
    target_include_directories(fable_ii PRIVATE "${REXSDK_DIR}/thirdparty/imgui")
    target_link_libraries(fable_ii PRIVATE imgui::imgui)
endif()
if(TARGET rexui)
    target_link_libraries(fable_ii PRIVATE rexui)
endif()
