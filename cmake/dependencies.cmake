include(FetchContent)

FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG v0.13.1
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

set(TRACY_ONLY_LOCALHOST ON CACHE BOOL "" FORCE)
set(TRACY_NO_FRAME_IMAGE ON CACHE BOOL "" FORCE)
set(TRACY_NO_VSYNC_CAPTURE ON CACHE BOOL "" FORCE)
set(TRACY_IGNORE_MEMORY_FAULTS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tracy)

FetchContent_Declare(zlib-ng GIT_REPOSITORY https://github.com/zlib-ng/zlib-ng GIT_TAG 2.3.2)
set(WITH_GTEST OFF CACHE BOOL "Disable building tests for zlib-ng")
set(WITH_FUZZERS OFF CACHE BOOL "Disable building examples for zlib-ng")
set(WITH_BENCHMARKS OFF CACHE BOOL "Disable building benchmarks for zlib-ng")
set(WITH_BENCHMARK_APPS OFF CACHE BOOL "Disable building benchmarks-apps for zlib-ng")
set(ZLIB_COMPAT OFF CACHE BOOL "Disable zlib-ng zlib compatibility layer")
set(ZLIBNG_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(ZLIBNG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ZLIBNG_ENABLE_PACKAGE_CONFIG OFF CACHE BOOL "" FORCE)
set(ZLIB_ENABLE_OPTIMIZATIONS ON CACHE BOOL "Enable zlib-ng optimizations")
FetchContent_MakeAvailable(zlib-ng)

FetchContent_Declare(
        zstd
        GIT_REPOSITORY "https://github.com/facebook/zstd"
        GIT_TAG v1.5.7
        SOURCE_SUBDIR build/cmake
)

SET(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
SET(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zstd)

set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
FetchContent_Declare(
        tinygltf
        GIT_REPOSITORY "https://github.com/syoyo/tinygltf"
        GIT_TAG v2.9.7
)
FetchContent_MakeAvailable(tinygltf)

FetchContent_Declare(
        glm
        GIT_REPOSITORY "https://github.com/g-truc/glm"
        GIT_TAG 1.0.3
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
        cli11_proj
        QUIET
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.6.2
)
FetchContent_MakeAvailable(cli11_proj)

FetchContent_Declare(
        json
        QUIET
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
        tinycpng
        QUIET
        GIT_REPOSITORY https://github.com/REDxEYE/tinycpng.git
)
FetchContent_MakeAvailable(tinycpng)