cmake_minimum_required(VERSION 3.14.0)
project(programs)

find_package(Boost COMPONENTS program_options QUIET)

function(add_program)
    get_filename_component(ProgramName ${ARGV0} NAME_WE)
    add_executable(${ProgramName} ${ARGV})
    target_link_libraries(${ProgramName} ffmpegwrapper ${LinkLibraries} gtest)
endfunction(add_program)

if(FFMPEG_FOUND)
    file(GLOB test_src_files
        "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg_samples/*.c??"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c??")
else()
    file(GLOB test_src_files
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c??")
endif()

foreach(file IN LISTS test_src_files)
    add_program(${file})
endforeach(file)

if(OpenCV_FOUND)
    add_subdirectory(CV_Samples)
endif()

set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/release")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    install(TARGETS node_srv RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
    install(DIRECTORY ${CMAKE_BINARY_DIR}/_deps/inference-sdk-src/lib/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        FILES_MATCHING PATTERN "*.so*")
endif()