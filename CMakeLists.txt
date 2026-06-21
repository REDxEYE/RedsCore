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
        src/platform/texture/texture.cpp
        src/platform/texture/texture_ops.cpp
        src/platform/texture/writers/dds_writer.cpp
        src/platform/texture/readers/dds_reader.cpp
        src/platform/file/file.cpp
        src/platform/file/memory_file.cpp
        src/platform/file/native_file.cpp
        src/stb.cpp
        src/utils/common.cpp
        src/utils/memory_debugger.cpp
        src/utils/simple_fileio.cpp
        src/utils/compression.cpp
        src/platform/vertex_buffer.cpp
        src/platform/model.cpp
)

target_precompile_headers(RedsCore PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h>"
)

target_include_directories(RedsCore PUBLIC include)
target_link_libraries(RedsCore PUBLIC Tracy::TracyClient glm::glm tinycpng-static zlib-ng-static libzstd_static CLI11::CLI11 nlohmann_json::nlohmann_json)
target_compile_definitions(RedsCore PRIVATE TRACY_MEMORY TRACY_ON_DEMAND)
