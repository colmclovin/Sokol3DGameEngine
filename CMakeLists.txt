cmake_minimum_required(VERSION 3.20)
project(Game)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)

# === Assimp: Use Release DLL + .lib ===
# Assimp headers
include_directories(
    ${CMAKE_SOURCE_DIR}/../External/Assimp/include         # for Assimp source headers
    ${CMAKE_SOURCE_DIR}/../External/Assimp/build/include   # for generated config.h
)
link_directories(${CMAKE_SOURCE_DIR}/../External/Assimp/build/lib/Release)

# === LIBRARY: Sokol ===
set(SOKOL_HEADERS
    ../External/Sokol/sokol_gfx.h
    ../External/Sokol/sokol_app.h
    ../External/Sokol/sokol_log.h
    ../External/Sokol/sokol_time.h
    ../External/Sokol/sokol_glue.h
    ../External/Sokol/sokol_audio.h
)
add_library(Sokol STATIC ../External/Sokol/sokol.c ${SOKOL_HEADERS})
set_target_properties(Sokol PROPERTIES LINKER_LANGUAGE C)

if (MSVC)
    target_compile_options(Sokol PRIVATE
        $<$<CONFIG:Debug>:/MTd>
        $<$<CONFIG:Release>:/MT>
    )
endif()

# === LIBRARY: Dear ImGui ===
set(IMGUI_SRC
    ../External/Imgui/imgui.cpp
    ../External/Imgui/imgui_draw.cpp
    ../External/Imgui/imgui_widgets.cpp
    ../External/Imgui/imgui_tables.cpp
    ../External/Imgui/imgui_demo.cpp
)
add_library(imgui STATIC ${IMGUI_SRC})
target_include_directories(imgui PUBLIC ../External/Imgui)

if (MSVC)
    target_compile_options(imgui PRIVATE
        $<$<CONFIG:Debug>:/MTd>
        $<$<CONFIG:Release>:/MT>
    )
endif()

if (MSVC)
    #set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MT")
endif()

# === EXECUTABLE ===
add_executable(Game WIN32 Main.cpp)
set_target_properties(Game PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# === Copy assets ===
add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:Game>/assets"
)

# === Always copy Release DLL ===
add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/../External/Assimp/build/bin/Release/assimp-vc143-mt.dll"
    "$<TARGET_FILE_DIR:Game>"
)

# === Link All Required Libraries ===
target_link_libraries(Game PRIVATE Sokol imgui assimp-vc143-mt.lib)

# === System libs (Linux only) ===
if (UNIX AND NOT APPLE)
    target_link_libraries(Game m dl pthread X11 Xi Xcursor GL)
endif()
