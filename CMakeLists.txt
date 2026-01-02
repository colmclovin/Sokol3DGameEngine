cmake_minimum_required(VERSION 3.20)
project(Game)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)

# Assimp layout root
set(ASSIMP_ROOT "${CMAKE_SOURCE_DIR}/../External/Assimp")

# Include dirs (global fall-back)
include_directories(
    ${ASSIMP_ROOT}/include
    ${ASSIMP_ROOT}/build/include
)

# Sokol library
add_library(Sokol STATIC ../External/Sokol/sokol.c ../External/Sokol/sokol_gfx.h ../External/Sokol/sokol_app.h)
set_target_properties(Sokol PROPERTIES LINKER_LANGUAGE C)

# ImGui
add_library(imgui STATIC
    ../External/Imgui/imgui.cpp
    ../External/Imgui/imgui_draw.cpp
    ../External/Imgui/imgui_widgets.cpp
    ../External/Imgui/imgui_tables.cpp
    ../External/Imgui/imgui_demo.cpp
)
target_include_directories(imgui PUBLIC ../External/Imgui)

# Executable and sources
add_executable(Game WIN32
    Main.cpp
    src/Renderer/Renderer.cpp
    src/Audio/AudioEngine.cpp
    src/Model/ModelLoader.cpp
    src/UI/UIManager.cpp
    src/ThirdParty/HandmadeMathImpl.cpp
)

target_include_directories(Game PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${ASSIMP_ROOT}/include
    ${ASSIMP_ROOT}/build/include
    ${CMAKE_SOURCE_DIR}/../External/Imgui
    ${CMAKE_SOURCE_DIR}/../External/Sokol
)

# Force using Assimp Release build artifacts (useful when Debug build of Game should still link Release Assimp)
set(ASSIMP_LIB_PATH "${ASSIMP_ROOT}/build/lib/Release/assimp-vc143-mt.lib")
set(ASSIMP_DLL_PATH "${ASSIMP_ROOT}/build/bin/Release/assimp-vc143-mt.dll")

if (NOT EXISTS "${ASSIMP_LIB_PATH}")
    message(FATAL_ERROR "Assimp library not found at ${ASSIMP_LIB_PATH}. Build Assimp Release or update ASSIMP_ROOT.")
endif()
if (NOT EXISTS "${ASSIMP_DLL_PATH}")
    message(WARNING "Assimp DLL not found at ${ASSIMP_DLL_PATH}. The executable may fail at runtime if the DLL is missing.")
endif()

target_link_libraries(Game PRIVATE Sokol imgui "${ASSIMP_LIB_PATH}")

# Copy assets
add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:Game>/assets"
)

# Copy the Release Assimp DLL next to the executable
add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ASSIMP_DLL_PATH}"
    "$<TARGET_FILE_DIR:Game>"
)
