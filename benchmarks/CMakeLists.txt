cmake_minimum_required(VERSION 3.0.0)
project(benchmarks)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

function(add_benchmark)
    get_filename_component(ProgramName ${ARGV0} NAME_WE)
    message("Benchmark: " ${ProgramName})
    add_executable(${ProgramName} ${ARGV})
    target_link_libraries(${ProgramName} ffmpegwrapper)
endfunction(add_benchmark)

function(add_benchmark_dir)
    file(GLOB src_files "${ARGV0}/*.c??")
    message(STATUS "Scan For: " ${ARGV0})
    foreach (file IN LISTS src_files)
        add_benchmark(${file})
    endforeach (file)
endfunction(add_benchmark_dir)

#
add_benchmark_dir(${CMAKE_CURRENT_SOURCE_DIR})

# OpenCV test
if (OpenCV_FOUND)
    add_benchmark_dir(${CMAKE_CURRENT_SOURCE_DIR}/OpenCV_benchmarks)
endif()