cmake_minimum_required(VERSION 4.0)
project(RedsCore)
set(CMAKE_CXX_STANDARD 23)

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL TRACY_CALLSTACK=32)
include(cmake/dependencies.cmake)

add_library(RedsCore STATIC
        src/platform/archive.cpp
        src/platform/app_state.cpp
        src/platform/archive_manager.cpp
        src/platform/gltf_helper.cpp
        src/platform/logger.cpp
        src/platform/texture.cpp
        src/platform/texture_ops.cpp
        src/platform/file/file.cpp
        src/platform/file/memory_file.cpp
        src/platform/file/native_file.cpp
        src/stb.cpp
        src/utils/common.cpp
        src/utils/memory_debugger.cpp
        src/utils/simple_fileio.cpp
        src/utils/compression.cpp
        include/redscore/utils/png.hpp
)
target_include_directories(RedsCore PUBLIC include)
target_link_libraries(RedsCore PUBLIC tinygltf glm::glm tinycpng-static Tracy::TracyClient zlib-ng-static libzstd_static CLI11::CLI11 nlohmann_json::nlohmann_json)

add_executable(ToolRunner src/ToolRunner.cpp)
target_link_libraries(ToolRunner PUBLIC RedsCore)
