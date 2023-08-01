cmake_minimum_required(VERSION 3.14.0)
project(inference-engine VERSION 1.4.27 LANGUAGES CXX C CUDA)

include(FetchContent)
include(ExternalProject)

if(MSVC)
    set(CMAKE_CXX_STANDARD 20)
else()
    set(CMAKE_CXX_STANDARD 17)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# 头文件引用
include_directories(src)
include_directories(deps)
include_directories(libs/detection/include)
include_directories(src/nodes)
include_directories(src/modules)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

include_directories(${CMAKE_BINARY_DIR}/external/include)
link_directories(${CMAKE_BINARY_DIR}/external/lib)

set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)
list(APPEND CMAKE_FIND_ROOT_PATH ${EXTERNAL_INSTALL_LOCATION})
set(ENV{PKG_CONFIG_PATH} $ENV{PKG_CONFIG_PATH}:${EXTERNAL_INSTALL_LOCATION}/lib/pkgconfig)

# 引入CMake片段代码新方法
file(GLOB CMakeFiles "cmake/modules/*.cmake")

foreach(cmake_module_file IN LISTS CMakeFiles)
    get_filename_component(CMakeName ${cmake_module_file} NAME_WLE)
    get_filename_component(CMakePath ${cmake_module_file} DIRECTORY)
    message("LoadCMake: ${CMakeName}")
    include(${cmake_module_file})
endforeach(cmake_module_file)

# Windows MSVC的专有特性配置
if(MSVC)
    find_package(fmt REQUIRED)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/bigobj>")
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(_WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

add_compile_definitions(BOOST_ALLOW_DEPRECATED_HEADERS)

# if(UNIX)
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer -g -O0")
# endif()

if(CMAKE_CROSSCOMPILING)
    link_libraries(resolv rt)
endif()

get_git_commit_version(GIT_BRANCH GIT_HASH)
string(TIMESTAMP BUILD_TIME "%Y-%m-%d %H:%M:%S")
configure_file("version.h.in" "version.h")

file(GLOB SRC_FILES "src/*.c??" "src/nodes/*.c??")
add_library(ffmpegwrapper STATIC ${SRC_FILES} ${LibFiles})
target_link_libraries(ffmpegwrapper ${LinkLibraries}
    $<$<BOOL:$<PLATFORM_ID:Linux>>:dl>)

add_subdirectory(programs)

add_subdirectory(benchmarks)