cmake_minimum_required(VERSION 3.20)
project(RedsCore)
set(CMAKE_CXX_STANDARD 23)

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL TRACY_CALLSTACK=32)
include(cmake/dependencies.cmake)

add_library(RedsCore STATIC
        src/platform/archive.cpp
        src/platform/app_state.cpp
        src/platform/archive_manager.cpp
        src/platform/model/writers/gltf/serialize.cpp
        src/platform/model/writers/gltf/scene.cpp
        src/platform/model/writers/gltf/mesh.cpp
        src/platform/model/writers/gltf/material.cpp
        src/platform/model/writers/gltf/helper_data.cpp
        src/platform/model/writers/gltf/helper_scene.cpp
        src/platform/model/writers/gltf/helper_save.cpp
        src/platform/logger.cpp
        src/platform/texture/texture.cpp
        src/platform/texture/texture_ops.cpp
        src/platform/texture/writers/dds_writer.cpp
        src/platform/texture/readers/dds_reader.cpp
        src/platform/file/file.cpp
        src/platform/file/memory_file.cpp
        src/platform/file/native_file.cpp
        src/platform/file/memmap_file.cpp
        src/stb.cpp
        src/utils/common.cpp
        src/utils/simple_fileio.cpp
        src/utils/compression.cpp
        src/platform/vertex_buffer.cpp
        src/platform/model.cpp
        src/platform/model/writers/cast/writer.cpp
        src/platform/model/writers/cast/transform.cpp
        src/platform/model/writers/cast/mesh.cpp
        src/platform/model/writers/cast/model.cpp
        src/platform/model/writers/cast/scene.cpp
        src/platform/texture_export_session.cpp
)

target_precompile_headers(RedsCore PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h>"
)

target_include_directories(RedsCore PUBLIC include)
target_link_libraries(RedsCore PUBLIC Tracy::TracyClient glm::glm tinycpng-static zlib-ng-static libzstd_static CLI11::CLI11 nlohmann_json::nlohmann_json)
#target_compile_definitions(RedsCore PRIVATE TRACY_MEMORY TRACY_ON_DEMAND)



option(REDSCORE_BUILD_MODEL_TESTS "Build virtual model serialization tests" OFF)
if(REDSCORE_BUILD_MODEL_TESTS)
    enable_testing()
    add_executable(RedsCoreModelTests tests/virtual_model_test.cpp)
    target_link_libraries(RedsCoreModelTests PRIVATE RedsCore)
    add_test(NAME RedsCore.VirtualModel COMMAND RedsCoreModelTests)
    add_executable(RedsCoreCastTests tests/virtual_model_cast_test.cpp)
    target_link_libraries(RedsCoreCastTests PRIVATE RedsCore)
    add_test(NAME RedsCore.Cast COMMAND RedsCoreCastTests)
    add_executable(RedsCoreTextureTests tests/texture_export_session_test.cpp)
    target_link_libraries(RedsCoreTextureTests PRIVATE RedsCore)
    add_test(NAME RedsCore.TextureExport COMMAND RedsCoreTextureTests)
endif()
